// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Particle Field volume: R=intensity, G=hue01.
 *  p01 is origin-local occupancy UV (Spatial Anchor at 0.5); pair with
 *  SampleGpuVolumeOriginLocal01. Do not clamp particles onto atlas faces.
 *  u_params: [0]=motion_clock [1]=mode [2]=count [3]=size01 [4]=thick01
 *            [5]=motion [6]=noise [7]=fill [8]=hue_scroll01 [9]=freq_n
 *  modes: 0 float, 1 snow, 2 embers, 3 sparkle, 4 attract, 5 rain, 6 fireworks
 *  motion_clock is unwrapped Speed cycles (0 freezes).
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
float pfKernel(float d, float soft)
{
    float s = max(soft, 1e-4);
    return exp(-(d * d) / (s * s));
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float clock = u_params[0];
    int mode = int(clamp(u_params[1], 0.0, 6.0) + 0.5);
    float count_f = clamp(u_params[2], 4.0, 48.0);
    float size01 = clamp(u_params[3], 0.025, 0.28);
    float thick = clamp(u_params[4], 0.018, 0.20);
    float motion = clamp(u_params[5], 0.05, 2.5);
    float noise_amt = clamp(u_params[6], 0.0, 1.5);
    float fill = clamp(u_params[7], 0.35, 1.6);
    float hue_scroll = fract(u_params[8]);
    float freq_n = clamp(u_params[9], 0.0, 1.0);

    float intensity = 0.0;
    float hue01 = hue_scroll;
    float golden = 2.39996323;
    float span = mix(0.18, 0.88, clamp((fill - 0.35) / 0.65, 0.0, 1.0));

    for(int i = 0; i < 48; i++)
    {
        float on = step(float(i) + 0.5, count_f);
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
        int use_aniso = 0;
        int wrap_field = 1;

        if(mode == 0)
        {
            vec3 n = pfDrift(base, clock * (0.55 + 0.45 * motion));
            c = pfWrap01(base + n * (0.10 * noise_amt + 0.045 * motion));
            rad *= 1.35;
            particle_hue = fract(particle_hue + 0.04 * n.x);
        }
        else if(mode == 1)
        {
            float cycle = 2.8 + 2.4 * pfHash(seed, 5.0);
            ph = fract(clock * (0.42 * motion) / cycle + pfHash(seed, 6.0));
            c.y = 1.0 - ph;
            float sway = sin(clock * 1.15 + seed) * 0.07 * motion;
            c.x = base.x + sway + (pfHash(seed, 7.0) - 0.5) * 0.05 * noise_amt;
            c.z = base.z + cos(clock * 0.92 + seed * 0.7) * 0.06 * motion;
            c = pfWrap01(c);
            rad *= 0.80;
            life = pfSmstep(0.0, 0.10, ph) * (1.0 - pfSmstep(0.90, 1.0, ph));
        }
        else if(mode == 2)
        {
            float cycle = 2.2 + 2.2 * pfHash(seed, 8.0);
            ph = fract(clock * (0.38 * motion) / cycle + pfHash(seed, 9.0));
            c.y = ph;
            c.x = base.x + sin(clock * 0.95 + seed) * 0.045 * motion;
            c.z = base.z + cos(clock * 0.82 + seed * 1.3) * 0.045 * motion;
            c = pfWrap01(c);
            float flick = 0.86 + 0.14 * sin(clock * 1.35 + seed);
            life = pfSmstep(0.0, 0.12, ph) * (1.0 - pfSmstep(0.78, 1.0, ph)) * flick;
            rad *= 0.95;
            particle_hue = fract(0.04 + 0.10 * pfHash(seed, 11.0) + hue_scroll);
        }
        else if(mode == 3)
        {
            float cycle = 1.6 + 1.8 * pfHash(seed, 12.0);
            ph = fract(clock * (0.55 * motion) / cycle + pfHash(seed, 13.0));
            float env = pfSmstep(0.0, 0.08, ph) * (1.0 - pfSmstep(0.18, 0.36, ph));
            float alive = step(0.58 - 0.20 * freq_n, pfHash(seed, 14.0));
            life = env * alive;
            c = base;
            c.y = 0.15 + 0.70 * pfHash(seed, 15.0);
            rad *= 0.50;
            wrap_field = 0;
        }
        else if(mode == 4)
        {
            float t = clock * (0.70 * motion);
            float pull = 0.28 + 0.50 * pfHash(seed, 16.0);
            vec3 orbit_p;
            orbit_p.x = 0.5 + cos(t + ang) * ring * span * (0.32 + 0.38 * noise_amt);
            orbit_p.y = 0.5 + sin(t * 0.82 + seed) * ring * span * 0.52;
            orbit_p.z = 0.5 + sin(t + ang) * ring * span * (0.32 + 0.38 * noise_amt);
            vec3 hub = vec3(0.5, 0.5, 0.5);
            c = mix(hub, mix(base, orbit_p, 0.72), 0.35 + 0.65 * pull);
            rad *= 1.12;
            wrap_field = 0;
        }
        else if(mode == 5)
        {
            float cycle = 0.85 + 0.90 * pfHash(seed, 21.0);
            ph = fract(clock * (0.95 * motion) / cycle + pfHash(seed, 22.0));
            c.y = 1.0 - ph;
            float slant = (pfHash(seed, 23.0) - 0.5) * 0.10 * motion;
            c.x = base.x + slant * ph + (pfHash(seed, 24.0) - 0.5) * 0.035 * noise_amt;
            c.z = base.z + slant * ph * 0.6;
            c = pfWrap01(c);
            rad *= 0.42;
            life = pfSmstep(0.0, 0.06, ph) * (1.0 - pfSmstep(0.86, 1.0, ph));
            particle_hue = fract(0.55 + fi * 0.03 + hue_scroll);
            use_aniso = 1;
        }
        else
        {
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
            rad *= 0.7 + 0.45 * (1.0 - ph);
            particle_hue = fract(pfHash(burst_id * 17.1, 38.0) * 0.9 + hue_scroll + ph * 0.08);
            wrap_field = 0;
            if(ph < 0.12)
            {
                c = mix(vec3(origin_b.x, 0.08, origin_b.z), origin_b, ph / 0.12);
                life = pfSmstep(0.0, 0.05, ph) * (1.0 - pfSmstep(0.10, 0.12, ph));
                rad *= 0.55;
            }
        }

        if(use_aniso == 1)
        {
            vec3 dlt = pfDelta01(p01, c);
            float dy = abs(dlt.y);
            float dxz = length(dlt.xz);
            float soft_y = thick * 2.4 * (0.7 + 0.3 * rad);
            float soft_xz = thick * 0.55 * (0.7 + 0.3 * rad);
            value = pfKernel(dxz, soft_xz) * pfKernel(dy, soft_y);
        }
        else if(wrap_field == 1)
        {
            float d = length(pfDelta01(p01, c));
            float soft = thick * (0.95 + 0.45 * rad);
            value = pfKernel(d, soft);
        }
        else
        {
            float d = length(p01 - c);
            float soft = thick * (0.95 + 0.45 * rad);
            value = pfKernel(d, soft);
        }
        value *= life * on;
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
