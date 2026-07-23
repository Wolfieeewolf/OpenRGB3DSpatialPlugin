package me.wolfi.openrgb;

import com.mojang.blaze3d.platform.NativeImage;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.block.BlockStateModelSet;
import net.minecraft.client.resources.model.sprite.Material;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.resources.Resource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Disk-backed average colours for all block textures, plus a small LRU of downscaled
 * sprite pixels for UV sampling. Never retains full HD pack textures permanently —
 * that previously pinned multi‑GiB heaps and froze the machine at ~99% RAM.
 */
final class BlockTexturePrecache
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final byte[] MAGIC = {'O', 'R', 'G', 'B', 'T', 'P', 'C', '1'};
    private static final int FORMAT_VERSION = 1;
    private static final long BUILD_BUDGET_NS = 1_250_000L;
    private static final int BUILD_MAX_SPRITES_PER_TICK = 16;
    private static final String CACHE_FILE = "block_texture_precache.bin";
    /** Default solid UV dim — plugin can raise to 2K/4K via Room Ambilight Texture quality. */
    private static final int UV_CACHE_DIM_DEFAULT = 512;
    private static final int MAX_PIXEL_ENTRIES_CAP = 200;
    /** Headroom for 2K/4K UV buffers on fast machines (LRU still evicts aggressively). */
    private static final long MAX_PIXEL_BYTES = 512L << 20; // 512 MiB

    private static final ConcurrentHashMap<Identifier, BlockDisplayColorSampler.TextureSample> ENTRIES =
            new ConcurrentHashMap<>();
    private static final Object PIXEL_LOCK = new Object();
    private static final LinkedHashMap<Identifier, SpritePixels> PIXEL_LRU =
            new LinkedHashMap<>(256, 0.75f, true);
    private static long pixelBytes = 0L;
    /** Live solid UV max side (64…4096). Anim strips use a capped fraction. */
    private static volatile int uvCacheMaxDim = UV_CACHE_DIM_DEFAULT;
    private static final ArrayDeque<Identifier> BUILD_QUEUE = new ArrayDeque<>();
    /** On-demand UV pixel loads for Room Ambilight hits (never decode on the ray hot path). */
    private static final ArrayDeque<Identifier> PIXEL_REQUEST_QUEUE = new ArrayDeque<>();
    private static final Set<Identifier> PIXEL_REQUESTED = ConcurrentHashMap.newKeySet();
    private static final long PIXEL_LOAD_BUDGET_NS = 1_000_000L;
    private static final int PIXEL_LOAD_MAX_PER_TICK = 2;
    /**
     * HD / 4K pack textures must not be fully scanned on the client tick — a single
     * 4096² pass freezes entities/doors until the queue drains (or the world reloads).
     */
    private static final int MAX_MAIN_THREAD_TEXEL_AREA = 512 * 512;
    private static final Set<Identifier> PIXEL_SKIP = ConcurrentHashMap.newKeySet();
    private static volatile int contentHash = 0;
    private static volatile boolean diskLoaded = false;
    private static volatile boolean buildScheduled = false;
    private static volatile boolean building = false;
    private static volatile int buildTotal = 0;

    private BlockTexturePrecache()
    {
    }

    static BlockDisplayColorSampler.TextureSample get(Identifier spriteId)
    {
        return ENTRIES.get(spriteId);
    }

    static SpritePixels getPixels(Identifier spriteId)
    {
        synchronized(PIXEL_LOCK)
        {
            return PIXEL_LRU.get(spriteId);
        }
    }

    /**
     * Apply plugin Texture quality. Clears the UV LRU when the dim changes so sprites
     * re-decode at the new resolution (async, not on the ray path).
     */
    static void setUvMaxDim(int dim)
    {
        final int snapped = RoomSampleConfigReader.snapUvTextureDim(dim);
        if(snapped == uvCacheMaxDim)
        {
            return;
        }
        uvCacheMaxDim = snapped;
        clearPixels();
        LOGGER.info("Room Ambilight UV texture quality set to {} (anim max {})",
                snapped,
                animMaxDim(snapped));
    }

    static int currentUvMaxDim()
    {
        return uvCacheMaxDim;
    }

    private static int animMaxDim(int solidDim)
    {
        // Fire/portal strips stack frames — keep them smaller than solids.
        return Math.min(512, Math.max(64, solidDim / 2));
    }

    private static int maxPixelEntriesForDim(int dim)
    {
        final long per = Math.max(1L, (long)dim * (long)dim * 4L);
        final int byBytes = (int)Math.max(4L, Math.min((long)MAX_PIXEL_ENTRIES_CAP, MAX_PIXEL_BYTES / per));
        return byBytes;
    }

    /** Average only — never retains full pixel buffers. */
    static BlockDisplayColorSampler.TextureSample getOrLoad(Minecraft client, Identifier spriteId)
    {
        final BlockDisplayColorSampler.TextureSample cached = get(spriteId);
        if(cached != null)
        {
            return cached;
        }
        final BlockDisplayColorSampler.TextureSample avg = readAverageOnly(client, spriteId);
        if(avg != null)
        {
            ENTRIES.putIfAbsent(spriteId, avg);
        }
        return avg;
    }

    static SpritePixels getOrLoadPixels(Minecraft client, Identifier spriteId)
    {
        final SpritePixels cached = getPixels(spriteId);
        if(cached != null)
        {
            return cached;
        }
        final SpritePixels pixels = readSpritePixelsDownscaled(client, spriteId);
        if(pixels != null)
        {
            putPixels(spriteId, pixels);
            ENTRIES.putIfAbsent(spriteId, pixels.average());
        }
        return pixels;
    }

    /**
     * Queue a UV buffer load for a sprite seen by a Room Ambilight ray. Cheap; decode happens in
     * {@link #tick} under a soft budget so the client tick never hitch-decodes PNGs.
     */
    static void requestPixels(Identifier spriteId)
    {
        if(spriteId == null || getPixels(spriteId) != null || PIXEL_SKIP.contains(spriteId))
        {
            return;
        }
        if(PIXEL_REQUESTED.add(spriteId))
        {
            synchronized(PIXEL_LOCK)
            {
                PIXEL_REQUEST_QUEUE.addLast(spriteId);
            }
        }
    }

    static void onModelsReady(Minecraft client)
    {
        if(client == null)
        {
            return;
        }
        final int hash = computeContentHash(client);
        if(diskLoaded && hash == contentHash && !ENTRIES.isEmpty())
        {
            return;
        }
        if(!diskLoaded || hash != contentHash)
        {
            ENTRIES.clear();
            clearPixels();
            contentHash = hash;
            if(loadFromDisk(hash))
            {
                LOGGER.info("Block texture pre-cache loaded ({} textures, pixel LRU empty)", ENTRIES.size());
                return;
            }
        }
        scheduleBuild(client, hash);
    }

    static boolean isBuilding()
    {
        return building;
    }

    static void tick(Minecraft client)
    {
        if(client == null)
        {
            return;
        }
        // Always drain UV requests — Room Ambilight detail depends on this, not the average warm-up.
        processPixelRequests(client);

        if(!building || BUILD_QUEUE.isEmpty())
        {
            return;
        }
        final long budgetStart = System.nanoTime();
        int remaining = BUILD_MAX_SPRITES_PER_TICK;
        while(remaining-- > 0 && !BUILD_QUEUE.isEmpty())
        {
            if((System.nanoTime() - budgetStart) > BUILD_BUDGET_NS)
            {
                break;
            }
            final Identifier spriteId = BUILD_QUEUE.pollFirst();
            if(spriteId == null || ENTRIES.containsKey(spriteId))
            {
                continue;
            }
            // Averages only during warm-up — UV pixels load on demand via requestPixels.
            final BlockDisplayColorSampler.TextureSample avg = readAverageOnly(client, spriteId);
            if(avg != null)
            {
                ENTRIES.put(spriteId, avg);
            }
        }
        if(BUILD_QUEUE.isEmpty())
        {
            building = false;
            // Keep any UV buffers already warmed for Room Ambilight — do not clearPixels here.
            scheduleSaveToDisk(contentHash);
            LOGGER.info("Block texture pre-cache built ({} averages; UV pixels load on demand)", ENTRIES.size());
        }
    }

    private static void processPixelRequests(Minecraft client)
    {
        final long budgetStart = System.nanoTime();
        int remaining = PIXEL_LOAD_MAX_PER_TICK;
        while(remaining-- > 0)
        {
            if((System.nanoTime() - budgetStart) > PIXEL_LOAD_BUDGET_NS)
            {
                break;
            }
            final Identifier spriteId;
            synchronized(PIXEL_LOCK)
            {
                spriteId = PIXEL_REQUEST_QUEUE.pollFirst();
            }
            if(spriteId == null)
            {
                break;
            }
            if(getPixels(spriteId) != null)
            {
                PIXEL_REQUESTED.remove(spriteId);
                continue;
            }
            final SpritePixels pixels = readSpritePixelsDownscaled(client, spriteId);
            if(pixels != null)
            {
                putPixels(spriteId, pixels);
                ENTRIES.putIfAbsent(spriteId, pixels.average());
            }
            else
            {
                // Oversized / failed decode — do not keep re-queueing every ray hit.
                PIXEL_SKIP.add(spriteId);
            }
            PIXEL_REQUESTED.remove(spriteId);
        }
    }

    private static void putPixels(Identifier spriteId, SpritePixels pixels)
    {
        if(spriteId == null || pixels == null || pixels.argb() == null)
        {
            return;
        }
        final long add = (long)pixels.argb().length * 4L;
        synchronized(PIXEL_LOCK)
        {
            final SpritePixels prev = PIXEL_LRU.remove(spriteId);
            if(prev != null && prev.argb() != null)
            {
                pixelBytes -= (long)prev.argb().length * 4L;
            }
            while((PIXEL_LRU.size() >= maxPixelEntriesForDim(uvCacheMaxDim) || pixelBytes + add > MAX_PIXEL_BYTES)
                    && !PIXEL_LRU.isEmpty())
            {
                final Map.Entry<Identifier, SpritePixels> eldest = PIXEL_LRU.entrySet().iterator().next();
                PIXEL_LRU.remove(eldest.getKey());
                if(eldest.getValue() != null && eldest.getValue().argb() != null)
                {
                    pixelBytes -= (long)eldest.getValue().argb().length * 4L;
                }
            }
            if(pixelBytes < 0L)
            {
                pixelBytes = 0L;
            }
            if(add <= MAX_PIXEL_BYTES)
            {
                PIXEL_LRU.put(spriteId, pixels);
                pixelBytes += add;
            }
        }
    }

    private static void clearPixels()
    {
        synchronized(PIXEL_LOCK)
        {
            PIXEL_LRU.clear();
            pixelBytes = 0L;
            PIXEL_REQUEST_QUEUE.clear();
        }
        PIXEL_REQUESTED.clear();
        PIXEL_SKIP.clear();
    }

    private static void scheduleBuild(Minecraft client, int hash)
    {
        if(buildScheduled && contentHash == hash)
        {
            return;
        }
        buildScheduled = true;
        contentHash = hash;
        BUILD_QUEUE.clear();
        building = true;
        clearPixels();

        final Set<Identifier> unique = collectSpriteIds(client);
        buildTotal = unique.size();
        BUILD_QUEUE.addAll(unique);
        LOGGER.info("Building block texture pre-cache ({} unique textures, averages only, ~{} ms/tick)...",
                buildTotal, BUILD_BUDGET_NS / 1_000_000L);
    }

    private static Set<Identifier> collectSpriteIds(Minecraft client)
    {
        final Set<Identifier> unique = new HashSet<>();
        try
        {
            final BlockStateModelSet modelSet = client.getModelManager().getBlockStateModelSet();
            for(Block block : BuiltInRegistries.BLOCK)
            {
                for(BlockState state : block.getStateDefinition().getPossibleStates())
                {
                    try
                    {
                        final Material.Baked particle = modelSet.getParticleMaterial(state);
                        if(particle != null && particle.sprite() != null)
                        {
                            unique.add(particle.sprite().contents().name());
                        }
                    }
                    catch(Throwable t)
                    {
                        QuietCatch.debug(LOGGER, "block texture precache helper failed", t);
                    }
                }
            }
        }
        catch(Throwable t)
        {
            LOGGER.warn("Failed to enumerate block textures for pre-cache", t);
        }
        return unique;
    }

    private static int computeContentHash(Minecraft client)
    {
        final StringBuilder sb = new StringBuilder(256);
        sb.append(FORMAT_VERSION);
        sb.append('|');
        final var mc = FabricLoader.getInstance().getModContainer("minecraft");
        if(mc.isPresent())
        {
            sb.append(mc.get().getMetadata().getVersion().getFriendlyString());
        }
        sb.append('|');
        sb.append(FabricLoader.getInstance().getModContainer("openrgb-minecraft-sender")
                .map(c -> c.getMetadata().getVersion().getFriendlyString())
                .orElse("unknown"));
        try
        {
            for(String packId : client.getResourcePackRepository().getSelectedIds())
            {
                sb.append('|').append(packId);
            }
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block texture precache helper failed", t);
        }
        return sb.toString().hashCode();
    }

    private static Path cachePath() throws java.io.IOException
    {
        return OpenRGBShmPaths.resolveFile(CACHE_FILE);
    }

    private static boolean loadFromDisk(int expectedHash)
    {
        try
        {
            final Path path = cachePath();
            if(!Files.isRegularFile(path))
            {
                diskLoaded = true;
                return false;
            }
            try(DataInputStream in = new DataInputStream(Files.newInputStream(path)))
            {
                final byte[] magic = in.readNBytes(MAGIC.length);
                if(magic.length != MAGIC.length || !java.util.Arrays.equals(magic, MAGIC))
                {
                    diskLoaded = true;
                    return false;
                }
                final int format = in.readInt();
                final int hash = in.readInt();
                final int count = in.readInt();
                if(format != FORMAT_VERSION || hash != expectedHash || count < 0 || count > 500_000)
                {
                    diskLoaded = true;
                    return false;
                }
                ENTRIES.clear();
                for(int i = 0; i < count; i++)
                {
                    final String id = in.readUTF();
                    final int r = in.readUnsignedByte();
                    final int g = in.readUnsignedByte();
                    final int b = in.readUnsignedByte();
                    final int a = in.readUnsignedByte();
                    ENTRIES.put(Identifier.parse(id),
                            new BlockDisplayColorSampler.TextureSample((r << 16) | (g << 8) | b, a));
                }
            }
            diskLoaded = true;
            buildScheduled = false;
            building = false;
            BUILD_QUEUE.clear();
            clearPixels();
            return true;
        }
        catch(Throwable t)
        {
            LOGGER.debug("Block texture pre-cache load failed", t);
            diskLoaded = true;
            return false;
        }
    }

    private static void scheduleSaveToDisk(int hash)
    {
        final List<Map.Entry<Identifier, BlockDisplayColorSampler.TextureSample>> snapshot =
                new ArrayList<>(ENTRIES.entrySet());
        final Thread saver = new Thread(() -> saveSnapshotToDisk(hash, snapshot), "openrgb-texture-precache-save");
        saver.setDaemon(true);
        saver.setPriority(Thread.NORM_PRIORITY - 1);
        saver.start();
    }

    private static void saveSnapshotToDisk(int hash,
            List<Map.Entry<Identifier, BlockDisplayColorSampler.TextureSample>> snapshot)
    {
        try
        {
            final Path path = cachePath();
            final Path tmp = path.resolveSibling(CACHE_FILE + ".tmp");
            try(DataOutputStream out = new DataOutputStream(Files.newOutputStream(tmp)))
            {
                out.write(MAGIC);
                out.writeInt(FORMAT_VERSION);
                out.writeInt(hash);
                out.writeInt(snapshot.size());
                for(var entry : snapshot)
                {
                    out.writeUTF(entry.getKey().toString());
                    final int rgb = entry.getValue().rgb();
                    out.writeByte((rgb >> 16) & 0xFF);
                    out.writeByte((rgb >> 8) & 0xFF);
                    out.writeByte(rgb & 0xFF);
                    out.writeByte(entry.getValue().averageAlpha());
                }
            }
            Files.move(tmp, path, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
            diskLoaded = true;
        }
        catch(Throwable t)
        {
            LOGGER.warn("Failed to save block texture pre-cache", t);
        }
    }

    static BlockDisplayColorSampler.TextureSample readTextureAverage(Minecraft client, Identifier spriteId)
    {
        return readAverageOnly(client, spriteId);
    }

    static SpritePixels readSpritePixels(Minecraft client, Identifier spriteId)
    {
        return readSpritePixelsDownscaled(client, spriteId);
    }

    private static BlockDisplayColorSampler.TextureSample readAverageOnly(Minecraft client, Identifier spriteId)
    {
        if(client == null || spriteId == null)
        {
            return null;
        }
        final Identifier resourceId = spriteId.withPrefix("textures/").withSuffix(".png");
        try
        {
            final Resource resource = client.getResourceManager().getResourceOrThrow(resourceId);
            try(InputStream in = resource.open())
            {
                final byte[] prefix = in.readNBytes(24);
                final int[] wh = peekPngSize(prefix);
                if(wh != null && (long)wh[0] * (long)wh[1] > MAX_MAIN_THREAD_TEXEL_AREA * 4L)
                {
                    // Extremely large assets: skip entirely (map/face fallbacks cover colour).
                    return null;
                }
                final byte[] rest = in.readAllBytes();
                final byte[] all = concatBytes(prefix, rest);
                try(NativeImage image = NativeImage.read(new ByteArrayInputStream(all)))
                {
                    long sumR = 0;
                    long sumG = 0;
                    long sumB = 0;
                    long sumA = 0;
                    int count = 0;
                    final int w = image.getWidth();
                    final int h = image.getHeight();
                    if(w <= 0 || h <= 0 || w > 4096 || h > 4096)
                    {
                        return null;
                    }
                    // Stride so HD pack textures stay within the per-tick build budget.
                    int step = 1;
                    final long area = (long)w * (long)h;
                    if(area > MAX_MAIN_THREAD_TEXEL_AREA)
                    {
                        step = (int)Math.ceil(Math.sqrt(area / (double)MAX_MAIN_THREAD_TEXEL_AREA));
                        step = Math.max(2, step);
                    }
                    for(int y = 0; y < h; y += step)
                    {
                        for(int x = 0; x < w; x += step)
                        {
                            final int px = image.getPixel(x, y);
                            final int a = (px >>> 24) & 0xFF;
                            if(a < 24)
                            {
                                continue;
                            }
                            sumR += (px >> 16) & 0xFF;
                            sumG += (px >> 8) & 0xFF;
                            sumB += px & 0xFF;
                            sumA += a;
                            count++;
                        }
                    }
                    if(count <= 0)
                    {
                        return null;
                    }
                    final int r = (int)(sumR / count);
                    final int g = (int)(sumG / count);
                    final int b = (int)(sumB / count);
                    final int avgA = (int)(sumA / count);
                    return new BlockDisplayColorSampler.TextureSample((r << 16) | (g << 8) | b, avgA);
                }
            }
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block texture precache helper failed", t);
            return null;
        }
    }

    /** Downscaled UV buffer — animation strips keep stacked frames for live fire/portal. */
    private static SpritePixels readSpritePixelsDownscaled(Minecraft client, Identifier spriteId)
    {
        if(client == null || spriteId == null)
        {
            return null;
        }
        final Identifier resourceId = spriteId.withPrefix("textures/").withSuffix(".png");
        try
        {
            final Resource resource = client.getResourceManager().getResourceOrThrow(resourceId);
            try(InputStream in = resource.open())
            {
                final byte[] prefix = in.readNBytes(24);
                final int[] wh = peekPngSize(prefix);
                if(wh != null && (long)wh[0] * (long)wh[1] > MAX_MAIN_THREAD_TEXEL_AREA)
                {
                    return null;
                }
                final byte[] rest = in.readAllBytes();
                final byte[] all = concatBytes(prefix, rest);
                try(NativeImage image = NativeImage.read(new ByteArrayInputStream(all)))
                {
                    final int srcW = image.getWidth();
                    final int srcH = image.getHeight();
                    if(srcW <= 0 || srcH <= 0 || srcW > 4096 || srcH > 4096)
                    {
                        return null;
                    }
                    // Skip UV buffers for huge pack textures — decode/scan freezes the client tick.
                    if((long)srcW * (long)srcH > MAX_MAIN_THREAD_TEXEL_AREA)
                    {
                        return null;
                    }

                    // Vertical strip of square frames (fire, campfire, portal, …).
                    int frameCount = 1;
                    int frameSrcH = srcH;
                    if(srcH > srcW && srcW > 0 && (srcH % srcW) == 0)
                    {
                        frameCount = srcH / srcW;
                        frameSrcH = srcW;
                    }

                    final int solidDim = uvCacheMaxDim;
                    final int maxDim = frameCount > 1 ? animMaxDim(solidDim) : solidDim;
                    final int dstW = Math.min(srcW, maxDim);
                    final int dstH = Math.min(frameSrcH, maxDim);
                    final int[] argb = new int[dstW * dstH * frameCount];
                    long sumR = 0;
                    long sumG = 0;
                    long sumB = 0;
                    long sumA = 0;
                    int count = 0;

                    for(int frame = 0; frame < frameCount; frame++)
                    {
                        final int srcY0 = frame * frameSrcH;
                        final int frameBase = frame * dstW * dstH;
                        for(int y = 0; y < dstH; y++)
                        {
                            final int sy = srcY0 + Math.min(frameSrcH - 1, (y * frameSrcH) / dstH);
                            for(int x = 0; x < dstW; x++)
                            {
                                final int sx = Math.min(srcW - 1, (x * srcW) / dstW);
                                final int px = image.getPixel(sx, sy);
                                argb[frameBase + y * dstW + x] = px;
                                final int a = (px >>> 24) & 0xFF;
                                if(a < 24)
                                {
                                    continue;
                                }
                                sumR += (px >> 16) & 0xFF;
                                sumG += (px >> 8) & 0xFF;
                                sumB += px & 0xFF;
                                sumA += a;
                                count++;
                            }
                        }
                    }
                    if(count <= 0)
                    {
                        return null;
                    }
                    final int r = (int)(sumR / count);
                    final int g = (int)(sumG / count);
                    final int b = (int)(sumB / count);
                    final int avgA = (int)(sumA / count);
                    return new SpritePixels(dstW, dstH, argb, (r << 16) | (g << 8) | b, avgA, frameCount);
                }
            }
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block texture precache helper failed", t);
            return null;
        }
    }

    /** PNG signature + IHDR width/height without decoding the full image. */
    private static int[] peekPngSize(byte[] prefix)
    {
        if(prefix == null || prefix.length < 24)
        {
            return null;
        }
        // \x89PNG\r\n\x1a\n
        if((prefix[0] & 0xFF) != 0x89 || prefix[1] != 'P' || prefix[2] != 'N' || prefix[3] != 'G')
        {
            return null;
        }
        if(prefix[12] != 'I' || prefix[13] != 'H' || prefix[14] != 'D' || prefix[15] != 'R')
        {
            return null;
        }
        final int w = ((prefix[16] & 0xFF) << 24) | ((prefix[17] & 0xFF) << 16)
                | ((prefix[18] & 0xFF) << 8) | (prefix[19] & 0xFF);
        final int h = ((prefix[20] & 0xFF) << 24) | ((prefix[21] & 0xFF) << 16)
                | ((prefix[22] & 0xFF) << 8) | (prefix[23] & 0xFF);
        if(w <= 0 || h <= 0)
        {
            return null;
        }
        return new int[] {w, h};
    }

    private static byte[] concatBytes(byte[] a, byte[] b)
    {
        if(a == null || a.length == 0)
        {
            return b != null ? b : new byte[0];
        }
        if(b == null || b.length == 0)
        {
            return a;
        }
        final byte[] out = new byte[a.length + b.length];
        System.arraycopy(a, 0, out, 0, a.length);
        System.arraycopy(b, 0, out, a.length, b.length);
        return out;
    }
}
