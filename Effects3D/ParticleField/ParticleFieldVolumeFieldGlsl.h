// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Particle Field volume: R=intensity, G=hue01.
 *  Room-fill organic particles (WLED PS–inspired looks, not a firmware port).
 *  Sample coords are origin-local (Spatial Anchor at 0.5).
 *  u_params: [0]=time [1]=mode [2]=count [3]=size01 [4]=thick01
 *            [5]=motion [6]=noise [7]=fill [8]=hue_scroll01 [9]=speed
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
    int mode = int(clamp(u_params[1], 0.0, 4.0) + 0.5);
    float count_f = clamp(u_params[2], 4.0, 48.0);
    int count = int(count_f + 0.5);
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

        if(mode == 0)
        {
            /* Float / Fuzzy — noise-advected soft blobs */
            float t = time_sec * speed * 0.55 * motion;
            vec3 n = vec3(
                pfN3(base * 3.1 + vec3(t, 0.0, 1.2)),
                pfN3(base * 2.7 + vec3(0.0, t * 1.1, 2.3)),
                pfN3(base * 3.4 + vec3(1.7, 0.0, t * 0.9)));
            c = base + (n - 0.5) * (0.18 * noise_amt + 0.08 * motion);
            rad *= 1.15;
        }
        else if(mode == 1)
        {
            /* Snow — fall + sway */
            float cycle = 2.4 + 2.2 * pfHash(seed, 5.0);
            float ph = fract(time_sec * speed * 0.35 * motion / cycle + pfHash(seed, 6.0));
            c.y = 1.0 - ph;
            float sway = sin(time_sec * speed * 1.4 + seed) * 0.08 * motion;
            c.x = base.x + sway + (pfHash(seed, 7.0) - 0.5) * 0.06 * noise_amt;
            c.z = base.z + cos(time_sec * speed * 1.1 + seed * 0.7) * 0.07 * motion;
            rad *= 0.72;
            life = smoothstep(0.0, 0.08, ph) * (1.0 - smoothstep(0.92, 1.0, ph));
        }
        else if(mode == 2)
        {
            /* Embers — rise + flicker */
            float cycle = 1.8 + 2.0 * pfHash(seed, 8.0);
            float ph = fract(time_sec * speed * 0.42 * motion / cycle + pfHash(seed, 9.0));
            c.y = ph;
            c.x = base.x + sin(time_sec * speed * 2.2 + seed) * 0.05 * motion;
            c.z = base.z + cos(time_sec * speed * 1.8 + seed * 1.3) * 0.05 * motion;
            float flick = 0.55 + 0.45 * sin(time_sec * speed * (8.0 + 6.0 * pfHash(seed, 10.0)) + seed);
            life = smoothstep(0.0, 0.1, ph) * (1.0 - smoothstep(0.75, 1.0, ph)) * flick;
            rad *= 0.85;
        }
        else if(mode == 3)
        {
            /* Sparkle — sparse short flashes */
            float cycle = 0.55 + 1.4 * pfHash(seed, 12.0);
            float ph = fract(time_sec * speed * 0.9 * motion / cycle + pfHash(seed, 13.0));
            float flash = 1.0 - abs(ph * 2.0 - 1.0);
            flash = pow(clamp(flash, 0.0, 1.0), 3.5);
            life = flash * step(0.55, pfHash(seed, 14.0));
            c = base;
            c.y = 0.15 + 0.70 * pfHash(seed, 15.0);
            rad *= 0.45;
        }
        else
        {
            /* Attract — soft ballpit pull toward Spatial Anchor (0.5) */
            float t = time_sec * speed * 0.5 * motion;
            float orbit = t + ang;
            float pull = 0.35 + 0.45 * pfHash(seed, 16.0);
            vec3 orbit_p;
            orbit_p.x = 0.5 + cos(orbit) * ring * spread * (0.35 + 0.4 * noise_amt);
            orbit_p.y = 0.5 + sin(orbit * 0.85 + seed) * ring * spread * 0.55;
            orbit_p.z = 0.5 + sin(orbit) * ring * spread * (0.35 + 0.4 * noise_amt);
            c = mix(base, mix(vec3(0.5), orbit_p, 0.65), pull);
            rad *= 1.05;
        }

        c = clamp(c, vec3(0.02), vec3(0.98));
        float d = length(p01 - c);
        float soft = thick * (0.85 + 0.35 * rad);
        float value = 1.0 / (1.0 + (d * d) / max(soft * soft, 1e-5));
        value *= life * on;
        if(value > intensity)
        {
            intensity = value;
            if(mode == 2)
                hue01 = fract(0.04 + 0.10 * pfHash(seed, 11.0) + hue_scroll);
            else
                hue01 = fract(fi * 0.097 + hue_scroll + 0.15 * pfHash(seed, 20.0));
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), hue01, 0.0, 1.0);
}
)";
}
