// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Bouncing Ball volume: R=intensity, G=hue01.
 *  Elastic bounce on all six faces of the occupancy box (floor, ceiling, walls)
 *  so balls travel the whole room — not floor-only gravity hops.
 *  u_params: [0]=sim_t [1]=count [2]=radius01 [3]=glow_mul
 *            [4]=motion [5]=hue_scroll01 [6]=hue_density
 */
inline const char* BouncingBallVolumeFieldGlsl()
{
    return R"(
float hash01(float n)
{
    return fract(sin(n) * 43758.5453);
}
/* Elastic 1D bounce between [lo, hi] at constant |speed|. */
float bounce1D(float x0, float v, float t, float lo, float hi)
{
    float L = max(hi - lo, 1e-4);
    float p = (x0 - lo) + v * t;
    float period = 2.0 * L;
    float m = mod(p, period);
    if(m < 0.0) m += period;
    return (m <= L) ? (lo + m) : (hi - (m - L));
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float sim_t = u_params[0];
    int count = int(clamp(u_params[1], 1.0, 32.0) + 0.5);
    float radius = clamp(u_params[2], 0.025, 0.28);
    float glow_mul = max(u_params[3], 1.0);
    float motion = clamp(u_params[4], 0.0, 1.0);
    float hue_scroll = fract(u_params[5]);
    float hue_dens = max(u_params[6], 1.0);

    float lo = radius;
    float hi = 1.0 - radius;
    /* Speed scales travel rate; keep a usable mid-slider floor so motion reads. */
    float speed = mix(0.18, 0.85, pow(max(motion, 0.02), 0.50));

    float intensity = 0.0;
    float hue01 = hue_scroll;

    for(int k = 0; k < 32; k++)
    {
        float fk = float(k);
        float slot_on = 1.0 - step(float(count), fk);
        float hx = hash01(fk * 131.0 + 1.7);
        float hy = hash01(fk * 313.0 + 5.0);
        float hz = hash01(fk * 919.0 + 2.3);
        float x0 = mix(lo, hi, hx);
        float y0 = mix(lo, hi, hy);
        float z0 = mix(lo, hi, hz);

        /* Independent axis speeds — vertical matches horizontal so ceiling is hit. */
        float vx = (hash01(fk * 733.0) * 2.0 - 1.0) * speed * mix(0.75, 1.15, hash01(fk * 11.0));
        float vy = (hash01(fk * 577.0) * 2.0 - 1.0) * speed * mix(0.85, 1.25, hash01(fk * 19.0));
        float vz = (hash01(fk * 829.0) * 2.0 - 1.0) * speed * mix(0.75, 1.15, hash01(fk * 29.0));
        /* Ensure no axis is nearly still (would skip floor/ceiling). */
        vy += (1.0 - 2.0 * step(0.5, hash01(fk * 41.0))) * speed * 0.35;

        float t = sim_t + hash01(fk * 47.0) * 3.0;
        float px = bounce1D(x0, vx, t, lo, hi);
        float py = bounce1D(y0, vy, t, lo, hi);
        float pz = bounce1D(z0, vz, t, lo, hi);
        vec3 c = vec3(px, py, pz);

        /* Mild squash near any wall so impacts read without looking like floor-only hops. */
        float wall_x = 1.0 - smoothstep(0.0, radius * 2.0, min(px - lo, hi - px));
        float wall_y = 1.0 - smoothstep(0.0, radius * 2.0, min(py - lo, hi - py));
        float wall_z = 1.0 - smoothstep(0.0, radius * 2.0, min(pz - lo, hi - pz));
        float wx = mix(1.0, 0.70, wall_x);
        float wy = mix(1.0, 0.70, wall_y);
        float wz = mix(1.0, 0.70, wall_z);
        vec3 dlt = p01 - c;
        dlt.x /= max(wx, 0.35);
        dlt.y /= max(wy, 0.35);
        dlt.z /= max(wz, 0.35);
        float d = length(dlt);

        float core_r = radius * 0.92;
        float glow_r = radius * (1.35 + 0.35 * glow_mul);
        float core = max(0.0, 1.0 - d / max(core_r, 1e-4));
        core = core * core;
        float outer = 0.45 * max(0.0, 1.0 - d / max(glow_r, 1e-4));
        float v = clamp(core * 1.35 + outer, 0.0, 1.0) * slot_on;
        if(v > intensity)
        {
            intensity = v;
            float dens_extra = max(hue_dens - 1.0, 0.0);
            float local = clamp(d / max(radius, 1e-4), 0.0, 2.0);
            hue01 = fract(hue_scroll + fk * 0.1056 + local * dens_extra * 0.14);
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), hue01, 0.0, 1.0);
}
)";
}
