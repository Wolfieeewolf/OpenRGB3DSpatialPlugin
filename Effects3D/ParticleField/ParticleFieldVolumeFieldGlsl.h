// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Particle Field volume: R=intensity, G=hue01.
 *  p01 is origin-local occupancy UV (Spatial Anchor at 0.5); pair with
 *  SampleGpuVolumeOriginLocal01. Do not clamp particles onto atlas faces.
 *  u_params: [0]=motion_clock [1]=mode [2]=count [3]=size01 [4]=thick01
 *            [5]=motion [6]=noise [7]=fill [8]=hue_scroll01 [9]=freq_n
 *  modes: 0 float, 1 snow, 2 embers, 3 sparkle, 4 attract, 5 rain, 6 fireworks
 *  motion_clock is unwrapped Speed cycles (0 freezes).
 *
 *  Per-mode shape kernels (flake / streak / star / wisp) stay distinct, but
 *  softness is floored to ~2 voxels so sparse LED grids still light up.
 */
inline const char* ParticleFieldVolumeFieldGlsl()
{
    return R"(
float pfHash(float seed, float salt)
{
    return fract(sin(seed * 12.9898 + salt * 78.233) * 43758.5453);
}
float pfSmstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
vec3 pfWrap01(vec3 p)
{
    return p - floor(p);
}
vec3 pfDelta01(vec3 a, vec3 b)
{
    vec3 d = a - b;
    d.x = d.x - floor(d.x + 0.5);
    d.y = d.y - floor(d.y + 0.5);
    d.z = d.z - floor(d.z + 0.5);
    return d;
}
vec3 pfDrift(vec3 p, float clock)
{
    vec3 d1 = vec3(
        sin(p.y * 4.2 + clock * 1.05 + 0.2),
        sin(p.z * 3.8 + clock * 0.88 + 1.1),
        sin(p.x * 4.0 + clock * 0.96 + 2.0));
    vec3 d2 = vec3(
        sin(p.z * 7.1 + clock * 0.42 + 0.7),
        sin(p.x * 6.6 + clock * 0.37 + 1.9),
        sin(p.y * 6.8 + clock * 0.40 + 0.3));
    return d1 * 0.65 + d2 * 0.35;
}
vec3 pfSafeDir(vec3 v, vec3 fallback)
{
    float dn = length(v);
    return (dn > 1e-4) ? (v / dn) : fallback;
}
/* Bright core + soft halo sized for LED occupancy sampling. */
float pfPoint(float d, float soft)
{
    float s = max(soft, 0.018);
    float core = exp(-(d * d) / (s * s * 0.35));
    float halo = exp(-(d * d) / (s * s * 1.55)) * 0.40;
    return clamp(core + halo, 0.0, 1.0);
}
/* Velocity / gravity streak: long along dir, readable sideways. */
float pfStreak(vec3 dlt, vec3 dir, float soft_along, float soft_side)
{
    vec3 u = pfSafeDir(dir, vec3(0.0, 1.0, 0.0));
    float along = dot(dlt, u);
    float side = length(dlt - u * along);
    float a = max(soft_along, 0.022);
    float b = max(soft_side, 0.014);
    float head = exp(-(along * along) / (a * a * 0.65));
    float trail = exp(-(along * along) / (a * a * 2.6)) * step(along, 0.0);
    float shaft = max(head, trail * 0.60);
    float rib = exp(-(side * side) / (b * b));
    return clamp(shaft * rib, 0.0, 1.0);
}
/* Snowflake: flat disc + rotated cross arms. */
float pfFlake(vec3 dlt, float soft, float rot)
{
    float s = max(soft, 0.022);
    float d = length(dlt);
    float disc = exp(-(d * d) / (s * s * 1.55)) * 0.55;
    float c = cos(rot);
    float sn = sin(rot);
    vec2 q = vec2(c * dlt.x + sn * dlt.z, -sn * dlt.x + c * dlt.z);
    float arm_w = s * 0.42;
    float arm_l = s * 1.45;
    float a0 = exp(-(q.x * q.x) / (arm_w * arm_w)) * exp(-(q.y * q.y) / (arm_l * arm_l));
    float a1 = exp(-(q.y * q.y) / (arm_w * arm_w)) * exp(-(q.x * q.x) / (arm_l * arm_l));
    float c2 = cos(rot + 1.04719755);
    float s2 = sin(rot + 1.04719755);
    vec2 q2 = vec2(c2 * dlt.x + s2 * dlt.z, -s2 * dlt.x + c2 * dlt.z);
    float a2 = exp(-(q2.x * q2.x) / (arm_w * arm_w)) * exp(-(q2.y * q2.y) / (arm_l * arm_l));
    float flat = exp(-(dlt.y * dlt.y) / (s * s * 0.70));
    return clamp((max(a0, max(a1, a2)) * 1.05 + disc) * flat, 0.0, 1.0);
}
/* Spark / diffraction star (plus + X). */
float pfStar(vec3 dlt, float soft, float rot)
{
    float s = max(soft, 0.018);
    float d = length(dlt);
    float core = exp(-(d * d) / (s * s * 0.28));
    float c = cos(rot);
    float sn = sin(rot);
    vec3 q = vec3(c * dlt.x + sn * dlt.z, dlt.y, -sn * dlt.x + c * dlt.z);
    float arm_w = s * 0.36;
    float arm_l = s * 1.85;
    float plus = max(
        exp(-(q.x * q.x) / (arm_w * arm_w)) * exp(-(q.y * q.y + q.z * q.z) / (arm_l * arm_l)),
        exp(-(q.y * q.y) / (arm_w * arm_w)) * exp(-(q.x * q.x + q.z * q.z) / (arm_l * arm_l)));
    float c45 = 0.70710678;
    vec3 r = vec3(c45 * (q.x - q.z), q.y, c45 * (q.x + q.z));
    float crossv = max(
        exp(-(r.x * r.x) / (arm_w * arm_w)) * exp(-(r.y * r.y + r.z * r.z) / (arm_l * arm_l)),
        exp(-(r.z * r.z) / (arm_w * arm_w)) * exp(-(r.x * r.x + r.y * r.y) / (arm_l * arm_l)));
    float halo = exp(-(d * d) / (s * s * 1.4)) * 0.30;
    return clamp(core + halo + max(plus, crossv) * 0.90, 0.0, 1.0);
}
/* Soft drifting wisp — elongated, still fills space. */
float pfWisp(vec3 dlt, vec3 axis, float soft)
{
    float s = max(soft, 0.024);
    vec3 u = pfSafeDir(axis, vec3(0.2, 1.0, 0.1));
    float along = abs(dot(dlt, u));
    float side = length(dlt - u * dot(dlt, u));
    float body = exp(-(along * along) / (s * s * 1.85) - (side * side) / (s * s * 0.75));
    return clamp(body, 0.0, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float clock = u_params[0];
    int mode = int(clamp(u_params[1], 0.0, 6.0) + 0.5);
    float count_f = clamp(u_params[2], 4.0, 48.0);
    float size01 = clamp(u_params[3], 0.025, 0.30);
    float thick = clamp(u_params[4], 0.018, 0.22);
    float motion = clamp(u_params[5], 0.05, 2.5);
    float noise_amt = clamp(u_params[6], 0.0, 1.5);
    float fill = clamp(u_params[7], 0.35, 1.6);
    float hue_scroll = fract(u_params[8]);
    float freq_n = clamp(u_params[9], 0.0, 1.0);

    float intensity = 0.0;
    float hue01 = hue_scroll;
    float golden = 2.39996323;
    float span = mix(0.28, 0.92, clamp((fill - 0.35) / 0.65, 0.0, 1.0));
    /* Never thinner than ~2 atlas voxels — LED sampling needs this. */
    float voxel = 2.0 / max(u_res, 8.0);

    for(int i = 0; i < 48; i++)
    {
        float slot_on = step(float(i) + 0.5, count_f);
        float fi = float(i);
        float seed = fi * 265.443 + 101.390;
        float ring = sqrt((fi + 0.5) / max(count_f, 1.0));
        float ang = fi * golden;

        vec3 base;
        base.x = 0.5 + cos(ang) * ring * span * (0.55 + 0.45 * pfHash(seed, 1.0));
        base.y = 0.12 + 0.76 * pfHash(seed, 2.0);
        base.z = 0.5 + sin(ang) * ring * span * (0.55 + 0.45 * pfHash(seed, 3.0));

        float life = 1.0;
        float rad = size01 * (0.55 + 0.45 * pfHash(seed, 4.0));
        vec3 c = base;
        float ph = 0.0;
        float value = 0.0;
        float particle_hue = fract(fi * 0.097 + hue_scroll + 0.15 * pfHash(seed, 20.0));
        int wrap_field = 1;
        int shape = 0;
        vec3 streak_dir = vec3(0.0, 1.0, 0.0);
        float spin = clock * (0.6 + 1.4 * pfHash(seed, 40.0)) + seed;

        if(mode == 0)
        {
            /* Float / Fuzzy — soft wisps. */
            vec3 n = pfDrift(base, clock * (0.55 + 0.45 * motion));
            c = pfWrap01(base + n * (0.10 * noise_amt + 0.045 * motion));
            rad *= 1.25;
            particle_hue = fract(particle_hue + 0.04 * n.x);
            shape = 5;
            streak_dir = pfSafeDir(n + vec3(0.05, 0.35, 0.02), vec3(0.1, 1.0, 0.05));
        }
        else if(mode == 1)
        {
            /* Snow — falling flakes with sway + rotation. */
            float cycle = 2.8 + 2.4 * pfHash(seed, 5.0);
            ph = fract(clock * (0.42 * motion) / cycle + pfHash(seed, 6.0));
            c.y = 1.0 - ph;
            float sway = sin(clock * 1.15 + seed) * 0.07 * motion;
            c.x = base.x + sway + (pfHash(seed, 7.0) - 0.5) * 0.05 * noise_amt;
            c.z = base.z + cos(clock * 0.92 + seed * 0.7) * 0.06 * motion;
            c = pfWrap01(c);
            rad *= 0.90;
            life = pfSmstep(0.0, 0.10, ph) * (1.0 - pfSmstep(0.90, 1.0, ph));
            particle_hue = fract(0.55 + 0.08 * pfHash(seed, 41.0) + hue_scroll * 0.15);
            shape = 1;
            spin = clock * 1.8 + seed * 2.1;
        }
        else if(mode == 2)
        {
            /* Embers — rising sparks with upward trail + flicker. */
            float cycle = 2.2 + 2.2 * pfHash(seed, 8.0);
            ph = fract(clock * (0.38 * motion) / cycle + pfHash(seed, 9.0));
            c.y = ph;
            float wob_x = sin(clock * 0.95 + seed) * 0.045 * motion;
            float wob_z = cos(clock * 0.82 + seed * 1.3) * 0.045 * motion;
            c.x = base.x + wob_x;
            c.z = base.z + wob_z;
            c = pfWrap01(c);
            float flick = 0.82 + 0.18 * sin(clock * 9.5 + seed * 3.1)
                        * (0.5 + 0.5 * sin(clock * 3.7 + seed));
            life = pfSmstep(0.0, 0.12, ph) * (1.0 - pfSmstep(0.75, 1.0, ph)) * flick;
            rad *= 0.78;
            particle_hue = fract(0.02 + 0.12 * pfHash(seed, 11.0) + 0.08 * (1.0 - ph) + hue_scroll);
            shape = 2;
            streak_dir = pfSafeDir(vec3(wob_x * 0.4, 1.0, wob_z * 0.4), vec3(0.0, 1.0, 0.0));
        }
        else if(mode == 3)
        {
            /* Sparkle — diffraction stars that blink. */
            float cycle = 1.6 + 1.8 * pfHash(seed, 12.0);
            ph = fract(clock * (0.55 * motion) / cycle + pfHash(seed, 13.0));
            float env = pfSmstep(0.0, 0.04, ph) * (1.0 - pfSmstep(0.08, 0.18, ph));
            float alive = step(0.62 - 0.20 * freq_n, pfHash(seed, 14.0));
            life = env * alive;
            c = base;
            c.y = 0.15 + 0.70 * pfHash(seed, 15.0);
            rad *= 0.55;
            wrap_field = 0;
            shape = 3;
            spin = seed * 1.7 + clock * 0.4;
            particle_hue = fract(0.12 + fi * 0.11 + hue_scroll);
        }
        else if(mode == 4)
        {
            /* Attract — points with short orbital trail. */
            float t = clock * (0.70 * motion);
            float pull = 0.28 + 0.50 * pfHash(seed, 16.0);
            vec3 orbit_p;
            orbit_p.x = 0.5 + cos(t + ang) * ring * span * (0.32 + 0.38 * noise_amt);
            orbit_p.y = 0.5 + sin(t * 0.82 + seed) * ring * span * 0.52;
            orbit_p.z = 0.5 + sin(t + ang) * ring * span * (0.32 + 0.38 * noise_amt);
            vec3 hub = vec3(0.5, 0.5, 0.5);
            c = mix(hub, mix(base, orbit_p, 0.72), 0.35 + 0.65 * pull);
            rad *= 0.95;
            wrap_field = 0;
            shape = 2;
            streak_dir = pfSafeDir(vec3(-sin(t + ang), cos(t * 0.82 + seed) * 0.35, cos(t + ang)),
                                   vec3(1.0, 0.0, 0.0));
        }
        else if(mode == 5)
        {
            /* Rain — thin needles (still LED-wide). */
            float cycle = 0.85 + 0.90 * pfHash(seed, 21.0);
            ph = fract(clock * (0.95 * motion) / cycle + pfHash(seed, 22.0));
            c.y = 1.0 - ph;
            float slant = (pfHash(seed, 23.0) - 0.5) * 0.12 * motion;
            c.x = base.x + slant * ph + (pfHash(seed, 24.0) - 0.5) * 0.035 * noise_amt;
            c.z = base.z + slant * ph * 0.6;
            c = pfWrap01(c);
            rad *= 0.48;
            life = pfSmstep(0.0, 0.06, ph) * (1.0 - pfSmstep(0.86, 1.0, ph));
            particle_hue = fract(0.55 + fi * 0.03 + hue_scroll);
            shape = 4;
            streak_dir = pfSafeDir(vec3(slant, -1.0, slant * 0.55), vec3(0.0, -1.0, 0.0));
        }
        else
        {
            /* Fireworks — radial spark streaks from burst. */
            float burst_id = floor(fi / 6.0);
            float slot = mod(fi, 6.0);
            float cycle = 2.0 + 1.6 * pfHash(burst_id * 17.1 + 3.0, 30.0);
            ph = fract(clock * (0.40 * motion) / cycle + pfHash(burst_id, 31.0));
            vec3 origin_b;
            origin_b.x = 0.5 + (pfHash(burst_id, 32.0) - 0.5) * span;
            origin_b.y = 0.38 + 0.28 * pfHash(burst_id, 33.0);
            origin_b.z = 0.5 + (pfHash(burst_id, 34.0) - 0.5) * span;
            float bang = pfSmstep(0.08, 0.20, ph);
            float fade = 1.0 - pfSmstep(0.58, 0.92, ph);
            float boom = bang * fade;
            float ang_b = slot * 1.04719755 + pfHash(burst_id, 35.0) * 6.2831853;
            float elev = (pfHash(seed, 36.0) - 0.35) * 1.2;
            float expand = boom * (0.10 + 0.26 * span) * (0.7 + 0.45 * motion);
            c = origin_b;
            c.x += cos(ang_b) * expand;
            c.z += sin(ang_b) * expand;
            c.y += elev * expand - boom * boom * 0.16 * motion;
            life = boom * (0.65 + 0.35 * step(0.2, pfHash(seed, 37.0)));
            rad *= 0.65 + 0.40 * (1.0 - ph);
            particle_hue = fract(pfHash(burst_id * 17.1, 38.0) * 0.9 + hue_scroll + ph * 0.08);
            wrap_field = 0;
            shape = 2;
            streak_dir = pfSafeDir(c - origin_b + vec3(0.0, 0.05, 0.0), vec3(0.0, 1.0, 0.0));
            if(ph < 0.12)
            {
                c = mix(vec3(origin_b.x, 0.08, origin_b.z), origin_b, ph / 0.12);
                life = pfSmstep(0.0, 0.05, ph) * (1.0 - pfSmstep(0.10, 0.12, ph));
                rad *= 0.55;
                shape = 0;
            }
        }

        vec3 dlt = (wrap_field == 1) ? pfDelta01(p01, c) : (p01 - c);
        float soft = max(thick * (0.55 + 0.55 * rad), voxel);

        if(shape == 1)
            value = pfFlake(dlt, soft * 1.10, spin);
        else if(shape == 2)
            value = pfStreak(dlt, streak_dir, soft * 2.2, soft * 0.55);
        else if(shape == 3)
            value = pfStar(dlt, soft * 1.05, spin);
        else if(shape == 4)
            value = pfStreak(dlt, streak_dir, soft * 2.8, soft * 0.38);
        else if(shape == 5)
            value = pfWisp(dlt, streak_dir, soft * 1.35);
        else
            value = pfPoint(length(dlt), soft * 0.95);

        value *= life * slot_on;
        if(value > intensity)
        {
            intensity = value;
            hue01 = particle_hue;
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), hue01, 0.0, 1.0);
}
)";
}
