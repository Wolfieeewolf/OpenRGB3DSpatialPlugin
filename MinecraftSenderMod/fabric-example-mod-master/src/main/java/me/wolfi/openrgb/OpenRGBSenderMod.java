package me.wolfi.openrgb;

import net.fabricmc.api.ClientModInitializer;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.color.world.BiomeColors;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.fluid.Fluids;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Box;
import net.minecraft.registry.RegistryKey;
import net.minecraft.block.Blocks;
import net.minecraft.block.MapColor;
import net.minecraft.util.math.Vec3d;
import net.minecraft.world.LightType;
import net.minecraft.world.World;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

public class OpenRGBSenderMod implements ClientModInitializer
{
    private static final String HOST = "127.0.0.1";
    private static final int PORT = 9876;
    private static final long DAMAGE_DIR_MAX_AGE_MS = 450L;
    private static final long LIGHTNING_EVENT_COOLDOWN_MS = 250L;
    private static final long LIGHTNING_EVENT_MAX_AGE_MS = 450L;

    private static OpenRGBSenderMod instance;

    private float lastHealth = -1.0f;
    /** ~10 Hz at 20 tps: send every other client tick. */
    private int clientTickCounter = 0;

    private static DatagramSocket sharedUdpSocket;
    private static InetAddress sharedLoopback;
    private static final Object sharedUdpLock = new Object();
    private boolean hasSmoothedWorld = false;
    private int smoothSkyR = 140, smoothSkyG = 170, smoothSkyB = 220;
    private int smoothMidR = 110, smoothMidG = 140, smoothMidB = 100;
    private int smoothGroundR = 90, smoothGroundG = 100, smoothGroundB = 70;
    private float smoothWorldIntensity = 0.6f;
    private long lastLightningSentMs = 0L;

    @Override
    public void onInitializeClient()
    {
        instance = this;
    }

    /**
     * Called from {@link me.wolfi.openrgb.mixin.MinecraftClientMixin} at end of each client tick (main thread only).
     */
    public static void onClientTick(MinecraftClient client)
    {
        if(instance == null || client == null)
        {
            return;
        }
        instance.tickSendTelemetry(client);
    }

    private void tickSendTelemetry(MinecraftClient client)
    {
        if(client.player == null || client.world == null)
        {
            return;
        }
        clientTickCounter++;
        if((clientTickCounter & 1) != 0)
        {
            return;
        }
        try
        {
            DatagramSocket socket = ensureSharedUdp();
            InetAddress address = sharedLoopback;
            ClientPlayerEntity player = client.player;
            sendPlayerPose(socket, address, player);
            sendHealthState(socket, address, player);
            sendWorldLight(socket, address, player, client.world);
            sendLightningEventIfNeeded(socket, address);
            sendDamageEventIfNeeded(socket, address, player);
        }
        catch(Exception ignored)
        {
        }
    }

    private static DatagramSocket ensureSharedUdp() throws Exception
    {
        synchronized(sharedUdpLock)
        {
            if(sharedUdpSocket == null || sharedUdpSocket.isClosed())
            {
                sharedUdpSocket = new DatagramSocket();
                sharedLoopback = InetAddress.getByName(HOST);
            }
            return sharedUdpSocket;
        }
    }

