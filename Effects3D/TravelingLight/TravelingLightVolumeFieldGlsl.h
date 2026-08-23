// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Traveling Light volume field. R=intensity, G=color_driver01, B=aux (Crossing=v2 / Panes=mix).
 *  Soft falloffs throughout to avoid atlas/LED stair-step jitter.
 *  Wave Fronts: traveling soft band on Circles/Squares/Lines/Diagonal (not a sin flash).
 *
 *  u_params: [0]=mode [1]=progress [2]=size_scale [3]=tight_inv [4]=axis [5]=plane
 *            [6]=glow [7]=wipe_edge [8]=ndiv [9]=front_shape [10]=front_edge [11]=front_thick01
 *            [12]=freq_n [13]=ox [14]=oy [15]=oz
 *  p01 = grid-normalized rotated sample.
 */
inline const char* TravelingLightVolumeFieldGlsl()
{
    return R"(
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
float softBand(float d, float sigma)
{
    float s = max(sigma, 0.02);
    return exp(-(d * d) / (s * s));
}
float axisComp(vec3 p, int ax)
{
    if(ax == 0) return p.x;
    if(ax == 1) return p.y;
    return p.z;
}
float wrapDist01(float a, float b)
{
    float d = abs(a - b);
    return min(d, 1.0 - d);
}
float frontEdgeProfile(float d, float thick, int edge)
{
    float sigma = mix(0.035, 0.20, clamp(thick, 0.05, 1.0));
    if(edge == 1)
    {
        // Sharp with tiny feather so atlas sampling doesn't sparkle
        float halfw = sigma * 0.55;
        return 1.0 - smstep(halfw * 0.65, halfw, d);
    }
    if(edge == 2)
    {
        float halfw = sigma * 0.95;
        return 1.0 - smstep(halfw * 0.75, halfw, d);
    }
    // Round
    return softBand(d, sigma);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    int mode = int(clamp(u_params[0], 0.0, 9.0) + 0.5);
    float progress = fract(u_params[1]);
    float size_scale = max(u_params[2], 0.05);
    float tight_inv = max(u_params[3], 0.25);
    int ax = int(clamp(u_params[4], 0.0, 2.0) + 0.5);
    int plane = int(clamp(u_params[5], 0.0, 2.0) + 0.5);
    float glow = clamp(u_params[6], 0.1, 1.0);
    int wipe_edge = int(clamp(u_params[7], 0.0, 2.0) + 0.5);
    int ndiv = int(clamp(u_params[8], 2.0, 16.0) + 0.5);
    int front_shape = int(clamp(u_params[9], 0.0, 3.0) + 0.5);
    int front_edge = int(clamp(u_params[10], 0.0, 2.0) + 0.5);
    float front_thick = clamp(u_params[11], 0.05, 1.0);
    float freq_n = max(u_params[12], 0.02);

    vec3 l = p01 * 2.0 - 1.0;
    float intensity = 0.0;
    float driver = progress;
    float aux = 0.0;

    if(mode == 4)
    {
        // KITT — soft scanner
        float axis_val = axisComp(p01, ax);
        float p_step = (progress < 0.5) ? (2.0 * progress) : (2.0 * (1.0 - progress));
        float beam = p_step;
        float w = clamp(0.15 * size_scale, 0.05, 0.5) * tight_inv;
        float dist = abs(beam - axis_val);
        intensity = softBand(dist, w * 0.55) * (0.55 + 0.45 * (1.0 - smstep(0.0, w, dist)));
        driver = clamp(1.0 - dist / max(w, 1e-5), 0.0, 1.0);
        aux = (progress < 0.5) ? 1.0 : 0.0;
    }
    else if(mode == 5)
    {
        // Wipe — triangular ping-pong with soft plane
        float prog2 = fract(progress * 0.5) * 2.0;
        if(prog2 > 1.0) prog2 = 2.0 - prog2;
        float edge_distance = abs(p01.x - prog2);
        float thickness_factor = 0.2 * size_scale * tight_inv;
        if(wipe_edge == 0)
        {
            float core = 1.0 - smstep(0.0, thickness_factor * 0.6, edge_distance);
            float glow_ = 0.4 * (1.0 - smstep(thickness_factor * 0.6, thickness_factor * 1.2, edge_distance));
            intensity = min(1.0, core + glow_);
        }
        else if(wipe_edge == 1)
            intensity = 1.0 - smstep(thickness_factor * 0.35, thickness_factor * 0.55, edge_distance);
        else
            intensity = 1.0 - smstep(thickness_factor * 0.70, thickness_factor * 1.05, edge_distance);
        float radial = length(l) * 0.5;
        float depth = 0.55 + 0.45 * (1.0 - min(1.0, radial / 1.7320508) * 0.5);
        intensity *= depth;
        driver = fract(prog2 + progress * freq_n * 0.02);
    }
    else if(mode == 6)
    {
        // Moving Panes
        float prim = axisComp(p01, ax);
        float sec = (ax == 0) ? p01.y : ((ax == 1) ? p01.z : p01.x);
        float zone_size = 1.0 / float(ndiv);
        int zone = int(clamp(floor(prim / zone_size), 0.0, float(ndiv - 1)));
        float zone_id = float(zone - (zone / 2) * 2);
        float s = 0.5 * (1.0 + sin(sec * 3.14159265 * 4.0 + (zone_id > 0.5 ? 1.0 : -1.0) * progress * 6.2831853 + 0.78539816));
        intensity = 1.0;
        driver = s;
        aux = zone_id;
    }
    else if(mode == 7)
    {
        // Crossing Beams — smooth glow falloff
        float sine_x = sin(progress * 3.14159265);
        float sine_y = sin(progress * 3.14159265 * 1.3);
        float xp = 0.5 + sine_x * 0.5;
        float yp = 0.5 + sine_y * 0.5;
        float thick = clamp(0.08 * size_scale, 0.02, 0.2) * tight_inv;
        float soft = mix(0.08, 0.35, glow);
        float v1 = softBand(abs(p01.x - xp), thick * (0.55 + soft));
        float v2 = softBand(abs(p01.y - yp), thick * (0.55 + soft));
        intensity = v1;
        driver = v2;
        aux = 1.0;
    }
    else if(mode == 8)
    {
        // Rotating Beam — soft wedge
        float beam_angle = progress * 6.2831853;
        float point_angle = (plane == 0) ? atan(l.z, l.x) : ((plane == 1) ? atan(l.x, l.y) : atan(l.z, l.y));
        float diff = mod(point_angle - beam_angle + 3.14159265, 6.2831853);
        if(diff < 0.0) diff += 6.2831853;
        diff -= 3.14159265;
        float abs_diff = abs(diff);
        float width = clamp(0.15 * size_scale, 0.05, 0.5) * 3.14159265 * tight_inv;
        float core = 1.0 - smstep(0.0, width * 0.55, abs_diff);
        float halo = softBand(abs_diff, width * (0.85 + 0.9 * glow));
        intensity = max(core, halo * 0.65);
        driver = fract(progress + progress * freq_n * 0.02);
    }
    else if(mode == 9)
    {
        // Wave Fronts — traveling soft band / expanding ring (not a room-wide sin flash)
        float pos01 = 0.0;
        if(front_shape == 0)
        {
            // Circles: radial from origin, 0 at center → ~1 at far corner
            pos01 = clamp(length(l.xz) / 2.828427, 0.0, 1.25);
        }
        else if(front_shape == 1)
        {
            // Squares: Chebyshev
            pos01 = clamp(max(abs(l.x), abs(l.z)) * 0.5, 0.0, 1.25);
        }
        else if(front_shape == 2)
        {
            // Lines: sweep along X
            pos01 = p01.x;
        }
        else
        {
            // Diagonal
            pos01 = fract(p01.x * 0.5 + p01.z * 0.5);
        }

        // Frequency adds extra crests (1..4 rings/bands)
        float crests = clamp(1.0 + floor(freq_n * 0.85 + 0.35), 1.0, 4.0);
        float d = 1.0;
        if(front_shape == 0 || front_shape == 1)
        {
            // Expanding rings: each crest at progress/crests offsets
            for(int r = 0; r < 4; r++)
            {
                if(float(r) >= crests) break;
                float front = fract(progress + float(r) / crests);
                // Also previous cycle so the ring doesn't pop when wrapping
                d = min(d, abs(pos01 - front));
                d = min(d, abs(pos01 - (front - 1.0)));
                d = min(d, abs(pos01 - (front + 1.0)));
            }
        }
        else
        {
            // Sweeping planes with wrap
            for(int r = 0; r < 4; r++)
            {
                if(float(r) >= crests) break;
                float front = fract(progress + float(r) / crests);
                d = min(d, wrapDist01(pos01, front));
            }
        }

        intensity = frontEdgeProfile(d, front_thick, front_edge);
        // Mild depth cue — keep walls readable
        float radial = length(l) * 0.5;
        float depth = 0.72 + 0.28 * (1.0 - min(1.0, radial / 1.7320508) * 0.55);
        intensity *= depth;
        driver = fract(pos01 * 0.35 + progress);
    }
    else
    {
        // Comet / Chase / Marquee / ZigZag
        float axis_val = axisComp(p01, ax);
        float span = 1.0;
        float tail_len = max(0.08, 0.25 * span * size_scale * tight_inv);
        if(mode == 1)
        {
            for(int c = 0; c < 4; c++)
            {
                float p = fract(progress + float(c) / 4.0);
                float head = p;
                float distance = head - axis_val;
                float i = 0.0;
                float hue_off = 0.0;
                if(distance >= -tail_len * 0.15 && distance <= tail_len)
                {
                    float t = clamp(distance / tail_len, 0.0, 1.0);
                    float head_w = softBand(min(0.0, distance), tail_len * 0.12);
                    float tail = (1.0 - t) * (1.0 - t);
                    i = max(head_w, tail * 0.85) * smstep(-tail_len * 0.15, 0.0, distance);
                    // Keep soft past tip
                    i *= 1.0 - smstep(tail_len * 0.92, tail_len, max(0.0, distance));
                    hue_off = (1.0 - t) * 60.0 / 360.0;
                }
                if(i > intensity) { intensity = i; driver = fract(progress + hue_off); }
            }
        }
        else if(mode == 2)
        {
            float head = progress;
            float distance = head - axis_val;
            float band = max(0.06, tail_len * 0.5);
            float core = 1.0 - smstep(0.0, band, max(0.0, distance));
            float tip = softBand(min(0.0, distance), band * 0.35) * 0.7;
            intensity = max(core * step(0.0, distance + band * 0.05), tip);
            intensity *= 1.0 - smstep(band * 0.9, band * 1.15, max(0.0, distance));
            driver = fract(progress + progress * freq_n * 0.02);
        }
        else if(mode == 3)
        {
            // ZigZag — soft snake along folded path
            float prim = axisComp(p01, ax);
            float sec = (ax == 0) ? p01.y : ((ax == 1) ? p01.z : p01.x);
            float n_cols = 16.0;
            float n_rows = 16.0;
            float col_cont = clamp(prim, 0.0, 0.999) * n_cols;
            float row_cont = clamp(sec, 0.0, 0.999) * n_rows;
            float seg = floor(col_cont);
            float local = (mod(seg, 2.0) < 0.5) ? row_cont : (n_rows - row_cont);
            float path_pos = clamp((seg * n_rows + local) / (n_cols * n_rows), 0.0, 1.0);
            float tail = clamp(0.28 * size_scale, 0.12, 0.55) * tight_inv;
            float dist_in_tail = progress - path_pos;
            // Wrap-aware soft membership along the snake
            if(dist_in_tail < 0.0) dist_in_tail += 1.0;
            if(dist_in_tail <= tail)
            {
                float t = dist_in_tail / tail;
                intensity = (1.0 - t);
                intensity = intensity * intensity * (1.0 - smstep(0.82, 1.0, t));
                driver = path_pos;
            }
        }
        else
        {
            // Comet — soft head + quadratic tail
            float head = progress;
            float dist = head - axis_val;
            if(dist >= -tail_len * 0.18 && dist <= tail_len)
            {
                float t = clamp(dist / tail_len, 0.0, 1.0);
                float head_w = softBand(min(0.0, dist), tail_len * 0.14);
                float tail = (1.0 - t) * (1.0 - t);
                intensity = max(head_w, tail);
                intensity *= 1.0 - smstep(tail_len * 0.9, tail_len, max(0.0, dist));
                driver = fract(progress + (1.0 - t) * (60.0 / 360.0));
            }
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), clamp(driver, 0.0, 1.0), clamp(aux, 0.0, 1.0), 1.0);
}
)";
}
