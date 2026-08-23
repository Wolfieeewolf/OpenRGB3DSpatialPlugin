// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Space / Starfield volume: R=intensity, G=palette01, B=hotness.
 *  Particle modes loop a capped star set once per atlas voxel (GPU), not per LED.
 *  u_params: [0]=progress [1]=time [2]=mode [3]=count
 *            [4]=thickness [5]=size_m [6]=fill [7]=drift
 *            [8]=twinkle [9]=hue_scroll
 *            [10]=ox [11]=oy [12]=oz
 *  mode: 0 Stars, 1 Twinkle, 2 Warp, 3 Hyperdrive, 4 Blackhole, 5 Wormhole
 */
inline const char* StarfieldVolumeFieldGlsl()
{
    return R"(
float hash11(float n)
{
    return fract(sin(n * 127.1) * 43758.5453);
}
float hash_signed(float n)
{
    return hash11(n) * 2.0 - 1.0;
}
float saturate(float v) { return clamp(v, 0.0, 1.0); }
float softband(float d, float s)
{
    float ss = max(s, 0.012);
    return exp(-(d * d) / (ss * ss));
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress = u_params[0];
    float time_sec = u_params[1];
    int mode = int(clamp(u_params[2], 0.0, 5.0) + 0.5);
    int count = int(clamp(u_params[3], 8.0, 48.0) + 0.5);
    float thickness = max(u_params[4], 0.02);
    float size_m = max(u_params[5], 0.25);
    float fill = clamp(u_params[6], 0.4, 1.0);
    float drift = clamp(u_params[7], 0.0, 1.0);
    float twinkle = clamp(u_params[8], 0.0, 1.0);
    float hue_scroll = u_params[9];
    vec3 origin01 = clamp(vec3(u_params[10], u_params[11], u_params[12]), 0.0, 1.0);

    // View space: origin-centered, FOV scaled.
    vec3 led = (p01 - origin01) * (2.0 / max(fill, 0.4));
    float sway = drift * 0.18;
    float sway_c = cos(time_sec * 0.35 * sway * 6.0 + 0.4);
    float sway_s = sin(time_sec * 0.28 * sway * 6.0);
    float led_vx = led.x * (1.0 + sway * 0.08 * sway_c) - led.y * sway * 0.12 * sway_s;
    float led_vy = led.y * (1.0 + sway * 0.08 * sway_c) + led.x * sway * 0.12 * sway_s;
    float led_vz = led.z;
    float radial = length(vec2(led_vx, led_vy));
    float ang = atan(led_vy, led_vx);

    float intensity = 0.0;
    float palette01 = 0.5;
    float hotness = 0.0;

    if(mode == 4)
    {
        // Blackhole — O(1)
        float r_xy = max(radial, 1e-4);
        float horizon = 0.14 + 0.05 * thickness;
        if(r_xy < horizon && led_vz > -0.2)
        {
            out_color = vec4(0.0);
            return;
        }
        float disk_thick = max(0.04, thickness * 0.50 * size_m);
        float plane = exp(-(led_vz * led_vz) / (disk_thick * disk_thick * 4.0))
                    * exp(-(led_vy * led_vy) / (disk_thick * disk_thick));
        float disk = plane * smoothstep(horizon * 1.1, horizon * 1.6, r_xy)
                           * (1.0 - smoothstep(0.55, 1.15, r_xy));
        float spiral = 0.5 + 0.5 * sin(4.0 * ang - log(r_xy) * 3.8 - progress * 3.5);
        float photon = exp(-pow((r_xy - horizon * 1.35) / (0.04 + 0.03 * thickness), 2.0));
        float lens = exp(-pow((r_xy - horizon) / 0.08, 2.0)) * 0.45;
        intensity = disk * (0.30 + 0.70 * spiral) + photon * 1.4 + lens;
        intensity *= 1.0 - smoothstep(0.9, 1.3, length(led));
        palette01 = fract(1.0 - r_xy + hue_scroll);
        hotness = saturate(1.0 - (r_xy - horizon) / 0.5) * 0.55 + photon * 0.4;
    }
    else if(mode == 5)
    {
        // Wormhole — O(1)
        float depth = saturate(led_vz * 0.5 + 0.5);
        float tunnel_r = 0.38 + 0.20 * size_m;
        float wall_w = max(0.03, thickness * 0.50 * size_m);
        float wall = exp(-pow((radial - tunnel_r) / wall_w, 2.0));
        float rings = 0.5 + 0.5 * cos((depth * 5.5 + progress * 0.85) * 6.2831853);
        float helix = 0.5 + 0.5 * cos(6.0 * ang + depth * (5.0 + drift * 4.0) - progress * 6.6);
        float perspective = 0.30 + 0.70 * (1.0 - depth);
        float core_glow = (1.0 - smoothstep(0.0, tunnel_r * 0.9, radial)) * 0.10
                          * (0.35 + 0.65 * rings) * perspective;
        intensity = wall * (0.28 + 0.40 * rings + 0.48 * helix) * perspective + core_glow;
        if(radial > tunnel_r + wall_w * 3.0)
            intensity = 0.0;
        intensity *= (1.0 - smoothstep(1.05, 1.35, abs(led_vx)))
                   * (1.0 - smoothstep(1.05, 1.35, abs(led_vy)));
        intensity *= 1.25;
        palette01 = fract(depth + hue_scroll);
        hotness = depth * 0.35;
    }
    else
    {
        // Particle field — capped loop (atlas, not per-LED).
        float base_thick = max(0.012, thickness * 0.40 * size_m);
        float sum_i = 0.0;
        float sum_p = 0.0;
        float sum_h = 0.0;
        for(int i = 0; i < 48; i++)
        {
            if(i >= count)
                break;
            float fi = float(i);
            float dir_x = hash_signed(fi * 12.9898 + 31.0);
            float dir_y = hash_signed(fi * 78.233 + 32.0);
            float dir_len = length(vec2(dir_x, dir_y)) + 1e-4;
            float ux = dir_x / dir_len;
            float uy = dir_y / dir_len;
            float aim = 0.15 + 0.95 * hash11(fi * 45.1 + 33.0);
            float seed_d = hash11(fi * 91.7 + 34.0);

            float depth;
            float stretch = 1.0;
            float bright = 1.0;
            if(mode == 1)
            {
                depth = 0.35 + 0.60 * seed_d;
                float rate = 1.0 + twinkle * 5.0 + hash11(fi + 35.0) * 3.0;
                float ph = time_sec * rate * 0.8 + seed_d * 6.2831853;
                float flash = pow(max(0.0, sin(ph)), 12.0);
                bright = 0.14 + 0.86 * flash;
            }
            else
            {
                float travel = progress * 0.22;
                if(mode == 2) travel *= 1.35;
                if(mode == 3) travel *= 2.05;
                depth = fract(seed_d - travel);
                float nearness = 1.0 - depth;
                if(mode == 0)
                {
                    stretch = 1.0 + nearness * nearness * (2.2 + 2.0 * size_m);
                    bright = 0.35 + 0.65 * nearness;
                    if(twinkle > 0.01)
                    {
                        float ph = time_sec * (1.5 + twinkle * 2.5) + fi;
                        bright *= 0.75 + 0.25 * pow(max(0.0, sin(ph)), 6.0);
                    }
                }
                else if(mode == 2)
                {
                    stretch = 1.0 + nearness * (8.0 + 10.0 * size_m);
                    bright = 0.25 + 0.90 * nearness;
                }
                else
                {
                    stretch = 1.0 + nearness * (14.0 + 16.0 * size_m);
                    bright = 0.20 + 1.10 * nearness;
                }
            }

            float persp = 1.0 / max(0.06, 0.08 + depth * 0.92);
            float px = ux * aim * persp * 0.95;
            float py = uy * aim * persp * 0.95;
            float pz = depth * 2.0 - 1.0;

            float dx = led_vx - px;
            float dy = led_vy - py;
            float dz = led_vz - pz;

            float mx = ux;
            float my = uy;
            float mz = -1.0;
            if(mode == 2 || mode == 3)
            {
                mz = -0.35 - 0.65 * (1.0 - depth);
            }
            float mlen = length(vec3(mx, my, mz)) + 1e-4;
            mx /= mlen; my /= mlen; mz /= mlen;

            float along = dx * mx + dy * my + dz * mz;
            vec3 across = vec3(dx, dy, dz) - vec3(mx, my, mz) * along;
            float across2 = dot(across, across);

            float thick = base_thick;
            if(mode == 1) thick *= 0.70;
            if(mode == 3) thick *= 0.85;
            float along_sig = thick * stretch;
            float across_sig = thick * ((mode == 1) ? 0.85 : 1.0);
            float d2 = (along * along) / (along_sig * along_sig) + across2 / (across_sig * across_sig);
            if(d2 > 10.0)
                continue;

            float contrib = exp(-d2) * bright;
            contrib *= smoothstep(-1.25, -0.85, led_vz)
                     * (1.0 - smoothstep(1.05, 1.35, abs(led_vx)))
                     * (1.0 - smoothstep(1.05, 1.35, abs(led_vy)));

            if(mode == 1)
            {
                float crossv = exp(-(abs(dx) + abs(dy)) * 18.0)
                             * pow(max(0.0, sin(time_sec * (2.0 + twinkle * 4.0) + fi)), 10.0);
                contrib = max(contrib, crossv * 1.2);
            }
            if(contrib < 0.02)
                continue;

            float hot = 0.0;
            if(mode == 2 || mode == 3)
                hot = saturate((1.0 - depth) * ((mode == 3) ? 0.75 : 0.45));
            else if(mode == 1)
                hot = saturate((bright - 0.5) * 1.2);

            sum_i += contrib;
            sum_p += (1.0 - depth) * contrib;
            sum_h += hot * contrib;
        }

        if(sum_i > 1e-5)
        {
            float gain = (mode == 3) ? 1.15 : 0.95;
            intensity = clamp(sum_i * gain, 0.0, 1.0);
            palette01 = fract(sum_p / sum_i + hue_scroll);
            hotness = clamp(sum_h / sum_i, 0.0, 1.0);
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), fract(palette01), clamp(hotness, 0.0, 1.0), 1.0);
}
)";
}
