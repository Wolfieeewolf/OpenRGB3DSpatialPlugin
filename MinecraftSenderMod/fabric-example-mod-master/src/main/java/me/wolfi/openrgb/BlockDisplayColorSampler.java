package me.wolfi.openrgb;

import net.minecraft.client.Minecraft;
import net.minecraft.client.color.block.BlockTintSource;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.Block;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.BeaconBeamBlock;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.IronBarsBlock;
import net.minecraft.world.level.block.LightBlock;
import net.minecraft.world.level.block.TransparentBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.level.material.MapColor;
import net.minecraft.world.phys.Vec3;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Block display colours from UV texel samples (with face-average fallback), tint sources, and
 * per-layer opacity for compositing through cutouts, water, glass, and light blocks.
 */
final class BlockDisplayColorSampler
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final float BLOCK_COLOR_SATURATION = 1.48f;
    private static final float EMISSIVE_SATURATION = 1.65f;
    private static final int SNOW_BLEND_RGB = 0xFFFFFF;
    private static final float SNOWY_LEAVES_BLEND = 0.45f;
    private static final ThreadLocal<int[]> TEXEL_RGBA = ThreadLocal.withInitial(() -> new int[4]);

    /** Average RGB and alpha from a block sprite PNG (shared with {@link BlockTexturePrecache}). */
    record TextureSample(int rgb, int averageAlpha)
    {
    }

    private BlockDisplayColorSampler()
    {
    }

    static boolean continuesViewportRay(BlockState state, FluidState fluid, int coverAlpha)
    {
        // Leaf canopies are full surfaces for LEDs — do not punch through holes to sky (reads white).
        if(state.is(BlockTags.LEAVES))
        {
            return false;
        }
        // Lava is a strong emissive surface — stop on it.
        if(fluid.is(Fluids.LAVA) || state.is(Blocks.LAVA) || state.is(Blocks.MAGMA_BLOCK))
        {
            return false;
        }
        if(isFireLike(state))
        {
            // Fire is a cutout — composite, then keep going so ground behind still contributes.
            return true;
        }
        if(BlockUvTexelSampler.shouldContinueThroughTexel(coverAlpha))
        {
            return true;
        }
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
        return viewportCoverAlpha(state, fluid, -1);
    }

    static int viewportCoverAlpha(BlockState state, FluidState fluid, int textureAlpha)
    {
        if(fluid.is(Fluids.LAVA) || state.is(Blocks.LAVA))
        {
            return 235;
        }
        if(fluid.is(Fluids.WATER))
        {
            return 115;
        }
        if(state.isAir())
        {
            return 0;
        }
        if(isFireLike(state))
        {
            return textureAlpha >= 0 ? ColorMath.clamp255(Math.max(140, textureAlpha)) : 175;
        }
        if(state.is(Blocks.MAGMA_BLOCK))
        {
            return 255;
        }
        if(state.is(Blocks.LIGHT))
        {
            int level = 15;
            if(state.hasProperty(LightBlock.LEVEL))
            {
                level = state.getValue(LightBlock.LEVEL);
            }
            return ColorMath.clamp255(35 + level * 4);
        }
        if(state.getBlock() instanceof BeaconBeamBlock)
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
        if(state.is(BlockTags.LEAVES))
        {
            // Keep canopy opaque even when the hit texel is a leaf hole.
            return 255;
        }
        if(textureAlpha >= 0)
        {
            return ColorMath.clamp255(textureAlpha);
        }
        return 255;
    }

    static void sampleWaterLayer(Level world, BlockPos pos, int[] out)
    {
        final int rgb = AtmosphereSampler.sampleWaterColor(world, pos, 0x3F76E4);
        writeLitLayer(world, pos, rgb, viewportCoverAlpha(Blocks.WATER.defaultBlockState(),
                world.getFluidState(pos)), out);
    }

    static void sampleLavaLayer(Level world, BlockPos pos, int[] out)
    {
        // Bright lava with slight spatial flicker so LEDs don't look flat.
        final int h = pos.hashCode();
        final int warm = 0xFF6A12;
        final int hot = 0xFFC23A;
        final float t = ((h >>> 8) & 0xFF) / 255.0f;
        final int rgb = ColorMath.blendRgb(warm, hot, 0.35f + 0.45f * t);
        writeEmissiveLayer(rgb, 235, out);
    }

    static void sampleViewportLayer(Level world, BlockPos pos, BlockState state, Direction face, int[] out)
    {
        sampleViewportLayer(world, pos, state, face, null, out);
    }

    static void sampleViewportLayer(Level world, BlockPos pos, BlockState state, Direction face, Vec3 hit,
                                    int[] out)
    {
        final FluidState fluid = world.getFluidState(pos);
        if(fluid.is(Fluids.LAVA) || state.is(Blocks.LAVA))
        {
            sampleLavaLayer(world, pos, out);
            return;
        }
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

        final int[] texel = TEXEL_RGBA.get();
        texel[0] = 0;
        texel[1] = 0;
        texel[2] = 0;
        texel[3] = -1;
        int rgb = resolveDisplayRgb(world, pos, state, face, hit, texel);
        final int alpha = viewportCoverAlpha(state, fluid, texel[3]);
        if(isFireLike(state) || state.is(Blocks.MAGMA_BLOCK))
        {
            if(texel[3] < 40)
            {
                rgb = state.is(Blocks.SOUL_FIRE) || state.is(Blocks.SOUL_CAMPFIRE) ? 0x3AD7FF : 0xFF7A18;
                texel[3] = 180;
            }
            writeEmissiveLayer(boostRgb(rgb, EMISSIVE_SATURATION), Math.max(alpha, 160), out);
            return;
        }
        writeLitLayer(world, pos, rgb, alpha, out);
    }

    private static boolean isFireLike(BlockState state)
    {
        return state.is(Blocks.FIRE) || state.is(Blocks.SOUL_FIRE)
                || state.is(Blocks.CAMPFIRE) || state.is(Blocks.SOUL_CAMPFIRE);
    }

    private static int boostRgb(int rgb, float sat)
    {
        int r = (rgb >> 16) & 0xFF;
        int g = (rgb >> 8) & 0xFF;
        int b = rgb & 0xFF;
        float gray = (r + g + b) / 3.0f;
        r = ColorMath.clamp255(Math.round(gray + (r - gray) * sat));
        g = ColorMath.clamp255(Math.round(gray + (g - gray) * sat));
        b = ColorMath.clamp255(Math.round(gray + (b - gray) * sat));
        return (r << 16) | (g << 8) | b;
    }

    private static void writeEmissiveLayer(int rgb, int alpha, int[] out)
    {
        boostSaturation((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, EMISSIVE_SATURATION, alpha, out);
    }

    private static int resolveDisplayRgb(Level world, BlockPos pos, BlockState state, Direction face, Vec3 hit,
                                         int[] texelScratch)
    {
        if(state.getBlock() instanceof BeaconBeamBlock beam)
        {
            texelScratch[3] = 255;
            return beam.getColor().getTextureDiffuseColor() & 0xFFFFFF;
        }
        if(state.is(Blocks.LIGHT))
        {
            texelScratch[3] = 80;
            return 0xFFFFFF;
        }

        int rgb = sampleTexturedBlockRgb(world, pos, state, face, hit, texelScratch);
        if(state.is(BlockTags.LEAVES) && isSnowyLeaves(world, pos, state))
        {
            rgb = ColorMath.blendRgb(rgb, SNOW_BLEND_RGB, SNOWY_LEAVES_BLEND);
        }
        return rgb;
    }

    private static int sampleTexturedBlockRgb(Level world, BlockPos pos, BlockState state, Direction face,
                                              Vec3 hit, int[] texelScratch)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client != null && client.level == world && hit != null)
        {
            try
            {
                if(BlockUvTexelSampler.sampleHitRgbA(client, pos, state, face, hit, texelScratch))
                {
                    final int baseRgb = (texelScratch[0] << 16) | (texelScratch[1] << 8) | texelScratch[2];
                    return applyBlockTint(world, pos, state, baseRgb);
                }
            }
            catch(Throwable t)
            {
                QuietCatch.debug(LOGGER, "block display sample failed", t);
            }
        }

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
                        texelScratch[3] = 255;
                        return applyBlockTint(world, pos, state, baseRgb);
                    }
                }
            }
            catch(Throwable t)
            {
                QuietCatch.debug(LOGGER, "block display sample failed", t);
            }
        }

        final int tintOnly = sampleBlockTintRgb(world, pos, state);
        if(tintOnly != 0 && tintOnly != 0xFFFFFF)
        {
            texelScratch[3] = 255;
            return tintOnly & 0xFFFFFF;
        }
        texelScratch[3] = 255;
        return mapColorRgb(world, pos, state, 0x808080);
    }

    private static int applyBlockTint(Level world, BlockPos pos, BlockState state, int baseRgb)
    {
        final int tint = sampleBlockTintRgb(world, pos, state);
        int r = mulChannel((baseRgb >> 16) & 0xFF, (tint >> 16) & 0xFF);
        int g = mulChannel((baseRgb >> 8) & 0xFF, (tint >> 8) & 0xFF);
        int b = mulChannel(baseRgb & 0xFF, tint & 0xFF);
        int rgb = (r << 16) | (g << 8) | b;
        // Leaf/grass textures are near-white grayscale; if tint failed, force biome foliage/grass colour.
        if(state.is(BlockTags.LEAVES) && isNearGrayOrWhite(rgb))
        {
            final int foliage = AtmosphereSampler.sampleFoliageColor(world, pos, 0x48B518);
            rgb = ColorMath.blendRgb(rgb, foliage & 0xFFFFFF, 0.88f);
        }
        else if((state.is(Blocks.GRASS_BLOCK) || state.is(Blocks.SHORT_GRASS) || state.is(Blocks.TALL_GRASS)
                || state.is(Blocks.FERN) || state.is(Blocks.LARGE_FERN) || state.is(BlockTags.FLOWERS))
                && isNearGrayOrWhite(rgb))
        {
            final int grass = AtmosphereSampler.sampleGrassColor(world, pos, 0x7CBD6B);
            rgb = ColorMath.blendRgb(rgb, grass & 0xFFFFFF, 0.80f);
        }
        // Flowers and small plants need punchier chroma on LEDs.
        if(state.is(BlockTags.FLOWERS) || state.is(Blocks.SHORT_GRASS) || state.is(Blocks.TALL_GRASS)
                || state.is(Blocks.FERN) || state.is(Blocks.LARGE_FERN) || state.is(Blocks.SWEET_BERRY_BUSH)
                || state.is(Blocks.TORCHFLOWER) || state.is(Blocks.PITCHER_PLANT))
        {
            rgb = boostRgb(rgb, 1.55f);
        }
        return rgb;
    }

    private static boolean isNearGrayOrWhite(int rgb)
    {
        final int r = (rgb >> 16) & 0xFF;
        final int g = (rgb >> 8) & 0xFF;
        final int b = rgb & 0xFF;
        final int max = Math.max(r, Math.max(g, b));
        final int min = Math.min(r, Math.min(g, b));
        return max >= 140 && (max - min) <= 28;
    }

    private static int sampleBlockTintRgb(Level world, BlockPos pos, BlockState state)
    {
        final Minecraft client = Minecraft.getInstance();
        if(client == null || client.level != world || !(world instanceof ClientLevel clientLevel))
        {
            return biomeTintFallback(world, pos, state);
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
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block display sample failed", t);
        }
        return biomeTintFallback(world, pos, state);
    }

    private static int biomeTintFallback(Level world, BlockPos pos, BlockState state)
    {
        if(state.is(BlockTags.LEAVES))
        {
            return AtmosphereSampler.sampleFoliageColor(world, pos, 0x48B518) & 0xFFFFFF;
        }
        if(state.is(Blocks.GRASS_BLOCK) || state.is(Blocks.SHORT_GRASS) || state.is(Blocks.TALL_GRASS)
                || state.is(Blocks.FERN) || state.is(Blocks.LARGE_FERN))
        {
            return AtmosphereSampler.sampleGrassColor(world, pos, 0x7CBD6B) & 0xFFFFFF;
        }
        return 0xFFFFFF;
    }

    private static boolean isSnowyLeaves(Level world, BlockPos pos, BlockState state)
    {
        if(!state.is(BlockTags.LEAVES))
        {
            return false;
        }
        // Only actual snow cover — biome "can snow" was bleaching temperate canopies white.
        final BlockState above = world.getBlockState(pos.above());
        return above.is(Blocks.SNOW) || above.is(Blocks.POWDER_SNOW) || above.is(Blocks.SNOW_BLOCK);
    }

    private static void writeLitLayer(Level world, BlockPos pos, int rgb, int alpha, int[] out)
    {
        int sky = world.getBrightness(LightLayer.SKY, pos);
        int block = world.getBrightness(LightLayer.BLOCK, pos);
        int light = Math.max(sky, block);
        float k = 0.55f + 0.45f * (light / 15.0f);
        int r = ColorMath.clamp255((int)(((rgb >> 16) & 0xFF) * k));
        int g = ColorMath.clamp255((int)(((rgb >> 8) & 0xFF) * k));
        int b = ColorMath.clamp255((int)((rgb & 0xFF) * k));
        boostSaturation(r, g, b, BLOCK_COLOR_SATURATION, alpha, out);
    }

    private static void boostSaturation(int r, int g, int b, float saturation, int alpha, int[] out)
    {
        float gray = (r + g + b) / 3.0f;
        out[0] = ColorMath.clamp255(Math.round(gray + (r - gray) * saturation));
        out[1] = ColorMath.clamp255(Math.round(gray + (g - gray) * saturation));
        out[2] = ColorMath.clamp255(Math.round(gray + (b - gray) * saturation));
        out[3] = ColorMath.clamp255(alpha);
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
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block display sample failed", t);
        }
        return fallback;
    }

    private static int mulChannel(int a, int b)
    {
        return (a * b) / 255;
    }
}

