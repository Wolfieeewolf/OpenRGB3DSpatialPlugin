// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Pulse ring: R=intensity, G=color_driver01.
 *  Sphere = 3D shell. Circle/Hex/Triangle/Square = FLAT extruded XZ rings
 *  (hard Y slab) so the pulse keeps the footprint — never length(xz,y).
 *  For Radial Rainbow, G stores continuous azimuth hue (no expand-progress).
 *  u_params: [0]=progress [1]=hole [2]=sigma [3]=amp [4]=detail
 *            [5]=style(0=pulse,1=radial) [6]=phase_offset [7]=shape
 *            [8]=size_scale [9]=hue_scroll01
 *            [10]=ox [11]=oy [12]=oz  (effect/ref origin in unit UV)
 */
inline const char* PulseRingVolumeFieldGlsl()
{
    return R"(
float hexMetric(vec2 p)
{
    vec2 a = abs(p);
    return max(dot(a, vec2(0.5, 0.8660254)), a.x);
}
float polyMetric(vec2 p, float n)
{
    float an = 6.2831853 / n;
    float a = atan(p.y, p.x);
    float r = length(p);
    return cos(floor(0.5 + a / an) * an - a) * r;
}
float footprintXZ(vec2 p, int shape)
{
    if(shape == 2)
        return hexMetric(p) / 0.8660254;
    if(shape == 3)
        return max(polyMetric(p, 3.0) / 0.5, 0.0);
    if(shape == 4)
        return max(abs(p.x), abs(p.y));
    return length(p);
}
float sharpBand(float dist_to_edge, float half_w)
{
    float t = 1.0 - smoothstep(0.0, max(half_w, 0.008), abs(dist_to_edge));
    return t * t;
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress = u_params[0];
    float hole_r = clamp(u_params[1], 0.0, 0.7);
    float sigma = max(u_params[2], 0.01);
    float amp = clamp(u_params[3], 0.2, 2.0);
    float detail = clamp(u_params[4], 0.05, 1.0);
    int style = int(u_params[5] + 0.5);
    float phase_offset = u_params[6];
    int shape = int(clamp(u_params[7], 0.0, 4.0) + 0.5);
    float size_scale = clamp(u_params[8], 0.25, 2.5);
    float hue_scroll = fract(u_params[9]);
    vec3 origin01 = clamp(vec3(u_params[10], u_params[11], u_params[12]), 0.0, 1.0);

    vec3 p = (p01 - origin01) * 2.0;
    float inv_size = 1.0 / max(size_scale, 0.25);
    float usable = max(0.12, 1.0 - hole_r);

    float half_w = max(0.010, sigma * mix(0.55, 0.18, detail));
    float y_half = mix(0.28, 0.07, detail);

    float d = 0.0;
    float height_mul = 1.0;
    if(shape == 1)
    {
        d = length(p) * inv_size;
        height_mul = 1.0;
    }
    else
    {
        d = footprintXZ(p.xz, shape) * inv_size;
        height_mul = 1.0 - smoothstep(y_half, y_half + 0.035, abs(p.y));
    }

    float intensity = 0.0;
    float color_drv = 0.0;
    float az01 = fract(atan(p.z, p.x) / 6.2831853 + 0.5 + hue_scroll);

    if(style == 1)
    {
        float fill = smoothstep(hole_r - 0.02, hole_r + 0.01, d);
        float outer = 1.0 - smoothstep(1.02, 1.20, d);
        intensity = fill * outer * height_mul;
        color_drv = az01;
    }
    else
    {
        float expand = fract(progress + phase_offset);
        float center = hole_r + expand * usable;
        intensity = sharpBand(d - center, half_w) * height_mul;
        intensity *= smoothstep(hole_r - 0.03, hole_r + 0.01, d);
        intensity *= amp;
        color_drv = fract(clamp((d - hole_r) / usable, 0.0, 1.0) * 0.85 + hue_scroll);
    }

    intensity = clamp(intensity, 0.0, 1.0);
    out_color = vec4(intensity, color_drv, 0.0, 1.0);
}
)";
}
