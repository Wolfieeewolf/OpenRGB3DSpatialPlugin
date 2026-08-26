// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Particle Field volume: R=intensity, G=hue01.
 *  Room-fill organic particles (WLED PS–inspired looks, not a firmware port).
 *  Sample coords are origin-local (Spatial Anchor at 0.5).
 *  u_params: [0]=time [1]=mode [2]=count [3]=size01 [4]=thick01
 *            [5]=motion [6]=noise [7]=fill [8]=hue_scroll01 [9]=speed
 *  modes: 0 float, 1 snow, 2 embers, 3 sparkle, 4 attract, 5 rain, 6 fireworks
 */
inline const char* ParticleFieldVolumeFieldGlsl()
{
    return R"(
float pfHash(float seed, float salt)
{
    return fract(sin(seed * 12.9898 + salt * 78.233) * 43758.5453);
}
float pfN3(vec3 p)
{
    return pfHash(dot(floor(p), vec3(1.0, 57.0, 113.0)), fract(p.x) + fract(p.y) * 1.7);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float time_sec = u_params[0];
    int mode = int(clamp(u_params[1], 0.0, 6.0) + 0.5);
    float count_f = clamp(u_params[2], 4.0, 48.0);
    float size01 = max(u_params[3], 0.03);
    float thick = max(u_params[4], 0.018);
    float motion = clamp(u_params[5], 0.05, 2.5);
    float noise_amt = clamp(u_params[6], 0.0, 1.5);
    float fill = clamp(u_params[7], 0.35, 1.6);
    float hue_scroll = fract(u_params[8]);
    float speed = max(u_params[9], 0.05);

    float intensity = 0.0;
    float hue01 = hue_scroll;
    const float golden = 2.39996323;
    float spread = 0.48 * fill;

    for(int i = 0; i < 48; i++)
    {
        float on = step(float(i) + 0.5, count_f);
        float fi = float(i);
        float seed = fi * 265.443 + 101.390;
        float ring = sqrt((fi + 0.5) / max(count_f, 1.0));
        float ang = fi * golden;

        vec3 base;
        base.x = 0.5 + cos(ang) * ring * spread * (0.55 + 0.45 * pfHash(seed, 1.0));
        base.y = 0.12 + 0.76 * pfHash(seed, 2.0);
        base.z = 0.5 + sin(ang) * ring * spread * (0.55 + 0.45 * pfHash(seed, 3.0));

        float life = 1.0;
        float rad = size01 * (0.55 + 0.45 * pfHash(seed, 4.0));
        vec3 c = base;
        float ph = 0.0;
        float value = 0.0;
        float particle_hue = fract(fi * 0.097 + hue_scroll + 0.15 * pfHash(seed, 20.0));
        int use_aniso = 0;

        if(mode == 0)
        {
            float t = time_sec * speed * 0.55 * motion;
            vec3 n = vec3(
                pfN3(base * 3.1 + vec3(t, 0.0, 1.2)),
                pfN3(base * 2.7 + vec3(0.0, t * 1.1, 2.3)),
                pfN3(base * 3.4 + vec3(1.7, 0.0, t * 0.9)));
            c = base + (n - 0.5) * (0.18 * noise_amt + 0.08 * motion);
            rad *= 1.25;
        }
        else if(mode == 1)
        {
            float cycle = 2.4 + 2.2 * pfHash(seed, 5.0);
            ph = fract(time_sec * speed * 0.35 * motion / cycle + pfHash(seed, 6.0));
            c.y = 1.0 - ph;
            float sway = sin(time_sec * speed * 1.4 + seed) * 0.08 * motion;
            c.x = base.x + sway + (pfHash(seed, 7.0) - 0.5) * 0.06 * noise_amt;
            c.z = base.z + cos(time_sec * speed * 1.1 + seed * 0.7) * 0.07 * motion;
            rad *= 0.85;
            life = smoothstep(0.0, 0.08, ph) * (1.0 - smoothstep(0.92, 1.0, ph));
        }
        else if(mode == 2)
        {
            float cycle = 1.8 + 2.0 * pfHash(seed, 8.0);
            ph = fract(time_sec * speed * 0.42 * motion / cycle + pfHash(seed, 9.0));
            c.y = ph;
            c.x = base.x + sin(time_sec * speed * 2.2 + seed) * 0.05 * motion;
            c.z = base.z + cos(time_sec * speed * 1.8 + seed * 1.3) * 0.05 * motion;
            float flick = 0.55 + 0.45 * sin(time_sec * speed * (8.0 + 6.0 * pfHash(seed, 10.0)) + seed);
            life = smoothstep(0.0, 0.1, ph) * (1.0 - smoothstep(0.75, 1.0, ph)) * flick;
            rad *= 0.95;
            particle_hue = fract(0.04 + 0.10 * pfHash(seed, 11.0) + hue_scroll);
        }
        else if(mode == 3)
        {
            float cycle = 0.55 + 1.4 * pfHash(seed, 12.0);
            ph = fract(time_sec * speed * 0.9 * motion / cycle + pfHash(seed, 13.0));
            float flash = 1.0 - abs(ph * 2.0 - 1.0);
            flash = pow(clamp(flash, 0.0, 1.0), 3.5);
            life = flash * step(0.45, pfHash(seed, 14.0));
            c = base;
            c.y = 0.15 + 0.70 * pfHash(seed, 15.0);
            rad *= 0.55;
        }
        else if(mode == 4)
        {
            float t = time_sec * speed * 0.5 * motion;
            float orbit = t + ang;
            float pull = 0.35 + 0.45 * pfHash(seed, 16.0);
            vec3 orbit_p;
            orbit_p.x = 0.5 + cos(orbit) * ring * spread * (0.35 + 0.4 * noise_amt);
            orbit_p.y = 0.5 + sin(orbit * 0.85 + seed) * ring * spread * 0.55;
            orbit_p.z = 0.5 + sin(orbit) * ring * spread * (0.35 + 0.4 * noise_amt);
            c = mix(base, mix(vec3(0.5), orbit_p, 0.65), pull);
            rad *= 1.15;
        }
        else if(mode == 5)
        {
            float cycle = 0.55 + 0.85 * pfHash(seed, 21.0);
            ph = fract(time_sec * speed * 1.15 * motion / cycle + pfHash(seed, 22.0));
            c.y = 1.0 - ph;
            float slant = (pfHash(seed, 23.0) - 0.5) * 0.12 * motion;
            c.x = base.x + slant * ph + (pfHash(seed, 24.0) - 0.5) * 0.04 * noise_amt;
            c.z = base.z + slant * ph * 0.6;
            rad *= 0.42;
            life = smoothstep(0.0, 0.04, ph) * (1.0 - smoothstep(0.88, 1.0, ph));
            particle_hue = fract(0.55 + fi * 0.03 + hue_scroll);
            use_aniso = 1;
        }
        else
        {
            float burst_id = floor(fi / 6.0);
            float slot = mod(fi, 6.0);
            float cycle = 1.6 + 1.4 * pfHash(burst_id * 17.1 + 3.0, 30.0);
            ph = fract(time_sec * speed * 0.55 * motion / cycle + pfHash(burst_id, 31.0));
            vec3 origin_b;
            origin_b.x = 0.5 + (pfHash(burst_id, 32.0) - 0.5) * 0.55 * fill;
            origin_b.y = 0.35 + 0.35 * pfHash(burst_id, 33.0);
            origin_b.z = 0.5 + (pfHash(burst_id, 34.0) - 0.5) * 0.55 * fill;
            float bang = smoothstep(0.08, 0.18, ph);
            float fade = 1.0 - smoothstep(0.55, 0.95, ph);
            float boom = bang * fade;
            float ang_b = slot * 1.04719755 + pfHash(burst_id, 35.0) * 6.2831853;
            float elev = (pfHash(seed, 36.0) - 0.35) * 1.2;
            float expand = boom * (0.12 + 0.28 * fill) * (0.7 + 0.5 * motion);
            c = origin_b;
            c.x += cos(ang_b) * expand;
            c.z += sin(ang_b) * expand;
            c.y += elev * expand - boom * boom * 0.18 * motion;
            life = boom * (0.65 + 0.35 * step(0.2, pfHash(seed, 37.0)));
            rad *= 0.7 + 0.5 * (1.0 - ph);
            particle_hue = fract(pfHash(burst_id * 17.1, 38.0) * 0.9 + hue_scroll + ph * 0.08);
            if(ph < 0.12)
            {
                c = mix(vec3(origin_b.x, 0.05, origin_b.z), origin_b, ph / 0.12);
                life = smoothstep(0.0, 0.05, ph) * (1.0 - smoothstep(0.1, 0.12, ph));
                rad *= 0.55;
            }
        }

        c = clamp(c, vec3(0.02), vec3(0.98));
        if(use_aniso == 1)
        {
            float dy = abs(p01.y - c.y);
            float dxz = length(p01.xz - c.xz);
            float soft_y = thick * 2.4 * (0.7 + 0.3 * rad);
            float soft_xz = thick * 0.55 * (0.7 + 0.3 * rad);
            value = 1.0 / (1.0 + (dxz * dxz) / max(soft_xz * soft_xz, 1e-5)
                               + (dy * dy) / max(soft_y * soft_y, 1e-5));
        }
        else
        {
            float d = length(p01 - c);
            float soft = thick * (0.95 + 0.45 * rad);
            value = 1.0 / (1.0 + (d * d) / max(soft * soft, 1e-5));
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
