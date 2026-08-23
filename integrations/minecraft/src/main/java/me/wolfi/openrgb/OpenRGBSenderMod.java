package me.wolfi.openrgb;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.resource.ResourceManagerHelper;
import net.fabricmc.fabric.api.resource.SimpleSynchronousResourceReloadListener;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.server.packs.PackType;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Locale;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class OpenRGBSenderMod implements ClientModInitializer
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final long DAMAGE_DIR_MAX_AGE_MS = 450L;

    private static OpenRGBSenderMod instance;

    private float lastHealth = -1.0f;
    private int clientTickCounter = 0;

    private static DatagramSocket sharedUdpSocket;
    private static InetAddress sharedUdpAddress;
    private static String cachedHost = null;
    private static int cachedPort = -1;
    private static final Object sharedUdpLock = new Object();
    /** Increments every client tick (not gated by telemetryTickDivisor) for maximum room sample rate. */
    private int roomTickCounter = 0;
    private final RoomSampleConfigReader roomSampleConfigReader = new RoomSampleConfigReader();
    private final RoomSampleFrameShmWriter roomSampleFrameShmWriter = new RoomSampleFrameShmWriter();
    private int lastLoggedRoomConfigId = -1;
    private int lastLoggedRoomSizeX = -1;
    private int lastLoggedRoomSizeY = -1;
    private int lastLoggedRoomSizeZ = -1;
    private long lastWaitingForConfigLogMs = 0L;
    /** Reused across frames to avoid per-tick allocation. Resized only when grid dimensions change. */
    private byte[] roomSampleRgbaBuffer = new byte[0];
    /** Round-robin cursor for mapped LED cubemap texels under the soft tick budget. */
    private int roomSampleCubemapCursor = 0;
    /** Soft cap per tick so Room Ambilight never stalls a client frame. */
    private static final long ROOM_SAMPLE_BUDGET_NS = 2_500_000L;
    /** Hard ceiling for all OpenRGB client-tick work (precache + room + telemetry setup). */
    private static final long CLIENT_TICK_WORK_BUDGET_NS = 5_000_000L;
    private static final double ENTITY_PROBE_RADIUS_MAX = 28.0;
    private int roomSampleSkipTicks = 0;
    private static final ThreadLocal<int[]> ROOM_CELL_SCRATCH = ThreadLocal.withInitial(() -> new int[4]);
    private static final ThreadLocal<int[]> ROOM_ACCUM_SCRATCH = ThreadLocal.withInitial(() -> new int[4]);
    private static final ThreadLocal<int[]> ROOM_LAYER_SCRATCH = ThreadLocal.withInitial(() -> new int[4]);
    /** Weighted RGB + weight sum for immersion veil (sunflower yellow, vines, snow, …). */
    private static final ThreadLocal<int[]> ROOM_IMMERSION_SCRATCH = ThreadLocal.withInitial(() -> new int[4]);
    private static final ThreadLocal<float[]> ROOM_CUBEMAP_DIR = ThreadLocal.withInitial(() -> new float[3]);
    /** When LED-first config changes, wipe so unused texels cannot keep stale colours. */
    private int roomSampleClearedForConfigId = -1;
    /**
     * Do not publish SHM frames until every mapped LED texel has been written once —
     * avoids the black→colour pop-in flash while the buffer warms.
     */
    private boolean roomSampleAwaitingFirstPass = true;

    @Override
    public void onInitializeClient()
    {
        instance = this;
        OpenRGBSenderConfig.get();
        // LZ4 + mmap publish runs off-tick; UDP notify uses the shared socket under lock.
        roomSampleFrameShmWriter.setPublishListener((frameId, timestampMs, configId) ->
        {
            synchronized(sharedUdpLock)
            {
                if(sharedUdpSocket == null || sharedUdpSocket.isClosed() || sharedUdpAddress == null)
                {
                    return;
                }
                try
                {
                    sendRoomSampleShmNotify(sharedUdpSocket, sharedUdpAddress, frameId, timestampMs, configId);
                }
                catch(Exception e)
                {
                    QuietCatch.debug(LOGGER, "room sample shm notify failed", e);
                }
            }
        });
        ResourceManagerHelper.get(PackType.CLIENT_RESOURCES).registerReloadListener(
                new SimpleSynchronousResourceReloadListener()
                {
                    @Override
                    public Identifier getFabricId()
                    {
                        return Identifier.fromNamespaceAndPath("openrgb-minecraft-sender", "block_model_precache");
                    }

                    @Override
                    public void onResourceManagerReload(net.minecraft.server.packs.resources.ResourceManager resourceManager)
                    {
                        final Minecraft client = Minecraft.getInstance();
                        if(client != null)
                        {
                            client.execute(() ->
                            {
                                BlockTexturePrecache.onModelsReady(client);
                                BlockFaceColorCache.onModelsReady(client);
                            });
                        }
                    }
                });
    }

    public static void invalidateUdpTarget()
    {
        synchronized(sharedUdpLock)
        {
            if(sharedUdpSocket != null)
            {
                sharedUdpSocket.close();
                sharedUdpSocket = null;
            }
            sharedUdpAddress = null;
            cachedHost = null;
            cachedPort = -1;
        }
    }

    /** Snapshot for Mod Menu status (OpenRGB → mod SHM config). */
    public static final class LinkStatus
    {
        public final boolean linked;
        public final int configId;
        public final int faceSize;
        public final int uvDim;
        public final int ledTexels;
        public final boolean skyEnabled;

        LinkStatus(boolean linked, int configId, int faceSize, int uvDim, int ledTexels, boolean skyEnabled)
        {
            this.linked = linked;
            this.configId = configId;
            this.faceSize = faceSize;
            this.uvDim = uvDim;
            this.ledTexels = ledTexels;
            this.skyEnabled = skyEnabled;
        }
    }

    public static LinkStatus getLinkStatus()
    {
        if(instance == null)
        {
            return new LinkStatus(false, 0, 0, 0, 0, false);
        }
        try
        {
            final RoomSampleConfigReader.Config cfg = instance.roomSampleConfigReader.readLatest();
            if(cfg == null || !cfg.isEnabled())
            {
                return new LinkStatus(false, 0, 0, 0, 0, false);
            }
            final int leds = cfg.hasImportantCells() ? cfg.importantFlatIndices.length : 0;
            final int face = cfg.isCubemap() ? cfg.sizeX : 0;
            return new LinkStatus(true, cfg.configId, face, cfg.uvTextureMaxDim, leds, cfg.isSkyEnabled());
        }
        catch(Exception e)
        {
            QuietCatch.debug(LOGGER, "link status read failed", e);
            return new LinkStatus(false, 0, 0, 0, 0, false);
        }
    }

    /**
     * Called from {@link me.wolfi.openrgb.mixin.MinecraftClientMixin} at end of each client tick (main thread only).
     */
    public static void onClientTick(Minecraft client)
    {
        if(instance == null || client == null)
        {
            return;
        }
        final long tickStart = System.nanoTime();
        BlockTexturePrecache.tick(client);
        if((System.nanoTime() - tickStart) < CLIENT_TICK_WORK_BUDGET_NS)
        {
            BlockFaceColorCache.tick(client);
        }
        // Room samples every tick — UV pixels load async; face-average warm-up must not starve LED fill
        // (that caused the startup pop-in flash under the old every-4th-tick + tiny budget path).
        if((System.nanoTime() - tickStart) < CLIENT_TICK_WORK_BUDGET_NS)
        {
            instance.tickRoomSamples(client, tickStart);
        }
        else if(instance.roomSampleSkipTicks < 40)
        {
            instance.roomSampleSkipTicks = 40;
        }
        instance.tickSendTelemetry(client);
    }

    private void tickRoomSamples(Minecraft client, long tickStartNs)
    {
        if(client.player == null || client.level == null)
        {
            return;
        }
        final OpenRGBSenderConfig cfg = OpenRGBSenderConfig.get();
        if(!cfg.enabled || !cfg.sendRoomSampleFrames)
        {
            return;
        }
        if(roomSampleSkipTicks > 0)
        {
            roomSampleSkipTicks--;
            return;
        }
        roomTickCounter++;
        try
        {
            // Read config once and share with both interval check and frame send.
            final RoomSampleConfigReader.Config roomCfg = roomSampleConfigReader.readLatest();
            if(roomCfg != null)
            {
                BlockTexturePrecache.setUvMaxDim(roomCfg.uvTextureMaxDim);
            }
            logRoomConfigState(roomCfg);
            final int roomInterval = effectiveRoomSampleInterval(cfg, roomCfg);
            if(roomTickCounter % roomInterval == 0)
            {
                DatagramSocket socket = ensureSharedUdp(cfg);
                sendRoomSampleFrame(socket, sharedUdpAddress, client.player, client.level, roomCfg, tickStartNs);
            }
        }
        catch(Exception e)
        {
            LOGGER.warn("Room sample send failed: {}", e.toString());
        }
    }

    private void tickSendTelemetry(Minecraft client)
    {
        if(client.player == null || client.level == null)
        {
            return;
        }
        final OpenRGBSenderConfig cfg = OpenRGBSenderConfig.get();
        if(!cfg.enabled)
        {
            return;
        }
        clientTickCounter++;
        if(clientTickCounter % cfg.telemetryTickDivisor != 0)
        {
            return;
        }
        try
        {
            DatagramSocket socket = ensureSharedUdp(cfg);
            InetAddress address = sharedUdpAddress;
            LocalPlayer player = client.player;
            sendPlayerPose(socket, address, player, cfg);
            sendHealthState(socket, address, player);
            sendDamageEventIfNeeded(socket, address, player);
        }
        catch(Exception e)
        {
            LOGGER.warn("Telemetry send failed: {}", e.toString());
        }
    }

    private static DatagramSocket ensureSharedUdp(OpenRGBSenderConfig cfg) throws Exception
    {
        synchronized(sharedUdpLock)
        {
            if(sharedUdpSocket != null && !sharedUdpSocket.isClosed() &&
               cfg.host.equals(cachedHost) && cfg.port == cachedPort && sharedUdpAddress != null)
            {
                return sharedUdpSocket;
            }
            if(sharedUdpSocket != null)
            {
                sharedUdpSocket.close();
            }
            sharedUdpSocket = new DatagramSocket();
            sharedUdpAddress = InetAddress.getByName(cfg.host);
            cachedHost = cfg.host;
            cachedPort = cfg.port;
            return sharedUdpSocket;
        }
    }

    private void sendPlayerPose(DatagramSocket socket, InetAddress address, LocalPlayer player, OpenRGBSenderConfig cfg) throws Exception
    {
        Vec3 look = player.getViewVector(1.0f);
        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"player_pose\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"fx\":%.5f,\"fy\":%.5f,\"fz\":%.5f,\"ux\":0.0,\"uy\":1.0,\"uz\":0.0,\"blocks_per_m\":%.4f}",
                System.currentTimeMillis(),
                player.getX(), player.getEyeY(), player.getZ(),
                look.x, look.y, look.z,
                cfg.blocksPerMeter);
        sendJson(socket, address, json);
    }

    private void sendHealthState(DatagramSocket socket, InetAddress address, LocalPlayer player) throws Exception
    {
        final int hunger = player.getFoodData().getFoodLevel();
        final int maxHunger = 20;
        final int air = player.getAirSupply();
        final int maxAir = player.getMaxAirSupply();
        final var held = player.getMainHandItem();
        final boolean hasDurability = !held.isEmpty() && held.getMaxDamage() > 0;
        final int durabilityMax = hasDurability ? Math.max(1, held.getMaxDamage()) : 1;
        final int durability = hasDurability ? Math.max(0, durabilityMax - held.getDamageValue()) : 0;
        final float hpPerHeart = 2.0f;
        final float hearts = player.getHealth() / hpPerHeart;
        final float heartsMax = Math.max(0.01f, player.getMaxHealth() / hpPerHeart);
        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"health_state\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"health\":%.4f,\"health_max\":%.4f,\"hp_per_heart\":%.4f,\"hearts\":%.4f,\"hearts_max\":%.4f,\"hunger\":%d,\"hunger_max\":%d,\"air\":%d,\"air_max\":%d,\"item_durability_valid\":%s,\"item_durability\":%d,\"item_durability_max\":%d}",
                System.currentTimeMillis(),
                player.getHealth(), player.getMaxHealth(),
                hpPerHeart, hearts, heartsMax,
                hunger, maxHunger, air, maxAir,
                hasDurability ? "true" : "false", durability, durabilityMax);
        sendJson(socket, address, json);
    }

    private static final int MAX_VIEWPORT_BLEND_LAYERS = 8;
    /** Extra marches allowed while skipping head-height cutouts (tall grass fields). */
    private static final int MAX_VIEWPORT_RAY_MARCHES = 28;
    private static final double VIEWPORT_RAY_EPS = 0.05;
    /** Inside this range, sparse plants are immersion only — do not trap every LED ray. */
    private static final double SPARSE_NEAR_SKIP_DIST = 2.35;
    private static final int VIEWPORT_WATER_COARSE_SAMPLES = 4;

    private static void clearRoomSampleRgba(int[] out)
    {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
    }

    /** Front-to-back compositing: nearer layer over existing accumulation. */
    private static void compositeViewportLayer(int[] accum, int[] layer)
    {
        final float la = layer[3] / 255.0f;
        if(la <= 0.001f)
        {
            return;
        }
        final float aa = accum[3] / 255.0f;
        final float lr = layer[0] * la;
        final float lg = layer[1] * la;
        final float lb = layer[2] * la;
        final float ar = accum[0] * aa;
        final float ag = accum[1] * aa;
        final float ab = accum[2] * aa;
        final float outA = la + aa * (1.0f - la);
        if(outA <= 0.001f)
        {
            clearRoomSampleRgba(accum);
            return;
        }
        final float invA = 1.0f / outA;
        accum[0] = ColorMath.clamp255(Math.round((lr + ar * (1.0f - la)) * invA));
        accum[1] = ColorMath.clamp255(Math.round((lg + ag * (1.0f - la)) * invA));
        accum[2] = ColorMath.clamp255(Math.round((lb + ab * (1.0f - la)) * invA));
        accum[3] = ColorMath.clamp255(Math.round(outA * 255.0f));
    }

    private static void writeAccumToRoomSample(int[] accum, int[] out)
    {
        if(accum[3] <= 0)
        {
            clearRoomSampleRgba(out);
            return;
        }
        out[0] = accum[0];
        out[1] = accum[1];
        out[2] = accum[2];
        out[3] = accum[3];
    }

    private static BlockHitResult raycastViewport(Level world, LocalPlayer player, Vec3 from, Vec3 to)
    {
        return world.clip(new ClipContext(
                from,
                to,
                ClipContext.Block.OUTLINE,
                ClipContext.Fluid.ANY,
                player));
    }

    private static BlockPos findFluidAlongRay(Level world, Vec3 eye, Vec3 target, boolean lava)
    {
        for(int i = 1; i <= VIEWPORT_WATER_COARSE_SAMPLES; i++)
        {
            final double t = i / (double)VIEWPORT_WATER_COARSE_SAMPLES;
            final BlockPos pos = BlockPos.containing(eye.lerp(target, t));
            final var fluid = world.getFluidState(pos);
            if(lava ? fluid.is(Fluids.LAVA) : fluid.is(Fluids.WATER))
            {
                return pos;
            }
        }
        return null;
    }

    private static Vec3 advanceRayPastBlock(Vec3 hitLocation, Vec3 marchDir)
    {
        return hitLocation.add(marchDir.scale(VIEWPORT_RAY_EPS));
    }

    /** Jump clear of a plant block so dense tall grass cannot re-trap the same cell. */
    private static Vec3 advanceRayPastBlockPos(Vec3 hitLocation, Vec3 marchDir, BlockPos pos)
    {
        Vec3 p = hitLocation.add(marchDir.scale(VIEWPORT_RAY_EPS));
        for(int i = 0; i < 28; i++)
        {
            if(!BlockPos.containing(p).equals(pos))
            {
                return p;
            }
            p = p.add(marchDir.scale(0.12));
        }
        return hitLocation.add(marchDir.scale(1.15));
    }

    private static void clearImmersion(int[] imm)
    {
        imm[0] = 0;
        imm[1] = 0;
        imm[2] = 0;
        imm[3] = 0;
    }

    private static void addImmersionSample(int[] imm, int[] layer)
    {
        if(layer == null || layer[3] <= 0)
        {
            return;
        }
        final int w = Math.max(1, layer[3]);
        imm[0] += layer[0] * w;
        imm[1] += layer[1] * w;
        imm[2] += layer[2] * w;
        imm[3] += w;
    }

    /** Gather real nearby cutout colours when the ray did not already sample them. */
    private static void sampleNearbyImmersionColors(Level world, Vec3 eye, int[] imm, int[] layer)
    {
        // Feet + 4 neighbours only — the old 3x3x3 (27) scan was a hitch source in tall grass.
        final BlockPos center = BlockPos.containing(eye);
        final int[][] offsets = {
                {0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}
        };
        for(int[] o : offsets)
        {
            final BlockPos pos = center.offset(o[0], o[1], o[2]);
            final BlockState state = world.getBlockState(pos);
            if(!BlockDisplayColorSampler.isSparseSkyCutout(state)
                    && !BlockDisplayColorSampler.isSoftWeatherVolume(state))
            {
                continue;
            }
            BlockDisplayColorSampler.sampleViewportLayer(world, pos, state, null, eye, layer);
            addImmersionSample(imm, layer);
        }
    }

    /**
     * Soft immersion veil from plants/snow the player is actually standing in.
     * Clear days: plant colour only, very light. Weather wash only when raining/storming.
     * Dimmed at night so tall grass doesn't glow neon after dark.
     */
    private static void applySparseImmersionVeil(Level world, Vec3 eye, int[] accum, int[] imm)
    {
        final int[] layer = ROOM_LAYER_SCRATCH.get();
        if(imm[3] <= 0)
        {
            sampleNearbyImmersionColors(world, eye, imm, layer);
        }
        if(imm[3] <= 0)
        {
            return;
        }
        int pr = ColorMath.clamp255(imm[0] / imm[3]);
        int pg = ColorMath.clamp255(imm[1] / imm[3]);
        int pb = ColorMath.clamp255(imm[2] / imm[3]);
        final float skyBright = AtmosphereSampler.skyBrightness(world);
        final float weather = AtmosphereSampler.weatherMoodStrength(world);
        if(weather > 0.12f)
        {
            final int fog = AtmosphereSampler.sampleFogColor(world, BlockPos.containing(eye),
                    (pr << 16) | (pg << 8) | pb);
            final float fw = Math.min(0.28f, weather * 0.28f);
            pr = ColorMath.clamp255(Math.round(pr * (1.0f - fw) + ((fog >> 16) & 0xFF) * fw));
            pg = ColorMath.clamp255(Math.round(pg * (1.0f - fw) + ((fog >> 8) & 0xFF) * fw));
            pb = ColorMath.clamp255(Math.round(pb * (1.0f - fw) + (fog & 0xFF) * fw));
        }
        // Night: crush veil brightness so immersion matches the world.
        final float nightK = 0.18f + 0.82f * skyBright;
        layer[0] = ColorMath.clamp255(Math.round(pr * nightK));
        layer[1] = ColorMath.clamp255(Math.round(pg * nightK));
        layer[2] = ColorMath.clamp255(Math.round(pb * nightK));
        layer[3] = ColorMath.clamp255(Math.round(10 + 6 * skyBright));
        compositeViewportLayer(accum, layer);
    }

    private static boolean isRayPassthroughFoliage(BlockState state)
    {
        // Flowers/crops are real LED detail — do not divert them into the soft immersion veil.
        if(BlockDisplayColorSampler.isFlowerOrCropDetail(state))
        {
            return false;
        }
        return BlockDisplayColorSampler.isSparseSkyCutout(state)
                || BlockDisplayColorSampler.isSoftWeatherVolume(state);
    }

    /**
     * Viewport ray with layered compositing: UV block texels, cutouts (plants/fire/glass),
     * entities (mobs/drops), water, and light blocks blend front-to-back.
     */
    private static void sampleRoomCellAtWorldTarget(Level world, LocalPlayer player, Vec3 target, int[] out)
    {
        BlockUvTexelSampler.beginRay();
        final Vec3 eye = player.getEyePosition();
        final int[] accum = ROOM_ACCUM_SCRATCH.get();
        final int[] layer = ROOM_LAYER_SCRATCH.get();
        final int[] immersion = ROOM_IMMERSION_SCRATCH.get();
        clearRoomSampleRgba(accum);
        clearImmersion(immersion);
        if(eye.distanceToSqr(target) < 1.0e-4)
        {
            final BlockPos at = BlockPos.containing(target);
            final var fluid = world.getFluidState(at);
            if(fluid.is(Fluids.LAVA))
            {
                BlockDisplayColorSampler.sampleLavaLayer(world, at, layer);
                compositeViewportLayer(accum, layer);
            }
            else if(fluid.is(Fluids.WATER))
            {
                BlockDisplayColorSampler.sampleWaterLayer(world, at, layer);
                compositeViewportLayer(accum, layer);
            }
            else if(!world.isEmptyBlock(at))
            {
                final BlockState atState = world.getBlockState(at);
                if(isRayPassthroughFoliage(atState))
                {
                    BlockDisplayColorSampler.sampleViewportLayer(world, at, atState, null, target, layer);
                    addImmersionSample(immersion, layer);
                    applySparseImmersionVeil(world, eye, accum, immersion);
                }
                else
                {
                    BlockDisplayColorSampler.sampleViewportLayer(world, at, atState, null, target, layer);
                    compositeViewportLayer(accum, layer);
                }
            }
            writeAccumToRoomSample(accum, out);
            return;
        }

        Vec3 rayStart = eye;
        final Vec3 marchDir = target.subtract(eye).normalize();
        final List<EntityDisplayColorSampler.EntityHit> entityHits =
                EntityDisplayColorSampler.collectHits(world, player, eye, target);
        int entityIndex = 0;
        double lastDistSq = 0.0;
        boolean immersedNearEye = false;
        int solidLayers = 0;

        for(int march = 0; march < MAX_VIEWPORT_RAY_MARCHES && solidLayers < MAX_VIEWPORT_BLEND_LAYERS; march++)
        {
            final BlockHitResult hit = raycastViewport(world, player, rayStart, target);
            final double blockDistSq = hit.getType() == HitResult.Type.BLOCK
                    ? eye.distanceToSqr(hit.getLocation())
                    : Double.POSITIVE_INFINITY;

            while(entityIndex < entityHits.size() && entityHits.get(entityIndex).distanceSq() <= blockDistSq
                    && entityHits.get(entityIndex).distanceSq() >= lastDistSq - 1.0e-6)
            {
                final EntityDisplayColorSampler.EntityHit eh = entityHits.get(entityIndex++);
                layer[0] = eh.r();
                layer[1] = eh.g();
                layer[2] = eh.b();
                layer[3] = eh.a();
                compositeViewportLayer(accum, layer);
                if(accum[3] >= 250)
                {
                    writeAccumToRoomSample(accum, out);
                    return;
                }
                solidLayers++;
            }

            if(hit.getType() != HitResult.Type.BLOCK)
            {
                final BlockPos lavaPos = findFluidAlongRay(world, eye, target, true);
                if(lavaPos != null)
                {
                    BlockDisplayColorSampler.sampleLavaLayer(world, lavaPos, layer);
                    compositeViewportLayer(accum, layer);
                }
                else
                {
                    final BlockPos waterPos = findFluidAlongRay(world, eye, target, false);
                    if(waterPos != null)
                    {
                        BlockDisplayColorSampler.sampleWaterLayer(world, waterPos, layer);
                        compositeViewportLayer(accum, layer);
                    }
                }
                while(entityIndex < entityHits.size())
                {
                    final EntityDisplayColorSampler.EntityHit eh = entityHits.get(entityIndex++);
                    layer[0] = eh.r();
                    layer[1] = eh.g();
                    layer[2] = eh.b();
                    layer[3] = eh.a();
                    compositeViewportLayer(accum, layer);
                }
                if(immersedNearEye)
                {
                    applySparseImmersionVeil(world, eye, accum, immersion);
                }
                writeAccumToRoomSample(accum, out);
                return;
            }

            final BlockPos pos = hit.getBlockPos();
            final BlockState state = world.getBlockState(pos);
            final var fluid = world.getFluidState(pos);
            if(state.isAir() && fluid.isEmpty())
            {
                if(immersedNearEye)
                {
                    applySparseImmersionVeil(world, eye, accum, immersion);
                }
                writeAccumToRoomSample(accum, out);
                return;
            }

            // Tall grass / sunflowers / snow layers / petals must not trap every LED ray.
            // Near-eye: skip + optional immersion. Farther: pass through with NO soft composite —
            // pale plant texels were washing dirt/cherry/grass into grey-white.
            if(isRayPassthroughFoliage(state))
            {
                final double dist = Math.sqrt(blockDistSq);
                if(dist <= SPARSE_NEAR_SKIP_DIST)
                {
                    immersedNearEye = true;
                    BlockDisplayColorSampler.sampleViewportLayer(world, pos, state, hit.getDirection(),
                            hit.getLocation(), layer);
                    addImmersionSample(immersion, layer);
                }
                lastDistSq = blockDistSq;
                rayStart = advanceRayPastBlockPos(hit.getLocation(), marchDir, pos);
                if(rayStart.distanceToSqr(target) >= eye.distanceToSqr(target) - 1.0e-6)
                {
                    if(immersedNearEye)
                    {
                        applySparseImmersionVeil(world, eye, accum, immersion);
                    }
                    writeAccumToRoomSample(accum, out);
                    return;
                }
                continue;
            }

            BlockDisplayColorSampler.sampleViewportLayer(world, pos, state, hit.getDirection(), hit.getLocation(),
                    layer);
            if(layer[3] > 0)
            {
                compositeViewportLayer(accum, layer);
            }

            final int coverAlpha = layer[3] > 0
                    ? layer[3]
                    : BlockDisplayColorSampler.viewportCoverAlpha(state, fluid);
            if(!BlockDisplayColorSampler.continuesViewportRay(state, fluid, coverAlpha))
            {
                if(immersedNearEye)
                {
                    applySparseImmersionVeil(world, eye, accum, immersion);
                }
                writeAccumToRoomSample(accum, out);
                return;
            }

            solidLayers++;
            lastDistSq = blockDistSq;
            rayStart = advanceRayPastBlock(hit.getLocation(), marchDir);
            if(rayStart.distanceToSqr(target) >= eye.distanceToSqr(target))
            {
                if(immersedNearEye)
                {
                    applySparseImmersionVeil(world, eye, accum, immersion);
                }
                writeAccumToRoomSample(accum, out);
                return;
            }
        }

        if(immersedNearEye)
        {
            applySparseImmersionVeil(world, eye, accum, immersion);
        }
        writeAccumToRoomSample(accum, out);
    }

    /**
     * Per-LED-direction sky: only for truly empty rays. Uses once-per-frame atmosphere
     * (sunrise glow toward the sun, cooler anti-sun, night zenith).
     */
    private static void maybeFillOutdoorSky(RoomSampleConfigReader.Config cfg,
                                            Level world,
                                            Vec3 eye,
                                            Vec3 target,
                                            AtmosphereSampler.Frame atmosphere,
                                            int[] cell)
    {
        if(cfg == null || !cfg.isSkyEnabled() || cell == null || cell.length < 4 || atmosphere == null)
        {
            return;
        }
        // Strict: only truly empty rays get sky. Immersion/plant α must not be overwritten —
        // that was washing grass into bluish-white on LEDs.
        if(cell[3] > 0)
        {
            return;
        }
        final BlockPos column = BlockPos.containing(target);
        if(!world.canSeeSky(column))
        {
            return;
        }
        final int skyLight = world.getBrightness(LightLayer.SKY, column);
        if(skyLight < 1)
        {
            return;
        }
        final int skyRgb = AtmosphereSampler.sampleDirectionalSky(atmosphere, eye, target);
        cell[0] = (skyRgb >> 16) & 0xFF;
        cell[1] = (skyRgb >> 8) & 0xFF;
        cell[2] = skyRgb & 0xFF;
        // Night sky stays translucent so it doesn't overpower dim foliage neighbours.
        final float night = atmosphere.skyBrightness();
        final int skyA = ColorMath.clamp255(Math.round((28 + skyLight * 3) * (0.45f + 0.55f * night)));
        cell[3] = skyA;
    }

    private static int effectiveRoomSampleInterval(OpenRGBSenderConfig cfg, RoomSampleConfigReader.Config roomCfg)
    {
        int interval = Math.max(1, cfg.roomSampleSendInterval);
        if(roomCfg != null && roomCfg.isEnabled())
        {
            final long cells;
            if(roomCfg.hasImportantCells())
            {
                cells = roomCfg.importantFlatIndices.length;
            }
            else
            {
                cells = (long)roomCfg.sizeX * (long)roomCfg.sizeY * (long)roomCfg.sizeZ;
            }
            if(cells > 300000L)
            {
                interval = Math.max(interval, 20);
            }
            else if(cells > 100000L)
            {
                interval = Math.max(interval, 10);
            }
            else if(cells > 30000L)
            {
                interval = Math.max(interval, 5);
            }
            else if(cells > 10000L)
            {
                interval = Math.max(interval, 3);
            }
            else if(cells > 4096L)
            {
                interval = Math.max(interval, 2);
            }
        }
        return interval;
    }

    private void sendRoomSampleFrame(DatagramSocket socket, InetAddress address, LocalPlayer player, Level world,
                                     RoomSampleConfigReader.Config cfg, long tickStartNs) throws Exception
    {
        if(cfg == null || !cfg.isEnabled() || !cfg.isCubemap())
        {
            return;
        }
        // LED-mapped cubemap only — no voxel volume, no full-face fill.
        if(!cfg.hasImportantCells())
        {
            return;
        }

        final int faceSize = cfg.sizeX;
        final int sx = faceSize;
        final int sy = faceSize;
        final int sz = RoomSampleCubemap.FACE_COUNT;
        final int texelCount = sx * sy * sz;
        final int rgbaCount = texelCount * 4;
        if(rgbaCount <= 0)
        {
            return;
        }

        if(roomSampleRgbaBuffer.length != rgbaCount)
        {
            roomSampleRgbaBuffer = new byte[rgbaCount];
            roomSampleClearedForConfigId = -1;
            roomSampleCubemapCursor = 0;
            roomSampleAwaitingFirstPass = true;
        }
        final byte[] rgba = roomSampleRgbaBuffer;
        final int[] cell = ROOM_CELL_SCRATCH.get();
        final float[] localDir = ROOM_CUBEMAP_DIR.get();
        // Full LED budget always — average precache is separate; starving rays caused startup flash.
        final long budgetNs = ROOM_SAMPLE_BUDGET_NS;
        final long budgetStart = System.nanoTime();
        final AtmosphereSampler.Frame atmosphere = AtmosphereSampler.captureFrame(world);
        final Vec3 eye = player.getEyePosition();
        final double probeRadius = Math.max(12.0,
                Math.min(ENTITY_PROBE_RADIUS_MAX, RoomSampleWorldMapper.roomHalfDiagonalBlocks(cfg) + 4.0));

        if(roomSampleClearedForConfigId != cfg.configId)
        {
            // Soft start: keep prior LED colours until overwritten — kills the full-buffer flash
            // when face size / LED set changes. Only wipe when the buffer was resized.
            roomSampleClearedForConfigId = cfg.configId;
            roomSampleCubemapCursor = 0;
            roomSampleAwaitingFirstPass = true;
        }

        EntityDisplayColorSampler.beginFrame(world, player, probeRadius);
        try
        {
            final int[] important = cfg.importantFlatIndices;
            final int n = important.length;
            int start = roomSampleCubemapCursor % n;
            int processed = 0;
            for(int step = 0; step < n; step++)
            {
                final long now = System.nanoTime();
                if((now - budgetStart) > budgetNs || (now - tickStartNs) > CLIENT_TICK_WORK_BUDGET_NS)
                {
                    break;
                }
                final int flat = important[(start + step) % n];
                if(flat < 0 || flat >= texelCount)
                {
                    continue;
                }
                writeCubemapTexel(world, player, cfg, eye, atmosphere, localDir, cell, rgba, flat, sx, sy, sz);
                processed++;
            }
            final int next = (start + Math.max(1, processed)) % n;
            // Completed a full revolution of mapped LED texels.
            if(roomSampleAwaitingFirstPass && processed > 0
                    && (start + processed) >= n)
            {
                roomSampleAwaitingFirstPass = false;
            }
            roomSampleCubemapCursor = next;
        }
        finally
        {
            EntityDisplayColorSampler.endFrame();
        }

        if(socket == null || address == null)
        {
            return;
        }
        if(roomSampleAwaitingFirstPass)
        {
            return;
        }
        final long now = System.currentTimeMillis();
        final int frameId = (int)(now & 0x7FFFFFFFL);
        roomSampleRgbaBuffer = roomSampleFrameShmWriter.offerSwap(
                frameId, now, cfg.configId, sx, sy, sz, rgba);
    }

    private static void writeCubemapTexel(Level world,
                                          LocalPlayer player,
                                          RoomSampleConfigReader.Config cfg,
                                          Vec3 eye,
                                          AtmosphereSampler.Frame atmosphere,
                                          float[] localDir,
                                          int[] cell,
                                          byte[] rgba,
                                          int flat,
                                          int sx,
                                          int sy,
                                          int sz)
    {
        final int face = flat % sz;
        final int t = flat / sz;
        final int v = t % sy;
        final int u = t / sy;
        final float uf = (u + 0.5f) / (float)sx;
        final float vf = (v + 0.5f) / (float)sy;
        RoomSampleCubemap.uvToDirection(face, uf, vf, localDir);
        final double range = RoomSampleWorldMapper.roomRayRangeBlocks(
                cfg, localDir[0], localDir[1], localDir[2]);
        final Vec3 target = RoomSampleWorldMapper.mapLocalDirToWorldTarget(
                cfg, player, localDir[0], localDir[1], localDir[2], range);
        sampleRoomCellAtWorldTarget(world, player, target, cell);
        maybeFillOutdoorSky(cfg, world, eye, target, atmosphere, cell);
        final int rgbaIndex = flat * 4;
        // While HQ UV sprites are still decoding, keep the last good LED colour instead of
        // thrashing face-average → UV (looks like a texture-load flash).
        if(BlockUvTexelSampler.rayHadOnlyUvMisses() && (rgba[rgbaIndex + 3] & 0xFF) > 8)
        {
            return;
        }
        rgba[rgbaIndex] = (byte)cell[0];
        rgba[rgbaIndex + 1] = (byte)cell[1];
        rgba[rgbaIndex + 2] = (byte)cell[2];
        rgba[rgbaIndex + 3] = (byte)cell[3];
    }

    private void logRoomConfigState(RoomSampleConfigReader.Config cfg)
    {
        if(cfg == null || !cfg.isEnabled())
        {
            final long now = System.currentTimeMillis();
            if(now - lastWaitingForConfigLogMs >= 15000L)
            {
                lastWaitingForConfigLogMs = now;
                LOGGER.info("Waiting for room config from OpenRGB (select Minecraft effect with Room Ambilight, then join world)");
            }
            return;
        }

        final int sx = cfg.sizeX;
        final int sy = cfg.sizeY;
        final int sz = cfg.sizeZ;
        final boolean first = lastLoggedRoomSizeX < 0;
        final boolean changed = cfg.configId != lastLoggedRoomConfigId
                || sx != lastLoggedRoomSizeX
                || sy != lastLoggedRoomSizeY
                || sz != lastLoggedRoomSizeZ;
        if(!first && !changed)
        {
            return;
        }

        final int ledTexels = cfg.hasImportantCells() ? cfg.importantFlatIndices.length : 0;
        if(first)
        {
            LOGGER.info("Room Ambilight cubemap via shared memory ({}x{} x6, UV {}, {} mapped LED texels, config id {})",
                    sx, sy, cfg.uvTextureMaxDim, ledTexels, cfg.configId);
        }
        else
        {
            LOGGER.info("Room Ambilight cubemap updated: {}x{}x{} -> {}x{}x{} ({} LED texels, config id {} -> {})",
                    lastLoggedRoomSizeX, lastLoggedRoomSizeY, lastLoggedRoomSizeZ,
                    sx, sy, sz, ledTexels,
                    lastLoggedRoomConfigId, cfg.configId);
        }

        lastLoggedRoomConfigId = cfg.configId;
        lastLoggedRoomSizeX = sx;
        lastLoggedRoomSizeY = sy;
        lastLoggedRoomSizeZ = sz;
    }

    private void sendDamageEventIfNeeded(DatagramSocket socket, InetAddress address, LocalPlayer player) throws Exception
    {
        float health = player.getHealth();
        if(lastHealth >= 0.0f && health < lastHealth)
        {
            float amount = Math.max(0.0f, lastHealth - health);
            long age = System.currentTimeMillis() - DamageTelemetryState.lastPacketMs;
            float dx;
            float dy;
            float dz;
            if(age >= 0L && age <= DAMAGE_DIR_MAX_AGE_MS)
            {
                dx = DamageTelemetryState.lastDirX;
                dy = DamageTelemetryState.lastDirY;
                dz = DamageTelemetryState.lastDirZ;
            }
            else
            {
                dx = 0.0f;
                dy = 0.0f;
                dz = 1.0f;
            }
            String json = String.format(Locale.US,
                    "{\"version\":1,\"type\":\"damage_event\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"amount\":%.4f,\"dir_x\":%.5f,\"dir_y\":%.5f,\"dir_z\":%.5f}",
                    System.currentTimeMillis(),
                    amount,
                    dx,
                    dy,
                    dz);
            sendJson(socket, address, json);
        }
        lastHealth = health;
    }

    private static void sendRoomSampleShmNotify(DatagramSocket socket, InetAddress address, int frameId, long timestampMs, int configId) throws Exception
    {
        final String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"room_sample_shm_notify\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\","
                        + "\"room_sample_frame_id\":%d,\"room_sample_config_id\":%d}",
                timestampMs,
                frameId,
                configId);
        sendJson(socket, address, json);
    }


    private static void sendJson(DatagramSocket socket, InetAddress address, String json) throws Exception
    {
        final int port = OpenRGBSenderConfig.get().port;
        byte[] data = json.getBytes(StandardCharsets.UTF_8);
        DatagramPacket packet = new DatagramPacket(data, data.length, address, port);
        socket.send(packet);
    }
}

