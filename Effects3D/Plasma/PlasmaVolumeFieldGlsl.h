// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** GLSL body for SpatialVolumeFieldEngine: plasma scalar in R (0..1).
 *  u_params[0] = progress, [1] = freq_scale, [2] = pattern (0..7),
 *  [3..5] = effect/ref origin in unit UV (centers radial modes).
 */
inline const char* PlasmaVolumeFieldGlsl()
{
    return R"(
float finalizePlasma01(float raw, float bias, float gain)
{
    float v = (raw + 6.0) / 12.0;
    v = clamp(v + bias, 0.0, 1.0);
    if(gain > 0.01 && abs(gain - 1.0) > 0.001)
        v = pow(v, gain);
    return clamp(v, 0.0, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float prog = u_params[0];
    float freq_scale_e = max(u_params[1], 0.05);
    int pattern_type = int(u_params[2] + 0.5);
    vec3 origin01 = clamp(vec3(u_params[3], u_params[4], u_params[5]), 0.0, 1.0);
    // Shift so the effect/ref origin sits at 0.5.
    float coord1 = clamp(p01.x - origin01.x + 0.5, 0.0, 1.0);
    float coord2 = clamp(p01.y - origin01.y + 0.5, 0.0, 1.0);
    float coord3 = clamp(p01.z - origin01.z + 0.5, 0.0, 1.0);
    float plasma_value = 0.5;

    if(pattern_type == 0)
    {
        plasma_value =
            sin((coord1 + prog * 2.0) * freq_scale_e * 10.0) +
            sin((coord2 + prog * 1.7) * freq_scale_e * 8.0) +
            sin((coord1 + coord2 + prog * 1.3) * freq_scale_e * 6.0) +
            cos((coord1 - coord2 + prog * 2.2) * freq_scale_e * 7.0) +
            sin(sqrt(coord1 * coord1 + coord2 * coord2) * freq_scale_e * 5.0 + prog * 1.5) +
            cos(coord3 * freq_scale_e * 4.0 + prog * 0.9);
        plasma_value = finalizePlasma01(plasma_value, 0.0, 1.12);
    }
    else if(pattern_type == 1)
    {
        float angle = atan(coord2 - 0.5, coord1 - 0.5);
        float radius = sqrt((coord1 - 0.5) * (coord1 - 0.5) + (coord2 - 0.5) * (coord2 - 0.5));
        plasma_value =
            sin(angle * 4.0 + radius * freq_scale_e * 9.5 + prog * 2.4) +
            sin(angle * 3.0 - radius * freq_scale_e * 7.0 + prog * 1.8) +
            cos(angle * 6.0 + radius * freq_scale_e * 4.5 - prog * 2.1) +
            sin(coord3 * freq_scale_e * 5.5 + prog) +
            cos((angle * 2.0 + coord3 * freq_scale_e * 3.5) + prog * 1.4);
        plasma_value = finalizePlasma01(plasma_value, -0.04, 1.38);
    }
    else if(pattern_type == 2)
    {
        float dist_from_center = sqrt((coord1 - 0.5) * (coord1 - 0.5) + (coord2 - 0.5) * (coord2 - 0.5));
        plasma_value =
            sin(dist_from_center * freq_scale_e * 12.0 - prog * 3.4) +
            sin(dist_from_center * freq_scale_e * 18.0 - prog * 2.6) +
            cos(dist_from_center * freq_scale_e * 9.0 + prog * 2.0) +
            sin((coord1 + coord2) * freq_scale_e * 5.0 + prog * 1.0) * 0.45 +
            cos(coord3 * freq_scale_e * 4.0 - prog * 0.6) * 0.35;
        plasma_value = finalizePlasma01(plasma_value, 0.02, 1.28);
    }
    else if(pattern_type == 3)
    {
        float flow1 = sin(coord1 * freq_scale_e * 8.0 + sin(coord2 * freq_scale_e * 12.0 + prog) + prog * 0.5);
        float flow2 = cos(coord2 * freq_scale_e * 9.0 + cos(coord3 * freq_scale_e * 11.0 + prog * 1.3));
        float flow3 = sin(coord3 * freq_scale_e * 7.0 + sin(coord1 * freq_scale_e * 13.0 + prog * 0.7));
        float flow4 = cos((coord1 + coord2) * freq_scale_e * 6.0 + sin(prog * 1.5));
        float flow5 = sin((coord2 + coord3) * freq_scale_e * 5.0 + cos(prog * 1.8));
        plasma_value = flow1 + flow2 + flow3 + flow4 + flow5;
        plasma_value = finalizePlasma01(plasma_value, 0.06, 0.88);
    }
    else if(pattern_type == 4)
    {
        float n1 = sin((coord1 + prog * 0.5) * freq_scale_e * 40.0) *
                   sin((coord2 + prog * 0.3) * freq_scale_e * 52.0) *
                   sin((coord3 + prog * 0.7) * freq_scale_e * 31.0);
        float n2 = sin((coord1 * 2.3 + coord2 + prog) * freq_scale_e * 20.0) *
                   cos((coord2 * 1.7 + coord3 + prog * 1.2) * freq_scale_e * 25.0);
        float n3 = cos((coord1 + coord2 * 2.1 + coord3) * freq_scale_e * 15.0 + prog * 2.0);
        plasma_value = n1 * 0.5 + n2 * 0.35 + n3 * 0.15;
        plasma_value = finalizePlasma01(plasma_value, -0.02, 1.55);
    }
    else if(pattern_type == 5)
    {
        float y = coord2 - prog * 0.55;
        float turb = 0.18 * sin((coord1 * 3.1 + coord3 * 2.7) * freq_scale_e * 8.0 + prog * 4.0);
        float xw = coord1 + turb * sin(y * freq_scale_e * 10.0 + prog * 2.0);
        float zw = coord3 + turb * cos(y * freq_scale_e * 9.0 - prog * 1.7);
        float tongues =
            sin(xw * freq_scale_e * 14.0 + prog * 3.2) +
            cos(zw * freq_scale_e * 12.0 - prog * 2.4) +
            0.7 * sin((xw + zw) * freq_scale_e * 9.0 + y * freq_scale_e * 16.0 - prog * 5.0) +
            0.5 * cos(y * freq_scale_e * 18.0 + prog * 1.5);
        float floor_heat = pow(clamp(1.0 - coord2, 0.0, 1.0), 0.55);
        plasma_value = tongues * (0.35 + 0.65 * floor_heat) + 1.25 * floor_heat;
        plasma_value = pow(clamp((plasma_value + 3.0) / 6.0, 0.0, 1.0), 0.72);
    }
    else
    {
        float r = sqrt((coord1 - 0.5) * (coord1 - 0.5) +
                       (coord2 - 0.5) * (coord2 - 0.5) +
                       (coord3 - 0.5) * (coord3 - 0.5));
        plasma_value =
            sin(r * freq_scale_e * 30.0 - prog * 2.0) +
            sin((coord1 + coord2) * freq_scale_e * 20.0 + prog * 1.5) * 0.6 +
            cos((coord2 + coord3) * freq_scale_e * 18.0 - prog * 1.2) * 0.5 +
            sin(coord3 * freq_scale_e * 25.0 + prog * 0.8) * 0.4;
        plasma_value = finalizePlasma01(plasma_value, 0.0, 1.2);
    }

    out_color = vec4(plasma_value, plasma_value, plasma_value, 1.0);
}
)";
}
