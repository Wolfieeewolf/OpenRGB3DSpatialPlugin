package me.wolfi.openrgb;

/**
 * Full sprite RGBA buffer for UV point sampling (Screen-Mirror style detail source).
 */
record SpritePixels(int width, int height, int[] argb, int avgRgb, int avgAlpha)
{
    BlockDisplayColorSampler.TextureSample average()
    {
        return new BlockDisplayColorSampler.TextureSample(avgRgb, avgAlpha);
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
        return argb[y * width + x];
    }

    /**
     * Nearest opaque texel around {@code u}/{@code v}, else opaque average.
     * Search radius is capped so Room VR sampling stays cheap on large pack textures.
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
        final int direct = argb[cy * width + cx];
        if(((direct >>> 24) & 0xFF) >= minAlpha)
        {
            return direct;
        }
        int best = -1;
        int bestDist = Integer.MAX_VALUE;
        final int radius = Math.min(3, Math.max(1, Math.min(width, height) / 8));
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
                final int px = argb[y * width + x];
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

    /** Pick the most opaque of a few nearby samples — better flowers/fire without a big search. */
    int sampleBestOfNeighborhood(float u, float v, int minAlpha)
    {
        int best = sampleOpaqueNear(u, v, minAlpha);
        int bestA = (best >>> 24) & 0xFF;
        if(bestA >= 200)
        {
            return best;
        }
        final float[][] offsets = {
                {0.08f, 0.0f}, {-0.08f, 0.0f}, {0.0f, 0.08f}, {0.0f, -0.08f},
                {0.06f, 0.06f}, {-0.06f, -0.06f}
        };
        for(float[] off : offsets)
        {
            final int px = sampleOpaqueNear(u + off[0], v + off[1], minAlpha);
            final int a = (px >>> 24) & 0xFF;
            if(a > bestA)
            {
                bestA = a;
                best = px;
            }
        }
        return best;
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
