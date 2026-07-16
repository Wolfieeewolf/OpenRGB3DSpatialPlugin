package me.wolfi.openrgb;

import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.block.BlockStateModelSet;
import net.minecraft.client.renderer.block.dispatch.BlockStateModel;
import net.minecraft.client.renderer.block.dispatch.BlockStateModelPart;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.core.Direction;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Phase 2: per-block-state, per-face average texture colours from all model quads (not just the
 * particle icon). Loaded from disk when valid; built incrementally on the client thread.
 */
final class BlockFaceColorCache
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final byte[] MAGIC = {'O', 'R', 'G', 'B', 'F', 'C', 'E', '1'};
    private static final int FORMAT_VERSION = 1;
    private static final int BUILD_BATCH_PER_TICK = 24;
    private static final int FACE_COUNT = 6;
    private static final String CACHE_FILE = "block_face_precache.bin";

    /** RGB per {@link Direction#get3DDataValue()} (0..5), plus index 6 = model-wide average. */
    record FaceColors(int[] rgbByFace)
    {
        int rgbFor(Direction face)
        {
            if(face != null)
            {
                final int idx = face.get3DDataValue();
                if(idx >= 0 && idx < FACE_COUNT)
                {
                    final int rgb = rgbByFace[idx];
                    if(rgb >= 0)
                    {
                        return rgb;
                    }
                }
            }
            return rgbByFace[FACE_COUNT];
        }
    }

    private static final ConcurrentHashMap<Integer, FaceColors> ENTRIES = new ConcurrentHashMap<>();
    private static final ArrayDeque<Integer> BUILD_QUEUE = new ArrayDeque<>();
    private static volatile int contentHash = 0;
    private static volatile boolean diskLoaded = false;
    private static volatile boolean building = false;

    private BlockFaceColorCache()
    {
    }

    static FaceColors get(int blockStateId)
    {
        return ENTRIES.get(blockStateId);
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
        ENTRIES.clear();
        contentHash = hash;
        if(loadFromDisk(hash))
        {
            LOGGER.info("Block face pre-cache loaded ({} block states)", ENTRIES.size());
            return;
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
            final Integer stateId = BUILD_QUEUE.pollFirst();
            if(stateId == null || ENTRIES.containsKey(stateId))
            {
                continue;
            }
            final BlockState state = Block.stateById(stateId);
            if(state != null)
            {
                final FaceColors colors = buildFaceColors(client, state);
                if(colors != null)
                {
                    ENTRIES.put(stateId, colors);
                }
            }
        }
        if(BUILD_QUEUE.isEmpty())
        {
            building = false;
            saveToDisk(contentHash);
            LOGGER.info("Block face pre-cache built ({} block states)", ENTRIES.size());
        }
    }

    private static void scheduleBuild(Minecraft client, int hash)
    {
        contentHash = hash;
        BUILD_QUEUE.clear();
        building = true;
        for(Block block : BuiltInRegistries.BLOCK)
        {
            for(BlockState state : block.getStateDefinition().getPossibleStates())
            {
                BUILD_QUEUE.addLast(Block.getId(state));
            }
        }
        LOGGER.info("Building block face pre-cache ({} block states, {} per tick)...",
                BUILD_QUEUE.size(), BUILD_BATCH_PER_TICK);
    }

    private static FaceColors buildFaceColors(Minecraft client, BlockState state)
    {
        try
        {
            final BlockStateModelSet modelSet = client.getModelManager().getBlockStateModelSet();
            final BlockStateModel model = modelSet.get(state);
            if(model == null)
            {
                return null;
            }

            final long[] sumR = new long[FACE_COUNT + 1];
            final long[] sumG = new long[FACE_COUNT + 1];
            final long[] sumB = new long[FACE_COUNT + 1];
            final int[] count = new int[FACE_COUNT + 1];

            final List<BlockStateModelPart> parts = new ArrayList<>();
            model.collectParts(RandomSource.create(Block.getId(state)), parts);
            for(BlockStateModelPart part : parts)
            {
                for(Direction direction : Direction.values())
                {
                    final List<BakedQuad> quads = part.getQuads(direction);
                    if(quads == null || quads.isEmpty())
                    {
                        continue;
                    }
                    for(BakedQuad quad : quads)
                    {
                        if(quad.materialInfo() == null || quad.materialInfo().sprite() == null)
                        {
                            continue;
                        }
                        final BlockDisplayColorSampler.TextureSample texture = BlockTexturePrecache.getOrLoad(
                                client, quad.materialInfo().sprite().contents().name());
                        if(texture == null)
                        {
                            continue;
                        }
                        final int faceIdx = direction.get3DDataValue();
                        sumR[faceIdx] += (texture.rgb() >> 16) & 0xFF;
                        sumG[faceIdx] += (texture.rgb() >> 8) & 0xFF;
                        sumB[faceIdx] += texture.rgb() & 0xFF;
                        count[faceIdx]++;
                        sumR[FACE_COUNT] += (texture.rgb() >> 16) & 0xFF;
                        sumG[FACE_COUNT] += (texture.rgb() >> 8) & 0xFF;
                        sumB[FACE_COUNT] += texture.rgb() & 0xFF;
                        count[FACE_COUNT]++;
                    }
                }
            }

            final int[] rgbByFace = new int[FACE_COUNT + 1];
            for(int i = 0; i <= FACE_COUNT; i++)
            {
                if(count[i] > 0)
                {
                    rgbByFace[i] = ((int)(sumR[i] / count[i]) << 16)
                            | ((int)(sumG[i] / count[i]) << 8)
                            | (int)(sumB[i] / count[i]);
                }
                else
                {
                    rgbByFace[i] = -1;
                }
            }

            if(count[FACE_COUNT] <= 0)
            {
                return null;
            }
            return new FaceColors(rgbByFace);
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block face cache helper failed", t);
            return null;
        }
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
            QuietCatch.debug(LOGGER, "block face cache helper failed", t);
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
                    final int stateId = in.readInt();
                    final int[] rgbByFace = new int[FACE_COUNT + 1];
                    for(int f = 0; f <= FACE_COUNT; f++)
                    {
                        final int r = in.readUnsignedByte();
                        final int g = in.readUnsignedByte();
                        final int b = in.readUnsignedByte();
                        rgbByFace[f] = (r << 16) | (g << 8) | b;
                    }
                    ENTRIES.put(stateId, new FaceColors(rgbByFace));
                }
            }
            diskLoaded = true;
            building = false;
            BUILD_QUEUE.clear();
            return true;
        }
        catch(Throwable t)
        {
            LOGGER.debug("Block face pre-cache load failed", t);
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
                    out.writeInt(entry.getKey());
                    for(int rgb : entry.getValue().rgbByFace())
                    {
                        if(rgb < 0)
                        {
                            out.writeByte(0);
                            out.writeByte(0);
                            out.writeByte(0);
                        }
                        else
                        {
                            out.writeByte((rgb >> 16) & 0xFF);
                            out.writeByte((rgb >> 8) & 0xFF);
                            out.writeByte(rgb & 0xFF);
                        }
                    }
                }
            }
            Files.move(tmp, path, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
            diskLoaded = true;
        }
        catch(Throwable t)
        {
            LOGGER.warn("Failed to save block face pre-cache", t);
        }
    }
}
