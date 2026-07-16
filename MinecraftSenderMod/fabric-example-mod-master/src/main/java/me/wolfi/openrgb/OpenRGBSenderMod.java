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
import net.minecraft.world.phys.AABB;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.MapColor;
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
import java.util.Locale;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class OpenRGBSenderMod implements ClientModInitializer
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final long DAMAGE_DIR_MAX_AGE_MS = 450L;
    private static final long LIGHTNING_EVENT_COOLDOWN_MS = 250L;
    private static final long LIGHTNING_EVENT_MAX_AGE_MS = 450L;

    private static OpenRGBSenderMod instance;

    private float lastHealth = -1.0f;
    private int clientTickCounter = 0;

    private static DatagramSocket sharedUdpSocket;
    private static InetAddress sharedUdpAddress;
    private static String cachedHost = null;
    private static int cachedPort = -1;
    private static final Object sharedUdpLock = new Object();
    private boolean hasSmoothedWorld = false;
    private int smoothSkyR = 140, smoothSkyG = 170, smoothSkyB = 220;
    private int smoothMidR = 110, smoothMidG = 140, smoothMidB = 100;
    private int smoothGroundR = 90, smoothGroundG = 100, smoothGroundB = 70;
    private float smoothWorldIntensity = 0.6f;
    private long lastLightningSentMs = 0L;
    /** Increments every client tick (not gated by telemetryTickDivisor) for maximum room sample rate. */
    private int roomTickCounter = 0;
    /** Exponential smoothing for directional probe RGB (reduces twitchy room tint). */
    private static final float PROBE_COLOR_SMOOTH = 0.11f;
    private static final float AMBIENT_RGB_SMOOTH = 0.14f;
    private final int[][] smoothLayeredProbeRgb = new int[4 * 9][3];
    private boolean hasSmoothLayeredProbeRgb = false;
    private int smoothAmbR = 0, smoothAmbG = 0, smoothAmbB = 0;
    private boolean hasSmoothAmbientRgb = false;
    private final RoomSampleConfigReader roomSampleConfigReader = new RoomSampleConfigReader();
    private final RoomSampleFrameShmWriter roomSampleFrameShmWriter = new RoomSampleFrameShmWriter();
    private int lastLoggedRoomConfigId = -1;
    private int lastLoggedRoomSizeX = -1;
    private int lastLoggedRoomSizeY = -1;
    private int lastLoggedRoomSizeZ = -1;
    private long lastWaitingForConfigLogMs = 0L;
    /** Reused across frames to avoid per-tick allocation. Resized only when grid dimensions change. */
    private byte[] roomSampleRgbaBuffer = new byte[0];
    /** Rotating slice index for large grids — spreads raycasts over several ticks. */
    private int roomSampleTemporalSlice = 0;
    private static final int ROOM_SAMPLE_TEMPORAL_SLICES = 4;
    private static final int ROOM_SAMPLE_TEMPORAL_THRESHOLD = 4096;

    @Override
    public void onInitializeClient()
    {
        instance = this;
        OpenRGBSenderConfig.get();
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

    /**
     * Called from {@link me.wolfi.openrgb.mixin.MinecraftClientMixin} at end of each client tick (main thread only).
     */
    public static void onClientTick(Minecraft client)
    {
        if(instance == null || client == null)
        {
            return;
        }
        BlockTexturePrecache.tick(client);
        BlockFaceColorCache.tick(client);
        // Room samples run every tick independently — not gated by telemetryTickDivisor —
        // so LED colours stay in sync with the game world at up to 20 Hz regardless of telemetry rate.
        instance.tickRoomSamples(client);
        instance.tickSendTelemetry(client);
    }

    private void tickRoomSamples(Minecraft client)
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
        roomTickCounter++;
        try
        {
            // Read config once and share with both interval check and frame send.
            final RoomSampleConfigReader.Config roomCfg = roomSampleConfigReader.readLatest();
            logRoomConfigState(roomCfg);
            final int roomInterval = effectiveRoomSampleInterval(cfg, roomCfg);
            if(roomTickCounter % roomInterval == 0)
            {
                DatagramSocket socket = ensureSharedUdp(cfg);
                sendRoomSampleFrame(socket, sharedUdpAddress, client.player, client.level, roomCfg);
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
            sendWorldLight(socket, address, player, client.level);
            sendLightningEventIfNeeded(socket, address);
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

    private void sendWorldLight(DatagramSocket socket, InetAddress address, LocalPlayer player, Level world) throws Exception
    {
        var pos = player.blockPosition();
        int blockLight = world.getBrightness(LightLayer.BLOCK, pos);
        int skyLight = world.getBrightness(LightLayer.SKY, pos);
        // Block-only is ~0 in open daylight (sky lights the scene); plugin multiplies tint by intensity,
        // so world tint never showed outdoors. Use max(sky, block) for blend weight.
        int combined = Math.max(blockLight, skyLight);
        float intensity = Math.max(0.0f, Math.min(1.0f, combined / 15.0f));

        // Biome grass/foliage/water tints from the 26.2 client renderer.
        int grass = AtmosphereSampler.sampleGrassColor(world, pos, 0x7FB238);
        int bioR = ((grass >> 16) & 0xFF);
        int bioG = ((grass >> 8) & 0xFF);
        int bioB = (grass & 0xFF);
        int water = AtmosphereSampler.sampleWaterColor(world, pos, 0x3F76E4);
        int waterR = (water >> 16) & 0xFF;
        int waterG = (water >> 8) & 0xFF;
        int waterB = water & 0xFF;

        int rawSky = AtmosphereSampler.sampleSkyColor(world, pos, ((bioR & 0xFF) << 16) | ((bioG & 0xFF) << 8) | (bioB & 0xFF));
        int rawFog = AtmosphereSampler.sampleFogColor(world, pos, rawSky);
        int rawSkyR = (rawSky >> 16) & 0xFF;
        int rawSkyG = (rawSky >> 8) & 0xFF;
        int rawSkyB = rawSky & 0xFF;
        int rawFogR = (rawFog >> 16) & 0xFF;
        int rawFogG = (rawFog >> 8) & 0xFF;
        int rawFogB = rawFog & 0xFF;
        int blockColor = sampleLocalBlockLightColor(world, pos, rawFog);
        int blockR = (blockColor >> 16) & 0xFF;
        int blockG = (blockColor >> 8) & 0xFF;
        int blockB = blockColor & 0xFF;
        int r;
        int g;
        int bl;
        if(combined <= 0)
        {
            r = 0;
            g = 0;
            bl = 0;
        }
        else
        {
            float blockWeight = (float)blockLight / (float)combined;
            float skyWeight = 1.0f - blockWeight;
            // All components come from game-derived samples: sky/fog + emissive block map colors.
            float skyAmbientR = rawFogR * 0.62f + rawSkyR * 0.38f;
            float skyAmbientG = rawFogG * 0.62f + rawSkyG * 0.38f;
            float skyAmbientB = rawFogB * 0.62f + rawSkyB * 0.38f;
            r = ColorMath.clamp255(Math.round(skyAmbientR * skyWeight + blockR * blockWeight));
            g = ColorMath.clamp255(Math.round(skyAmbientG * skyWeight + blockG * blockWeight));
            bl = ColorMath.clamp255(Math.round(skyAmbientB * skyWeight + blockB * blockWeight));
        }
        int skyLightHere = world.getBrightness(LightLayer.SKY, pos);
        int skyR;
        int skyG;
        int skyB;
        if(skyLightHere <= 3)
        {
            // In caves/indoors, use overhead ceiling tint instead of open-sky model.
            int ceiling = sampleCeilingColor(world, pos, ((bioR & 0xFF) << 16) | ((bioG & 0xFF) << 8) | (bioB & 0xFF));
            int cr = (ceiling >> 16) & 0xFF;
            int cg = (ceiling >> 8) & 0xFF;
            int cb = ceiling & 0xFF;
            // Scale by actual light only — no minimum floor (avoids gray/white glow in pitch black).
            int caveCombined = Math.max(blockLight, skyLightHere);
            float caveFactor = ColorMath.clamp01(caveCombined / 15.0f);
            skyR = ColorMath.clamp255(Math.round(cr * caveFactor));
            skyG = ColorMath.clamp255(Math.round(cg * caveFactor));
            skyB = ColorMath.clamp255(Math.round(cb * caveFactor));
        }
        else
        {
            // Use the game's current sky color directly (already includes time/weather/dimension effects).
            skyR = rawSkyR;
            skyG = rawSkyG;
            skyB = rawSkyB;
        }

        // World-tint layers are strictly position-based (not camera/crosshair based).
        // Blend biome foliage/grass with nearby horizontal samples to avoid dull single-tone green.
        int horizonColor = sampleHorizontalColor(world, pos, ((bioR & 0xFF) << 16) | ((bioG & 0xFF) << 8) | (bioB & 0xFF));
        int midR0 = ((horizonColor >> 16) & 0xFF);
        int midG0 = ((horizonColor >> 8) & 0xFF);
        int midB0 = (horizonColor & 0xFF);
        int midR = ColorMath.clamp255(Math.round(midR0 * 0.72f + waterR * 0.10f + bioR * 0.18f));
        int midG = ColorMath.clamp255(Math.round(midG0 * 0.72f + waterG * 0.10f + bioG * 0.18f));
        int midB = ColorMath.clamp255(Math.round(midB0 * 0.72f + waterB * 0.10f + bioB * 0.18f));
        int midColor = ((midR & 0xFF) << 16) | ((midG & 0xFF) << 8) | (midB & 0xFF);

        int groundColor = sampleGroundColor(world, pos, midColor);
        int groundR = (groundColor >> 16) & 0xFF;
        int groundG = (groundColor >> 8) & 0xFF;
        int groundB = groundColor & 0xFF;
        Vec3 look = player.getViewVector(1.0f);
        double fx = look.x;
        double fz = look.z;
        double fl = Math.sqrt(fx * fx + fz * fz);
        if(fl <= 1e-5)
        {
            fx = 0.0;
            fz = 1.0;
        }
        else
        {
            fx /= fl;
            fz /= fl;
        }
        double rx = -fz;
        double rz = fx;
        final float waterSubmerge = estimateWaterSubmerge(world, player);
        if(waterSubmerge > 1e-4f)
        {
            final float gw = ColorMath.clamp01(0.20f + 0.75f * waterSubmerge);
            final float mw = ColorMath.clamp01(0.14f + 0.62f * waterSubmerge);
            final float sw = ColorMath.clamp01(0.10f + 0.48f * waterSubmerge);
            groundR = ColorMath.clamp255(Math.round(groundR * (1.0f - gw) + waterR * gw));
            groundG = ColorMath.clamp255(Math.round(groundG * (1.0f - gw) + waterG * gw));
            groundB = ColorMath.clamp255(Math.round(groundB * (1.0f - gw) + waterB * gw));
            midR = ColorMath.clamp255(Math.round(midR * (1.0f - mw) + waterR * mw));
            midG = ColorMath.clamp255(Math.round(midG * (1.0f - mw) + waterG * mw));
            midB = ColorMath.clamp255(Math.round(midB * (1.0f - mw) + waterB * mw));
            skyR = ColorMath.clamp255(Math.round(skyR * (1.0f - sw) + waterR * sw));
            skyG = ColorMath.clamp255(Math.round(skyG * (1.0f - sw) + waterG * sw));
            skyB = ColorMath.clamp255(Math.round(skyB * (1.0f - sw) + waterB * sw));
            midColor = ((midR & 0xFF) << 16) | ((midG & 0xFF) << 8) | (midB & 0xFF);
        }

        if(!hasSmoothedWorld)
        {
            smoothSkyR = skyR; smoothSkyG = skyG; smoothSkyB = skyB;
            smoothMidR = midR; smoothMidG = midG; smoothMidB = midB;
            smoothGroundR = groundR; smoothGroundG = groundG; smoothGroundB = groundB;
            smoothWorldIntensity = intensity;
            hasSmoothedWorld = true;
        }
        smoothSkyR = ColorMath.lerpInt(smoothSkyR, skyR, 0.16f);
        smoothSkyG = ColorMath.lerpInt(smoothSkyG, skyG, 0.16f);
        smoothSkyB = ColorMath.lerpInt(smoothSkyB, skyB, 0.16f);
        smoothMidR = ColorMath.lerpInt(smoothMidR, midR, 0.20f);
        smoothMidG = ColorMath.lerpInt(smoothMidG, midG, 0.20f);
        smoothMidB = ColorMath.lerpInt(smoothMidB, midB, 0.20f);
        smoothGroundR = ColorMath.lerpInt(smoothGroundR, groundR, 0.18f);
        smoothGroundG = ColorMath.lerpInt(smoothGroundG, groundG, 0.18f);
        smoothGroundB = ColorMath.lerpInt(smoothGroundB, groundB, 0.18f);
        smoothWorldIntensity = ColorMath.lerpFloat(smoothWorldIntensity, intensity, 0.16f);

        int upperColor = ColorMath.blendRgb(midColor, rawSky, 0.45f);
        int[] layerFallbacks = {groundColor, midColor, upperColor, rawSky};
        double[] dirX = {fx, (fx + rx), rx, (-fx + rx), -fx, (-fx - rx), -rx, (fx - rx)};
        double[] dirZ = {fz, (fz + rz), rz, (-fz + rz), -fz, (-fz - rz), -rz, (fz - rz)};
        for(int i = 0; i < 8; i++)
        {
            double dl = Math.sqrt(dirX[i] * dirX[i] + dirZ[i] * dirZ[i]);
            if(dl > 1e-6)
            {
                dirX[i] /= dl;
                dirZ[i] /= dl;
            }
        }
        int[][] layeredRaw = new int[4 * 9][3];
        for(int layer = 0; layer < 4; layer++)
        {
            int yOff = layer - 1;
            int fallback = layerFallbacks[layer];
            for(int sector = 0; sector < 8; sector++)
            {
                int c = sampleDirectionalColor(world, pos, dirX[sector], dirZ[sector], yOff, fallback);
                int idx = layer * 9 + sector;
                layeredRaw[idx][0] = (c >> 16) & 0xFF;
                layeredRaw[idx][1] = (c >> 8) & 0xFF;
                layeredRaw[idx][2] = c & 0xFF;
            }
            int centerIdx = layer * 9 + 8;
            layeredRaw[centerIdx][0] = (fallback >> 16) & 0xFF;
            layeredRaw[centerIdx][1] = (fallback >> 8) & 0xFF;
            layeredRaw[centerIdx][2] = fallback & 0xFF;
        }
        if(!hasSmoothLayeredProbeRgb)
        {
            for(int i = 0; i < 4 * 9; i++)
            {
                smoothLayeredProbeRgb[i][0] = layeredRaw[i][0];
                smoothLayeredProbeRgb[i][1] = layeredRaw[i][1];
                smoothLayeredProbeRgb[i][2] = layeredRaw[i][2];
            }
            hasSmoothLayeredProbeRgb = true;
        }
        else
        {
            for(int i = 0; i < 4 * 9; i++)
            {
                smoothLayeredProbeRgb[i][0] = ColorMath.lerpInt(smoothLayeredProbeRgb[i][0], layeredRaw[i][0], PROBE_COLOR_SMOOTH);
                smoothLayeredProbeRgb[i][1] = ColorMath.lerpInt(smoothLayeredProbeRgb[i][1], layeredRaw[i][1], PROBE_COLOR_SMOOTH);
                smoothLayeredProbeRgb[i][2] = ColorMath.lerpInt(smoothLayeredProbeRgb[i][2], layeredRaw[i][2], PROBE_COLOR_SMOOTH);
            }
        }
        StringBuilder layeredProbeRgbJson = new StringBuilder(4 * 9 * 12);
        for(int i = 0; i < 4 * 9; i++)
        {
            if(i > 0)
            {
                layeredProbeRgbJson.append(',');
            }
            layeredProbeRgbJson.append(smoothLayeredProbeRgb[i][0]).append(',')
                               .append(smoothLayeredProbeRgb[i][1]).append(',')
                               .append(smoothLayeredProbeRgb[i][2]);
        }

        if(!hasSmoothAmbientRgb)
        {
            smoothAmbR = r;
            smoothAmbG = g;
            smoothAmbB = bl;
            hasSmoothAmbientRgb = true;
        }
        else
        {
            smoothAmbR = ColorMath.lerpInt(smoothAmbR, r, AMBIENT_RGB_SMOOTH);
            smoothAmbG = ColorMath.lerpInt(smoothAmbG, g, AMBIENT_RGB_SMOOTH);
            smoothAmbB = ColorMath.lerpInt(smoothAmbB, bl, AMBIENT_RGB_SMOOTH);
        }
        r = smoothAmbR;
        g = smoothAmbG;
        bl = smoothAmbB;

        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"world_light\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"r\":%d,\"g\":%d,\"b\":%d,\"intensity\":%.4f,\"sky_r\":%d,\"sky_g\":%d,\"sky_b\":%d,\"mid_r\":%d,\"mid_g\":%d,\"mid_b\":%d,\"ground_r\":%d,\"ground_g\":%d,\"ground_b\":%d,\"probe_rgb\":[%s]}",
                System.currentTimeMillis(),
                r, g, bl, smoothWorldIntensity,
                smoothSkyR, smoothSkyG, smoothSkyB,
                smoothMidR, smoothMidG, smoothMidB,
                smoothGroundR, smoothGroundG, smoothGroundB,
                layeredProbeRgbJson.toString());
        sendJson(socket, address, json);
    }

    private static int sampleDirectionalColor(net.minecraft.world.level.Level world,
                                              net.minecraft.core.BlockPos center,
                                              double dirX,
                                              double dirZ,
                                              int yOffset,
                                              int fallback)
    {
        int[] distances = {2, 4, 6};
        int sumR = 0;
        int sumG = 0;
        int sumB = 0;
        int count = 0;
        for(int d : distances)
        {
            int ox = (int)Math.round(dirX * d);
            int oz = (int)Math.round(dirZ * d);
            net.minecraft.core.BlockPos p = center.offset(ox, yOffset, oz);
            int c = getMapColorRgb(world, p, fallback);
            sumR += (c >> 16) & 0xFF;
            sumG += (c >> 8) & 0xFF;
            sumB += c & 0xFF;
            count++;
        }
        if(count <= 0)
        {
            return fallback;
        }
        int r = ColorMath.clamp255(sumR / count);
        int g = ColorMath.clamp255(sumG / count);
        int b = ColorMath.clamp255(sumB / count);
        return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    }

    private static int sampleLocalBlockLightColor(net.minecraft.world.level.Level world, BlockPos center, int fallback)
    {
        double sr = 0.0;
        double sg = 0.0;
        double sb = 0.0;
        double sw = 0.0;
        for(int dx = -4; dx <= 4; dx++)
        {
            for(int dy = -2; dy <= 3; dy++)
            {
                for(int dz = -4; dz <= 4; dz++)
                {
                    BlockPos p = center.offset(dx, dy, dz);
                    var state = world.getBlockState(p);
                    int lum = state.getLightEmission();
                    if(lum <= 0)
                    {
                        continue;
                    }
                    int mc = getMapColorRgb(world, p, 0);
                    if(mc == 0)
                    {
                        continue;
                    }
                    double dist2 = (double)dx * dx + (double)dy * dy + (double)dz * dz;
                    double w = (double)lum / (0.35 + dist2);
                    sr += ((mc >> 16) & 0xFF) * w;
                    sg += ((mc >> 8) & 0xFF) * w;
                    sb += (mc & 0xFF) * w;
                    sw += w;
                }
            }
        }
        if(sw <= 1e-5)
        {
            return fallback;
        }
        int r = ColorMath.clamp255((int)Math.round(sr / sw));
        int g = ColorMath.clamp255((int)Math.round(sg / sw));
        int b = ColorMath.clamp255((int)Math.round(sb / sw));
        return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    }

    private static float estimateWaterSubmerge(net.minecraft.world.level.Level world, LocalPlayer player)
    {
        final AABB bb = player.getBoundingBox();
        final double cx = (bb.minX + bb.maxX) * 0.5;
        final double cz = (bb.minZ + bb.maxZ) * 0.5;
        final double[] ySamples = {0.06, 0.22, 0.38, 0.56, 0.74, 0.90};
        final double[][] xzOffsets = {
                {0.0, 0.0},
                {0.22, 0.0},
                {-0.22, 0.0},
                {0.0, 0.22},
                {0.0, -0.22}
        };
        int waterHits = 0;
        int total = 0;
        for(double yf : ySamples)
        {
            final double y = bb.minY + (bb.maxY - bb.minY) * yf;
            for(double[] off : xzOffsets)
            {
                final double x = cx + off[0];
                final double z = cz + off[1];
                BlockPos p = BlockPos.containing(x, y, z);
                var fs = world.getFluidState(p);
                total++;
                if(fs.is(Fluids.WATER))
                {
                    float fh = fs.getHeight(world, p);
                    double levelY = p.getY() + fh;
                    if(y <= levelY + 1e-4)
                    {
                        waterHits++;
                    }
                }
            }
        }
        if(total <= 0)
        {
            return 0.0f;
        }
        return ColorMath.clamp01((float)waterHits / (float)total);
    }

    private static int sampleVerticalColor(net.minecraft.world.level.Level world,
                                           net.minecraft.core.BlockPos pos,
                                           int fallback,
                                           boolean upward,
                                           int maxSteps)
    {
        net.minecraft.core.BlockPos p = upward ? pos.above() : pos.below();
        for(int i = 0; i < maxSteps; i++)
        {
            if(!world.getBlockState(p).isAir())
            {
                int c = getMapColorRgb(world, p, fallback);
                if(c != 0)
                {
                    return c;
                }
            }
            p = upward ? p.above() : p.below();
        }
        return fallback;
    }

    private static int sampleGroundColor(net.minecraft.world.level.Level world, net.minecraft.core.BlockPos pos, int fallback)
    {
        return sampleVerticalColor(world, pos, fallback, false, 8);
    }

    private static int sampleCeilingColor(net.minecraft.world.level.Level world, net.minecraft.core.BlockPos pos, int fallback)
    {
        return sampleVerticalColor(world, pos, fallback, true, 10);
    }

    private static int sampleHorizontalColor(net.minecraft.world.level.Level world, net.minecraft.core.BlockPos center, int fallback)
    {
        int[] ox = {3, -3, 0, 0, 2, 2, -2, -2};
        int[] oz = {0, 0, 3, -3, 2, -2, 2, -2};
        int sumR = 0, sumG = 0, sumB = 0, count = 0;
        for(int i = 0; i < ox.length; i++)
        {
            net.minecraft.core.BlockPos p = center.offset(ox[i], -1, oz[i]);
            int c = getMapColorRgb(world, p, fallback);
            sumR += (c >> 16) & 0xFF;
            sumG += (c >> 8) & 0xFF;
            sumB += c & 0xFF;
            count++;
        }
        if(count <= 0)
        {
            return fallback;
        }
        int r = ColorMath.clamp255(sumR / count);
        int g = ColorMath.clamp255(sumG / count);
        int b = ColorMath.clamp255(sumB / count);
        return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    }

    private static final int MAX_VIEWPORT_BLEND_LAYERS = 6;
    private static final double VIEWPORT_RAY_EPS = 0.05;
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
                ClipContext.Fluid.SOURCE_ONLY,
                player));
    }

    private static BlockPos findWaterAlongRay(Level world, Vec3 eye, Vec3 target)
    {
        for(int i = 1; i <= VIEWPORT_WATER_COARSE_SAMPLES; i++)
        {
            final double t = i / (double)VIEWPORT_WATER_COARSE_SAMPLES;
            final BlockPos pos = BlockPos.containing(eye.lerp(target, t));
            if(world.getFluidState(pos).is(Fluids.WATER))
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

    /**
     * Viewport ray with layered compositing: semi-transparent water, leaves, glass, light blocks,
     * and stained glass blend with surfaces behind instead of fully occluding them.
     */
    private static void sampleRoomCellAtWorldTarget(Level world, LocalPlayer player, Vec3 target, int[] out)
    {
        final Vec3 eye = player.getEyePosition();
        if(eye.distanceToSqr(target) < 1.0e-4)
        {
            final BlockPos at = BlockPos.containing(target);
            final int[] accum = new int[4];
            if(world.getFluidState(at).is(Fluids.WATER))
            {
                final int[] layer = new int[4];
                BlockDisplayColorSampler.sampleWaterLayer(world, at, layer);
                compositeViewportLayer(accum, layer);
            }
            else if(!world.isEmptyBlock(at))
            {
                final int[] layer = new int[4];
                BlockDisplayColorSampler.sampleViewportLayer(world, at, world.getBlockState(at), null, layer);
                compositeViewportLayer(accum, layer);
            }
            writeAccumToRoomSample(accum, out);
            return;
        }

        Vec3 rayStart = eye;
        final int[] accum = new int[4];
        final int[] layer = new int[4];
        final Vec3 marchDir = target.subtract(eye).normalize();

        for(int step = 0; step < MAX_VIEWPORT_BLEND_LAYERS; step++)
        {
            final BlockHitResult hit = raycastViewport(world, player, rayStart, target);
            if(hit.getType() != HitResult.Type.BLOCK)
            {
                final BlockPos waterPos = findWaterAlongRay(world, eye, target);
                if(waterPos != null)
                {
                    BlockDisplayColorSampler.sampleWaterLayer(world, waterPos, layer);
                    compositeViewportLayer(accum, layer);
                }
                writeAccumToRoomSample(accum, out);
                return;
            }

            final BlockPos pos = hit.getBlockPos();
            final BlockState state = world.getBlockState(pos);
            final var fluid = world.getFluidState(pos);
            if(state.isAir() && !fluid.is(Fluids.WATER))
            {
                writeAccumToRoomSample(accum, out);
                return;
            }

            BlockDisplayColorSampler.sampleViewportLayer(world, pos, state, hit.getDirection(), layer);
            if(layer[3] > 0)
            {
                compositeViewportLayer(accum, layer);
            }

            final int coverAlpha = BlockDisplayColorSampler.viewportCoverAlpha(state, fluid);
            if(!BlockDisplayColorSampler.continuesViewportRay(state, fluid, coverAlpha))
            {
                writeAccumToRoomSample(accum, out);
                return;
            }

            rayStart = advanceRayPastBlock(hit.getLocation(), marchDir);
            if(rayStart.distanceToSqr(target) >= eye.distanceToSqr(target))
            {
                writeAccumToRoomSample(accum, out);
                return;
            }
        }

        writeAccumToRoomSample(accum, out);
    }

    private static int effectiveRoomSampleInterval(OpenRGBSenderConfig cfg, RoomSampleConfigReader.Config roomCfg)
    {
        int interval = Math.max(1, cfg.roomSampleSendInterval);
        if(roomCfg != null && roomCfg.isEnabled())
        {
            final long cells = (long)roomCfg.sizeX * (long)roomCfg.sizeY * (long)roomCfg.sizeZ;
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
                                     RoomSampleConfigReader.Config cfg) throws Exception
    {
        if(cfg == null || !cfg.isEnabled())
        {
            return;
        }

        final int sx = cfg.sizeX;
        final int sy = cfg.sizeY;
        final int sz = cfg.sizeZ;
        final int rgbaCount = sx * sy * sz * 4;
        if(rgbaCount <= 0)
        {
            return;
        }

        if(roomSampleRgbaBuffer.length != rgbaCount)
        {
            roomSampleRgbaBuffer = new byte[rgbaCount];
        }
        final byte[] rgba = roomSampleRgbaBuffer;
        int rgbaIndex = 0;
        final int[] cell = new int[4];
        final int cellCount = sx * sy * sz;
        final boolean temporal = cellCount > ROOM_SAMPLE_TEMPORAL_THRESHOLD;
        int flatIndex = 0;
        for(int ix = 0; ix < sx; ix++)
        {
            for(int iy = 0; iy < sy; iy++)
            {
                for(int iz = 0; iz < sz; iz++)
                {
                    if(temporal && (flatIndex % ROOM_SAMPLE_TEMPORAL_SLICES) != roomSampleTemporalSlice)
                    {
                        rgbaIndex += 4;
                        flatIndex++;
                        continue;
                    }
                    final float roomX = RoomSampleWorldMapper.roomCellCenterX(cfg, ix);
                    final float roomY = RoomSampleWorldMapper.roomCellCenterY(cfg, iy);
                    final float roomZ = RoomSampleWorldMapper.roomCellCenterZ(cfg, iz);
                    final Vec3 target = RoomSampleWorldMapper.mapRoomToWorldTarget(cfg, player, roomX, roomY, roomZ);
                    sampleRoomCellAtWorldTarget(world, player, target, cell);
                    rgba[rgbaIndex++] = (byte)cell[0];
                    rgba[rgbaIndex++] = (byte)cell[1];
                    rgba[rgbaIndex++] = (byte)cell[2];
                    rgba[rgbaIndex++] = (byte)cell[3];
                    flatIndex++;
                }
            }
        }
        if(temporal)
        {
            roomSampleTemporalSlice = (roomSampleTemporalSlice + 1) % ROOM_SAMPLE_TEMPORAL_SLICES;
        }

        final long now = System.currentTimeMillis();
        final int frameId = (int)(now & 0x7FFFFFFFL);
        if(roomSampleFrameShmWriter.writeFrame(frameId, now, cfg.configId, sx, sy, sz, rgba))
        {
            sendRoomSampleShmNotify(socket, address, frameId, now, cfg.configId);
        }
    }

    private void logRoomConfigState(RoomSampleConfigReader.Config cfg)
    {
        if(cfg == null || !cfg.isEnabled())
        {
            final long now = System.currentTimeMillis();
            if(now - lastWaitingForConfigLogMs >= 15000L)
            {
                lastWaitingForConfigLogMs = now;
                LOGGER.info("Waiting for room config from OpenRGB (select Minecraft effect with Room VR tint, then join world)");
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

        if(first)
        {
            LOGGER.info("Room VR samples via shared memory ({}x{}x{} cells, config id {})", sx, sy, sz, cfg.configId);
        }
        else
        {
            LOGGER.info("Room VR config updated: {}x{}x{} -> {}x{}x{} (config id {} -> {})",
                    lastLoggedRoomSizeX, lastLoggedRoomSizeY, lastLoggedRoomSizeZ,
                    sx, sy, sz,
                    lastLoggedRoomConfigId, cfg.configId);
        }

        lastLoggedRoomConfigId = cfg.configId;
        lastLoggedRoomSizeX = sx;
        lastLoggedRoomSizeY = sy;
        lastLoggedRoomSizeZ = sz;
    }

    private static int getMapColorRgb(net.minecraft.world.level.Level world, net.minecraft.core.BlockPos pos, int fallback)
    {
        try
        {
            MapColor mapColor = world.getBlockState(pos).getMapColor(world, pos);
            if(mapColor != null)
            {
                return mapColor.col;
            }
        }
        catch(Exception ignored)
        {
        }
        return fallback;
    }

    private void sendLightningEventIfNeeded(DatagramSocket socket, InetAddress address) throws Exception
    {
        long now = System.currentTimeMillis();
        long age = now - LightningTelemetryState.lastPacketMs;
        if(age >= 0L &&
           age <= LIGHTNING_EVENT_MAX_AGE_MS &&
           (now - lastLightningSentMs) >= LIGHTNING_EVENT_COOLDOWN_MS)
        {
            float strength = ColorMath.clamp01(LightningTelemetryState.lastStrength);
            String json = String.format(Locale.US,
                    "{\"version\":1,\"type\":\"lightning_event\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"strength\":%.4f,\"dir_x\":%.5f,\"dir_y\":%.5f,\"dir_z\":%.5f,\"dir_focus\":%.4f}",
                    now,
                    strength,
                    LightningTelemetryState.lastDirX,
                    LightningTelemetryState.lastDirY,
                    LightningTelemetryState.lastDirZ,
                    ColorMath.clamp01(LightningTelemetryState.lastDirFocus));
            sendJson(socket, address, json);
            lastLightningSentMs = now;
        }
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

