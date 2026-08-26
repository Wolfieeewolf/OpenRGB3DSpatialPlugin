// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/* R=intensity G=color_driver01
 * u_params: [0]=R_l [1]=breath_phase [2]=progress [3]=detail [4]=edge
 *           [5]=hole_frac [6]=shape [7]=ax [8]=az [9]=pulse
 *           [10]=sx [11]=sy [12]=sz
 */
inline const char* BreathingSphereVolumeFieldGlsl()
{
    return R"(
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
float polyRadialXZ(vec2 p, float n)
{
    /* Iso-value 1 on the regular n-gon with inradius 1. */
    float an = 6.2831853 / max(n, 3.0);
    float a = atan(p.y, p.x);
    float r = length(p);
    return cos(floor(0.5 + a / an) * an - a) * r / cos(3.14159265 / n);
}
float shapeMetric(vec3 l, int shape, float ax, float az)
{
    if(shape == 1)
        return max(max(abs(l.x), abs(l.y)), abs(l.z));
    if(shape == 2)
        return max(max(abs(l.x) / max(ax, 1e-4), abs(l.y)), abs(l.z) / max(az, 1e-4));
    if(shape == 3 || shape == 4)
    {
        float n = (shape == 3) ? 3.0 : 5.0;
        return max(polyRadialXZ(l.xz, n), abs(l.y));
    }
    return length(l);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float R = max(u_params[0], 0.02);
    float breath_phase = u_params[1];
    float progress = u_params[2];
    float detail = max(u_params[3], 0.05);
    int edge = int(clamp(u_params[4], 0.0, 1.0) + 0.5);
    float hole_frac = clamp(u_params[5], 0.0, 0.95);
    int shape = int(clamp(u_params[6], 0.0, 5.0) + 0.5);
    float ax = clamp(u_params[7], 0.15, 1.0);
    float az = clamp(u_params[8], 0.15, 1.0);
    float pulse_strength = clamp(u_params[9], 0.0, 1.0);
    vec3 s = max(vec3(u_params[10], u_params[11], u_params[12]), vec3(0.25));

    /* p01 is origin-local (0.5 = Spatial Anchor). Keep world-isotropic scale via s. */
    vec3 l = (p01 - vec3(0.5)) * s;

    if(shape == 5)
    {
        float dist_norm = clamp(length(l) * 0.5, 0.0, 1.5);
        float inhale = sin(breath_phase) * pulse_strength;
        float exhale = sin(breath_phase + 1.2) * pulse_strength;
        /* Spatial coeffs match CPU (l = rel/med); detail is pre-scaled by 1/tight_mul. */
        float wave = sin(inhale * 3.14159265 * 1.15 - dist_norm * (9.0 + 5.0 * detail)) * pulse_strength;
        float ripple = sin(breath_phase * 2.1 - dist_norm * 6.2831853 * 2.2 + l.y * 0.02 * detail) * pulse_strength;
        float rush = sin(exhale * 1.7 + (l.x + l.z) * 0.015 * detail) * 0.4 * pulse_strength;
        float air = 0.78 + 0.22 * (0.5 + 0.5 * sin(breath_phase * 1.05)) * (0.55 + 0.45 * pulse_strength);
        if(pulse_strength < 0.001)
            air = 0.85;
        float hue01 = fract(0.38 + 0.32 * inhale + 0.24 * wave + 0.12 * ripple + rush * 0.1 + progress * 0.04);
        out_color = vec4(clamp(air, 0.0, 1.0), hue01, 0.0, 1.0);
        return;
    }

    float distance = shapeMetric(l, shape, ax, az);
    float sphere_intensity = 0.0;
    float norm_in_shell = 0.0;

    /* Soft = wider falloff; Crisp = tight silhouette. */
    float band = (edge == 1) ? 0.018 : 0.16;
    band = max(band * (0.7 + 0.3 / max(detail, 0.2)), (edge == 1) ? 0.012 : 0.02);

    if(hole_frac <= 0.001)
    {
        /* Filled shape: hard inside, zero outside the shape metric (no sphere bloom). */
        if(edge == 1)
        {
            float inside = 1.0 - smstep(R - band * 0.12, R + band * 0.55, distance);
            float surface = 1.0 - smstep(0.0, band * 0.55, abs(distance - R));
            inside = max(inside, surface * 0.4 * step(distance, R + band));
            if(distance > R + band)
                inside = 0.0;
            sphere_intensity = clamp(inside, 0.0, 1.0);
        }
        else
        {
            float inside = 1.0 - smstep(R - band * 0.35, R + band, distance);
            float soft_out = 1.0 - smstep(R, R + band * 2.2, distance);
            inside = max(inside, soft_out * 0.35);
            sphere_intensity = clamp(inside, 0.0, 1.0);
        }
        norm_in_shell = clamp(distance / (R + 1e-5), 0.0, 1.2);
    }
    else
    {
        float r_in = hole_frac * R * 0.9;
        float iw = (edge == 1) ? band * 0.4 : band * 1.1;
        float ow = (edge == 1) ? band * 0.5 : band * 1.6;
        float inner_open = smstep(r_in - iw, r_in + iw, distance);
        float outer_open = 1.0 - smstep(R - ow * 0.2, R + ow, distance);
        float span_eff = max(R - r_in, R * 0.08);
        float u = clamp((distance - r_in) / span_eff, 0.0, 1.0);
        float bell = sin(u * 3.14159265);
        if(edge == 1)
        {
            float b2 = bell * bell;
            bell = b2 * b2;
            if(distance > R + ow)
                outer_open = 0.0;
        }
        sphere_intensity = clamp(inner_open * outer_open * (0.15 + 0.85 * bell), 0.0, 1.0);
        norm_in_shell = clamp((distance - r_in) / max(R - r_in, 1e-4), 0.0, 1.2);
    }

    out_color = vec4(clamp(sphere_intensity, 0.0, 1.0), clamp(norm_in_shell, 0.0, 1.0), 0.0, 1.0);
}
)";
}
