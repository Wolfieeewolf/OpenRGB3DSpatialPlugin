// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Spectrum Bars volume field: R=bar energy, G=gradient_pos01.
 *  p01 = room UV. Spectrum along X, height Y, radial from room center.
 *  sampler2D u_media — 1×N grayscale row of smoothed band levels (R channel).
 *  u_params: [0]=band_count [1]=roll_speed [2]=roll_phase
 *            [3]=size_m [4]=detail [5]=speed_mul [6]=tight_mul
 *            [7]=falloff_scale [8]=peak_boost [9]=sweep_phase
 */
inline const char* SpectrumBarsVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float band_count = max(u_params[0], 1.0);
    float roll_phase = u_params[2];
    float size_m = max(u_params[3], 0.35);
    float detail = clamp(u_params[4], 0.05, 1.0);
    float speed_mul = max(u_params[5], 0.15);
    float tight_mul = max(u_params[6], 0.25);
    float falloff_scale = max(u_params[7], 0.25);
    float peak_boost = clamp(u_params[8], 0.0, 4.0);
    float sweep_phase = u_params[9];

    float axis = p01.x;
    float center = 0.5;
    axis = clamp(center + (axis - center) * (0.6 + 0.4 * size_m) * (0.7 + 0.3 * detail * tight_mul), 0.0, 1.0);
    float axis_rolled = fract(axis + roll_phase + 1.0);

    float u = (floor(axis_rolled * band_count) + 0.5) / band_count;
    float band_value = texture2D(u_media, vec2(u, 0.5)).r;
    band_value = clamp(band_value * (1.0 + peak_boost * 0.35), 0.0, 1.0);

    float height_norm = p01.y;
    float height_profile = pow(clamp(height_norm, 0.0, 1.0), 1.6 / max(0.5, tight_mul * falloff_scale));
    vec3 c = p01 - vec3(0.5);
    float radial = clamp(length(c) / 0.8660254, 0.0, 1.0);
    float radial_profile = clamp(1.0 - radial, 0.0, 1.0);
    float sweep = 0.7 + 0.3 * sin((sweep_phase * speed_mul + axis_rolled) * 6.2831853);
    float energy = band_value * height_profile * (0.5 + 0.5 * radial_profile) * sweep;
    energy = clamp(energy, 0.0, 1.0);

    float gradient = axis_rolled;
    out_color = vec4(energy, gradient, 0.0, 1.0);
}
)";
}
