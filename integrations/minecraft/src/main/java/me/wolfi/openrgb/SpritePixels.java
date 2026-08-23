package me.wolfi.openrgb;

/**
 * Downscaled sprite RGBA for UV point sampling. May hold a vertical animation strip
 * (fire, portal, …) as stacked frames; {@link #sampleNorm} picks the current frame by time.
 */
record SpritePixels(int width, int height, int[] argb, int avgRgb, int avgAlpha, int frameCount)
{
    SpritePixels
    {
        if(frameCount < 1)
        {
            frameCount = 1;
        }
    }

    static SpritePixels ofSingle(int width, int height, int[] argb, int avgRgb, int avgAlpha)
    {
        return new SpritePixels(width, height, argb, avgRgb, avgAlpha, 1);
    }

    BlockDisplayColorSampler.TextureSample average()
    {
        return new BlockDisplayColorSampler.TextureSample(avgRgb, avgAlpha);
    }

    private int frameBase()
    {
        if(frameCount <= 1 || width <= 0 || height <= 0 || argb == null)
        {
            return 0;
        }
        final int frame = (int)((System.currentTimeMillis() / 50L) % (long)frameCount);
        final int stride = width * height;
        final int base = frame * stride;
        if(base + stride > argb.length)
        {
            return 0;
        }
        return base;
    }

    /** Sample nearest texel; {@code u}/{@code v} in 0..1 sprite space. Returns ARGB. */
    int sampleNorm(float u, float v)
    {
        if(width <= 0 || height <= 0 || argb == null || argb.length == 0)
        {
            return (avgAlpha << 24) | (avgRgb & 0xFFFFFF);
        }
        float uu = u - (float)Math.floor(u);
        float vv = v - (float)Math.floor(v);
        if(uu < 0.0f)
        {
            uu += 1.0f;
        }
        if(vv < 0.0f)
        {
            vv += 1.0f;
        }
        int x = Math.min(width - 1, Math.max(0, (int)(uu * width)));
        int y = Math.min(height - 1, Math.max(0, (int)(vv * height)));
        return argb[frameBase() + y * width + x];
    }

    /**
     * Bilinear sample in sprite space — smoother grain on solid blocks (stone, wood, ore)
     * without the muddy face-average path.
     */
    int sampleBilinearNorm(float u, float v)
    {
        if(width <= 1 || height <= 1 || argb == null || argb.length == 0)
        {
            return sampleNorm(u, v);
        }
        float uu = u - (float)Math.floor(u);
        float vv = v - (float)Math.floor(v);
        if(uu < 0.0f)
        {
            uu += 1.0f;
        }
        if(vv < 0.0f)
        {
            vv += 1.0f;
        }
        final float fx = uu * (width - 1);
        final float fy = vv * (height - 1);
        final int x0 = Math.min(width - 1, Math.max(0, (int)Math.floor(fx)));
        final int y0 = Math.min(height - 1, Math.max(0, (int)Math.floor(fy)));
        final int x1 = Math.min(width - 1, x0 + 1);
        final int y1 = Math.min(height - 1, y0 + 1);
        final float tx = fx - x0;
        final float ty = fy - y0;
        final int base = frameBase();
        final int c00 = argb[base + y0 * width + x0];
        final int c10 = argb[base + y0 * width + x1];
        final int c01 = argb[base + y1 * width + x0];
        final int c11 = argb[base + y1 * width + x1];
        return packBilinear(c00, c10, c01, c11, tx, ty);
    }

    private static int packBilinear(int c00, int c10, int c01, int c11, float tx, float ty)
    {
        final float w00 = (1.0f - tx) * (1.0f - ty);
        final float w10 = tx * (1.0f - ty);
        final float w01 = (1.0f - tx) * ty;
        final float w11 = tx * ty;
        int r = 0;
        int g = 0;
        int b = 0;
        float aw = 0.0f;
        final int[] cs = {c00, c10, c01, c11};
        final float[] ws = {w00, w10, w01, w11};
        for(int i = 0; i < 4; i++)
        {
            final int c = cs[i];
            final float w = ws[i];
            final int ca = (c >>> 24) & 0xFF;
            if(ca < 8)
            {
                continue;
            }
            final float wa = w * ca;
            aw += wa;
            r += Math.round(((c >> 16) & 0xFF) * wa);
            g += Math.round(((c >> 8) & 0xFF) * wa);
            b += Math.round((c & 0xFF) * wa);
        }
        if(aw < 1.0f)
        {
            return c00;
        }
        final float inv = 1.0f / aw;
        final int outA = Math.min(255, Math.max(1, Math.round(aw)));
        return (outA << 24)
                | ((Math.min(255, Math.round(r * inv)) & 0xFF) << 16)
                | ((Math.min(255, Math.round(g * inv)) & 0xFF) << 8)
                | (Math.min(255, Math.round(b * inv)) & 0xFF);
    }

