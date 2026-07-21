package me.wolfi.openrgb;

import net.minecraft.client.Camera;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.client.renderer.BiomeColors;
import net.minecraft.client.renderer.fog.environment.AtmosphericFogEnvironment;
import net.minecraft.client.renderer.state.level.SkyRenderState;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Samples live sky, fog, and biome tint colours from the 26.2 client renderer.
 * Uses {@link SkyRenderState} and {@link AtmosphericFogEnvironment} instead of the
 * removed BiomeColors sky/fog helpers.
 */
final class AtmosphereSampler
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final AtmosphericFogEnvironment ATMOSPHERIC_FOG = new AtmosphericFogEnvironment();
    private static final SkyRenderState SKY_SCRATCH = new SkyRenderState();

    /**
     * One extract per Room VR frame — sky disc, sunrise/sunset glow, and day/night factor.
     * {@code skyBrightness} is 1 at noon and ~0 at midnight (from {@link Level#getSkyDarken()}).
     */
    record Frame(int skyColor,
                 int sunriseSunsetColor,
                 float sunAngleRad,
                 float starBrightness,
                 float skyBrightness,
                 float weather,
                 int fogColor)
    {
        static final Frame EMPTY = new Frame(0x87CEEB, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0x87CEEB);
    }

    private AtmosphereSampler()
    {
    }

    /** Capture sky/fog once per tick — do not call per LED cell. */
    static Frame captureFrame(Level world)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client == null || client.level != world || !(world instanceof ClientLevel level))
        {
            return Frame.EMPTY;
        }
        try
        {
            final Camera camera = client.gameRenderer.mainCamera();
            if(!camera.isInitialized())
            {
                return Frame.EMPTY;
            }
            final DeltaTracker delta = client.getDeltaTracker();
            final float partialTick = camera.getCameraEntityPartialTicks(delta);
            SKY_SCRATCH.reset();
            client.levelRenderer.skyRenderer().extractRenderState(level, partialTick, camera, SKY_SCRATCH);

            final int skyDarken = Math.max(0, Math.min(15, world.getSkyDarken()));
            final float skyBrightness = 1.0f - (skyDarken / 15.0f);
            final float weather = weatherMoodStrength(world);
            final int fog = sampleFogColor(client, SKY_SCRATCH.skyColor);
            return new Frame(
                    SKY_SCRATCH.skyColor,
                    SKY_SCRATCH.sunriseAndSunsetColor,
                    SKY_SCRATCH.sunAngle,
                    SKY_SCRATCH.starBrightness,
                    skyBrightness,
                    weather,
                    fog);
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "atmosphere sample failed", t);
            return Frame.EMPTY;
        }
    }

    /**
     * Directional sky for one LED ray: zenith/horizon uses disc colour; rays facing the sun
     * near sunrise/sunset pick up the warm glow (and the opposite side stays cooler/darker).
     */
    static int sampleDirectionalSky(Frame frame, Vec3 eye, Vec3 target)
    {
        if(frame == null)
        {
            return 0x87CEEB;
        }
        int sr = (frame.skyColor() >> 16) & 0xFF;
        int sg = (frame.skyColor() >> 8) & 0xFF;
        int sb = frame.skyColor() & 0xFF;

        final double dx = target.x - eye.x;
        final double dy = target.y - eye.y;
        final double dz = target.z - eye.z;
        final double len = Math.sqrt(dx * dx + dy * dy + dz * dz);
        if(len < 1.0e-4)
        {
            return frame.skyColor();
        }
        final float rx = (float)(dx / len);
        final float ry = (float)(dy / len);
        final float rz = (float)(dz / len);

        // Sun path in the Y/X plane. Negate vs raw celestial angle so warm glow
        // appears on LEDs looking *toward* sunrise/sunset (was inverted before).
        final float sunX = -(float)Math.sin(frame.sunAngleRad());
        final float sunY = -(float)Math.cos(frame.sunAngleRad());
        final float sunZ = 0.0f;

        // Strong warm glow only when the sun is near the horizon.
        float horizonSun = 1.0f - Math.abs(sunY);
        horizonSun = clamp01(horizonSun);
        horizonSun = horizonSun * horizonSun;

        final float facingSun = clamp01(rx * sunX + ry * sunY + rz * sunZ);
        final float horizonBand = clamp01(1.0f - Math.abs(ry)) * horizonSun;
        // Gate horizon wash by facingSun so the anti-sun side stays cool blue.
        float warm = clamp01(facingSun * horizonSun * (0.95f + horizonBand * 0.40f));

        final int glow = frame.sunriseSunsetColor();
        final int gr = (glow >> 16) & 0xFF;
        final int gg = (glow >> 8) & 0xFF;
        final int gb = glow & 0xFF;
        final boolean glowAlive = (gr + gg + gb) > 24;
        if(glowAlive && warm > 0.02f)
        {
            sr = ColorMath.clamp255(Math.round(sr * (1.0f - warm) + gr * warm));
            sg = ColorMath.clamp255(Math.round(sg * (1.0f - warm) + gg * warm));
            sb = ColorMath.clamp255(Math.round(sb * (1.0f - warm) + gb * warm));
        }

        // Opposite sky during dawn/dusk: cool off slightly so one side isn't mirrored warm.
        if(glowAlive && horizonSun > 0.35f)
        {
            final float anti = clamp01((-facingSun) * horizonSun * 0.35f);
            if(anti > 0.02f)
            {
                final int coolR = Math.max(0, sr - 40);
                final int coolG = Math.max(0, sg - 25);
                final int coolB = Math.min(255, sb + 20);
                sr = ColorMath.clamp255(Math.round(sr * (1.0f - anti) + coolR * anti));
                sg = ColorMath.clamp255(Math.round(sg * (1.0f - anti) + coolG * anti));
                sb = ColorMath.clamp255(Math.round(sb * (1.0f - anti) + coolB * anti));
            }
        }

        // Night: pull empty sky toward deep blue and stars (subtle).
        if(frame.skyBrightness() < 0.35f)
        {
            final float night = clamp01(1.0f - frame.skyBrightness() / 0.35f);
            final int nr = 8;
            final int ng = 10;
            final int nb = 28;
            sr = ColorMath.clamp255(Math.round(sr * (1.0f - night * 0.55f) + nr * night * 0.55f));
            sg = ColorMath.clamp255(Math.round(sg * (1.0f - night * 0.55f) + ng * night * 0.55f));
            sb = ColorMath.clamp255(Math.round(sb * (1.0f - night * 0.45f) + nb * night * 0.45f));
            if(frame.starBrightness() > 0.15f && ry > 0.25f)
            {
                final float sparkle = frame.starBrightness() * 0.12f * clamp01(ry);
                sr = ColorMath.clamp255(Math.round(sr + 40 * sparkle));
                sg = ColorMath.clamp255(Math.round(sg + 45 * sparkle));
                sb = ColorMath.clamp255(Math.round(sb + 55 * sparkle));
            }
        }

        if(frame.weather() > 0.12f)
        {
            final float fw = Math.min(0.40f, frame.weather() * 0.35f);
            sr = ColorMath.clamp255(Math.round(sr * (1.0f - fw) + ((frame.fogColor() >> 16) & 0xFF) * fw));
            sg = ColorMath.clamp255(Math.round(sg * (1.0f - fw) + ((frame.fogColor() >> 8) & 0xFF) * fw));
            sb = ColorMath.clamp255(Math.round(sb * (1.0f - fw) + (frame.fogColor() & 0xFF) * fw));
        }

        // Mild chroma only — heavy sat was flattening sunrise oranges into neon blue.
        final float gray = (sr + sg + sb) / 3.0f;
        final float sat = 1.12f + 0.18f * warm;
        sr = ColorMath.clamp255(Math.round(gray + (sr - gray) * sat));
        sg = ColorMath.clamp255(Math.round(gray + (sg - gray) * sat));
        sb = ColorMath.clamp255(Math.round(gray + (sb - gray) * sat));
        return (sr << 16) | (sg << 8) | sb;
    }

    static int sampleSkyColor(Level world, BlockPos pos, int fallback)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client == null || client.level != world)
        {
            return fallback;
        }
        return sampleSkyColor(client, fallback);
    }

    static int sampleSkyColor(Minecraft client, int fallback)
    {
        try
        {
            final ClientLevel level = client.level;
            if(level == null)
            {
                return fallback;
            }
            final Camera camera = client.gameRenderer.mainCamera();
            if(!camera.isInitialized())
            {
                return fallback;
            }
            final DeltaTracker delta = client.getDeltaTracker();
            final float partialTick = camera.getCameraEntityPartialTicks(delta);
            SKY_SCRATCH.reset();
            client.levelRenderer.skyRenderer().extractRenderState(level, partialTick, camera, SKY_SCRATCH);
            return SKY_SCRATCH.skyColor;
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "atmosphere sample failed", t);
            return fallback;
        }
    }

    static int sampleFogColor(Level world, BlockPos pos, int fallback)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client == null || client.level != world)
        {
            return fallback;
        }
        return sampleFogColor(client, fallback);
    }

    static int sampleFogColor(Minecraft client, int fallback)
    {
        try
        {
            final ClientLevel level = client.level;
            if(level == null)
            {
                return fallback;
            }
            final Camera camera = client.gameRenderer.mainCamera();
            if(!camera.isInitialized())
            {
                return fallback;
            }
            final DeltaTracker delta = client.getDeltaTracker();
            final float partialTick = camera.getCameraEntityPartialTicks(delta);
            final int renderDistance = client.options.getEffectiveRenderDistance();
            return ATMOSPHERIC_FOG.getBaseColor(level, camera, renderDistance, partialTick);
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "atmosphere sample failed", t);
            return fallback;
        }
    }

    /**
     * 0 on clear days; rises with rain/snow/thunder. Used so fog only washes LED colours
     * when weather is actually happening — not every clear afternoon.
     */
    static float weatherMoodStrength(Level world)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client == null || client.level != world || !(world instanceof ClientLevel level))
        {
            return 0.0f;
        }
        try
        {
            final Camera camera = client.gameRenderer.mainCamera();
            final DeltaTracker delta = client.getDeltaTracker();
            final float partialTick = camera.isInitialized()
                    ? camera.getCameraEntityPartialTicks(delta)
                    : 1.0f;
            final float rain = clamp01(level.getRainLevel(partialTick));
            final float thunder = clamp01(level.getThunderLevel(partialTick));
            // Thunder storms read heavier; light drizzle stays subtle.
            return clamp01(rain * 0.85f + thunder * 0.55f);
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "atmosphere sample failed", t);
            return 0.0f;
        }
    }

    /** Day/night factor for block shading: 1 noon, ~0 midnight. */
    static float skyBrightness(Level world)
    {
        if(world == null)
        {
            return 1.0f;
        }
        final int skyDarken = Math.max(0, Math.min(15, world.getSkyDarken()));
        return 1.0f - (skyDarken / 15.0f);
    }

    private static float clamp01(float v)
    {
        if(v < 0.0f)
        {
            return 0.0f;
        }
        if(v > 1.0f)
        {
            return 1.0f;
        }
        return v;
    }

    static int sampleGrassColor(Level world, BlockPos pos, int fallback)
    {
        if(world instanceof ClientLevel clientLevel)
        {
            try
            {
                return BiomeColors.getAverageGrassColor(clientLevel, pos);
            }
            catch(Throwable t)
            {
                QuietCatch.debug(LOGGER, "biome color sample failed", t);
            }
        }
        return fallback;
    }

    static int sampleFoliageColor(Level world, BlockPos pos, int fallback)
    {
        if(world instanceof ClientLevel clientLevel)
        {
            try
            {
                return BiomeColors.getAverageFoliageColor(clientLevel, pos);
            }
            catch(Throwable t)
            {
                QuietCatch.debug(LOGGER, "biome color sample failed", t);
            }
        }
        return fallback;
    }

    static int sampleWaterColor(Level world, BlockPos pos, int fallback)
    {
        if(world instanceof ClientLevel clientLevel)
        {
            try
            {
                return BiomeColors.getAverageWaterColor(clientLevel, pos);
            }
            catch(Throwable t)
            {
                QuietCatch.debug(LOGGER, "biome color sample failed", t);
            }
        }
        return fallback;
    }
}
