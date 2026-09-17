// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Wave surface: R=intensity, G=pos_norm. p01 unit cube → l = p01*2-1.
 *  u_params: [0]=travel [1]=freq [2]=amp [3]=style [4]=dir_rad [5]=sigma
 */
inline const char* WaveSurfaceVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float travel = u_params[0];
    float freq = max(u_params[1], 0.2);
    float amp = max(u_params[2], 0.2);
    int style = int(u_params[3] + 0.5);
    float dir_rad = u_params[4];
    float sigma = max(u_params[5], 0.02);

    vec3 l = p01 * 2.0 - 1.0;
    float r = length(l.xz);
    float wave_pos = cos(dir_rad) * l.x + sin(dir_rad) * l.z;
    float phase = travel;
    float surface_y;

    if(style == 1)
    {
        surface_y = amp * sin(freq * r * 3.0 + travel);
    }
    else if(style == 2)
    {
        surface_y = amp * sin(freq * wave_pos * 4.0 + travel);
    }
    else if(style == 3)
    {
        surface_y = amp * (sin(freq * r + travel) * 0.5 +
                           sin(phase * 0.7 + freq * r * 1.5 + travel * 1.2) * 0.3 +
                           sin(phase * 0.5 + r * 2.0 + travel * 0.8) * 0.2);
    }
    else if(style == 4)
    {
        surface_y = amp * (0.5 + 0.5 * sin(freq * r + wave_pos * 2.0 + travel));
    }
    else
    {
        surface_y = amp * sin(freq * r + wave_pos * 2.0 + travel);
    }

    float d = abs(l.y - surface_y);
    float d_cutoff = 3.0 * sigma * max(1.0, amp);
    float intensity = 0.0;
    if(d <= d_cutoff)
    {
        intensity = exp(-d * d / (sigma * sigma));
        intensity = min(1.0, intensity);
    }
    float pos_norm = clamp((surface_y / amp + 1.0) * 0.5, 0.0, 1.0);
    out_color = vec4(intensity, pos_norm, 0.0, 1.0);
}
)";
}
