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
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Steam-style disk pre-cache for block texture average colours. Built once per MC / resource-pack
 * fingerprint, loaded instantly on later launches; missing entries are filled incrementally on the
 * client thread without blocking gameplay.
 */
final class BlockTexturePrecache
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final byte[] MAGIC = {'O', 'R', 'G', 'B', 'T', 'P', 'C', '1'};
    private static final int FORMAT_VERSION = 1;
    private static final int BUILD_BATCH_PER_TICK = 48;
    private static final String CACHE_FILE = "block_texture_precache.bin";

    private static final ConcurrentHashMap<Identifier, BlockDisplayColorSampler.TextureSample> ENTRIES =
            new ConcurrentHashMap<>();
    private static final ArrayDeque<Identifier> BUILD_QUEUE = new ArrayDeque<>();
    private static volatile int contentHash = 0;
    private static volatile boolean diskLoaded = false;
    private static volatile boolean buildScheduled = false;
    private static volatile boolean building = false;
    private static volatile int buildTotal = 0;
    private static volatile int buildDone = 0;

    private BlockTexturePrecache()
    {
    }

    static BlockDisplayColorSampler.TextureSample get(Identifier spriteId)
    {
        return ENTRIES.get(spriteId);
    }

    static BlockDisplayColorSampler.TextureSample getOrLoad(Minecraft client, Identifier spriteId)
    {
        final BlockDisplayColorSampler.TextureSample cached = get(spriteId);
        if(cached != null)
        {
            return cached;
        }
        final BlockDisplayColorSampler.TextureSample sample = readTextureAverage(client, spriteId);
        if(sample != null)
        {
            ENTRIES.putIfAbsent(spriteId, sample);
        }
        return sample;
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
            contentHash = hash;
            if(loadFromDisk(hash))
            {
                LOGGER.info("Block texture pre-cache loaded ({} textures)", ENTRIES.size());
                return;
            }
        }
        scheduleBuild(client, hash);
    }

    static void tick(Minecraft client)
    {
        if(!building || client == null || BUILD_QUEUE.isEmpty())
        {
            return;
        }
        int batch = BUILD_BATCH_PER_TICK;
        while(batch-- > 0 && !BUILD_QUEUE.isEmpty())
        {
            final Identifier spriteId = BUILD_QUEUE.pollFirst();
            if(spriteId == null || ENTRIES.containsKey(spriteId))
            {
                continue;
            }
            final BlockDisplayColorSampler.TextureSample sample = readTextureAverage(client, spriteId);
            if(sample != null)
            {
                ENTRIES.put(spriteId, sample);
            }
            buildDone++;
        }
        if(BUILD_QUEUE.isEmpty())
        {
            building = false;
            saveToDisk(contentHash);
            LOGGER.info("Block texture pre-cache built ({} textures)", ENTRIES.size());
        }
    }

    static boolean isWarm()
    {
        return diskLoaded && !building && BUILD_QUEUE.isEmpty();
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
        buildDone = 0;

        final Set<Identifier> unique = collectSpriteIds(client);
        buildTotal = unique.size();
        BUILD_QUEUE.addAll(unique);
        LOGGER.info("Building block texture pre-cache ({} unique textures, {} per tick)...",
                buildTotal, BUILD_BATCH_PER_TICK);
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
                    catch(Throwable ignored)
                    {
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
        catch(Throwable ignored)
        {
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
            return true;
        }
        catch(Throwable t)
        {
            LOGGER.debug("Block texture pre-cache load failed", t);
            diskLoaded = true;
            return false;
        }
    }

    private static void saveToDisk(int hash)
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
                out.writeInt(ENTRIES.size());
                for(var entry : ENTRIES.entrySet())
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
                for(int y = 0; y < h; y++)
                {
                    for(int x = 0; x < w; x++)
                    {
                        final int argb = image.getPixel(x, y);
                        final int a = (argb >> 24) & 0xFF;
                        if(a < 24)
                        {
                            continue;
                        }
                        sumR += (argb >> 16) & 0xFF;
                        sumG += (argb >> 8) & 0xFF;
                        sumB += argb & 0xFF;
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
        catch(Throwable ignored)
        {
            return null;
        }
    }
}
