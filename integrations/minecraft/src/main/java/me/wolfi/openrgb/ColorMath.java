package me.wolfi.openrgb;

final class ColorMath
{
    private ColorMath()
    {
    }

    static int clamp255(int v)
    {
        return Math.max(0, Math.min(255, v));
    }

    static float clamp01(float v)
    {
        return Math.max(0.0f, Math.min(1.0f, v));
    }

    static int blendRgb(int a, int b, float t)
    {
        t = clamp01(t);
        int ar = (a >> 16) & 0xFF;
        int ag = (a >> 8) & 0xFF;
        int ab = a & 0xFF;
        int br = (b >> 16) & 0xFF;
        int bg = (b >> 8) & 0xFF;
        int bb = b & 0xFF;
        int r = clamp255(Math.round(ar + (br - ar) * t));
        int g = clamp255(Math.round(ag + (bg - ag) * t));
        int bl = clamp255(Math.round(ab + (bb - ab) * t));
        return (r << 16) | (g << 8) | bl;
    }

    static int lerpInt(int a, int b, float t)
    {
        t = clamp01(t);
        return clamp255(Math.round(a + (b - a) * t));
    }

    static float lerpFloat(float a, float b, float t)
    {
        t = clamp01(t);
        return a + (b - a) * t;
    }
}
