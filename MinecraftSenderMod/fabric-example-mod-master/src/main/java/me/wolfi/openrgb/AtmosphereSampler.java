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

    private AtmosphereSampler()
    {
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
