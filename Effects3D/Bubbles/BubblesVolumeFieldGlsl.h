// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Bubbles volume: R=intensity, G=hue01.
 *  Rising expanding shells evaluated once per atlas voxel (not per LED).
 *  Overlap push is skipped on GPU (golden-angle spacing is enough at room scale).
 *  u_params: [0]=time [1]=count [2]=thick01 [3]=rise_rate
 *            [4]=interval [5]=max_r01 [6]=fill [7]=launch_jitter
 *            [8]=hue_scroll01 [9]=ox [10]=oy [11]=oz
 */
inline const char* BubblesVolumeFieldGlsl()
{
    return R"(
float hash01(float seed, float salt)
{
    return fract(sin(seed * 12.9898 + salt * 78.233) * 43758.5453);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float time_sec = u_params[0];
    int count = int(clamp(u_params[1], 4.0, 48.0) + 0.5);
    float thick = max(u_params[2], 0.008);
    float rise_rate = max(u_params[3], 0.01);
    float interval = max(u_params[4], 0.12);
    float max_r = max(u_params[5], 0.04);
    float fill = clamp(u_params[6], 0.5, 1.8);
    float launch_jitter = clamp(u_params[7], 0.0, 1.0);
    float hue_scroll = fract(u_params[8]);
    vec3 origin01 = clamp(vec3(u_params[9], u_params[10], u_params[11]), 0.0, 1.0);

    // Local space around effect/ref origin; Y spans the room height for rise.
    vec3 local = p01 - origin01;
    float intensity = 0.0;
    float hue01 = hue_scroll;
    const float golden = 2.39996323;

    for(int i = 0; i < 48; i++)
    {
        if(i >= count)
            break;
        float fi = float(i);
        float seed = fi * 265.443 + 101.390;
        float cycle_mul = (1.0 + 0.35 * launch_jitter) + (2.0 * launch_jitter) * hash01(seed, 11.0);
        float active_frac = (0.52 - 0.26 * launch_jitter) + (0.06 + 0.22 * launch_jitter) * hash01(seed, 12.0);
        float cycle_i = max(0.12, interval * cycle_mul);
        float active_window = max(0.04, cycle_i * active_frac);
        float offset_i = cycle_i * hash01(seed, 13.0);
        float phase_i = mod(time_sec * rise_rate + offset_i, cycle_i);
        if(phase_i > active_window)
            continue;

        float radius_phase = phase_i / active_window;
        float radius = (0.18 + 0.82 * radius_phase) * max_r * 0.55;
        float ring = sqrt((fi + 0.5) / float(count));
        float ang = fi * golden;
        // Centers in origin-local UV: XZ spread by fill, Y rises 0→1 of room.
        vec3 c;
        c.x = origin01.x + cos(ang) * ring * 0.50 * fill;
        c.y = radius_phase; // rise through unit height
        c.z = origin01.z + sin(ang) * ring * 0.50 * fill;
        c = clamp(c, vec3(0.0), vec3(1.0));

        float d = length(p01 - c);
        float shallow = abs(d - radius) / thick;
        float value = 1.0 / (1.0 + shallow * shallow);
        if(value > intensity)
        {
            intensity = value;
            hue01 = fract(fi * 0.111 + hue_scroll);
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), hue01, 0.0, 1.0);
}
)";
}
