package me.wolfi.openrgb;

import net.fabricmc.api.ClientModInitializer;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.color.world.BiomeColors;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.block.MapColor;
import net.minecraft.util.hit.BlockHitResult;
import net.minecraft.util.hit.HitResult;
import net.minecraft.util.math.Vec3d;
import net.minecraft.world.LightType;
import net.minecraft.world.RaycastContext;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

public class OpenRGBSenderMod implements ClientModInitializer
{
    private static final String HOST = "127.0.0.1";
    private static final int PORT = 9876;
    private static final long SEND_INTERVAL_MS = 100;
    private static final int PROBE_COUNT = 48;
    private static final Vec3d[] SPHERE_PROBE_DIRS = buildSphereProbeDirs();

    private volatile boolean running = true;
    private float lastHealth = -1.0f;
    private boolean hasSmoothedWorld = false;
    private int smoothSkyR = 140, smoothSkyG = 170, smoothSkyB = 220;
    private int smoothMidR = 110, smoothMidG = 140, smoothMidB = 100;
    private int smoothGroundR = 90, smoothGroundG = 100, smoothGroundB = 70;
    private float smoothWorldIntensity = 0.6f;
    private final int[] smoothProbeR = new int[PROBE_COUNT];
    private final int[] smoothProbeG = new int[PROBE_COUNT];
    private final int[] smoothProbeB = new int[PROBE_COUNT];
    private final float[] smoothProbeI = new float[PROBE_COUNT];

    @Override
    public void onInitializeClient()
    {
        Thread senderThread = new Thread(this::senderLoop, "openrgb-telemetry-sender");
        senderThread.setDaemon(true);
        senderThread.start();
    }

    private void senderLoop()
    {
        try(DatagramSocket socket = new DatagramSocket())
        {
            InetAddress address = InetAddress.getByName(HOST);
            while(running)
            {
                try
                {
                    MinecraftClient client = MinecraftClient.getInstance();
                    ClientPlayerEntity player = (client != null) ? client.player : null;
                    if(player != null && client != null && client.world != null)
                    {
                        sendPlayerPose(socket, address, player);
                        sendHealthState(socket, address, player);
                        sendWorldLight(socket, address, player, client.world);
                        sendDamageEventIfNeeded(socket, address, player);
                    }
                }
                catch(Exception ignored)
                {
                }

                try
                {
                    Thread.sleep(SEND_INTERVAL_MS);
                }
                catch(InterruptedException ignored)
                {
                }
            }
        }
        catch(Exception ignored)
        {
        }
    }

