// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

/**
 * Shared display-finish GLSL matching ViewportShaders::applyFinish (overlay).
 * Used by SpatialVolumeFieldEngine to bake finished RGB into LED point samples.
 *
 * u_material_rich: 0 = Flat/Basic (legacy colour × intensity / classic HSV rainbow)
 *                  1 = Rich/Masterpiece (shade + highlight + spectral rainbow)
 * u_hue_shift / u_sat_mul: HSV tweak applied after material sample (Look/Custom).
 */
namespace VolumeDisplayFinish410
{
inline QString FragmentSource()
{
    return QStringLiteral(R"GLSL(
#version 410 core
uniform sampler2D u_raw;
uniform vec2 u_point_atlas;
uniform int u_point_count;
uniform int u_finish;
uniform float u_hue_scroll;
uniform int u_rainbow;
uniform int u_palette_count;
uniform vec3 u_palette[8];
uniform int u_material_rich;
uniform float u_hue_shift;
uniform float u_sat_mul;
out vec4 frag_color;

vec3 rainbowHue(float hue_deg)
{
    float hue = mod(hue_deg, 360.0);
    if(hue < 0.0) hue += 360.0;
    float c = 1.0;
    float x = 1.0 - abs(mod(hue / 60.0, 2.0) - 1.0);
    if(hue < 60.0) return vec3(c, x, 0.0);
    if(hue < 120.0) return vec3(x, c, 0.0);
    if(hue < 180.0) return vec3(0.0, c, x);
    if(hue < 240.0) return vec3(0.0, x, c);
    if(hue < 300.0) return vec3(x, 0.0, c);
    return vec3(c, 0.0, x);
}

vec3 hsv2rgb(float h, float s, float v)
{
    h = fract(h);
    s = clamp(s, 0.0, 1.0);
    v = clamp(v, 0.0, 1.0);
    float hf = h * 6.0;
    int i = int(mod(floor(hf), 6.0));
    float f = hf - floor(hf);
    float p = v * (1.0 - s);
    float q = v * (1.0 - f * s);
    float t = v * (1.0 - (1.0 - f) * s);
    if(i == 0) return vec3(v, t, p);
    if(i == 1) return vec3(q, v, p);
    if(i == 2) return vec3(p, v, t);
    if(i == 3) return vec3(p, q, v);
    if(i == 4) return vec3(t, p, v);
    return vec3(v, p, q);
}

vec3 rgb2hsv(vec3 c)
{
    float cmax = max(c.r, max(c.g, c.b));
    float cmin = min(c.r, min(c.g, c.b));
    float d = cmax - cmin;
    float h = 0.0;
    if(d > 1e-5)
    {
        if(cmax == c.r) h = mod((c.g - c.b) / d, 6.0);
        else if(cmax == c.g) h = (c.b - c.r) / d + 2.0;
        else h = (c.r - c.g) / d + 4.0;
        h /= 6.0;
        if(h < 0.0) h += 1.0;
    }
    float s = (cmax <= 1e-5) ? 0.0 : d / cmax;
    return vec3(h, s, cmax);
}

vec3 applyHueSat(vec3 rgb)
{
    vec3 hsv = rgb2hsv(rgb);
    hsv.x = fract(hsv.x + u_hue_shift / 360.0);
    hsv.y = clamp(hsv.y * max(u_sat_mul, 0.0), 0.0, 1.0);
    return hsv2rgb(hsv.x, hsv.y, hsv.z);
}

vec3 samplePalette(float t)
{
    int n = clamp(u_palette_count, 1, 8);
    float pos = clamp(t, 0.0, 1.0) * float(n - 1);
    int i0 = int(floor(pos));
    int i1 = min(i0 + 1, n - 1);
    float f = pos - float(i0);
    return mix(u_palette[i0], u_palette[i1], f);
}

/* Rich rainbow: still the vivid HSV spectrum, with light shade — never chalk/wash. */
vec3 spectralIridescent(float t01, float shade, float gloss)
{
    float t = fract(t01);
    vec3 base = rainbowHue(t * 360.0);
    vec3 lobe = rainbowHue(fract(t + 0.05) * 360.0);
    base = mix(base, lobe, 0.15);
    float lum = mix(0.42, 1.0, pow(clamp(shade, 0.0, 1.0), 0.70));
    vec3 col = base * lum;
    /* Same-hue tip lift — do not mix toward white (that killed the unicorn vomit). */
    col = mix(col, min(base * 1.22, vec3(1.0)), clamp(gloss, 0.0, 1.0) * 0.22 * clamp(shade, 0.0, 1.0));
    return clamp(col, 0.0, 1.0);
}

vec3 paletteOrRainbowFlat(float t01, float scroll_deg)
{
    if(u_rainbow != 0 || u_palette_count <= 0)
    {
        return rainbowHue(t01 * 360.0 + scroll_deg);
    }
    return samplePalette(fract(t01 + scroll_deg / 360.0));
}

vec3 paletteOrRainbowRich(float t01, float scroll_deg, float shade, float gloss)
{
    if(u_rainbow != 0 || u_palette_count <= 0)
    {
        return spectralIridescent(t01 + scroll_deg / 360.0, shade, gloss);
    }
    vec3 base = samplePalette(fract(t01 + scroll_deg / 360.0));
    return applyHueSat(base);
}

/* Lit material: keep the authored hue. Red stays red — only value/shade moves. */
vec3 litMaterial(vec3 base, float shade, float gloss)
{
    float sh = clamp(shade, 0.0, 1.0);
    float gl = clamp(gloss, 0.0, 1.0);
    /* Flat is colour×intensity; Rich adds depth without bleaching chroma. */
    float lum = mix(0.38, 1.0, pow(sh, 0.72));
    vec3 body = base * lum;
    vec3 tip = min(base * mix(1.0, 1.28, gl), vec3(1.0));
    float tip_w = gl * 0.28 * sh;
    return clamp(mix(body, tip, tip_w), 0.0, 1.0);
}

vec3 applyFinish(vec3 samp)
{
    int mode = u_finish;
    float scroll = u_hue_scroll;
    int rich = u_material_rich;
    if(mode == 0) return samp;
    if(mode == 1)
    {
        if(samp.y <= 1e-5) return vec3(0.0);
        return hsv2rgb(samp.z + scroll / 360.0, samp.x, samp.y);
    }
    if(mode == 2)
    {
        float inten = samp.x;
        if(inten < 0.01) return vec3(0.0);
        float gloss = clamp(samp.z, 0.0, 1.0);
        if(rich != 0)
        {
            vec3 base = (u_palette_count > 0)
                ? applyHueSat(samplePalette(samp.y))
                : spectralIridescent(samp.y + scroll / 360.0, inten, gloss);
            return litMaterial(base, inten, gloss);
        }
        vec3 c = (u_palette_count > 0) ? samplePalette(samp.y)
                                       : rainbowHue(samp.y * 360.0 + scroll);
        return c * inten;
    }
    if(mode == 3)
    {
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        if(rich != 0)
            return litMaterial(spectralIridescent(v + scroll / 360.0, v, samp.z), v, clamp(samp.z, 0.0, 1.0));
        return rainbowHue(v * 360.0 + scroll) * v;
    }
    if(mode == 4)
    {
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        if(rich != 0)
            return litMaterial(applyHueSat(samplePalette(v)), v, clamp(samp.z, 0.0, 1.0));
        return samplePalette(v) * v;
    }
    if(mode == 5)
    {
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        if(rich != 0)
            return litMaterial(spectralIridescent(samp.y + scroll / 360.0, v, samp.z), v, clamp(samp.z, 0.0, 1.0));
        return rainbowHue(samp.y * 360.0 + scroll) * v;
    }
    if(mode == 6)
    {
        float hue = atan(samp.y * 2.0 - 1.0, samp.x * 2.0 - 1.0) / 6.2831853;
        float v = samp.z > 0.0 ? clamp(samp.z, 0.0, 1.0) : 1.0;
        if(v < 0.01) return vec3(0.0);
        /* Atlas hue wheel: no forced gloss (that carved a blue/dark seam). */
        if(rich != 0)
            return litMaterial(paletteOrRainbowRich(fract(hue + 1.0), scroll, v, 0.0), v, 0.0);
        return paletteOrRainbowFlat(fract(hue + 1.0), scroll) * v;
    }
    if(mode == 7)
    {
        float inten = clamp(samp.x, 0.0, 1.0);
        if(inten < 0.04) return vec3(0.0);
        float lift = clamp(0.08 + 0.92 * pow(inten, 0.88), 0.0, 1.15);
        float gloss = clamp(samp.z, 0.0, 1.0);
        if(rich != 0)
            return litMaterial(paletteOrRainbowRich(clamp(samp.y, 0.0, 1.0), scroll, inten, gloss), lift, gloss);
        return paletteOrRainbowFlat(clamp(samp.y, 0.0, 1.0), scroll) * lift;
    }
    if(mode == 8)
    {
        float v = clamp(samp.x, 0.0, 1.0);
        if(v < 0.01) return vec3(0.0);
        if(rich != 0)
            return litMaterial(paletteOrRainbowRich(v, scroll, v, samp.z), pow(v, 0.85), clamp(samp.z, 0.0, 1.0));
        return paletteOrRainbowFlat(v, scroll) * pow(v, 0.85);
    }
    if(mode == 9)
    {
        /* finish_hex palette: heat in .g, gloss in .b — Flat keeps legacy multiply. */
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        float heat = clamp(samp.y, 0.0, 1.0);
        float gloss = clamp(samp.z, 0.0, 1.0);
        if(rich != 0)
        {
            vec3 base = paletteOrRainbowRich(heat, scroll, v, gloss);
            return litMaterial(base, v, gloss);
        }
        return paletteOrRainbowFlat(heat, scroll) * v;
    }
    return samp;
}

void main()
{
    int ix = int(floor(gl_FragCoord.x));
    int iy = int(floor(gl_FragCoord.y));
    int i = iy * int(u_point_atlas.x) + ix;
    if(i < 0 || i >= u_point_count)
    {
        frag_color = vec4(0.0);
        return;
    }
    vec3 samp = texelFetch(u_raw, ivec2(ix, iy), 0).rgb;
    frag_color = vec4(clamp(applyFinish(samp), 0.0, 1.0), 1.0);
}
)GLSL");
}
} // namespace VolumeDisplayFinish410
