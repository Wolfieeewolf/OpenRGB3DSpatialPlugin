// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Space / Starfield volume: R=intensity, G=palette01, B=hotness.
 *  Sample coords are origin-local (Spatial Anchor at 0.5); pair with
 *  SampleGpuVolumeOriginLocal01. Size scales features so ~1.0 fills the
 *  occupancy box; Scale sets that box. fill widens / narrows FOV.
 *  u_params: [0]=progress [1]=time [2]=mode [3]=count
 *            [4]=thickness [5]=size_m [6]=fill [7]=drift
 *            [8]=twinkle [9]=hue_scroll
 *  mode: 0 Stars, 1 Twinkle, 2 Warp, 3 Hyperdrive, 4 Blackhole, 5 Wormhole,
 *        6 Point Tunnel, 7 Fibonacci Sphere, 8 Crystal, 9 Plasma Globe
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
    float ss = max(s, 0.014);
    return exp(-(d * d) / (ss * ss));
}
/* Sharp star core + tiny halo (reads as a star, not a blob). */
float sfPoint(float d, float soft)
{
    float s = max(soft, 0.016);
    float core = exp(-(d * d) / (s * s * 0.28));
    float halo = exp(-(d * d) / (s * s * 1.45)) * 0.32;
    return clamp(core + halo, 0.0, 1.0);
}
/* Diffraction cross for twinkle / sparkle. */
float sfCross(vec3 dlt, float soft, float rot)
{
    float s = max(soft, 0.016);
    float c = cos(rot);
    float sn = sin(rot);
    vec3 q = vec3(c * dlt.x + sn * dlt.z, dlt.y, -sn * dlt.x + c * dlt.z);
    float arm_w = s * 0.32;
    float arm_l = s * 2.4;
    float plus = max(
        exp(-(q.x * q.x) / (arm_w * arm_w)) * exp(-(q.y * q.y + q.z * q.z) / (arm_l * arm_l)),
        exp(-(q.y * q.y) / (arm_w * arm_w)) * exp(-(q.x * q.x + q.z * q.z) / (arm_l * arm_l)));
    float c45 = 0.70710678;
    vec3 r = vec3(c45 * (q.x - q.z), q.y, c45 * (q.x + q.z));
    float xarm = max(
        exp(-(r.x * r.x) / (arm_w * arm_w)) * exp(-(r.y * r.y + r.z * r.z) / (arm_l * arm_l)),
        exp(-(r.z * r.z) / (arm_w * arm_w)) * exp(-(r.x * r.x + r.y * r.y) / (arm_l * arm_l)));
    return clamp(max(plus, xarm), 0.0, 1.0);
}
/* Motion streak along dir. */
float sfStreak(vec3 dlt, vec3 dir, float soft_along, float soft_side)
{
    float dn = length(dir);
    vec3 u = (dn > 1e-4) ? (dir / dn) : vec3(0.0, 0.0, 1.0);
    float along = dot(dlt, u);
    float side = length(dlt - u * along);
    float a = max(soft_along, 0.020);
    float b = max(soft_side, 0.012);
    float head = exp(-(along * along) / (a * a * 0.55));
    float trail = exp(-(along * along) / (a * a * 2.8)) * step(along, 0.0);
    return clamp(max(head, trail * 0.55) * exp(-(side * side) / (b * b)), 0.0, 1.0);
}
vec2 tunnelPath(float x, float sway)
{
    float TAU = 6.2831853;
    vec2 offs = vec2(0.0);
    offs.x = 0.2 * sin(TAU * x * 0.5) + 0.4 * sin(TAU * x * 0.2 + 0.3);
    offs.y = 0.3 * cos(TAU * x * 0.3) + 0.2 * cos(TAU * x * 0.1);
    offs *= smoothstep(1.0, 4.0, abs(x) + 1.0);
    offs *= (0.2 + 0.8 * clamp(sway, 0.0, 1.0));
    return offs;
}
vec3 sfRotX(vec3 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}
vec3 sfRotY(vec3 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}
vec3 sfRotZ(vec3 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress = u_params[0];
    float time_sec = u_params[1];
    int mode = int(clamp(u_params[2], 0.0, 9.0) + 0.5);
    int count = int(clamp(u_params[3], 8.0, 48.0) + 0.5);
    float thickness = max(u_params[4], 0.02);
    float size_m = clamp(u_params[5], 0.35, 2.5);
    float fill = clamp(u_params[6], 0.4, 1.0);
    float drift = clamp(u_params[7], 0.0, 1.0);
    float twinkle = clamp(u_params[8], 0.0, 1.0);
    float hue_scroll = u_params[9];

    /* Occupancy UV → room box [-1,1]. fill zooms FOV (1 = full room). */
    float fov = 2.0 / max(fill, 0.4);
    vec3 led = (p01 - vec3(0.5)) * fov;
    float sway = drift * 0.22;
    float sway_c = cos(time_sec * 0.35 * max(sway, 0.02) * 6.0 + 0.4);
    float sway_s = sin(time_sec * 0.28 * max(sway, 0.02) * 6.0);
    float led_vx = led.x * (1.0 + sway * 0.08 * sway_c) - led.y * sway * 0.12 * sway_s;
    float led_vy = led.y * (1.0 + sway * 0.08 * sway_c) + led.x * sway * 0.12 * sway_s;
    float led_vz = led.z;
    vec3 led_p = vec3(led_vx, led_vy, led_vz);
    float radial = length(vec2(led_vx, led_vy));
    float ang = atan(led_vy, led_vx);
    float voxel = 2.0 / max(u_res, 8.0);
    /* Feature span: Size ~1 reaches near the occupancy walls. */
    float span = 0.96 * (0.72 + 0.48 * size_m);

    float intensity = 0.0;
    float palette01 = 0.5;
    float hotness = 0.0;

    if(mode == 4)
    {
        /* Blackhole — room-scaled accretion disk + event horizon + photon ring. */
        float r_xy = max(radial, 1e-4);
        float horizon = 0.16 * (0.85 + 0.55 * size_m);
        float disk_outer = span * 1.02;
        float disk_thick = max(0.055, thickness * 0.70 * size_m);
        float in_horizon = step(r_xy, horizon) * step(-0.35 * size_m, led_vz) * step(led_vz, 0.35 * size_m);

        float plane = exp(-(led_vz * led_vz) / (disk_thick * disk_thick * 3.2));
        float disk = plane * smoothstep(horizon * 1.05, horizon * 1.55, r_xy)
                           * (1.0 - smoothstep(disk_outer * 0.82, disk_outer, r_xy));
        float spiral = 0.5 + 0.5 * sin(5.0 * ang - log(r_xy + 0.02) * 4.2 - progress * 4.0);
        float arms = 0.5 + 0.5 * sin(2.0 * ang - log(r_xy + 0.02) * 2.1 - progress * 1.8);
        float photon = exp(-pow((r_xy - horizon * 1.28) / max(0.035 + 0.04 * thickness, 0.02), 2.0));
        float lens = exp(-pow((r_xy - horizon) / 0.10, 2.0)) * 0.55;
        /* Soft polar jets for a fuller 3D read. */
        float jet = exp(-(r_xy * r_xy) / max(0.04 * size_m, 0.02))
                  * exp(-pow(abs(led_vz) - disk_outer * 0.55, 2.0) / max(0.18, 0.08 * size_m))
                  * 0.35;

        intensity = disk * (0.25 + 0.50 * spiral + 0.25 * arms) + photon * 1.55 + lens + jet;
        intensity *= 1.0 - in_horizon;
        /* Gentle room edge falloff — keep disk readable to the walls. */
        intensity *= 1.0 - smoothstep(1.15, 1.55, length(vec2(led_vx, led_vy)) / max(fov * 0.5, 1.0));
        palette01 = fract(1.0 - r_xy / max(disk_outer, 0.1) + hue_scroll);
        hotness = saturate(1.0 - (r_xy - horizon) / max(disk_outer * 0.55, 0.2)) * 0.60 + photon * 0.45;
    }
    else if(mode == 5)
    {
        /* Wormhole — large tunnel through the room with rings + helix. */
        float depth = saturate(led_vz * 0.5 + 0.5);
        float tunnel_r = 0.58 * (0.75 + 0.55 * size_m);
        float wall_w = max(0.045, thickness * 0.62 * size_m);
        float wall = exp(-pow((radial - tunnel_r) / wall_w, 2.0));
        float rings = 0.5 + 0.5 * cos((depth * 6.5 + progress * 1.1) * 6.2831853);
        float helix = 0.5 + 0.5 * cos(7.0 * ang + depth * (6.0 + drift * 5.0) - progress * 7.5);
        float perspective = 0.35 + 0.65 * (1.0 - depth * 0.85);
        float core_glow = (1.0 - smoothstep(0.0, tunnel_r * 0.85, radial)) * 0.14
                          * (0.40 + 0.60 * rings) * perspective;
        /* Mouth glow at near/far ends so the tunnel fills Z. */
        float mouth = exp(-pow(abs(led_vz) - 0.92 * span / max(fov * 0.5, 1.0), 2.0) / 0.12)
                    * smoothstep(tunnel_r * 0.7, tunnel_r * 1.15, radial) * 0.45;

        intensity = wall * (0.30 + 0.38 * rings + 0.50 * helix) * perspective + core_glow + mouth;
        if(radial > tunnel_r + wall_w * 3.5)
            intensity *= 0.0;
        intensity *= 1.30;
        palette01 = fract(depth + ang * 0.08 + hue_scroll);
        hotness = depth * 0.40 + mouth * 0.35;
    }
    else if(mode == 6)
    {
        /* Point Tunnel — Second Reality style ring dots, room-scaled. */
        float TAU = 6.2831853;
        float cam_z = progress * 2.6;
        float n_pts = max(float(count), 12.0);
        float rep = TAU / n_pts;
        float point_sz = max(0.034, max(thickness * 0.48 * size_m, voxel));
        float z_sig = max(0.14, point_sz * 3.8 + 0.10 * size_m);
        float best = 0.0;
        float best_shade = 0.0;
        float spin = cam_z * 0.35;
        for(int i = 1; i <= 24; i++)
        {
            float fi = float(i);
            float layers = 24.0;
            float pz = 1.0 - (fi / layers);
            pz -= mod(cam_z, 4.0 / layers);
            float pz_draw = pz;
            if(pz_draw < 0.05)
                pz_draw += 4.0 / layers;
            if(pz_draw > 0.03 && pz_draw < 1.2)
            {
                float path_t = cam_z + pz_draw;
                vec2 cam_offs = tunnelPath(cam_z, drift);
                vec2 offs = tunnelPath(path_t, drift) - cam_offs;
                float den = pz_draw * 0.8 + 0.4;
                float ring_rad = 0.22 * (1.0 / (den * den)) * (0.80 + 0.70 * size_m);
                float ring_z = pz_draw * 2.0 - 1.0;
                vec2 p = vec2(led_vx, led_vy) + offs;
                float pa = atan(p.y, p.x) + spin;
                float slot = floor(pa / rep + 0.5);
                float a0 = slot * rep;
                vec2 nearest = vec2(cos(a0), sin(a0)) * ring_rad;
                float pdist = length(p - nearest);
                float zdist = abs(led_vz - ring_z);
                float shade = clamp(1.0 - pz_draw, 0.05, 1.0);
                float glow = softband(pdist, point_sz) * softband(zdist, z_sig);
                float stripe = (mod(floor(fi * 0.5), 2.0) < 0.5) ? 1.0 : 0.55;
                float contrib = glow * shade * stripe;
                if(contrib > best)
                {
                    best = contrib;
                    best_shade = shade;
                }
            }
        }
        intensity = clamp(best * 1.90, 0.0, 1.0);
        palette01 = fract(best_shade + hue_scroll);
        hotness = clamp(best_shade * best_shade * 0.85, 0.0, 1.0);
        if(twinkle > 0.01)
        {
            float shimmer = 0.82 + 0.18 * sin(time_sec * (2.5 + twinkle * 5.0) + ang * 8.0 + cam_z);
            intensity *= mix(1.0, shimmer, twinkle);
        }
    }
    else if(mode == 7)
    {
        /* Fibonacci Sphere — room-filling rotating shell of points. */
        float golden = 2.39996323;
        float n = max(float(count), 12.0);
        float rad = span;
        float point_sz = max(0.036, max(thickness * 0.48 * size_m, voxel));
        float tumble = 0.35 + 1.4 * drift;
        float ax = progress * tumble * 0.85;
        float ay = progress * tumble * 1.15 + time_sec * 0.15 * tumble;
        float best = 0.0;
        float best_y = 0.0;
        for(int i = 0; i < 48; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(float(count), fi);
            float y = 1.0 - (fi + 0.5) / n * 2.0;
            float rr = sqrt(max(0.0, 1.0 - y * y));
            float th = fi * golden;
            vec3 pt = vec3(cos(th) * rr, y, sin(th) * rr) * rad;
            pt = sfRotY(sfRotX(pt, ax), ay);
            float d = length(led_p - pt);
            float glow = softband(d, point_sz) * slot_on;
            if(glow > best)
            {
                best = glow;
                best_y = y * 0.5 + 0.5;
            }
        }
        intensity = clamp(best * 1.80, 0.0, 1.0);
        palette01 = fract(best_y + hue_scroll);
        hotness = clamp(best * 0.55, 0.0, 1.0);
        if(twinkle > 0.01)
            intensity *= 0.85 + 0.15 * sin(time_sec * (3.0 + twinkle * 4.0) + best_y * 9.0);
    }
    else if(mode == 8)
    {
        /* Crystal — room-filling octahedron lattice. */
        float n = max(float(count), 12.0);
        float rad = span;
        float point_sz = max(0.034, max(thickness * 0.46 * size_m, voxel));
        float tumble = 0.4 + 1.5 * drift;
        float ax = progress * tumble * 1.05;
        float ay = progress * tumble * 0.75;
        float az = progress * tumble * 0.55;
        float best = 0.0;
        float best_h = 0.0;
        for(int i = 0; i < 48; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(float(count), fi);
            float y = 1.0 - (fi + 0.5) / n * 2.0;
            float ring = max(0.08, 1.0 - abs(y));
            float th = fi * 2.39996323 + floor(fi * 0.25) * 0.785398;
            vec3 pt = normalize(vec3(cos(th) * ring, y, sin(th) * ring)) * rad;
            float l1 = abs(pt.x) + abs(pt.y) + abs(pt.z);
            pt *= (rad * 1.05) / max(l1, 1e-3);
            pt = sfRotZ(sfRotY(sfRotX(pt, ax), ay), az);
            float d = length(led_p - pt);
            float glow = softband(d, point_sz) * slot_on;
            float edge = softband(d, point_sz * 2.2) * 0.22 * slot_on;
            float contrib = max(glow, edge);
            if(contrib > best)
            {
                best = contrib;
                best_h = fract(fi / n + abs(y) * 0.35);
            }
        }
        intensity = clamp(best * 1.75, 0.0, 1.0);
        palette01 = fract(best_h + hue_scroll);
        hotness = clamp(best * 0.5, 0.0, 1.0);
    }
    else if(mode == 9)
    {
        /* Plasma Globe — room-filling glass shell + arcs. */
        float R = span;
        float rr = length(led_p);
        float shell_w = max(0.042, max(thickness * 0.48 * size_m, voxel));
        float shell = softband(rr - R, shell_w);
        float glass = pow(1.0 - saturate(abs(rr - R) / max(R * 0.18, 0.04)), 2.2) * 0.50;
        float core = exp(-(rr * rr) / max(0.05 * size_m, 0.025)) * 0.22;
        int arc_n = int(clamp(float(count) * 0.35, 4.0, 14.0) + 0.5);
        float arcs = 0.0;
        float hot_arc = 0.0;
        float spin = progress * (0.5 + drift);
        for(int i = 0; i < 14; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(float(arc_n), fi);
            vec3 ax = normalize(vec3(
                hash_signed(fi * 3.1 + 1.0),
                hash_signed(fi * 5.7 + 2.0),
                hash_signed(fi * 7.3 + 3.0)));
            ax = sfRotY(sfRotX(ax, spin * 0.7 + fi * 0.4), spin * 1.1);
            vec3 pdir = led_p / max(rr, 1e-3);
            float eq = abs(dot(pdir, ax));
            float bolt_w = 0.050 + 0.055 * thickness;
            float on_shell = softband(rr - R, shell_w * 1.6);
            float pulse = 0.55 + 0.45 * sin(time_sec * (2.2 + twinkle * 3.5) + fi * 2.3 + progress * 4.0);
            float jag = 0.5 + 0.5 * sin(dot(pdir, ax.yzx) * 14.0 + time_sec * 5.0 + fi);
            float bolt = softband(eq, bolt_w) * on_shell * pulse * (0.45 + 0.55 * jag) * slot_on;
            arcs = max(arcs, bolt);
            hot_arc = max(hot_arc, bolt * pulse);
        }
        intensity = clamp(glass * 0.55 + shell * 0.25 + arcs * 1.65 + core, 0.0, 1.0);
        palette01 = fract(rr / max(R, 0.1) + hue_scroll + arcs * 0.15);
        hotness = clamp(hot_arc * 0.9 + core * 0.5, 0.0, 1.0);
    }
    else if(mode == 0 || mode == 1)
    {
        /* Stars / Twinkle — volumetric points filling the whole occupancy box. */
        float soft = max(0.028, max(thickness * 0.55 * size_m, voxel));
        float travel = progress * ((mode == 0) ? 0.45 : 0.08);
        float best = 0.0;
        float best_h = 0.0;
        float best_hot = 0.0;
        for(int i = 0; i < 48; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(float(count), fi);
            float hx = hash11(fi * 12.9898 + 11.0);
            float hy = hash11(fi * 78.233 + 22.0);
            float hz = hash11(fi * 45.164 + 33.0);
            vec3 c = vec3(hx, hy, hz) * 2.0 - 1.0;
            c *= span;
            /* Gentle fly-through along Z (Stars); Twinkle barely drifts. */
            float z01 = fract(hz - travel + drift * 0.02 * sin(time_sec + fi));
            c.z = (z01 * 2.0 - 1.0) * span;
            /* Mild parallax sway. */
            c.x += sin(time_sec * 0.35 + fi) * sway * 0.08 * span;
            c.y += cos(time_sec * 0.28 + fi * 1.3) * sway * 0.08 * span;

            vec3 dlt = led_p - c;
            float d = length(dlt);
            float glow = sfPoint(d, soft);
            float mag = 0.55 + 0.45 * hash11(fi * 3.7 + 40.0);
            float bright = mag;

            if(mode == 1)
            {
                float rate = 1.2 + twinkle * 5.5 + hash11(fi + 35.0) * 3.0;
                float ph = time_sec * rate * 0.85 + hz * 6.2831853;
                float flash = pow(max(0.0, sin(ph)), 10.0);
                bright = 0.10 + 0.90 * flash;
                float crossv = sfCross(dlt, soft * 0.95, fi * 1.7) * flash;
                glow = max(glow * (0.35 + 0.65 * flash), crossv * 1.15);
            }
            else if(twinkle > 0.01)
            {
                float ph = time_sec * (1.4 + twinkle * 2.8) + fi;
                bright *= 0.70 + 0.30 * pow(max(0.0, sin(ph)), 5.0);
            }

            float contrib = glow * bright * slot_on;
            if(contrib > best)
            {
                best = contrib;
                best_h = fract(hx * 0.7 + hy * 0.3 + hue_scroll);
                best_hot = (mode == 1) ? saturate((bright - 0.45) * 1.3) : (1.0 - z01) * 0.35;
            }
        }
        intensity = clamp(best * 1.85, 0.0, 1.0);
        palette01 = best_h;
        hotness = best_hot;
    }
    else
    {
        /* Warp / Hyperdrive — fly-through with long radial streaks. */
        float soft = max(0.026, max(thickness * 0.50 * size_m, voxel));
        float travel = progress * ((mode == 3) ? 1.55 : 0.95);
        float streak_mul = (mode == 3) ? (12.0 + 14.0 * size_m) : (7.0 + 9.0 * size_m);
        float best = 0.0;
        float best_h = 0.0;
        float best_hot = 0.0;
        for(int i = 0; i < 48; i++)
        {
            float fi = float(i);
            float slot_on = 1.0 - step(float(count), fi);
            float dir_x = hash_signed(fi * 12.9898 + 31.0);
            float dir_y = hash_signed(fi * 78.233 + 32.0);
            float dir_len = length(vec2(dir_x, dir_y)) + 1e-4;
            float ux = dir_x / dir_len;
            float uy = dir_y / dir_len;
            float aim = 0.20 + 1.05 * hash11(fi * 45.1 + 33.0);
            float seed_d = hash11(fi * 91.7 + 34.0);

            float depth = fract(seed_d - travel);
            float nearness = 1.0 - depth;
            float persp = 1.0 / max(0.05, 0.06 + depth * 0.94);
            float px = ux * aim * persp * span * 0.95;
            float py = uy * aim * persp * span * 0.95;
            float pz = (depth * 2.0 - 1.0) * span;

            vec3 c = vec3(px, py, pz);
            vec3 dlt = led_p - c;
            /* Streak points slightly toward vanishing point + into depth. */
            vec3 motion = normalize(vec3(px * 0.15, py * 0.15, -1.0) + vec3(ux, uy, 0.0) * 0.05);
            float along_s = soft * (1.0 + nearness * streak_mul * 0.12);
            float side_s = soft * ((mode == 3) ? 0.55 : 0.70);
            float streak = sfStreak(dlt, motion, along_s, side_s);
            float core = sfPoint(length(dlt), soft * 0.85);
            float bright = 0.22 + 0.95 * nearness;
            if(mode == 3)
                bright = 0.18 + 1.15 * nearness;

            float contrib = max(streak, core * 0.85) * bright * slot_on;
            /* Soft FOV gate — keep edges lit for room fill. */
            contrib *= 1.0 - smoothstep(1.35, 1.85, length(vec2(led_vx, led_vy)) / max(span, 0.5));

            if(contrib > best)
            {
                best = contrib;
                best_h = fract(nearness * 0.65 + hue_scroll + fi * 0.02);
                best_hot = saturate(nearness * ((mode == 3) ? 0.85 : 0.50));
            }
        }
        intensity = clamp(best * ((mode == 3) ? 1.95 : 1.75), 0.0, 1.0);
        palette01 = best_h;
        hotness = best_hot;
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), fract(palette01), clamp(hotness, 0.0, 1.0), 1.0);
}
)";
}