    private void sendPlayerPose(DatagramSocket socket, InetAddress address, ClientPlayerEntity player) throws Exception
    {
        Vec3d look = player.getRotationVec(1.0f);
        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"player_pose\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"fx\":%.5f,\"fy\":%.5f,\"fz\":%.5f,\"ux\":0.0,\"uy\":1.0,\"uz\":0.0}",
                System.currentTimeMillis(),
                player.getX(), player.getY(), player.getZ(),
                look.x, look.y, look.z);
        sendJson(socket, address, json);
    }

    private void sendHealthState(DatagramSocket socket, InetAddress address, ClientPlayerEntity player) throws Exception
    {
        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"health_state\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"health\":%.4f,\"health_max\":%.4f}",
                System.currentTimeMillis(),
                player.getHealth(), player.getMaxHealth());
        sendJson(socket, address, json);
    }

    private void sendWorldLight(DatagramSocket socket, InetAddress address, ClientPlayerEntity player, net.minecraft.world.World world) throws Exception
    {
        MinecraftClient client = MinecraftClient.getInstance();
        var pos = player.getBlockPos();
        int blockLight = world.getLightLevel(LightType.BLOCK, pos);
        int skyLight = world.getLightLevel(LightType.SKY, pos);
        // Block-only is ~0 in open daylight (sky lights the scene); plugin multiplies tint by intensity,
        // so world tint never showed outdoors. Use max(sky, block) for blend weight.
        int combined = Math.max(blockLight, skyLight);
        float intensity = Math.max(0.0f, Math.min(1.0f, combined / 15.0f));

        float blockWeight = (combined > 0) ? ((float)blockLight / (float)combined) : 0.0f;
        float skyWeight = 1.0f - blockWeight;

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
            skyR = clamp255(Math.round(skyBaseR * 255.0f));
            skyG = clamp255(Math.round(skyBaseG * 255.0f));
            skyB = clamp255(Math.round(skyBaseB * 255.0f));
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
        // Underwater floor should be washed blue-green, not raw grass/sand.
        if(player.isSubmergedInWater())
        {
            groundR = clamp255(Math.round(groundR * 0.35f + waterR * 0.65f));
            groundG = clamp255(Math.round(groundG * 0.50f + waterG * 0.50f));
            groundB = clamp255(Math.round(groundB * 0.22f + waterB * 0.78f));
            midR = clamp255(Math.round(midR * 0.55f + waterR * 0.45f));
            midG = clamp255(Math.round(midG * 0.62f + waterG * 0.38f));
            midB = clamp255(Math.round(midB * 0.42f + waterB * 0.58f));
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

        // Sphere-map is player-relative for mapping, but sampled in world-space from player orientation.
        Vec3d lookRaw = player.getRotationVec(1.0f).normalize();
        // Lock room orientation to yaw only so looking up/down does not rotate the environment mapping.
        Vec3d look = new Vec3d(lookRaw.x, 0.0, lookRaw.z);
        if(look.lengthSquared() < 1e-6)
        {
            look = new Vec3d(0.0, 0.0, 1.0);
        }
        else
        {
            look = look.normalize();
        }
        Vec3d worldUp = new Vec3d(0.0, 1.0, 0.0);
        // Player's actual right = look × worldUp (NOT worldUp × look).
        // worldUp × look gives the player's LEFT; look × worldUp gives the player's RIGHT.
        // Verified for all four headings:
        //   South(0,0,+1) → (−1,0,0) = West = right ✓
        //   North(0,0,−1) → (+1,0,0) = East = right ✓
        //   East (+1,0,0) → (0,0,+1) = South = right ✓
        //   West (−1,0,0) → (0,0,−1) = North = right ✓
        Vec3d right = look.crossProduct(worldUp);
        if(right.lengthSquared() < 1e-6) {
            right = new Vec3d(-1.0, 0.0, 0.0);
        } else {
            right = right.normalize();
        }
        // Since look is horizontal (y=0), worldUp is already perpendicular — use it directly.
        Vec3d up = worldUp;

        StringBuilder probes = new StringBuilder();
        probes.append(",\"probe_count\":").append(SPHERE_PROBE_DIRS.length);
        for (int i = 0; i < SPHERE_PROBE_DIRS.length; i++) {
            Vec3d localDir = SPHERE_PROBE_DIRS[i];
            // Canonical probe direction: no sender-side axis flips.
            Vec3d sendDir = localDir;
            Vec3d worldDir = right.multiply(sendDir.x).add(up.multiply(sendDir.y)).add(look.multiply(sendDir.z)).normalize();
            ProbeSample ps = sampleProbe(world, player, worldDir, midColor);
            // Decay toward black faster than rise toward bright (stops smoothed probes from lingering as a gray haze).
            float lumPs = (0.2126f * ps.r + 0.7152f * ps.g + 0.0722f * ps.b);
            float lumSm = (0.2126f * smoothProbeR[i] + 0.7152f * smoothProbeG[i] + 0.0722f * smoothProbeB[i]);
            float cAlpha = (ps.intensity < smoothProbeI[i] - 0.03f || lumPs < lumSm - 3.0f) ? 0.52f : 0.28f;
            float iAlpha = (ps.intensity < smoothProbeI[i]) ? 0.52f : 0.28f;
            smoothProbeR[i] = lerpInt(smoothProbeR[i], ps.r, cAlpha);
            smoothProbeG[i] = lerpInt(smoothProbeG[i], ps.g, cAlpha);
            smoothProbeB[i] = lerpInt(smoothProbeB[i], ps.b, cAlpha);
            smoothProbeI[i] = lerpFloat(smoothProbeI[i], ps.intensity, iAlpha);
            probes.append(String.format(Locale.US,
                    ",\"p%d_dx\":%.4f,\"p%d_dy\":%.4f,\"p%d_dz\":%.4f,\"p%d_r\":%d,\"p%d_g\":%d,\"p%d_b\":%d,\"p%d_i\":%.4f",
                    i, sendDir.x, i, sendDir.y, i, sendDir.z,
                    i, smoothProbeR[i], i, smoothProbeG[i], i, smoothProbeB[i], i, smoothProbeI[i]));
        }

        String json = String.format(Locale.US,
                "{\"version\":1,\"type\":\"world_light\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,\"r\":%d,\"g\":%d,\"b\":%d,\"intensity\":%.4f,\"sky_r\":%d,\"sky_g\":%d,\"sky_b\":%d,\"mid_r\":%d,\"mid_g\":%d,\"mid_b\":%d,\"ground_r\":%d,\"ground_g\":%d,\"ground_b\":%d%s}",
                System.currentTimeMillis(),
                player.getX(), player.getY() + 1.0, player.getZ(),
                r, g, bl, smoothWorldIntensity,
                smoothSkyR, smoothSkyG, smoothSkyB,
                smoothMidR, smoothMidG, smoothMidB,
                smoothGroundR, smoothGroundG, smoothGroundB,
                probes.toString());
        sendJson(socket, address, json);
    }

    private static class ProbeSample
    {
        final int r;
        final int g;
        final int b;
        final float intensity;

        ProbeSample(int r, int g, int b, float intensity)
        {
            this.r = r;
            this.g = g;
            this.b = b;
            this.intensity = intensity;
        }
    }

    private static ProbeSample sampleProbe(net.minecraft.world.World world, ClientPlayerEntity player, Vec3d dir, int fallbackColor)
    {
        Vec3d start = new Vec3d(player.getX(), player.getY() + 1.4, player.getZ());
        Vec3d end = start.add(dir.multiply(12.0));
        BlockHitResult hit = world.raycast(new RaycastContext(
                start,
                end,
                RaycastContext.ShapeType.COLLIDER,
                RaycastContext.FluidHandling.ANY,
                player
        ));

        net.minecraft.util.math.BlockPos sp;
        if(hit != null && hit.getType() == HitResult.Type.BLOCK)
        {
            sp = hit.getBlockPos();
        }
        else
        {
            sp = net.minecraft.util.math.BlockPos.ofFloored(end);
        }

        int sampled = getMapColorRgb(world, sp, fallbackColor);
        int blockLight = world.getLightLevel(LightType.BLOCK, sp);
        int skyLight = world.getLightLevel(LightType.SKY, sp);
        int maxLight = Math.max(blockLight, skyLight);
        float intensity = clamp01(maxLight / 15.0f);
        // No minimum intensity: in true darkness every probe weight goes to zero on the plugin side.
        if(maxLight <= 0)
        {
            return new ProbeSample(0, 0, 0, 0.0f);
        }

        int rr = (sampled >> 16) & 0xFF;
        int gg = (sampled >> 8) & 0xFF;
        int bb = sampled & 0xFF;
        // Brighter, more vivid probes; lightCurve still pulls true-black toward black without gray haze.
        float boost = 0.86f + 0.52f * intensity;
        rr = clamp255(Math.round(rr * boost));
        gg = clamp255(Math.round(gg * boost));
        bb = clamp255(Math.round(bb * boost));
        float sat = 1.14f + 0.18f * intensity;
        float con = 1.06f + 0.14f * intensity;
        int[] vivid = applyVividTone(rr, gg, bb, sat, con);
        rr = vivid[0];
        gg = vivid[1];
        bb = vivid[2];
        // Softer than 1.42: more LED pop in normal/indoor light; intensity=0 still returns black above.
        float lightCurve = (float)Math.pow(intensity, 1.20);
        rr = clamp255(Math.round(rr * lightCurve));
        gg = clamp255(Math.round(gg * lightCurve));
        bb = clamp255(Math.round(bb * lightCurve));
        return new ProbeSample(rr, gg, bb, intensity);
    }

    private static Vec3d[] buildSphereProbeDirs()
    {
        Vec3d[] dirs = new Vec3d[PROBE_COUNT];
        final double goldenAngle = Math.PI * (3.0 - Math.sqrt(5.0));
        for(int i = 0; i < PROBE_COUNT; i++)
        {
            double t = (i + 0.5) / (double)PROBE_COUNT;
            double y = 1.0 - 2.0 * t;
            double r = Math.sqrt(Math.max(0.0, 1.0 - y * y));
            double theta = i * goldenAngle;
            double x = Math.cos(theta) * r;
            double z = Math.sin(theta) * r;
            dirs[i] = new Vec3d(x, y, z).normalize();
        }
        return dirs;
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

    private static int[] applyVividTone(int r, int g, int b, float saturation, float contrast)
    {
        float rf = clamp01(r / 255.0f);
        float gf = clamp01(g / 255.0f);
        float bf = clamp01(b / 255.0f);

        float luma = 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
        rf = clamp01(luma + (rf - luma) * saturation);
        gf = clamp01(luma + (gf - luma) * saturation);
        bf = clamp01(luma + (bf - luma) * saturation);

        rf = clamp01((rf - 0.5f) * contrast + 0.5f);
        gf = clamp01((gf - 0.5f) * contrast + 0.5f);
        bf = clamp01((bf - 0.5f) * contrast + 0.5f);

        // White suppression: reduce low-chroma bright whites so vivid hues dominate.
        float maxc = Math.max(rf, Math.max(gf, bf));
        float minc = Math.min(rf, Math.min(gf, bf));
        float chroma = maxc - minc;
        if(maxc > 0.70f && chroma < 0.16f)
        {
            float k = clamp01((0.16f - chroma) / 0.16f) * clamp01((maxc - 0.70f) / 0.30f);
            float neutral = (rf + gf + bf) / 3.0f;
            float target = neutral * (0.78f - 0.22f * k);
            rf = clamp01(rf + (target - rf) * 0.55f * k);
            gf = clamp01(gf + (target - gf) * 0.55f * k);
            bf = clamp01(bf + (target - bf) * 0.55f * k);
        }

        return new int[] {
                clamp255(Math.round(rf * 255.0f)),
                clamp255(Math.round(gf * 255.0f)),
                clamp255(Math.round(bf * 255.0f))
        };
    }

    private void sendDamageEventIfNeeded(DatagramSocket socket, InetAddress address, ClientPlayerEntity player) throws Exception
    {
        float health = player.getHealth();
        if(lastHealth >= 0.0f && health < lastHealth)
        {
            float amount = Math.max(0.0f, lastHealth - health);
            String json = String.format(Locale.US,
                    "{\"version\":1,\"type\":\"damage_event\",\"timestamp_ms\":%d,\"source\":\"minecraft-fabric\",\"amount\":%.4f,\"dir_x\":0.0,\"dir_y\":0.0,\"dir_z\":1.0}",
                    System.currentTimeMillis(),
                    amount);
            sendJson(socket, address, json);
        }
        lastHealth = health;
    }

    private static void sendJson(DatagramSocket socket, InetAddress address, String json) throws Exception
    {
        byte[] data = json.getBytes(StandardCharsets.UTF_8);
        DatagramPacket packet = new DatagramPacket(data, data.length, address, PORT);
        socket.send(packet);
    }
}
