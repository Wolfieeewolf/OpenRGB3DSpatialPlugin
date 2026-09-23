// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Audio Paintbrush — MoonLight-inspired soft Lissajous ribbons driven by bass/mid/treble.
 *  R=intensity, G=color_driver01.
 *  u_params: [0]=bass [1]=mid [2]=treble [3]=time_boost [4]=thickness
 *            [5]=size_scale [6]=detail [7]=hue_scroll [8]=chaos [9]=ribbon_count
 *
 *  Fixed loop bounds (no break) for Windows GLSL 1.10 drivers.
 */
inline const char* AudioPaintBrushVolumeFieldGlsl()
{
    return R"(
float softband(float d, float w)
{
    float hw = max(w, 0.010);
    float t = 1.0 - smoothstep(0.0, hw, abs(d));
    return t * t;
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float bass = clamp(u_params[0], 0.0, 1.0);
    float mid = clamp(u_params[1], 0.0, 1.0);
    float treble = clamp(u_params[2], 0.0, 1.0);
    float tboost = fract(u_params[3] * 0.1591549) * 6.2831853;
    float thick = max(u_params[4], 0.015);
    float size_m = clamp(u_params[5], 0.35, 2.0);
    float detail = clamp(u_params[6], 0.05, 1.0);
    float hue_scroll = fract(u_params[7]);
    float chaos = clamp(u_params[8], 0.0, 1.5);
    float n_rib = clamp(u_params[9], 1.0, 4.0);

    vec3 p = (p01 - 0.5) * 2.0;
    float half_w = max(0.014, thick * mix(0.55, 0.22, detail));
    float best = 0.0;
    float best_h = 0.0;
    float audio = max(bass, max(mid, treble));
    /* Size 100 should sweep most of the room; audio fattens the orbit. */
    float amp = (0.58 + 0.42 * bass + 0.22 * mid + 0.12 * audio) * size_m;

    for(int r = 0; r < 4; r++)
    {
        float fr = float(r);
        float slot_on = 1.0 - step(n_rib, fr);
        float phase = fr * 1.57 + chaos * fr * 0.7;
        float best_seg = 0.0;
        for(int i = 0; i < 12; i++)
        {
            float u = float(i) / 11.0;
            float a = tboost + u * 6.2831853 + phase;
            vec3 c = vec3(
                sin(a * (1.0 + 0.35 * fr) + mid * 1.2) * amp,
                sin(a * (1.3 + 0.2 * fr) + bass * 2.0 + phase) * amp * (0.55 + 0.35 * bass),
                cos(a * (0.85 + 0.25 * fr) + treble * 1.5) * amp);
            float d = length(p - c);
            float glow = softband(d, half_w * (1.0 + 0.40 * treble));
            best_seg = max(best_seg, glow);
        }
        float seg = best_seg * slot_on;
        best = max(best, seg);
        if(seg >= best - 1e-5)
            best_h = fract(fr * 0.19 + hue_scroll + mid * 0.25);
    }

    float gain = 0.45 + 0.95 * audio;
    float intensity = clamp(best * gain, 0.0, 1.0);
    out_color = vec4(intensity, best_h, 0.0, 1.0);
}
)";
}
