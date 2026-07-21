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
    /** UV pixel cache: downscale side length (keeps memory bounded with HD packs). */
    private static final int UV_CACHE_MAX_DIM = 64;
    private static final int MAX_PIXEL_ENTRIES = 160;
    private static final long MAX_PIXEL_BYTES = 16L << 20; // 16 MiB hard cap

    private static final ConcurrentHashMap<Identifier, BlockDisplayColorSampler.TextureSample> ENTRIES =
            new ConcurrentHashMap<>();
    private static final Object PIXEL_LOCK = new Object();
    private static final LinkedHashMap<Identifier, SpritePixels> PIXEL_LRU =
            new LinkedHashMap<>(256, 0.75f, true);
    private static long pixelBytes = 0L;
    private static final ArrayDeque<Identifier> BUILD_QUEUE = new ArrayDeque<>();
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
        if(!building || client == null || BUILD_QUEUE.isEmpty())
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
            // Averages only during warm-up — do not retain ARGB for every atlas sprite.
            final BlockDisplayColorSampler.TextureSample avg = readAverageOnly(client, spriteId);
            if(avg != null)
            {
                ENTRIES.put(spriteId, avg);
            }
        }
        if(BUILD_QUEUE.isEmpty())
        {
            building = false;
            clearPixels();
            scheduleSaveToDisk(contentHash);
            LOGGER.info("Block texture pre-cache built ({} averages, pixel cache cleared)", ENTRIES.size());
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
            while((PIXEL_LRU.size() >= MAX_PIXEL_ENTRIES || pixelBytes + add > MAX_PIXEL_BYTES)
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
        }
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
            try(InputStream in = resource.open();
                NativeImage image = NativeImage.read(in))
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
                for(int y = 0; y < h; y++)
                {
                    for(int x = 0; x < w; x++)
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
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block texture precache helper failed", t);
            return null;
        }
    }

    /** Downscaled UV buffer — never keeps raw HD atlas sprites in RAM. */
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
            try(InputStream in = resource.open();
                NativeImage image = NativeImage.read(in))
            {
                final int srcW = image.getWidth();
                final int srcH = image.getHeight();
                if(srcW <= 0 || srcH <= 0 || srcW > 4096 || srcH > 4096)
                {
                    return null;
                }
                final int dstW = Math.min(srcW, UV_CACHE_MAX_DIM);
                final int dstH = Math.min(srcH, UV_CACHE_MAX_DIM);
                final int[] argb = new int[dstW * dstH];
                long sumR = 0;
                long sumG = 0;
                long sumB = 0;
                long sumA = 0;
                int count = 0;
                for(int y = 0; y < dstH; y++)
                {
                    final int sy = Math.min(srcH - 1, (y * srcH) / dstH);
                    for(int x = 0; x < dstW; x++)
                    {
                        final int sx = Math.min(srcW - 1, (x * srcW) / dstW);
                        final int px = image.getPixel(sx, sy);
                        argb[y * dstW + x] = px;
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
                return new SpritePixels(dstW, dstH, argb, (r << 16) | (g << 8) | b, avgA);
            }
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block texture precache helper failed", t);
            return null;
        }
    }
}