    private void sendPlayerPose(DatagramSocket socket, InetAddress address, ClientPlayerEntity player) throws Exception
    {
        Vec3d look = player.getRotationVec(1.0f);
        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"player_pose\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"fx\":%.5f,\"fy\":%.5f,\"fz\":%.5f,\"ux\":0.0,\"uy\":1.0,\"uz\":0.0,\"blocks_per_m\":1.0}",
                System.currentTimeMillis(),
                player.getX(), player.getY(), player.getZ(),
                look.x, look.y, look.z);
        sendJson(socket, address, json);
    }

    private void sendHealthState(DatagramSocket socket, InetAddress address, ClientPlayerEntity player) throws Exception
    {
        final int hunger = player.getHungerManager().getFoodLevel();
        final int maxHunger = 20;
        final int air = player.getAir();
        final int maxAir = player.getMaxAir();
        final var held = player.getMainHandStack();
        final boolean hasDurability = !held.isEmpty() && held.isDamageable();
        final int durabilityMax = hasDurability ? Math.max(1, held.getMaxDamage()) : 1;
        final int durability = hasDurability ? Math.max(0, durabilityMax - held.getDamage()) : 0;
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

    private void sendWorldLight(DatagramSocket socket, InetAddress address, ClientPlayerEntity player, net.minecraft.world.World world) throws Exception
    {
        var pos = player.getBlockPos();
        Vec3d lightDir = estimateLocalLightDirection(world, player, pos);
        float lightFocus = clamp01((float)lightDir.length());
        if(lightFocus > 1e-4f)
        {
            lightDir = lightDir.normalize();
        }
        else
        {
            lightDir = Vec3d.ZERO;
        }
        int blockLight = world.getLightLevel(LightType.BLOCK, pos);
        int skyLight = world.getLightLevel(LightType.SKY, pos);
        // Block-only is ~0 in open daylight (sky lights the scene); plugin multiplies tint by intensity,
        // so world tint never showed outdoors. Use max(sky, block) for blend weight.
        int combined = Math.max(blockLight, skyLight);
        float intensity = Math.max(0.0f, Math.min(1.0f, combined / 15.0f));

        float blockWeight = (combined > 0) ? ((float)blockLight / (float)combined) : 0.0f;
        float skyWeight = 1.0f - blockWeight;
        final float warmBias = estimateWarmLightBias(world, pos);

        // MineLights-style read: biome carries distinct hue (forest green, desert sand, etc.) — see
        // https://github.com/megabytesme/MineLights — we sample vanilla grass + foliage tint.
        int grass = BiomeColors.getGrassColor(world, pos);
        int foliage = BiomeColors.getFoliageColor(world, pos);
        int bioR = (((grass >> 16) & 0xFF) + ((foliage >> 16) & 0xFF)) / 2;
        int bioG = (((grass >> 8) & 0xFF) + ((foliage >> 8) & 0xFF)) / 2;
        int bioB = ((grass & 0xFF) + (foliage & 0xFF)) / 2;
        int water = BiomeColors.getWaterColor(world, pos);
        int waterR = (water >> 16) & 0xFF;
        int waterG = (water >> 8) & 0xFF;
        int waterB = water & 0xFF;

        // Clear blue sky bias (not near-white), torch warm when block lights dominate.
        float skyBiasR = 0.22f + 0.18f * intensity;
        float skyBiasG = 0.40f + 0.28f * intensity;
        float skyBiasB = 0.78f + 0.18f * intensity;
        float blkR = 1.0f;
        float blkG = 0.50f + 0.22f * intensity;
        float blkB = 0.15f + 0.22f * intensity;

        float br = bioR / 255.0f;
        float bg = bioG / 255.0f;
        float bb = bioB / 255.0f;
        float amp = 0.58f + 0.42f * intensity;
        float fr = br * amp + skyBiasR * skyWeight * intensity * 0.82f + blkR * blockWeight * intensity * 0.92f;
        float fg = bg * amp + skyBiasG * skyWeight * intensity * 0.82f + blkG * blockWeight * intensity * 0.92f;
        float fb = bb * amp + skyBiasB * skyWeight * intensity * 0.82f + blkB * blockWeight * intensity * 0.92f;
        if(warmBias > 1e-4f)
        {
            final float wr = 1.00f;
            final float wg = 0.62f;
            final float wb = 0.12f;
            final float wm = clamp01(0.24f + 0.62f * warmBias);
            fr = fr * (1.0f - wm) + wr * wm;
            fg = fg * (1.0f - wm) + wg * wm;
            fb = fb * (1.0f - wm) + wb * wm;
        }

        int r = clamp255(Math.round(fr * 255.0f));
        int g = clamp255(Math.round(fg * 255.0f));
        int bl = clamp255(Math.round(fb * 255.0f));

        float rain = world.getRainGradient(1.0f);
        float thunder = world.getThunderGradient(1.0f);
        long dayTicks = world.getTimeOfDay() % 24000L;
        float sun = 1.0f - Math.min(1.0f, Math.abs((dayTicks - 6000.0f) / 7000.0f));
        float skyDarken = 1.0f - (0.62f * rain + 0.42f * thunder);
        float sunset = Math.max(0.0f, 1.0f - Math.abs(dayTicks - 12000.0f) / 2300.0f);
        float skyBaseR = (0.04f + 0.56f * sun + 0.55f * sunset) * skyDarken;
        float skyBaseG = (0.06f + 0.60f * sun + 0.24f * sunset) * skyDarken;
        float skyBaseB = (0.16f + 0.88f * sun + 0.04f * sunset) * skyDarken;
        int skyLightHere = world.getLightLevel(LightType.SKY, pos);
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
            float caveFactor = clamp01(caveCombined / 15.0f);
            skyR = clamp255(Math.round(cr * caveFactor));
            skyG = clamp255(Math.round(cg * caveFactor));
            skyB = clamp255(Math.round(cb * caveFactor));
        }
        else
        {
            /* Open sky: blend time/rain model with vanilla biome sky tint (BiomeEffects#getSkyColor). */
            int bskyR = clamp255(Math.round(skyBaseR * 255.0f));
            int bskyG = clamp255(Math.round(skyBaseG * 255.0f));
            int bskyB = clamp255(Math.round(skyBaseB * 255.0f));
            float openR = skyBaseR * 255.0f * 0.42f + bskyR * 0.58f;
            float openG = skyBaseG * 255.0f * 0.42f + bskyG * 0.58f;
            float openB = skyBaseB * 255.0f * 0.42f + bskyB * 0.58f;
            skyR = clamp255(Math.round(openR));
            skyG = clamp255(Math.round(openG));
            skyB = clamp255(Math.round(openB));
        }

        // World-tint layers are strictly position-based (not camera/crosshair based).
        // Blend biome foliage/grass with nearby horizontal samples to avoid dull single-tone green.
        int horizonColor = sampleHorizontalColor(world, pos, ((bioR & 0xFF) << 16) | ((bioG & 0xFF) << 8) | (bioB & 0xFF));
        int midR0 = ((horizonColor >> 16) & 0xFF);
        int midG0 = ((horizonColor >> 8) & 0xFF);
        int midB0 = (horizonColor & 0xFF);
        int midR = clamp255(Math.round(midR0 * 0.72f + waterR * 0.10f + bioR * 0.18f));
        int midG = clamp255(Math.round(midG0 * 0.72f + waterG * 0.10f + bioG * 0.18f));
        int midB = clamp255(Math.round(midB0 * 0.72f + waterB * 0.10f + bioB * 0.18f));
        int midColor = ((midR & 0xFF) << 16) | ((midG & 0xFF) << 8) | (midB & 0xFF);

        int groundColor = sampleGroundColor(world, pos, midColor);
        int groundR = (groundColor >> 16) & 0xFF;
        int groundG = (groundColor >> 8) & 0xFF;
        int groundB = groundColor & 0xFF;
        final float waterSubmerge = estimateWaterSubmerge(world, player);
        if(waterSubmerge > 1e-4f)
        {
            final float gw = clamp01(0.20f + 0.75f * waterSubmerge);
            final float mw = clamp01(0.14f + 0.62f * waterSubmerge);
            final float sw = clamp01(0.10f + 0.48f * waterSubmerge);
            groundR = clamp255(Math.round(groundR * (1.0f - gw) + waterR * gw));
            groundG = clamp255(Math.round(groundG * (1.0f - gw) + waterG * gw));
            groundB = clamp255(Math.round(groundB * (1.0f - gw) + waterB * gw));
            midR = clamp255(Math.round(midR * (1.0f - mw) + waterR * mw));
            midG = clamp255(Math.round(midG * (1.0f - mw) + waterG * mw));
            midB = clamp255(Math.round(midB * (1.0f - mw) + waterB * mw));
            skyR = clamp255(Math.round(skyR * (1.0f - sw) + waterR * sw));
            skyG = clamp255(Math.round(skyG * (1.0f - sw) + waterG * sw));
            skyB = clamp255(Math.round(skyB * (1.0f - sw) + waterB * sw));
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
        smoothSkyR = lerpInt(smoothSkyR, skyR, 0.16f);
        smoothSkyG = lerpInt(smoothSkyG, skyG, 0.16f);
        smoothSkyB = lerpInt(smoothSkyB, skyB, 0.16f);
        smoothMidR = lerpInt(smoothMidR, midR, 0.20f);
        smoothMidG = lerpInt(smoothMidG, midG, 0.20f);
        smoothMidB = lerpInt(smoothMidB, midB, 0.20f);
        smoothGroundR = lerpInt(smoothGroundR, groundR, 0.18f);
        smoothGroundG = lerpInt(smoothGroundG, groundG, 0.18f);
        smoothGroundB = lerpInt(smoothGroundB, groundB, 0.18f);
        smoothWorldIntensity = lerpFloat(smoothWorldIntensity, intensity, 0.16f);

        int bioSkyR = clamp255(Math.round(skyBaseR * 255.0f));
        int bioSkyG = clamp255(Math.round(skyBaseG * 255.0f));
        int bioSkyB = clamp255(Math.round(skyBaseB * 255.0f));
        int bioFogR = clamp255(Math.round(bioSkyR * 0.62f));
        int bioFogG = clamp255(Math.round(bioSkyG * 0.66f));
        int bioFogB = clamp255(Math.round(bioSkyB * 0.72f));
        int waterFogR = clamp255(Math.round(waterR * 0.25f + 8.0f));
        int waterFogG = clamp255(Math.round(waterG * 0.52f + 10.0f));
        int waterFogB = clamp255(Math.round(waterB * 0.90f + 28.0f));
        String dimensionId = sanitizeDimensionKey(world.getRegistryKey());

        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"world_light\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"dir_x\":%.5f,\"dir_y\":%.5f,\"dir_z\":%.5f,\"dir_focus\":%.4f,\"r\":%d,\"g\":%d,\"b\":%d,\"intensity\":%.4f,\"sky_r\":%d,\"sky_g\":%d,\"sky_b\":%d,\"mid_r\":%d,\"mid_g\":%d,\"mid_b\":%d,\"ground_r\":%d,\"ground_g\":%d,\"ground_b\":%d,\"biome_sky_r\":%d,\"biome_sky_g\":%d,\"biome_sky_b\":%d,\"biome_fog_r\":%d,\"biome_fog_g\":%d,\"biome_fog_b\":%d,\"water_fog_r\":%d,\"water_fog_g\":%d,\"water_fog_b\":%d,\"water_submerge\":%.4f,\"env_rain\":%.4f,\"env_thunder\":%.4f,\"dimension\":\"%s\"}",
                System.currentTimeMillis(),
                player.getX(), player.getY() + 1.0, player.getZ(),
                lightDir.x, lightDir.y, lightDir.z, lightFocus,
                r, g, bl, smoothWorldIntensity,
                smoothSkyR, smoothSkyG, smoothSkyB,
                smoothMidR, smoothMidG, smoothMidB,
                smoothGroundR, smoothGroundG, smoothGroundB,
                bioSkyR, bioSkyG, bioSkyB,
                bioFogR, bioFogG, bioFogB,
                waterFogR, waterFogG, waterFogB,
                waterSubmerge,
                rain, thunder,
                dimensionId);
        sendJson(socket, address, json);
    }

    private static Vec3d estimateLocalLightDirection(net.minecraft.world.World world, ClientPlayerEntity player, BlockPos center)
    {
        Vec3d eye = player.getEyePos();
        double ax = 0.0, ay = 0.0, az = 0.0;
        double aw = 0.0;
        for(int dx = -4; dx <= 4; dx++)
        {
            for(int dy = -2; dy <= 3; dy++)
            {
                for(int dz = -4; dz <= 4; dz++)
                {
                    BlockPos p = center.add(dx, dy, dz);
                    var state = world.getBlockState(p);
                    int lum = state.getLuminance();
                    if(lum <= 0)
                    {
                        continue;
                    }
                    Vec3d src = new Vec3d(p.getX() + 0.5, p.getY() + 0.5, p.getZ() + 0.5);
                    Vec3d d = src.subtract(eye);
                    double dist2 = Math.max(0.20, d.lengthSquared());
                    double w = (double)lum / dist2;
                    ax += d.x * w;
                    ay += d.y * w;
                    az += d.z * w;
                    aw += w;
                }
            }
        }
        if(aw <= 1e-6)
        {
            return Vec3d.ZERO;
        }
        return new Vec3d(ax / aw, ay / aw, az / aw);
    }

    private static float estimateWaterSubmerge(net.minecraft.world.World world, ClientPlayerEntity player)
    {
        final Box bb = player.getBoundingBox();
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
                BlockPos p = BlockPos.ofFloored(x, y, z);
                var fs = world.getFluidState(p);
                total++;
                if(fs.isOf(Fluids.WATER))
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
        return clamp01((float)waterHits / (float)total);
    }

    private static float estimateWarmLightBias(net.minecraft.world.World world, BlockPos center)
    {
        int warm = 0;
        int total = 0;
        for(int dx = -2; dx <= 2; dx++)
        {
            for(int dy = -1; dy <= 2; dy++)
            {
                for(int dz = -2; dz <= 2; dz++)
                {
                    BlockPos p = center.add(dx, dy, dz);
                    var state = world.getBlockState(p);
                    int lum = state.getLuminance();
                    if(lum <= 0)
                    {
                        continue;
                    }
                    total += lum;
                    boolean isWarm =
                            state.isOf(Blocks.FIRE) ||
                            state.isOf(Blocks.SOUL_FIRE) ||
                            state.isOf(Blocks.LAVA) ||
                            state.isOf(Blocks.MAGMA_BLOCK) ||
                            state.isOf(Blocks.TORCH) ||
                            state.isOf(Blocks.WALL_TORCH) ||
                            state.isOf(Blocks.SOUL_TORCH) ||
                            state.isOf(Blocks.SOUL_WALL_TORCH) ||
                            state.isOf(Blocks.LANTERN) ||
                            state.isOf(Blocks.SOUL_LANTERN) ||
                            state.isOf(Blocks.CAMPFIRE) ||
                            state.isOf(Blocks.SOUL_CAMPFIRE);
                    if(isWarm)
                    {
                        warm += lum;
                    }
                }
            }
        }
        if(total <= 0)
        {
            return 0.0f;
        }
        return clamp01((float)warm / (float)total);
    }

    private static int sampleGroundColor(net.minecraft.world.World world, net.minecraft.util.math.BlockPos pos, int fallback)
    {
        net.minecraft.util.math.BlockPos p = pos.down();
        for(int i = 0; i < 8; i++)
        {
            if(!world.getBlockState(p).isAir())
            {
                int c = getMapColorRgb(world, p, fallback);
                if(c != 0)
                {
                    return c;
                }
            }
            p = p.down();
        }
        return fallback;
    }

    private static int sampleCeilingColor(net.minecraft.world.World world, net.minecraft.util.math.BlockPos pos, int fallback)
    {
        net.minecraft.util.math.BlockPos p = pos.up();
        for(int i = 0; i < 10; i++)
        {
            if(!world.getBlockState(p).isAir())
            {
                int c = getMapColorRgb(world, p, fallback);
                if(c != 0)
                {
                    return c;
                }
            }
            p = p.up();
        }
        return fallback;
    }

    private static int sampleHorizontalColor(net.minecraft.world.World world, net.minecraft.util.math.BlockPos center, int fallback)
    {
        int[] ox = {3, -3, 0, 0, 2, 2, -2, -2};
        int[] oz = {0, 0, 3, -3, 2, -2, 2, -2};
        int sumR = 0, sumG = 0, sumB = 0, count = 0;
        for(int i = 0; i < ox.length; i++)
        {
            net.minecraft.util.math.BlockPos p = center.add(ox[i], -1, oz[i]);
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
        int r = clamp255(sumR / count);
        int g = clamp255(sumG / count);
        int b = clamp255(sumB / count);
        return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    }

    private static int getMapColorRgb(net.minecraft.world.World world, net.minecraft.util.math.BlockPos pos, int fallback)
    {
        try
        {
            MapColor mapColor = world.getBlockState(pos).getMapColor(world, pos);
            if(mapColor != null)
            {
                return mapColor.color;
            }
        }
        catch(Exception ignored)
        {
        }
        return fallback;
    }

    private static int clamp255(int v)
    {
        return Math.max(0, Math.min(255, v));
    }

    private static float clamp01(float v)
    {
        return Math.max(0.0f, Math.min(1.0f, v));
    }

    private static int lerpInt(int a, int b, float t)
    {
        t = clamp01(t);
        return clamp255(Math.round(a + (b - a) * t));
    }

    private static float lerpFloat(float a, float b, float t)
    {
        t = clamp01(t);
        return a + (b - a) * t;
    }

    private void sendLightningEventIfNeeded(DatagramSocket socket, InetAddress address) throws Exception
    {
        long now = System.currentTimeMillis();
        long age = now - LightningTelemetryState.lastPacketMs;
        if(age >= 0L &&
           age <= LIGHTNING_EVENT_MAX_AGE_MS &&
           (now - lastLightningSentMs) >= LIGHTNING_EVENT_COOLDOWN_MS)
        {
            float strength = clamp01(LightningTelemetryState.lastStrength);
            String json = String.format(Locale.US,
                    "{\"version\":1,\"type\":\"lightning_event\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"strength\":%.4f}",
                    now,
                    strength);
            sendJson(socket, address, json);
            lastLightningSentMs = now;
        }
    }

    private void sendDamageEventIfNeeded(DatagramSocket socket, InetAddress address, ClientPlayerEntity player) throws Exception
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

    private static String sanitizeDimensionKey(RegistryKey<World> key)
    {
        if(key == null)
        {
            return "unknown";
        }
        String id = key.getValue().toString();
        StringBuilder sb = new StringBuilder(id.length());
        for(int i = 0; i < id.length(); i++)
        {
            char c = id.charAt(i);
            if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ':' || c == '.' || c == '-')
            {
                sb.append(c);
            }
        }
        if(sb.length() == 0)
        {
            return "unknown";
        }
        return sb.toString();
    }

    private static void sendJson(DatagramSocket socket, InetAddress address, String json) throws Exception
    {
        byte[] data = json.getBytes(StandardCharsets.UTF_8);
        DatagramPacket packet = new DatagramPacket(data, data.length, address, PORT);
        socket.send(packet);
    }
}
