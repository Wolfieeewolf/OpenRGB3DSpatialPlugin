package me.wolfi.openrgb;

import net.minecraft.client.Minecraft;
import net.minecraft.client.color.block.BlockTintSource;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.Block;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.BeaconBeamBlock;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.IronBarsBlock;
import net.minecraft.world.level.block.LightBlock;
import net.minecraft.world.level.block.TransparentBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.level.material.MapColor;

/**
 * Block display colours from loaded block textures and in-game tint sources (biome foliage,
 * birch/spruce constants, water, etc.). Supports snowy leaves and per-layer opacity for
 * viewport compositing through semi-transparent surfaces.
 */
final class BlockDisplayColorSampler
{
    private static final float BLOCK_COLOR_SATURATION = 1.72f;
    private static final int SNOW_BLEND_RGB = 0xFFFFFF;
    private static final float SNOWY_LEAVES_BLEND = 0.72f;

    /** Average RGB and alpha from a block sprite PNG (shared with {@link BlockTexturePrecache}). */
    record TextureSample(int rgb, int averageAlpha)
    {
    }

    private BlockDisplayColorSampler()
    {
    }

    static boolean continuesViewportRay(BlockState state, FluidState fluid, int coverAlpha)
    {
        if(coverAlpha >= 250)
        {
            return false;
        }
        if(fluid.is(Fluids.WATER))
        {
            return true;
        }
        if(state.isAir())
        {
            return false;
        }
        if(state.is(BlockTags.LEAVES))
        {
            return true;
        }
        if(state.is(Blocks.LIGHT))
        {
            return true;
        }
        if(state.getBlock() instanceof TransparentBlock || state.getBlock() instanceof IronBarsBlock)
        {
            return true;
        }
        return false;
    }

    static int viewportCoverAlpha(BlockState state, FluidState fluid)
    {
        if(fluid.is(Fluids.WATER))
        {
            return 115;
        }
        if(state.isAir())
        {
            return 0;
        }
        if(state.is(BlockTags.LEAVES))
        {
            return 145;
        }
        if(state.is(Blocks.LIGHT))
        {
            int level = 15;
            if(state.hasProperty(LightBlock.LEVEL))
            {
                level = state.getValue(LightBlock.LEVEL);
            }
            return clamp255(35 + level * 4);
        }
        if(state.getBlock() instanceof BeaconBeamBlock beam)
        {
            return 95;
        }
        if(state.is(Blocks.TINTED_GLASS))
        {
            return 105;
        }
        if(state.getBlock() instanceof TransparentBlock)
        {
            return 75;
        }
        if(state.getBlock() instanceof IronBarsBlock)
        {
            return 65;
        }
        return 255;
    }

    static void sampleWaterLayer(Level world, BlockPos pos, int[] out)
    {
        final int rgb = AtmosphereSampler.sampleWaterColor(world, pos, 0x3F76E4);
        writeLitLayer(world, pos, rgb, viewportCoverAlpha(Blocks.WATER.defaultBlockState(),
                world.getFluidState(pos)), out);
    }

    static void sampleViewportLayer(Level world, BlockPos pos, BlockState state, Direction face, int[] out)
    {
        final FluidState fluid = world.getFluidState(pos);
        if(fluid.is(Fluids.WATER))
        {
            sampleWaterLayer(world, pos, out);
            return;
        }
        if(state.isAir())
        {
            clearLayer(out);
            return;
        }

        int rgb = resolveDisplayRgb(world, pos, state, face);
        final int alpha = viewportCoverAlpha(state, fluid);
        writeLitLayer(world, pos, rgb, alpha, out);
    }

    private static int resolveDisplayRgb(Level world, BlockPos pos, BlockState state, Direction face)
    {
        if(state.getBlock() instanceof BeaconBeamBlock beam)
        {
            return 0xFF000000 | (beam.getColor().getTextureDiffuseColor() & 0xFFFFFF);
        }
        if(state.is(Blocks.LIGHT))
        {
            return 0xFFFFFF;
        }

        int rgb = sampleTexturedBlockRgb(world, pos, state, face);
        if(state.is(BlockTags.LEAVES) && isSnowyLeaves(world, pos, state))
        {
            rgb = blendRgb(rgb, SNOW_BLEND_RGB, SNOWY_LEAVES_BLEND);
        }
        return rgb;
    }

