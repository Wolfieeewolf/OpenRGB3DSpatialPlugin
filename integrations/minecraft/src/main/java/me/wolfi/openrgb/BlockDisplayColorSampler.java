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
    private static final float BLOCK_COLOR_SATURATION = 1.30f;
    private static final float EMISSIVE_SATURATION = 1.40f;
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

    /** Flower / crop / bush — keep as LED detail, not soft immersion wash. */
    static boolean isFlowerOrCropDetail(BlockState state)
    {
        if(state == null)
        {
            return false;
        }
        return state.is(BlockTags.FLOWERS) || state.is(BlockTags.CROPS)
                || state.is(Blocks.SUNFLOWER) || state.is(Blocks.LILAC) || state.is(Blocks.ROSE_BUSH)
                || state.is(Blocks.PEONY) || state.is(Blocks.PITCHER_PLANT) || state.is(Blocks.TORCHFLOWER)
                || state.is(Blocks.SWEET_BERRY_BUSH);
    }

    /** Fire / campfire — cutout emissive overlay. */
    static boolean isFireLike(BlockState state)
    {
        return state != null && (state.is(Blocks.FIRE) || state.is(Blocks.SOUL_FIRE)
                || state.is(Blocks.CAMPFIRE) || state.is(Blocks.SOUL_CAMPFIRE));
    }

    /**
     * Torches, lanterns, candles, end rods — small bright sources that must read as warm/cool
     * light on LEDs, not as a dim stick washed into the wall behind them.
     */
    static boolean isPointLightEmissive(BlockState state)
    {
        if(state == null)
        {
            return false;
        }
        if(state.is(Blocks.TORCH) || state.is(Blocks.WALL_TORCH)
                || state.is(Blocks.SOUL_TORCH) || state.is(Blocks.SOUL_WALL_TORCH)
                || state.is(Blocks.REDSTONE_TORCH) || state.is(Blocks.REDSTONE_WALL_TORCH)
                || state.is(Blocks.LANTERN) || state.is(Blocks.SOUL_LANTERN)
                || state.is(Blocks.END_ROD))
        {
            return true;
        }
        if(state.is(BlockTags.CANDLES) || state.is(BlockTags.CANDLE_CAKES))
        {
            return isLitCandle(state);
        }
        return false;
    }

    private static boolean isLitCandle(BlockState state)
    {
        for(var prop : state.getProperties())
        {
            if(prop.getName().equals("lit") && state.getValue(prop) instanceof Boolean lit)
            {
                return lit;
            }
        }
        // Untyped / missing lit — treat as lit so candles still show.
        return true;
    }

    /** Fallback flame colour when UV hits a transparent hole or stick-only average. */
    static int pointLightFallbackRgb(BlockState state)
    {
        if(state.is(Blocks.SOUL_TORCH) || state.is(Blocks.SOUL_WALL_TORCH)
                || state.is(Blocks.SOUL_LANTERN) || state.is(Blocks.SOUL_FIRE)
                || state.is(Blocks.SOUL_CAMPFIRE))
        {
            return 0x5AD4FF;
        }
        if(state.is(Blocks.REDSTONE_TORCH) || state.is(Blocks.REDSTONE_WALL_TORCH))
        {
            return 0xFF3A28;
        }
        if(state.is(Blocks.END_ROD))
        {
            return 0xF0E8FF;
        }
        return 0xFFB24A;
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
        // Torches / lanterns: stop on a solid flame texel so the wall behind cannot bury them.
        // Transparent sprite holes still pass through.
        if(isPointLightEmissive(state))
        {
            return coverAlpha < 110;
        }
        // Flowers / crops: stop when we actually hit a petal/veg texel so grass behind cannot wash
        // white/pink into light green. Empty sprite holes still pass through.
        if(isFlowerOrCropDetail(state))
        {
            return coverAlpha < 150;
        }
        // Tall grass / sugarcane etc. must not kill open-sky rays for nearby LEDs.
        if(isSparseSkyCutout(state))
        {
            return true;
        }
        if(isSoftWeatherVolume(state))
        {
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

    /**
     * Head-height / weather cutouts that must not trap every LED ray when the player
     * stands inside them (tall grass fields, snow layers, petal carpets, vines, etc.).
     */
    static boolean isSparseSkyCutout(BlockState state)
    {
        if(state == null)
        {
            return false;
        }
        if(state.is(BlockTags.FLOWERS) || state.is(BlockTags.CROPS))
        {
            return true;
        }
        // Thin snow / petal carpets — rain/fog are atmosphere; these are the block hazards.
        if(state.is(Blocks.SNOW) || state.is(Blocks.PINK_PETALS) || state.is(Blocks.MOSS_CARPET)
                || state.is(Blocks.PALE_MOSS_CARPET))
        {
            return true;
        }
        return state.is(Blocks.SHORT_GRASS) || state.is(Blocks.TALL_GRASS)
                || state.is(Blocks.FERN) || state.is(Blocks.LARGE_FERN)
                || state.is(Blocks.SUGAR_CANE) || state.is(Blocks.BAMBOO) || state.is(Blocks.BAMBOO_SAPLING)
                || state.is(Blocks.SUNFLOWER) || state.is(Blocks.LILAC) || state.is(Blocks.ROSE_BUSH)
                || state.is(Blocks.PEONY) || state.is(Blocks.PITCHER_PLANT) || state.is(Blocks.TORCHFLOWER)
                || state.is(Blocks.SWEET_BERRY_BUSH) || state.is(Blocks.CAVE_VINES)
                || state.is(Blocks.CAVE_VINES_PLANT) || state.is(Blocks.VINE) || state.is(Blocks.GLOW_LICHEN)
                || state.is(Blocks.DEAD_BUSH) || state.is(Blocks.SEAGRASS) || state.is(Blocks.TALL_SEAGRASS)
                || state.is(Blocks.KELP) || state.is(Blocks.KELP_PLANT) || state.is(Blocks.COBWEB)
                || state.is(Blocks.HANGING_ROOTS) || state.is(Blocks.SPORE_BLOSSOM)
                || state.is(Blocks.NETHER_SPROUTS) || state.is(Blocks.WARPED_ROOTS)
                || state.is(Blocks.CRIMSON_ROOTS) || state.is(Blocks.WEEPING_VINES)
                || state.is(Blocks.WEEPING_VINES_PLANT) || state.is(Blocks.TWISTING_VINES)
                || state.is(Blocks.TWISTING_VINES_PLANT)
                || state.is(Blocks.OAK_SAPLING) || state.is(Blocks.BIRCH_SAPLING)
                || state.is(Blocks.SPRUCE_SAPLING) || state.is(Blocks.JUNGLE_SAPLING)
                || state.is(Blocks.ACACIA_SAPLING) || state.is(Blocks.DARK_OAK_SAPLING)
                || state.is(Blocks.CHERRY_SAPLING) || state.is(Blocks.PALE_OAK_SAPLING)
                || state.is(Blocks.MANGROVE_PROPAGULE);
    }

    /** Powder snow is denser than a carpet — soft continue, not a hard wall for Room Ambilight. */
    static boolean isSoftWeatherVolume(BlockState state)
    {
        return state != null && state.is(Blocks.POWDER_SNOW);
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
        if(isPointLightEmissive(state))
        {
            // Keep torch/lantern flame opaque enough to own the LED colour.
            if(textureAlpha >= 0)
            {
                return textureAlpha < 36
                        ? ColorMath.clamp255(textureAlpha)
                        : ColorMath.clamp255(Math.max(210, textureAlpha));
            }
            return 230;
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
        // Flowers / crops / bushes: keep distinctive petal/veg texels opaque enough to win over grass.
        if(isFlowerOrCropDetail(state))
        {
            if(textureAlpha >= 0)
            {
                // Holes stay soft; solid petal/veg texels become a hard LED colour.
                return textureAlpha < 40 ? ColorMath.clamp255(textureAlpha) : ColorMath.clamp255(Math.max(200, textureAlpha));
            }
            return 210;
        }
        if(isSparseSkyCutout(state))
        {
            // Soft plant veil — enough to tint LEDs, not enough to bury sky behind them.
            final int tex = textureAlpha >= 0 ? textureAlpha : 90;
            return ColorMath.clamp255(Math.min(72, Math.max(28, tex / 3)));
        }
        if(isSoftWeatherVolume(state))
        {
            return 70;
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
        if(isPointLightEmissive(state))
        {
            // Stick / hole texels → warm flame fallback so torches never vanish into the wall.
            if(texel[3] < 50 || isNearGrayOrWhite(rgb) || isDimStickRgb(rgb))
            {
                rgb = pointLightFallbackRgb(state);
                texel[3] = 220;
            }
            writeEmissiveLayer(boostRgb(rgb, EMISSIVE_SATURATION), Math.max(alpha, 200), out);
            return;
        }
        writeLitLayer(world, pos, rgb, alpha, out);
    }

    private static boolean isDimStickRgb(int rgb)
    {
        final int r = (rgb >> 16) & 0xFF;
        final int g = (rgb >> 8) & 0xFF;
        final int b = rgb & 0xFF;
        final int luma = (r * 3 + g * 6 + b) / 10;
        // Dark brown torch stick — not the flame tip.
        return luma < 90 && r < 140;
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
        // Multiply only by real client tint sources (or white). Never force green onto
        // already-coloured leaves/flowers — that turns cherry pink into yellow/brown.
        final int tint = sampleBlockTintRgb(world, pos, state);
        int r = mulChannel((baseRgb >> 16) & 0xFF, (tint >> 16) & 0xFF);
        int g = mulChannel((baseRgb >> 8) & 0xFF, (tint >> 8) & 0xFF);
        int b = mulChannel(baseRgb & 0xFF, tint & 0xFF);
        int rgb = (r << 16) | (g << 8) | b;
        // Grayscale plant bases still need biome colour when tint sources were missing.
        if(isNearGrayOrWhite(rgb))
        {
            if(state.is(BlockTags.LEAVES) && usesBiomeFoliageTint(state))
            {
                final int foliage = AtmosphereSampler.sampleFoliageColor(world, pos, 0x48B518);
                rgb = ColorMath.blendRgb(rgb, foliage & 0xFFFFFF, 0.88f);
            }
            else if(state.is(Blocks.GRASS_BLOCK) || state.is(Blocks.SHORT_GRASS) || state.is(Blocks.TALL_GRASS)
                    || state.is(Blocks.FERN) || state.is(Blocks.LARGE_FERN))
            {
                final int grass = AtmosphereSampler.sampleGrassColor(world, pos, 0x7CBD6B);
                rgb = ColorMath.blendRgb(rgb, grass & 0xFFFFFF, 0.80f);
            }
        }
        return rgb;
    }

    /** Classic green canopies — not cherry / azalea / other pre-coloured leaf blocks. */
    private static boolean usesBiomeFoliageTint(BlockState state)
    {
        if(!state.is(BlockTags.LEAVES))
        {
            return false;
        }
        return !(state.is(Blocks.CHERRY_LEAVES)
                || state.is(Blocks.AZALEA_LEAVES)
                || state.is(Blocks.FLOWERING_AZALEA_LEAVES)
                || state.is(Blocks.PALE_OAK_LEAVES));
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
        // White = "no multiply". Coloured leaf textures must not be multiplied by green.
        if(state.is(BlockTags.LEAVES) && usesBiomeFoliageTint(state))
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
        final int sky = world.getBrightness(LightLayer.SKY, pos);
        final int block = world.getBrightness(LightLayer.BLOCK, pos);
        final float skyBright = AtmosphereSampler.skyBrightness(world);
        // Restore the pre-day/night light-level curve (looked good on grass), then apply a
        // gentler day↔night factor only to sky-dominated samples. Hard 0.14 floors + sat
        // stacking was crushing greens toward cyan/blue on LEDs.
        final float local = Math.max(sky, block) / 15.0f;
        float k = 0.72f + 0.28f * local;
        if(sky >= block)
        {
            // Night outdoor: dim, but keep enough headroom that chroma survives.
            k *= (0.45f + 0.55f * skyBright);
        }
        int r = ColorMath.clamp255((int)(((rgb >> 16) & 0xFF) * k));
        int g = ColorMath.clamp255((int)(((rgb >> 8) & 0xFF) * k));
        int b = ColorMath.clamp255((int)((rgb & 0xFF) * k));
        // Same sat as before day/night work — do not scale sat with skyBright.
        boostSaturation(r, g, b, BLOCK_COLOR_SATURATION, alpha, out);
        // Soft warm bias when outdoor night would otherwise read as cold cyan-green.
        if(sky >= block && skyBright < 0.85f && out[1] > out[0] && out[2] > out[0])
        {
            final float cool = (1.0f - skyBright) * 0.22f;
            out[2] = ColorMath.clamp255(Math.round(out[2] * (1.0f - cool)));
            out[0] = ColorMath.clamp255(Math.round(out[0] + (out[1] - out[0]) * cool * 0.35f));
        }
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

