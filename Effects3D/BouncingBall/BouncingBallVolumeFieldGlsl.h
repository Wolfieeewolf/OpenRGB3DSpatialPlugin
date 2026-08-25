// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Bouncing Ball volume: R=intensity, G=hue01.
 *  Closed-form wall bounce + gravity hops — same look as CPU physics without
 *  LED×ball loops. Evaluated once per atlas voxel.
 *  u_params: [0]=sim_t [1]=count [2]=radius01 [3]=glow_mul
 *            [4]=motion [5]=hue_scroll01 [6]=detail
 */
inline const char* BouncingBallVolumeFieldGlsl()
{
    return R"(
float hash01(float n)
{
    return fract(sin(n) * 43758.5453);
}
// Elastic 1D bounce between [lo, hi] at constant |speed|.
float bounce1D(float x0, float v, float t, float lo, float hi)
{
    float L = max(hi - lo, 1e-4);
    float p = (x0 - lo) + v * t;
    float period = 2.0 * L;
    float m = mod(p, period);
    if(m < 0.0) m += period;
    return (m <= L) ? (lo + m) : (hi - (m - L));
}
// Floor hop with fixed takeoff speed v0 and gravity g (CPU floor_bounce reset).
float hopY(float t, float phase, float v0, float g, float lo, float hi)
{
    float T = max(2.0 * v0 / max(g, 1e-4), 1e-3);
    float tm = mod(t + phase, T);
    float y = lo + v0 * tm - 0.5 * g * tm * tm;
    return clamp(y, lo, hi);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float sim_t = u_params[0];
    int count = int(clamp(u_params[1], 1.0, 32.0) + 0.5);
    float radius = max(u_params[2], 0.03);
    float glow_mul = max(u_params[3], 1.15);
    float motion = clamp(u_params[4], 0.02, 1.0);
    float hue_scroll = fract(u_params[5]);
    float detail = clamp(u_params[6], 0.05, 1.0);

    float lo = radius;
    float hi = 1.0 - radius;
    float g = (0.35 + 0.55 * motion) * 1.8;
    float v_hop_base = sqrt(max(2.0 * g * 0.35, 1e-4));
    float horiz = (0.12 + 0.45 * motion) * 0.55;

    float intensity = 0.0;
    float hue01 = hue_scroll;
    float core_r = radius * 0.8;
    float glow_r = radius * 2.0 * glow_mul;

    for(int k = 0; k < 32; k++)
    {
        if(k >= count)
            break;
        float fk = float(k);
        float hx = hash01(fk * 131.0 + 1.7);
        float hy = hash01(fk * 313.0 + 5.0);
        float hz = hash01(fk * 919.0 + 2.3);
        float x0 = mix(lo, hi, hx);
        float z0 = mix(lo, hi, hz);
        float y0 = mix(lo, hi, 0.18 + hy * 0.72);
        float vx = (hash01(fk * 733.0) * 2.0 - 1.0) * horiz;
        float vz = (hash01(fk * 829.0) * 2.0 - 1.0) * horiz;
        float drop = 0.35 + 0.55 * hash01(fk * 419.0 + 11.0);
        float v0 = v_hop_base * sqrt(drop) * 1.05;
        float phase = hash01(fk * 577.0) * 6.2831853;

        // Seed-relative time so balls don't all sync.
        float t = sim_t + hash01(fk * 47.0) * 3.0;
        float px = bounce1D(x0, vx, t, lo, hi);
        float pz = bounce1D(z0, vz, t, lo, hi);
        float py = hopY(t, phase, v0, g, lo, hi);
        vec3 c = mix(vec3(px, py, pz), vec3(0.5), 0.08);

        float d = length(p01 - c);
        if(d > glow_r)
            continue;
        float core = max(0.0, 1.0 - d / max(core_r, 1e-4));
        float outer = 0.7 * max(0.0, 1.0 - d / max(glow_r, 1e-4));
        float v = clamp((pow(core, 0.9) + outer) * 1.6, 0.0, 1.0);
        if(v > intensity)
        {
            intensity = v;
            float hue_spatial = (c.x * 0.39 + c.y * 0.56 + c.z * 0.33) * (0.55 + 0.45 * detail);
            hue01 = fract(hue_scroll + fk * 0.1056 + hue_spatial);
        }
    }

    out_color = vec4(clamp(intensity, 0.0, 1.0), hue01, 0.0, 1.0);
}
)";
}
