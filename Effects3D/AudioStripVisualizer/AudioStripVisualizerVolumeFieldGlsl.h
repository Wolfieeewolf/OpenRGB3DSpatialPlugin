// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Audio Strip Visualizer volume field: R=energy, G=path01 (gradient).
 *  p01 = room UV. Path axis + orthogonal axis form path×disp.
 *  sampler2D u_media — bars: 1×N columns; spectrogram: N×history (y=0 newest).
 *  u_params: [0]=display_mode (0=bars,1=spectrogram) [1]=path_axis
 *            [2]=mirror_bars [3]=size_m [4]=bar_edge
 *            [5]=scroll_offset [6]=peak_boost [7]=col_count
 *            [8]=row_count [9]=speed_mul
 */
inline const char* AudioStripVisualizerVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    int display_mode = int(floor(u_params[0] + 0.5));
    int path_axis = int(floor(u_params[1] + 0.5));
    float mirror_bars = step(0.5, u_params[2]);
    float size_m = max(u_params[3], 0.2);
    float bar_edge = max(u_params[4], 0.02);
    float scroll_offset = fract(u_params[5]);
    float peak_boost = clamp(u_params[6], 0.0, 4.0);
    float col_count = max(u_params[7], 1.0);
    float row_count = max(u_params[8], 1.0);
    float speed_mul = max(u_params[9], 0.15);

    float ax = p01.x;
    float ay = p01.y;
    float az = p01.z;
    float path01 = ax;
    float disp01 = ay;
    if(path_axis == 1)
    {
        path01 = ay;
        disp01 = ax;
    }
    else if(path_axis == 2)
    {
        path01 = az;
        disp01 = ay;
    }

    if(mirror_bars > 0.5 && display_mode == 0)
        path01 = abs(path01 * 2.0 - 1.0);
    path01 = clamp(path01, 0.0, 1.0);
    disp01 = clamp(disp01, 0.0, 1.0);

    float energy = 0.0;
    if(display_mode == 1)
    {
        float age01 = clamp(1.0 - disp01 + scroll_offset * 0.15, 0.0, 1.0);
        float u = (floor(path01 * (col_count - 1.0) + 0.5) + 0.5) / col_count;
        float v = (floor(age01 * (row_count - 1.0) + 0.5) + 0.5) / row_count;
        float band = texture2D(u_media, vec2(u, v)).r;
        band = clamp(band * (1.0 + peak_boost * 0.35), 0.0, 1.0);
        energy = band;
    }
    else
    {
        float u = (floor(path01 * (col_count - 1.0) + 0.5) + 0.5) / col_count;
        float level = texture2D(u_media, vec2(u, 0.5)).r;
        level = clamp(level * (1.0 + peak_boost * 0.35), 0.0, 1.0);
        float edge = bar_edge / max(0.2, size_m);
        float bar = clamp((level - disp01) / edge + 0.5, 0.0, 1.0);
        energy = level * bar;
        energy *= (0.85 + 0.15 * speed_mul);
    }

    energy = clamp(energy, 0.0, 1.0);
    out_color = vec4(energy, path01, 0.0, 1.0);
}
)";
}
