package me.wolfi.openrgb;

import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.block.BlockStateModelSet;
import net.minecraft.client.renderer.block.dispatch.BlockStateModel;
import net.minecraft.client.renderer.block.dispatch.BlockStateModelPart;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;

import org.joml.Vector3fc;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.List;

/**
 * Point-samples the block texture texel at a ray hit via baked-quad UV interpolation.
 */
final class BlockUvTexelSampler
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final float CUTOUT_ALPHA_CONTINUE = 0.78f;
    private static final float QUAD_PLANE_EPS = 0.12f;

    private BlockUvTexelSampler()
    {
    }

    /**
     * Writes RGB + texture alpha into {@code out} (length 4).
     * Returns true when a texel was sampled.
     */
    static boolean sampleHitRgbA(Minecraft client, BlockPos pos, BlockState state, Direction face, Vec3 hit,
                                 int[] out)
    {
        if(client == null || state == null || hit == null || out == null || out.length < 4)
        {
            return false;
        }
        try
        {
            final BlockStateModelSet modelSet = client.getModelManager().getBlockStateModelSet();
            final BlockStateModel model = modelSet.get(state);
            if(model == null)
            {
                return false;
            }

            final double lx = hit.x - pos.getX();
            final double ly = hit.y - pos.getY();
            final double lz = hit.z - pos.getZ();

            final List<BlockStateModelPart> parts = new ArrayList<>();
            model.collectParts(RandomSource.create(Block.getId(state)), parts);

            BakedQuad best = null;
            float bestDist = Float.MAX_VALUE;
            float bestU = 0.5f;
            float bestV = 0.5f;

            for(BlockStateModelPart part : parts)
            {
                if(face != null)
                {
                    final Object[] facePick = pickBestQuad(part.getQuads(face), lx, ly, lz);
                    if(facePick != null)
                    {
                        final float dist = (Float)facePick[1];
                        if(dist < bestDist)
                        {
                            best = (BakedQuad)facePick[0];
                            bestDist = dist;
                            bestU = (Float)facePick[2];
                            bestV = (Float)facePick[3];
                        }
                    }
                }
                final Object[] nullPick = pickBestQuad(part.getQuads(null), lx, ly, lz);
                if(nullPick != null)
                {
                    final float dist = (Float)nullPick[1];
                    if(dist < bestDist)
                    {
                        best = (BakedQuad)nullPick[0];
                        bestDist = dist;
                        bestU = (Float)nullPick[2];
                        bestV = (Float)nullPick[3];
                    }
                }
            }

            if(best == null || best.materialInfo() == null || best.materialInfo().sprite() == null)
            {
                return false;
            }

            final TextureAtlasSprite sprite = best.materialInfo().sprite();
            final float u0 = sprite.getU0();
            final float u1 = sprite.getU1();
            final float v0 = sprite.getV0();
            final float v1 = sprite.getV1();
            final float du = u1 - u0;
            final float dv = v1 - v0;
            float uNorm = du != 0.0f ? (bestU - u0) / du : 0.5f;
            float vNorm = dv != 0.0f ? (bestV - v0) / dv : 0.5f;
            uNorm = clamp01(uNorm);
            vNorm = clamp01(vNorm);

            // Hot path: never decode PNGs here (that hitch causes Room VR stutters).
            // Precache fills PIXEL_ENTRIES asynchronously; face-average fallback covers misses.
            final SpritePixels pixels = BlockTexturePrecache.getPixels(sprite.contents().name());
            if(pixels == null)
            {
                return false;
            }
            // Neighborhood pick improves flowers/fire/cutouts without a large search.
            final int argb = pixels.sampleBestOfNeighborhood(uNorm, vNorm, 28);
            out[0] = (argb >> 16) & 0xFF;
            out[1] = (argb >> 8) & 0xFF;
            out[2] = argb & 0xFF;
            out[3] = (argb >>> 24) & 0xFF;
            return true;
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "block UV texel sample failed", t);
            return false;
        }
    }

    static boolean shouldContinueThroughTexel(int textureAlpha)
    {
        return textureAlpha < (int)(CUTOUT_ALPHA_CONTINUE * 255.0f);
    }

    private static Object[] pickBestQuad(List<BakedQuad> quads, double lx, double ly, double lz)
    {
        if(quads == null || quads.isEmpty())
        {
            return null;
        }
        BakedQuad best = null;
        float bestDist = Float.MAX_VALUE;
        float bestU = 0.5f;
        float bestV = 0.5f;
        final float[] uv = new float[2];
        for(BakedQuad quad : quads)
        {
            if(quad == null || quad.materialInfo() == null || quad.materialInfo().sprite() == null)
            {
                continue;
            }
            final float planeDist = planeDistance(quad, lx, ly, lz);
            if(planeDist > QUAD_PLANE_EPS)
            {
                continue;
            }
            if(!interpolateUv(quad, lx, ly, lz, uv))
            {
                continue;
            }
            if(planeDist < bestDist)
            {
                bestDist = planeDist;
                best = quad;
                bestU = uv[0];
                bestV = uv[1];
            }
        }
        if(best == null)
        {
            return null;
        }
        return new Object[] {best, bestDist, bestU, bestV};
    }

    private static float planeDistance(BakedQuad quad, double lx, double ly, double lz)
    {
        final Vector3fc p0 = quad.position(0);
        final Vector3fc p1 = quad.position(1);
        final Vector3fc p2 = quad.position(2);
        final double ax = p1.x() - p0.x();
        final double ay = p1.y() - p0.y();
        final double az = p1.z() - p0.z();
        final double bx = p2.x() - p0.x();
        final double by = p2.y() - p0.y();
        final double bz = p2.z() - p0.z();
        final double nx = ay * bz - az * by;
        final double ny = az * bx - ax * bz;
        final double nz = ax * by - ay * bx;
        final double invLen = 1.0 / Math.max(1.0e-8, Math.sqrt(nx * nx + ny * ny + nz * nz));
        final double dx = lx - p0.x();
        final double dy = ly - p0.y();
        final double dz = lz - p0.z();
        return (float)Math.abs((nx * dx + ny * dy + nz * dz) * invLen);
    }

    private static boolean interpolateUv(BakedQuad quad, double lx, double ly, double lz, float[] outUv)
    {
        if(barycentricUv(quad, 0, 1, 2, lx, ly, lz, outUv))
        {
            return true;
        }
        return barycentricUv(quad, 0, 2, 3, lx, ly, lz, outUv);
    }

    private static boolean barycentricUv(BakedQuad quad, int i0, int i1, int i2, double lx, double ly, double lz,
                                         float[] outUv)
    {
        final Vector3fc a = quad.position(i0);
        final Vector3fc b = quad.position(i1);
        final Vector3fc c = quad.position(i2);
        final double v0x = b.x() - a.x();
        final double v0y = b.y() - a.y();
        final double v0z = b.z() - a.z();
        final double v1x = c.x() - a.x();
        final double v1y = c.y() - a.y();
        final double v1z = c.z() - a.z();
        final double v2x = lx - a.x();
        final double v2y = ly - a.y();
        final double v2z = lz - a.z();

        final double d00 = v0x * v0x + v0y * v0y + v0z * v0z;
        final double d01 = v0x * v1x + v0y * v1y + v0z * v1z;
        final double d11 = v1x * v1x + v1y * v1y + v1z * v1z;
        final double d20 = v2x * v0x + v2y * v0y + v2z * v0z;
        final double d21 = v2x * v1x + v2y * v1y + v2z * v1z;
        final double denom = d00 * d11 - d01 * d01;
        if(Math.abs(denom) < 1.0e-10)
        {
            return false;
        }
        final double inv = 1.0 / denom;
        final double w1 = (d11 * d20 - d01 * d21) * inv;
        final double w2 = (d00 * d21 - d01 * d20) * inv;
        final double w0 = 1.0 - w1 - w2;
        if(w0 < -0.08 || w1 < -0.08 || w2 < -0.08)
        {
            return false;
        }

        final long p0 = quad.packedUV(i0);
        final long p1 = quad.packedUV(i1);
        final long p2 = quad.packedUV(i2);
        outUv[0] = (float)(w0 * PackedUv.unpackU(p0) + w1 * PackedUv.unpackU(p1) + w2 * PackedUv.unpackU(p2));
        outUv[1] = (float)(w0 * PackedUv.unpackV(p0) + w1 * PackedUv.unpackV(p1) + w2 * PackedUv.unpackV(p2));
        return true;
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
}
