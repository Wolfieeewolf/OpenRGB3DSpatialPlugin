// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Grid Kit — LED-cube style soft volumes for a room grid.
 *  R=intensity, G=color_driver01.
 *  u_params: [0]=mode [1]=progress [2]=thickness [3]=size_scale
 *            [4]=axis(0=X,1=Y,2=Z) [5]=detail [6]=hue_scroll
 *            [7]=sway [8]=density/count [9]=phase2
 *  modes: 0 Plane Sweep, 1 Wireframe Box, 2 Moving Boxes,
 *         3 Rubik Cube, 4 Sphere Roam, 5 Axis Send, 6 Lightning
 *
 *  Rubik G encodes face id as (face+0.5)/6 for a fixed 6-color palette on CPU.
 */
inline const char* GridKitVolumeFieldGlsl()
{
    return R"(
float softband(float d, float w)
{
    float hw = max(w, 0.010);
    float t = 1.0 - smoothstep(0.0, hw, abs(d));
    return t * t;
}
float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
vec3 gkRotY(vec3 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}
vec3 gkRotX(vec3 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}
vec3 gkRotZ(vec3 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}
float boxEdgeGlow(vec3 p, vec3 b, float edge_w)
{
    float near_x = softband(abs(p.x) - b.x, edge_w);
    float near_y = softband(abs(p.y) - b.y, edge_w);
    float near_z = softband(abs(p.z) - b.z, edge_w);
    float inside = step(abs(p.x), b.x + edge_w)
                 * step(abs(p.y), b.y + edge_w)
                 * step(abs(p.z), b.z + edge_w);
    return max(near_x * near_y, max(near_y * near_z, near_z * near_x)) * inside;
}
float facePlane(vec3 p, vec3 b, float edge_w, int face)
{
    float d = 0.0;
    float mask = 1.0;
    if(face == 0)
    {
        d = p.x - b.x;
        mask = step(abs(p.y), b.y + edge_w) * step(abs(p.z), b.z + edge_w);
    }
    else if(face == 1)
    {
        d = -p.x - b.x;
        mask = step(abs(p.y), b.y + edge_w) * step(abs(p.z), b.z + edge_w);
    }
    else if(face == 2)
    {
        d = p.y - b.y;
        mask = step(abs(p.x), b.x + edge_w) * step(abs(p.z), b.z + edge_w);
    }
    else if(face == 3)
    {
        d = -p.y - b.y;
        mask = step(abs(p.x), b.x + edge_w) * step(abs(p.z), b.z + edge_w);
    }
    else if(face == 4)
    {
        d = p.z - b.z;
        mask = step(abs(p.x), b.x + edge_w) * step(abs(p.y), b.y + edge_w);
    }
    else
    {
        d = -p.z - b.z;
        mask = step(abs(p.x), b.x + edge_w) * step(abs(p.y), b.y + edge_w);
    }
    return softband(d, edge_w) * mask;
}
/* Closest face of an AABB (−1 if inside far from surface). */
int nearestFace(vec3 p, vec3 b)
{
    float dxp = abs(p.x - b.x);
    float dxm = abs(p.x + b.x);
    float dyp = abs(p.y - b.y);
    float dym = abs(p.y + b.y);
    float dzp = abs(p.z - b.z);
    float dzm = abs(p.z + b.z);
    float best = dxp;
    int face = 0;
    if(dxm < best) { best = dxm; face = 1; }
    if(dyp < best) { best = dyp; face = 2; }
    if(dym < best) { best = dym; face = 3; }
    if(dzp < best) { best = dzp; face = 4; }
    if(dzm < best) { best = dzm; face = 5; }
    return face;
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    int mode = int(clamp(u_params[0], 0.0, 6.0) + 0.5);
    float progress = fract(u_params[1]);
    float thick = max(u_params[2], 0.015);
    float size_m = clamp(u_params[3], 0.35, 2.5);
    int axis = int(clamp(u_params[4], 0.0, 2.0) + 0.5);
    float detail = clamp(u_params[5], 0.05, 1.0);
    float hue_scroll = fract(u_params[6]);
    float sway = clamp(u_params[7], 0.0, 1.5);
    float density = clamp(u_params[8], 0.15, 1.0);
    float phase2 = fract(u_params[9]);

    vec3 p = (p01 - 0.5) * 2.0;
    float half_w = max(0.018, thick * mix(0.72, 0.32, detail));
    float intensity = 0.0;
    float color_drv = hue_scroll;

    if(mode == 0)
    {
        float tri = 1.0 - abs(2.0 * progress - 1.0);
        float span = 0.95 * clamp(size_m, 0.55, 1.85);
        float center = (tri * 2.0 - 1.0) * span;
        float coord = (axis == 0) ? p.x : ((axis == 1) ? p.y : p.z);
        intensity = softband(coord - center, half_w * 1.85);
        float lat0 = (axis == 0) ? p.y : p.x;
        float lat1 = (axis == 2) ? p.y : p.z;
        color_drv = fract(0.5 + 0.5 * lat0 + 0.25 * lat1 + hue_scroll);
    }
    else if(mode == 1)
    {
        /* Wireframe Box — room-filling edges + faint faces so the cube reads. */
        float box_s = 0.96 * clamp(size_m, 0.55, 1.55);
        vec3 b = vec3(box_s);
        float ew = half_w * 2.1;
        float edges = boxEdgeGlow(p, b, ew);
        float faces = 0.0;
        for(int f = 0; f < 6; f++)
            faces = max(faces, facePlane(p, b, ew * 1.15, f));
        intensity = max(edges, faces * 0.38);
        float ang = atan(p.z, p.x) / 6.2831853;
        color_drv = fract(ang + 0.5 + hue_scroll + 0.12 * p.y);
    }
    else if(mode == 2)
    {
        float best = 0.0;
        float best_h = 0.0;
        float n_vis = mix(2.0, 4.0, density);
        for(int i = 0; i < 4; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(n_vis, fi);
            float seed = fi * 1.7 + 0.3;
            float ax = progress * 6.2831853 * (0.7 + 0.2 * fi) + seed;
            float ay = progress * 6.2831853 * (0.55 + 0.15 * fi) + phase2 * 6.2831853;
            vec3 c = vec3(
                sin(ax) * (0.55 + 0.22 * sway),
                sin(ay * 1.3 + seed) * (0.42 + 0.18 * sway),
                cos(ax * 0.9 + ay) * (0.55 + 0.22 * sway));
            c *= 0.78 * clamp(size_m, 0.55, 1.65);
            float half_box = mix(0.14, 0.28, 0.5 + 0.5 * hash21(vec2(seed, 2.1)))
                           * clamp(size_m, 0.55, 1.55);
            vec3 d = abs(p - c) - vec3(half_box);
            float box_d = length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
            float glow = softband(box_d, half_w * 1.45) * slot_on;
            best = max(best, glow);
            if(glow >= best - 1e-5)
                best_h = fract(fi * 0.17 + hue_scroll);
        }
        intensity = best;
        color_drv = best_h;
    }
    else if(mode == 3)
    {
        /* Rubik Cube — 3×3 stickers, classic face colors, scramble↔solve forever. */
        float box_s = 0.94 * clamp(size_m, 0.55, 1.45);
        vec3 b = vec3(box_s);
        float cell = max(box_s * 2.0 / 3.0, 0.12);

        /* Ping-pong: 0 = solved, 1 = fully scrambled, then back. */
        float cycle = fract(progress * 0.35 + phase2 * 0.02);
        float scramble = 1.0 - abs(2.0 * cycle - 1.0);
        scramble = smoothstep(0.05, 0.92, scramble);

        /* Continuous slice turns while scrambling / resolving. */
        float turn_clock = progress * 5.0;
        float turn_id = floor(turn_clock);
        float turn_u = fract(turn_clock);
        float turn_ease = smoothstep(0.0, 0.55, turn_u) * (1.0 - smoothstep(0.70, 1.0, turn_u));
        float spin = turn_ease * 1.5707963; /* up to 90° */
        int turn_face = int(mod(turn_id, 6.0));
        float layer = floor(mod(turn_id * 1.7, 3.0)) - 1.0; /* -1,0,1 slice */

        vec3 q = p;
        float layer_coord = q.x;
        if(turn_face == 2 || turn_face == 3)
            layer_coord = q.y;
        else if(turn_face == 4 || turn_face == 5)
            layer_coord = q.z;
        float on_slice = 1.0 - smoothstep(cell * 0.42, cell * 0.62, abs(layer_coord - layer * cell));
        float twist = spin * on_slice * mix(0.25, 1.0, scramble);
        if(turn_face == 0 || turn_face == 1)
            q = mix(q, gkRotX(q, twist * (turn_face == 0 ? 1.0 : -1.0)), on_slice);
        else if(turn_face == 2 || turn_face == 3)
            q = mix(q, gkRotY(q, twist * (turn_face == 2 ? 1.0 : -1.0)), on_slice);
        else
            q = mix(q, gkRotZ(q, twist * (turn_face == 4 ? 1.0 : -1.0)), on_slice);

        /* Surface stickers only (thin shell). */
        vec3 aq = abs(q);
        float m = max(aq.x, max(aq.y, aq.z));
        float shell = softband(m - box_s, half_w * 1.6);
        float in_face = step(abs(q.x), b.x + half_w * 2.0)
                      * step(abs(q.y), b.y + half_w * 2.0)
                      * step(abs(q.z), b.z + half_w * 2.0);

        int face = nearestFace(q, b);
        /* Sticker cell indices on the dominant face. */
        vec2 uv;
        if(face == 0 || face == 1)
            uv = q.yz;
        else if(face == 2 || face == 3)
            uv = q.xz;
        else
            uv = q.xy;
        vec2 cell_i = floor((uv + box_s) / cell);
        cell_i = clamp(cell_i, vec2(0.0), vec2(2.0));

        /* Groove grid between stickers. */
        vec2 local = abs(mod(uv + box_s, cell) - cell * 0.5);
        float groove = softband(min(local.x, local.y), half_w * 0.55);
        float sticker = shell * in_face * (1.0 - groove * 0.85);

        float solved_id = (float(face) + 0.5) / 6.0;
        float scramble_id = (floor(hash21(cell_i + vec2(float(face) * 3.1, scramble * 0.01)) * 6.0) + 0.5) / 6.0;
        color_drv = mix(solved_id, scramble_id, scramble);

        float edges = boxEdgeGlow(q, b, half_w * 1.35);
        intensity = max(sticker * 1.15, edges * 0.75);
    }
    else if(mode == 4)
    {
        /* Sphere Roam — solid soft ball + trail (not a hollow shell). */
        float a = progress * 6.2831853;
        float span = 0.78 * clamp(size_m, 0.55, 1.65);
        vec3 c = vec3(
            sin(a) * 0.72,
            sin(a * 1.3 + phase2 * 6.2831853) * 0.52,
            cos(a * 0.85 + 1.1) * 0.72) * span;
        float rad = mix(0.18, 0.36, 0.35 + 0.65 * density) * clamp(size_m, 0.55, 1.55);
        float d0 = length(p - c);
        intensity = exp(-(d0 * d0) / max(rad * rad * 0.55, 1e-4));
        for(int i = 1; i <= 4; i++)
        {
            float fi = float(i);
            float a2 = a - fi * 0.28 * (0.55 + 0.45 * sway);
            vec3 c2 = vec3(
                sin(a2) * 0.72,
                sin(a2 * 1.3 + phase2 * 6.2831853) * 0.52,
                cos(a2 * 0.85 + 1.1) * 0.72) * span;
            float d2 = length(p - c2);
            float r2 = rad * (1.0 - 0.12 * fi);
            intensity = max(intensity, exp(-(d2 * d2) / max(r2 * r2 * 0.65, 1e-4)) * (1.0 - 0.18 * fi));
        }
        color_drv = fract(progress * 0.5 + hue_scroll);
    }
    else if(mode == 5)
    {
        /* Axis Send — elongated sparks traveling the full axis span. */
        float best = 0.0;
        float best_h = 0.0;
        float n_vis = mix(6.0, 12.0, density);
        float span = 0.95 * clamp(size_m, 0.55, 1.75);
        for(int i = 0; i < 12; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(n_vis, fi);
            float h = hash21(vec2(fi * 0.37, 1.9));
            float h2 = hash21(vec2(fi * 0.91, 4.2));
            float lat0 = (h * 2.0 - 1.0) * 0.82 * span;
            float lat1 = (h2 * 2.0 - 1.0) * 0.82 * span;
            float speed = 0.55 + 0.9 * hash21(vec2(fi, 7.1));
            float pos = (fract(progress * speed + h) * 2.0 - 1.0) * 0.96 * span;
            vec3 c;
            vec3 dir;
            if(axis == 0)
            {
                c = vec3(pos, lat0, lat1);
                dir = vec3(1.0, 0.0, 0.0);
            }
            else if(axis == 1)
            {
                c = vec3(lat0, pos, lat1);
                dir = vec3(0.0, 1.0, 0.0);
            }
            else
            {
                c = vec3(lat0, lat1, pos);
                dir = vec3(0.0, 0.0, 1.0);
            }
            vec3 dlt = p - c;
            float along = abs(dot(dlt, dir));
            float side = length(dlt - dir * dot(dlt, dir));
            float glow = exp(-(along * along) / max(half_w * half_w * 6.5, 1e-4))
                       * exp(-(side * side) / max(half_w * half_w * 1.1, 1e-4));
            glow *= slot_on;
            best = max(best, glow);
            if(glow >= best)
                best_h = fract(h + hue_scroll);
        }
        intensity = best;
        color_drv = best_h;
    }
    else
    {
        /* Lightning — longer visible bolts. */
        float best = 0.0;
        float best_h = 0.0;
        float bolts = mix(2.0, 3.0, density);
        float span = 0.94 * clamp(size_m, 0.55, 1.75);
        for(int b = 0; b < 3; b++)
        {
            float fb = float(b);
            float slot_on = 1.0 - step(bolts, fb);
            float seed = fb * 2.3 + floor(progress * 5.0 + fb);
            float flash = fract(progress * (1.4 + fb * 0.35) + hash21(vec2(seed, 0.2)));
            float alive = (1.0 - smoothstep(0.0, 0.72, flash)) * slot_on;
            vec3 a0 = vec3(
                hash21(vec2(seed, 1.0)) * 2.0 - 1.0,
                hash21(vec2(seed, 2.0)) * 2.0 - 1.0,
                hash21(vec2(seed, 3.0)) * 2.0 - 1.0) * 0.92 * span;
            vec3 a1 = vec3(
                hash21(vec2(seed, 4.0)) * 2.0 - 1.0,
                hash21(vec2(seed, 5.0)) * 2.0 - 1.0,
                hash21(vec2(seed, 6.0)) * 2.0 - 1.0) * 0.92 * span;
            vec3 m0 = mix(a0, a1, 0.33) + vec3(
                hash21(vec2(seed, 7.0)) - 0.5,
                hash21(vec2(seed, 8.0)) - 0.5,
                hash21(vec2(seed, 9.0)) - 0.5) * 0.38 * sway;
            vec3 m1 = mix(a0, a1, 0.66) + vec3(
                hash21(vec2(seed, 10.0)) - 0.5,
                hash21(vec2(seed, 11.0)) - 0.5,
                hash21(vec2(seed, 12.0)) - 0.5) * 0.38 * sway;
            float dline = 1e5;
            for(int s = 0; s < 3; s++)
            {
                vec3 u = (s == 0) ? a0 : ((s == 1) ? m0 : m1);
                vec3 v = (s == 0) ? m0 : ((s == 1) ? m1 : a1);
                vec3 ab = v - u;
                float t = clamp(dot(p - u, ab) / max(dot(ab, ab), 1e-4), 0.0, 1.0);
                dline = min(dline, length(p - (u + ab * t)));
            }
            float glow = softband(dline, half_w * 1.65) * alive;
            best = max(best, glow);
            if(glow >= best)
                best_h = fract(0.55 + fb * 0.08 + hue_scroll);
        }
        intensity = best;
        color_drv = best_h;
    }

    intensity = clamp(intensity * mix(1.10, 1.50, detail * 0.5 + 0.5), 0.0, 1.0);
    out_color = vec4(intensity, fract(color_drv), 0.0, 1.0);
}
)";
}
