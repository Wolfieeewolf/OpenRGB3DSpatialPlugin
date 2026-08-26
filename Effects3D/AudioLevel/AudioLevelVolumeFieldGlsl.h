// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Audio Level volume field: R=fill intensity, G=gradient_pos01.
 *  p01 = room UV [0,1]^3. Path axis selects fill direction.
 *  u_params: [0]=fill_level [1]=wave_amount [2]=edge
 *            [3]=path_axis (0/1/2) [4]=size_m [5]=wave_freq
 *            [6]=detail [7]=speed_mul [8]=tight_mul [9]=time_e
 */
inline const char* AudioLevelVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float fill_level = clamp(u_params[0], 0.0, 1.0);
    float wave_amount = clamp(u_params[1], 0.0, 0.5);
    float edge = max(u_params[2], 0.01);
    int path_axis = int(floor(u_params[3] + 0.5));
    float size_m = max(u_params[4], 0.35);
    float wave_freq = max(u_params[5], 0.05);
    float detail = clamp(u_params[6], 0.05, 1.0);
    float speed_mul = max(u_params[7], 0.15);
    float tight_mul = max(u_params[8], 0.25);
    float time_e = u_params[9];

    float ax = p01.x;
    float ay = p01.y;
    float az = p01.z;
    float axis_pos = ay;
    float axis_other = ax;
    if(path_axis == 0)
    {
        axis_pos = ax;
        axis_other = ay;
    }
    else if(path_axis == 2)
    {
        axis_pos = az;
        axis_other = ay;
    }

    float wave = wave_amount * (1.0 + 0.45 * fill_level) *
        sin(time_e * 4.0 * speed_mul * wave_freq +
            axis_other * 6.283185 * (0.6 + 0.4 * detail * tight_mul));
    float fill_boundary = clamp(fill_level + wave, 0.0, 1.0);
    float edge_s = edge / max(0.35, size_m * tight_mul);
    float intensity = clamp((fill_boundary - axis_pos) / edge_s + 0.5, 0.0, 1.0);

    /* Radial from room center for gradient (matches prior CPU). */
    vec3 c = p01 - vec3(0.5);
    float radial = clamp(length(c) / 0.8660254, 0.0, 1.0);
    float gradient = clamp(0.65 * axis_pos + 0.35 * (1.0 - radial), 0.0, 1.0);

    out_color = vec4(intensity, gradient, 0.0, 1.0);
}
)";
}