    /**
     * Nearest opaque texel around {@code u}/{@code v}, else opaque average.
     * Search radius is capped so Room Ambilight sampling stays cheap on large pack textures.
     */
    int sampleOpaqueNear(float u, float v, int minAlpha)
    {
        if(width <= 0 || height <= 0 || argb == null || argb.length == 0)
        {
            return (avgAlpha << 24) | (avgRgb & 0xFFFFFF);
        }
        float uu = clamp01(u - (float)Math.floor(u));
        float vv = clamp01(v - (float)Math.floor(v));
        int cx = Math.min(width - 1, Math.max(0, (int)(uu * width)));
        int cy = Math.min(height - 1, Math.max(0, (int)(vv * height)));
        final int base = frameBase();
        final int direct = argb[base + cy * width + cx];
        if(((direct >>> 24) & 0xFF) >= minAlpha)
        {
            return direct;
        }
        return searchOpaqueNear(base, cx, cy, minAlpha, Math.min(3, Math.max(1, Math.min(width, height) / 8)));
    }

    /**
     * Wider opaque search for leaf canopies — hole texels should resolve to nearby leaf colour
     * instead of a washed average that tints poorly on LEDs.
     */
    int sampleOpaqueNearLeaves(float u, float v, int minAlpha)
    {
        if(width <= 0 || height <= 0 || argb == null || argb.length == 0)
        {
            return (avgAlpha << 24) | (avgRgb & 0xFFFFFF);
        }
        float uu = clamp01(u - (float)Math.floor(u));
        float vv = clamp01(v - (float)Math.floor(v));
        int cx = Math.min(width - 1, Math.max(0, (int)(uu * width)));
        int cy = Math.min(height - 1, Math.max(0, (int)(vv * height)));
        final int base = frameBase();
        final int direct = argb[base + cy * width + cx];
        if(((direct >>> 24) & 0xFF) >= minAlpha)
        {
            return direct;
        }
        final int radius = Math.min(6, Math.max(3, Math.min(width, height) / 4));
        return searchOpaqueNear(base, cx, cy, minAlpha, radius);
    }

    private int searchOpaqueNear(int base, int cx, int cy, int minAlpha, int radius)
    {
        int best = -1;
        int bestDist = Integer.MAX_VALUE;
        for(int dy = -radius; dy <= radius; dy++)
        {
            for(int dx = -radius; dx <= radius; dx++)
            {
                if(dx == 0 && dy == 0)
                {
                    continue;
                }
                final int x = cx + dx;
                final int y = cy + dy;
                if(x < 0 || y < 0 || x >= width || y >= height)
                {
                    continue;
                }
                final int px = argb[base + y * width + x];
                if(((px >>> 24) & 0xFF) < minAlpha)
                {
                    continue;
                }
                final int dist = dx * dx + dy * dy;
                if(dist < bestDist)
                {
                    bestDist = dist;
                    best = px;
                }
            }
        }
        if(best >= 0)
        {
            return best;
        }
        return (Math.max(minAlpha, avgAlpha) << 24) | (avgRgb & 0xFFFFFF);
    }

    /**
     * Pick the richest nearby opaque texel — better flowers/fire without a big search.
     * Prefers higher alpha, then higher chroma so flames stay multi-coloured.
     */
    int sampleBestOfNeighborhood(float u, float v, int minAlpha)
    {
        int best = sampleOpaqueNear(u, v, minAlpha);
        int bestScore = scoreTexel(best);
        final float[][] offsets = {
                {0.08f, 0.0f}, {-0.08f, 0.0f}, {0.0f, 0.08f}, {0.0f, -0.08f},
                {0.06f, 0.06f}, {-0.06f, -0.06f}, {0.12f, 0.04f}, {-0.04f, 0.12f},
                {0.04f, -0.12f}, {-0.12f, -0.04f},
                // Wider taps for small flower/crop sprites — still O(1), not every texel.
                {0.18f, 0.0f}, {-0.18f, 0.0f}, {0.0f, 0.18f}, {0.0f, -0.18f},
                {0.14f, 0.14f}, {-0.14f, 0.14f}, {0.14f, -0.14f}, {-0.14f, -0.14f}
        };
        for(float[] off : offsets)
        {
            final int px = sampleOpaqueNear(u + off[0], v + off[1], minAlpha);
            final int score = scoreTexel(px);
            if(score > bestScore)
            {
                bestScore = score;
                best = px;
            }
        }
        return best;
    }

    private static int scoreTexel(int argb)
    {
        final int a = (argb >>> 24) & 0xFF;
        final int r = (argb >> 16) & 0xFF;
        final int g = (argb >> 8) & 0xFF;
        final int b = argb & 0xFF;
        final int max = Math.max(r, Math.max(g, b));
        final int min = Math.min(r, Math.min(g, b));
        final int chroma = max - min;
        final int luma = (r * 3 + g * 6 + b) / 10;
        // Grass/stem green is the wash culprit — prefer petals/whites/pinks/yellows over it.
        final boolean greenDominant = g >= r + 18 && g >= b + 18 && g > 55;
        int score = a * 3 + chroma * 4;
        if(!greenDominant)
        {
            // White flower petals: low chroma, high luma — give them a strong boost.
            score += luma * 2 + chroma * 2;
            if(luma > 200 && chroma < 40)
            {
                score += 180;
            }
            // Warm torch/lantern flame tip over brown stick wood.
            if(r > g + 12 && r > b + 12 && luma > 70)
            {
                score += 120 + (r - g);
            }
            // Cool soul-flame blues.
            if(b > r + 20 && b > g + 10 && luma > 60)
            {
                score += 100 + (b - r);
            }
        }
        else
        {
            score += chroma;
        }
        return score;
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
