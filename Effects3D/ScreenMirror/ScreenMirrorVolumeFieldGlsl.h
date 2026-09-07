// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Screen Mirror volume field: atlas stores graded RGB after plane mapping.
 *  p01 = unit room UV (front-left floor = 0). Pair with SampleGpuRoomVolume01.
 *  u_media tiles: columns = monitors (1..2), rows = wave history (1..4, 0 = newest).
 *
 *  Shared u_params:
 *    [0..2] AABB span_mm  [3] layout  [4] pack(steps_u, steps_v)  [5] avg_frame_ms
 *    layout = nmon + 10*nhist + 100*use_q + 1000*flip0 + 2000*flip1
 *
 *  Per monitor (21 floats), base 6 and 27:
 *    [0..2] ref_uv  [3..5] plane_right  [6..8] plane_up
 *    packed pairs (4095/4096): coverage/invert, softness/curve, roll/radial,
 *    bias TL/TR, BL/BR, letterbox/pillarbox, corner str/zone,
 *    zone u, zone v, wave speed/decay, fb/lr, tb/blend
 */
inline const char* ScreenMirrorVolumeFieldGlsl()
{
    return R"(
void smUnpack01(float p, out float a, out float b)
{
    float bi = floor(mod(p, 4096.0));
    float ai = floor(p / 4096.0);
    a = ai / 4095.0;
    b = bi / 4095.0;
}
float smStep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
void smMap(vec3 p01, vec3 ref_uv, vec3 span_mm, vec3 right, vec3 up,
           out float u, out float v, out float dist_mm)
{
    vec3 delta_mm = (p01 - ref_uv) * span_mm;
    dist_mm = length(delta_mm);
    if(dist_mm < 1e-4)
    {
        u = 0.5;
        v = 0.5;
        return;
    }
    vec3 dir = delta_mm / dist_mm;
    float dir_right = dot(dir, right);
    float dir_up = dot(dir, up);
    float cc = 0.9025853;
    float ss = -0.4305111;
    float dir_r_rot = dir_right * cc - dir_up * ss;
    float dir_u_rot = dir_right * ss + dir_up * cc;
    u = clamp(0.5 + 0.5 * dir_r_rot * 1.41421356, 0.0, 1.0);
    v = clamp(0.5 + 0.5 * dir_u_rot * 1.41421356, 0.0, 1.0);
}
void smRadial(inout float u, inout float v, float exp_pct, float btl, float btr, float bbl, float bbr)
{
    if(abs(exp_pct) < 0.05 && abs(btl) < 0.05 && abs(btr) < 0.05 && abs(bbl) < 0.05 && abs(bbr) < 0.05)
        return;
    float du = u - 0.5;
    float dv = v - 0.5;
    float wexp = 4.0 * abs(du) * abs(dv);
    float g = exp_pct / 100.0;
    u = 0.5 + du * (1.0 + wexp * g);
    v = 0.5 + dv * (1.0 + wexp * g);
    float k = 0.0028;
    float wtl = (u > 0.5 || v > 0.5) ? 0.0 : (1.0 - 2.0 * u) * (1.0 - 2.0 * v);
    float wtr = (u < 0.5 || v > 0.5) ? 0.0 : (2.0 * u - 1.0) * (1.0 - 2.0 * v);
    float wbl = (u > 0.5 || v < 0.5) ? 0.0 : (1.0 - 2.0 * u) * (2.0 * v - 1.0);
    float wbr = (u < 0.5 || v < 0.5) ? 0.0 : (2.0 * u - 1.0) * (2.0 * v - 1.0);
    u += k * (-wtl * btl + wtr * btr - wbl * bbl + wbr * bbr);
    v += k * (-wtl * btl - wtr * btr + wbl * bbl + wbr * bbr);
    u = clamp(u, 0.0, 1.0);
    v = clamp(v, 0.0, 1.0);
}
void smRoll(inout float u, inout float v, float roll_deg)
{
    if(abs(roll_deg) < 0.005)
        return;
    float rad = roll_deg * 0.0174532925;
    float c = cos(rad);
    float s = sin(rad);
    float du = u - 0.5;
    float dv = v - 0.5;
    u = clamp(0.5 + c * du - s * dv, 0.0, 1.0);
    v = clamp(0.5 + s * du + c * dv, 0.0, 1.0);
}
float smFalloff(float dist_mm, float max_mm, float coverage, float softness, float curve, float inverted)
{
    coverage = max(coverage, 0.0);
    if(coverage <= 0.0001 || max_mm <= 0.0)
        return 0.0;
    if(inverted > 0.5)
    {
        float range = max(max_mm * coverage, 10.0);
        if(dist_mm <= range * (1.0 - softness / 100.0))
            return 1.0;
        if(dist_mm >= range)
            return 0.0;
        float feather = range * (softness / 100.0);
        float core = range - feather;
        float t = clamp((dist_mm - core) / max(feather, 1e-4), 0.0, 1.0);
        return 1.0 - smStep(0.0, 1.0, t);
    }
    if(coverage >= 0.999)
        return 1.0;
    float nd = clamp(dist_mm / max(max_mm, 1.0), 0.0, 1.0);
    nd = pow(nd, clamp(curve, 0.25, 4.0));
    float boundary = max(0.0, 1.0 - coverage);
    if(boundary <= 0.0005)
        return 1.0;
    float feather_band = clamp(softness / 100.0, 0.0, 0.95) * 0.5;
    float fade_start = max(0.0, boundary - feather_band);
    float w = smStep(fade_start, boundary, nd);
    if(coverage >= 1.0 && w < 1.0)
        w = max(w, min(coverage - 0.99, 1.0));
    return w;
}
vec3 smCornerSample(vec2 uv, vec2 tile_origin, vec2 tile_scale, vec2 uv_min, vec2 uv_max,
                    float strength, float zone)
{
    vec2 mid_uv = (tile_origin + uv * tile_scale);
    vec3 c_mid = texture2D(u_media, mid_uv).rgb;
    if(strength <= 0.004)
        return c_mid;
    vec2 un = clamp((uv - uv_min) / max(uv_max - uv_min, vec2(1e-6)), 0.0, 1.0);
    float edge_u = min(un.x, 1.0 - un.x);
    float edge_v = min(un.y, 1.0 - un.y);
    float near_u = 1.0 - smStep(0.0, zone, edge_u);
    float near_v = 1.0 - smStep(0.0, zone, edge_v);
    float cw = min(near_u, near_v);
    float w = cw * strength;
    if(w <= 0.004)
        return c_mid;
    float snap_u = (un.x < 0.5) ? uv_min.x : uv_max.x;
    float snap_v = (un.y < 0.5) ? uv_min.y : uv_max.y;
    vec3 c_vert = texture2D(u_media, tile_origin + vec2(snap_u, uv.y) * tile_scale).rgb;
    vec3 c_horiz = texture2D(u_media, tile_origin + vec2(uv.x, snap_v) * tile_scale).rgb;
    return mix(c_mid, 0.5 * (c_vert + c_horiz), w);
}
void smEvalMonitor(vec3 p01, vec3 span_mm, float nmon, float nhist, float use_q, float flip_v,
                   float mon, float steps_u, float steps_v, float avg_frame_ms,
                   float p0, float p1, float p2, float p3, float p4, float p5,
                   float p6, float p7, float p8, float p9, float p10, float p11,
                   float p12, float p13, float p14, float p15, float p16, float p17,
                   float p18, float p19, float p20,
                   out vec3 rgb, out float weight)
{
    rgb = vec3(0.0);
    weight = 0.0;
    vec3 ref_uv = vec3(p0, p1, p2);
    vec3 right = vec3(p3, p4, p5);
    vec3 up = vec3(p6, p7, p8);
    float u = 0.5;
    float v = 0.5;
    float dist_mm = 0.0;
    smMap(p01, ref_uv, span_mm, right, up, u, v, dist_mm);

    float zu0, zu1, zv0, zv1;
    smUnpack01(p16, zu0, zu1);
    smUnpack01(p17, zv0, zv1);
    if(u < zu0 || u > zu1 || v < zv0 || v > zv1)
        return;

    float coverage, invert;
    smUnpack01(p9, coverage, invert);
    coverage *= 3.0;
    float softness, curve01;
    smUnpack01(p10, softness, curve01);
    softness *= 100.0;
    float curve = 0.25 + curve01 * 3.75;
    float roll01, radial01;
    smUnpack01(p11, roll01, radial01);
    float roll = roll01 * 360.0 - 180.0;
    float radial = radial01 * 100.0 - 50.0;
    float btl01, btr01, bbl01, bbr01;
    smUnpack01(p12, btl01, btr01);
    smUnpack01(p13, bbl01, bbr01);
    smRadial(u, v, radial, btl01 * 100.0 - 50.0, btr01 * 100.0 - 50.0,
             bbl01 * 100.0 - 50.0, bbr01 * 100.0 - 50.0);
    smRoll(u, v, roll);

    float max_mm = 0.0;
    max_mm = max(max_mm, length((vec3(0.0, 0.0, 0.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(1.0, 0.0, 0.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(0.0, 1.0, 0.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(1.0, 1.0, 0.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(0.0, 0.0, 1.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(1.0, 0.0, 1.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(0.0, 1.0, 1.0) - ref_uv) * span_mm));
    max_mm = max(max_mm, length((vec3(1.0, 1.0, 1.0) - ref_uv) * span_mm));
    if(max_mm <= 0.0)
        max_mm = 3000.0;

    float fall = smFalloff(dist_mm, max_mm, coverage, softness, curve, invert);
    float spd01, decay01;
    smUnpack01(p18, spd01, decay01);
    float speed = spd01 * 500.0;
    float decay = decay01 * 10000.0;
    float delay_ms = 0.0;
    if(speed >= 0.1)
        delay_ms = clamp(dist_mm / max(speed, 0.1), 0.0, 60000.0);
    float wave_env = 1.0;
    if(speed >= 0.1 && decay > 0.1)
        wave_env = exp(-delay_ms / max(decay, 0.1));
    weight = fall * wave_env;

    float fb01, lr01, tb01, blend01;
    smUnpack01(p19, fb01, lr01);
    smUnpack01(p20, tb01, blend01);
    float fb = fb01 * 200.0 - 100.0;
    float lr = lr01 * 200.0 - 100.0;
    float tb = tb01 * 200.0 - 100.0;
    if(max_mm > 0.001 && (abs(fb) > 0.5 || abs(lr) > 0.5 || abs(tb) > 0.5))
    {
        vec3 nrm = cross(right, up);
        vec3 d_uv = p01 - ref_uv;
        float lat = dot(d_uv * span_mm, right);
        float vert = dot(d_uv * span_mm, up);
        float depth = dot(d_uv * span_mm, nrm);
        float inv_max = 1.0 / max_mm;
        float dir_fb = clamp(1.0 + (fb / 100.0) * clamp(depth * inv_max, -1.0, 1.0), 0.0, 2.0);
        float dir_lr = clamp(1.0 + (lr / 100.0) * clamp(lat * inv_max, -1.0, 1.0), 0.0, 2.0);
        float dir_tb = clamp(1.0 + (tb / 100.0) * clamp(vert * inv_max, -1.0, 1.0), 0.0, 2.0);
        weight *= dir_fb * dir_lr * dir_tb;
    }
    if(weight <= 0.01)
    {
        weight = 0.0;
        return;
    }

    float lp, pp;
    smUnpack01(p14, lp, pp);
    lp *= 0.49;
    pp *= 0.49;
    vec2 uv_min = vec2(pp, lp);
    vec2 uv_max = vec2(1.0 - pp, 1.0 - lp);
    float su = clamp(u, uv_min.x, uv_max.x);
    float sv = clamp(v, uv_min.y, uv_max.y);
    if(flip_v > 0.5)
        sv = clamp(1.0 - v, uv_min.y, uv_max.y);
    if(use_q > 0.5)
    {
        su = floor(su * steps_u) / steps_u;
        sv = floor(sv * steps_v) / steps_v;
    }

    float hist = 0.0;
    if(nhist > 1.5 && speed >= 0.1)
    {
        float frame_off = delay_ms / max(avg_frame_ms, 1.0);
        hist = clamp(floor(frame_off + 0.5), 0.0, nhist - 1.0);
    }

    float str01, zone01;
    smUnpack01(p15, str01, zone01);
    float zone = zone01 * 0.32;
    vec2 tile_scale = vec2(1.0 / max(nmon, 1.0), 1.0 / max(nhist, 1.0));
    vec2 tile_origin = vec2(mon, hist) * tile_scale;
    rgb = smCornerSample(vec2(su, sv), tile_origin, tile_scale, uv_min, uv_max, str01, zone);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    vec3 span_mm = vec3(u_params[0], u_params[1], u_params[2]);
    float L = floor(u_params[3] + 0.5);
    float flip1 = step(1999.5, L);
    L -= flip1 * 2000.0;
    float flip0 = step(999.5, L);
    L -= flip0 * 1000.0;
    float use_q = step(99.5, L);
    L -= use_q * 100.0;
    float nhist = floor(L / 10.0);
    float nmon = max(L - nhist * 10.0, 1.0);
    nhist = max(nhist, 1.0);
    float su01, sv01;
    smUnpack01(u_params[4], su01, sv01);
    float steps_u = max(su01 * 2048.0, 2.0);
    float steps_v = max(sv01 * 2048.0, 2.0);
    float avg_frame_ms = max(u_params[5], 1.0);

    vec3 rgb0 = vec3(0.0);
    float w0 = 0.0;
    smEvalMonitor(p01, span_mm, nmon, nhist, use_q, flip0, 0.0, steps_u, steps_v, avg_frame_ms,
                  u_params[6], u_params[7], u_params[8], u_params[9], u_params[10], u_params[11],
                  u_params[12], u_params[13], u_params[14], u_params[15], u_params[16], u_params[17],
                  u_params[18], u_params[19], u_params[20], u_params[21], u_params[22], u_params[23],
                  u_params[24], u_params[25], u_params[26],
                  rgb0, w0);

    vec3 rgb1 = vec3(0.0);
    float w1 = 0.0;
    if(nmon > 1.5)
    {
        smEvalMonitor(p01, span_mm, nmon, nhist, use_q, flip1, 1.0, steps_u, steps_v, avg_frame_ms,
                      u_params[27], u_params[28], u_params[29], u_params[30], u_params[31], u_params[32],
                      u_params[33], u_params[34], u_params[35], u_params[36], u_params[37], u_params[38],
                      u_params[39], u_params[40], u_params[41], u_params[42], u_params[43], u_params[44],
                      u_params[45], u_params[46], u_params[47],
                      rgb1, w1);
    }

    vec3 rgb = vec3(0.0);
    if(w0 <= 0.01 && w1 <= 0.01)
    {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    if(w1 <= 0.01)
        rgb = rgb0;
    else if(w0 <= 0.01)
        rgb = rgb1;
    else
    {
        float a0, b0, a1, blend1;
        smUnpack01(u_params[26], a0, b0);
        smUnpack01(u_params[47], a1, blend1);
        float blend = 0.5 * (b0 + blend1);
        float aw0 = w0 * (0.5 + 0.5 * blend);
        float aw1 = w1 * (0.5 + 0.5 * blend);
        if(blend < 0.01)
        {
            rgb = (w0 >= w1) ? rgb0 : rgb1;
        }
        else
        {
            float tw = aw0 + aw1;
            rgb = (aw0 * rgb0 + aw1 * rgb1) / max(tw, 1e-6);
            float best_w = max(aw0, aw1);
            if(best_w / max(tw, 1e-6) >= 0.50)
                rgb = (aw0 >= aw1) ? rgb0 : rgb1;
            else
            {
                float maxc = max(rgb.r, max(rgb.g, rgb.b));
                float minc = min(rgb.r, min(rgb.g, rgb.b));
                if(maxc > 0.004)
                {
                    float sat_mix = (maxc - minc) / maxc;
                    if(sat_mix < 0.30)
                    {
                        float gray = (rgb.r + rgb.g + rgb.b) / 3.0;
                        float k = min(1.0 + (0.30 - sat_mix) * 1.15, 1.48);
                        rgb = clamp(gray + (rgb - vec3(gray)) * k, 0.0, 1.0);
                    }
                }
            }
        }
    }
    out_color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";
}