    private static int sampleTexturedBlockRgb(Level world, BlockPos pos, BlockState state, Direction face)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client != null && client.level == world)
        {
            try
            {
                final BlockFaceColorCache.FaceColors faceColors =
                        BlockFaceColorCache.get(Block.getId(state));
                if(faceColors != null)
                {
                    final int baseRgb = faceColors.rgbFor(face);
                    if(baseRgb >= 0)
                    {
                        return applyBlockTint(world, pos, state, baseRgb);
                    }
                }
            }
            catch(Throwable ignored)
            {
            }
        }

        final int tintOnly = sampleBlockTintRgb(world, pos, state);
        if(tintOnly != 0 && tintOnly != 0xFFFFFF)
        {
            return tintOnly & 0xFFFFFF;
        }
        return mapColorRgb(world, pos, state, 0x808080);
    }

    private static int applyBlockTint(Level world, BlockPos pos, BlockState state, int baseRgb)
    {
        final int tint = sampleBlockTintRgb(world, pos, state);
        int r = mulChannel((baseRgb >> 16) & 0xFF, (tint >> 16) & 0xFF);
        int g = mulChannel((baseRgb >> 8) & 0xFF, (tint >> 8) & 0xFF);
        int b = mulChannel(baseRgb & 0xFF, tint & 0xFF);
        return (r << 16) | (g << 8) | b;
    }

    private static int sampleBlockTintRgb(Level world, BlockPos pos, BlockState state)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client == null || client.level != world || !(world instanceof ClientLevel clientLevel))
        {
            return 0xFFFFFF;
        }
        try
        {
            int tr = 255;
            int tg = 255;
            int tb = 255;
            boolean hasTint = false;
            final var tints = client.getBlockColors().getTintSources(state);
            for(int i = 0; i < tints.size(); i++)
            {
                final BlockTintSource source = tints.get(i);
                if(source == null)
                {
                    continue;
                }
                final int tint = source.colorInWorld(state, clientLevel, pos);
                if(tint < 0)
                {
                    continue;
                }
                tr = mulChannel(tr, (tint >> 16) & 0xFF);
                tg = mulChannel(tg, (tint >> 8) & 0xFF);
                tb = mulChannel(tb, tint & 0xFF);
                hasTint = true;
            }
            if(hasTint)
            {
                return (tr << 16) | (tg << 8) | tb;
            }
        }
        catch(Throwable ignored)
        {
        }
        return 0xFFFFFF;
    }

    private static boolean isSnowyLeaves(Level world, BlockPos pos, BlockState state)
    {
        if(!state.is(BlockTags.LEAVES))
        {
            return false;
        }
        final BlockState above = world.getBlockState(pos.above());
        if(above.is(Blocks.SNOW) || above.is(Blocks.POWDER_SNOW) || above.is(Blocks.SNOW_BLOCK))
        {
            return true;
        }
        if(world instanceof LevelReader reader)
        {
            try
            {
                final Biome biome = world.getBiome(pos).value();
                if(biome.shouldSnow(reader, pos))
                {
                    return true;
                }
                if(biome.coldEnoughToSnow(pos, world.getSeaLevel()) && world.isRainingAt(pos))
                {
                    return true;
                }
            }
            catch(Throwable ignored)
            {
            }
        }
        return false;
    }

    private static void writeLitLayer(Level world, BlockPos pos, int rgb, int alpha, int[] out)
    {
        int sky = world.getBrightness(LightLayer.SKY, pos);
        int block = world.getBrightness(LightLayer.BLOCK, pos);
        int light = Math.max(sky, block);
        float k = 0.55f + 0.45f * (light / 15.0f);
        int r = clamp255((int)(((rgb >> 16) & 0xFF) * k));
        int g = clamp255((int)(((rgb >> 8) & 0xFF) * k));
        int b = clamp255((int)((rgb & 0xFF) * k));
        boostSaturation(r, g, b, BLOCK_COLOR_SATURATION, alpha, out);
    }

    private static void boostSaturation(int r, int g, int b, float saturation, int alpha, int[] out)
    {
        float gray = (r + g + b) / 3.0f;
        out[0] = clamp255(Math.round(gray + (r - gray) * saturation));
        out[1] = clamp255(Math.round(gray + (g - gray) * saturation));
        out[2] = clamp255(Math.round(gray + (b - gray) * saturation));
        out[3] = clamp255(alpha);
    }

    private static void clearLayer(int[] out)
    {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
    }

    private static int mapColorRgb(Level world, BlockPos pos, BlockState state, int fallback)
    {
        try
        {
            MapColor mapColor = state.getMapColor(world, pos);
            if(mapColor != null)
            {
                return mapColor.col;
            }
        }
        catch(Throwable ignored)
        {
        }
        return fallback;
    }

    private static int mulChannel(int a, int b)
    {
        return (a * b) / 255;
    }

    private static int blendRgb(int rgb, int overlay, float t)
    {
        t = Math.max(0.0f, Math.min(1.0f, t));
        int r1 = (rgb >> 16) & 0xFF;
        int g1 = (rgb >> 8) & 0xFF;
        int b1 = rgb & 0xFF;
        int r2 = (overlay >> 16) & 0xFF;
        int g2 = (overlay >> 8) & 0xFF;
        int b2 = overlay & 0xFF;
        int r = clamp255(Math.round(r1 + (r2 - r1) * t));
        int g = clamp255(Math.round(g1 + (g2 - g1) * t));
        int b = clamp255(Math.round(b1 + (b2 - b1) * t));
        return (r << 16) | (g << 8) | b;
    }

    private static int clamp255(int v)
    {
        return Math.max(0, Math.min(255, v));
    }
}
