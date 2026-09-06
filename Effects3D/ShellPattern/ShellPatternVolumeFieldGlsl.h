// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** ShellPattern full volume: R=intensity, G=(k+1)/2.
 *  Compose after SpatialStripKernelEvalGlsl + StripUnfoldFieldGlsl.
 *  GLSL 1.10-safe (no inverted smoothstep edges; local smstep only).
 *  u_params: [0]=disp [1]=amp [2]=motion_clock [3]=sigma [4]=detail [5]=size_m
 *            [6]=freq_n [7]=kid [8]=phase_clock [9]=repeats [10]=unfold [11]=dir_deg
 *
 *  motion_clock is unwrapped Speed cycles (0 when Speed is 0). Cube displays
 *  overlap staggered lives so the field never globally fades to black.
 */
inline const char* ShellPatternVolumeFieldGlsl()
{
    return R"(
float cubeHash11(float n)
{
    return fractf(sin(n * 127.1) * 43758.5453);
}
float softBand(float d, float sigma)
{
    float s = max(sigma, 0.02);
    return exp(-(d * d) / (s * s));
}
float lifeEnv(float life)
{
    float a = smstep(0.0, 0.14, life);
    float b = 1.0 - smstep(0.78, 1.0, life);
    return a * b;
}
float staggerLife(float clock, float rate, float seed)
{
    return fractf(clock * rate + seed);
}
float patternMod(float k)
{
    return 0.72 + 0.28 * (0.5 + 0.5 * k);
}
vec3 burstOrigin(float seed)
{
    return vec3(cubeHash11(seed) * 2.0 - 1.0,
                cubeHash11(seed + 1.7) * 2.0 - 1.0,
                cubeHash11(seed + 3.1) * 2.0 - 1.0) * 0.42;
}
float evalBars(vec3 l, float k, float amp, float clock, float sigma, float detail)
{
    float n = 2.0 + floor(2.5 * detail + 0.5);
    float cell = 2.0 / max(n, 1.0);
    float cx = (floor((l.x + 1.0) / cell) + 0.5) * cell - 1.0;
    float cz = (floor((l.z + 1.0) / cell) + 0.5) * cell - 1.0;
    float dx = l.x - cx;
    float dz = l.z - cz;
    float half_w = cell * (0.34 + 0.12 * amp);
    if(abs(dx) > half_w || abs(dz) > half_w)
        return 0.0;
    float id = cubeHash11(cx * 12.7 + cz * 91.3);
    float breathe = 0.5 + 0.5 * sin(clock * 6.2831853 + id * 6.2831853);
    float h = clamp(0.22 + 0.58 * (0.5 + 0.5 * k) * amp + 0.32 * breathe, 0.12, 1.15);
    float y01 = (l.y + 1.0) * 0.5;
    if(y01 < 0.0 || y01 > h + 0.10)
        return 0.0;
    float edge = 1.0 - max(abs(dx), abs(dz)) / max(half_w, 0.001);
    float body = edge * (0.55 + 0.45 * clamp(y01 / max(h, 0.001), 0.0, 1.0));
    float tip = softBand(y01 - h, 0.12 + sigma * 0.5);
    return clamp(body + tip * 0.55, 0.0, 1.0);
}
float rippleRing(float r, float clock, float rate, float seed, float size_m, float sigma)
{
    float age = staggerLife(clock, rate, seed);
    float R = age * (1.05 + 0.55 * size_m);
    return softBand(r - R, 0.06 + sigma * 0.35) * (1.0 - age) * lifeEnv(age);
}
float evalRipples(vec3 l, float k, float amp, float clock, float sigma, float detail, float size_m)
{
    float r = length(vec2(l.x, l.z));
    float freq = 1.6 + 2.4 * detail;
    float cycles = clock * (3.2 + 2.8 * amp);
    float travel = fractf(cycles) * 6.2831853;
    float wave1 = sin(r * freq - travel);
    float crest1 = pow(max(0.0, 0.5 + 0.5 * wave1), 1.9 + 1.2 * (1.0 - clamp(sigma * 2.0, 0.0, 1.0)));
    float travel2 = fractf(cycles * 0.55) * 6.2831853;
    float wave2 = sin(r * freq * 0.62 - travel2 + 1.3);
    float crest2 = pow(max(0.0, 0.5 + 0.5 * wave2), 2.4) * 0.48;
    float drop = max(rippleRing(r, clock, 0.55, 0.17, size_m, sigma),
                 max(rippleRing(r, clock, 0.55, 0.50, size_m, sigma),
                     rippleRing(r, clock, 0.55, 0.83, size_m, sigma)));
    float fall = exp(-r * (0.16 + 0.14 * (1.0 - clamp(size_m * 0.45, 0.0, 1.0))));
    float y_plane = softBand(l.y, 0.32 + 0.42 * size_m);
    float v = (crest1 + crest2) * fall + drop * 0.95;
    return clamp(v * y_plane * patternMod(k) * (0.85 + 0.25 * amp), 0.0, 1.0);
}
float oneDroplet(vec3 l, vec3 c, float life, float rad, float sigma)
{
    float py = 1.05 - life * 2.35;
    vec3 p = vec3(c.x, py, c.z);
    float d = length(l - p);
    return softBand(d, rad + sigma * 0.35) * lifeEnv(life);
}
float evalDroplets(vec3 l, float k, float amp, float clock, float sigma, float detail, float size_m, float freq_n)
{
    float n = 2.4 + 2.4 * detail + 2.0 * freq_n;
    float cell = 2.0 / max(n, 1.0);
    float cx = (floor((l.x + 1.0) / cell) + 0.5) * cell - 1.0;
    float cz = (floor((l.z + 1.0) / cell) + 0.5) * cell - 1.0;
    float id = cubeHash11(cx * 9.3 + cz * 17.1);
    float rate = 0.55 + 0.85 * amp;
    float rad = 0.10 + 0.08 * size_m;
    vec3 c = vec3(cx, 0.0, cz);
    float v = max(oneDroplet(l, c, staggerLife(clock, rate, id), rad, sigma),
                  oneDroplet(l, c, staggerLife(clock, rate, id + 0.5), rad, sigma));
    return clamp(v * patternMod(k), 0.0, 1.0);
}
float oneBurst(vec3 l, vec3 origin, float life, float amp, float sigma, float detail, float size_m, float clock)
{
    float fade = lifeEnv(life);
    float expand = max(0.0, (life - 0.18) / 0.82);
    float R = expand * (0.35 + 0.55 * size_m);
    vec3 dlt = l - origin;
    float d = length(dlt);
    float v = softBand(d - R, 0.07 + sigma * 0.4) * (1.0 - expand);
    float ang = atan(dlt.z, dlt.x);
    float spark = 0.55 + 0.45 * sin(ang * (5.0 + 3.0 * detail) + clock * 3.4);
    return v * spark * fade * (0.9 + 0.25 * amp);
}
float evalFireworks(vec3 l, float k, float amp, float clock, float sigma, float detail, float size_m)
{
    float rate = 0.42 + 0.28 * amp;
    float v = max(oneBurst(l, burstOrigin(0.11), staggerLife(clock, rate, 0.22), amp, sigma, detail, size_m, clock),
              max(oneBurst(l, burstOrigin(0.37), staggerLife(clock, rate, 0.52), amp, sigma, detail, size_m, clock),
                  oneBurst(l, burstOrigin(0.73), staggerLife(clock, rate, 0.74), amp, sigma, detail, size_m, clock)));
    return clamp(v * patternMod(k), 0.0, 1.0);
}
float oneBlast(vec3 l, float life, float amp, float sigma, float detail, float size_m)
{
    float fade = lifeEnv(life);
    float R = life * (0.7 + 0.85 * size_m);
    float d = length(l);
    float shell = softBand(d - R, 0.09 + sigma * 0.45) * (1.0 - life * 0.85);
    float ang = atan(l.z, l.x);
    float rays = 0.5 + 0.5 * sin(ang * (6.0 + 6.0 * detail) + life * 6.2831853);
    float streak = softBand(d - R * 0.7, 0.22) * rays * (1.0 - life);
    float core = 0.0;
    if(life < 0.22)
        core = softBand(d, 0.16) * (1.0 - life / 0.22);
    return max(shell, max(streak * 0.8, core)) * fade;
}
float evalExplosion(vec3 l, float k, float amp, float clock, float sigma, float detail, float size_m)
{
    float rate = 0.38 + 0.22 * amp;
    float v = max(oneBlast(l, staggerLife(clock, rate, 0.20), amp, sigma, detail, size_m),
              max(oneBlast(l, staggerLife(clock, rate, 0.52), amp, sigma, detail, size_m),
                  oneBlast(l, staggerLife(clock, rate, 0.74), amp, sigma, detail, size_m)));
    return clamp(v * patternMod(k), 0.0, 1.0);
}
float oneStreak(vec3 l, float cx, float cz, float life, float slant, float sigma, float size_m)
{
    float py = 1.2 - life * 2.5;
    float dx = l.x - cx - slant * (l.y - py) * 0.18;
    float radial = length(vec2(dx, l.z - cz));
    float along = softBand(l.y - py, 0.32 + 0.2 * size_m);
    float thin = softBand(radial, 0.055 + sigma * 0.28);
    return thin * along * lifeEnv(life);
}
float evalRain(vec3 l, float k, float amp, float clock, float sigma, float detail, float size_m, float freq_n)
{
    float n = 3.2 + 3.2 * detail + 2.4 * freq_n;
    float cell = 2.0 / max(n, 1.0);
    float cx = (floor((l.x + 1.0) / cell) + 0.5) * cell - 1.0;
    float cz = (floor((l.z + 1.0) / cell) + 0.5) * cell - 1.0;
    float col = cubeHash11(cx * 11.3 + cz * 19.7);
    float rate = 0.70 + 0.90 * amp;
    float slant = 0.4 + 0.3 * freq_n;
    float v = max(oneStreak(l, cx, cz, staggerLife(clock, rate, col), slant, sigma, size_m),
                  oneStreak(l, cx, cz, staggerLife(clock, rate, col + 0.5), slant, sigma, size_m));
    return clamp(v * patternMod(k), 0.0, 1.0);
}

void volumeMain(out vec4 out_color, in vec3 p01)
{
    int disp = int(clamp(u_params[0], 0.0, 9.0) + 0.5);
    float amp = clamp(u_params[1], 0.2, 2.5);
    float clock = u_params[2];
    float sigma = max(u_params[3], 0.03);
    float detail = clamp(u_params[4], 0.0, 1.0);
    float size_m = clamp(u_params[5], 0.08, 3.0);
    float freq_n = clamp(u_params[6], 0.0, 1.0);
    int kid = int(u_params[7] + 0.5);
    float phase_clock = u_params[8];
    float repeats = max(u_params[9], 1.0);
    int unfold_mode = int(u_params[10] + 0.5);
    float dir_deg = u_params[11];

    vec3 l = p01 * 2.0 - 1.0;
    vec3 uf = stripUnfoldKernelInputs(l.x, l.y, l.z, unfold_mode, dir_deg, phase_clock, clock);
    float k = evalStripKernelSigned(kid, uf.x, uf.y, repeats, uf.z);

    float intensity = 0.0;
    if(disp == 0)
        intensity = shellIntensityGaussian(l.y, amp * k, max(sigma, 0.02), amp);
    else if(disp == 1)
        intensity = clamp((k + 1.0) * 0.5, 0.0, 1.0);
    else if(disp == 2)
    {
        float k01 = clamp((k + 1.0) * 0.5, 0.0, 1.0);
        float r_span = (0.65 + 0.55 * clamp(amp, 0.2, 2.5) / 2.0) * size_m;
        float surface_r = (0.15 + 0.85 * k01) * r_span;
        intensity = shellIntensityGaussian(length(vec2(l.x, l.z)), surface_r, sigma, max(1.0, amp));
    }
    else if(disp == 3)
    {
        float sig = max(0.035, 0.05 + sigma * 0.45);
        intensity = exp(-(k * k) / (sig * sig));
    }
    else if(disp == 4)
        intensity = evalBars(l, k, amp, clock, sigma, detail);
    else if(disp == 5)
        intensity = evalRipples(l, k, amp, clock, sigma, detail, size_m);
    else if(disp == 6)
        intensity = evalDroplets(l, k, amp, clock, sigma, detail, size_m, freq_n);
    else if(disp == 7)
        intensity = evalFireworks(l, k, amp, clock, sigma, detail, size_m);
    else if(disp == 8)
        intensity = evalExplosion(l, k, amp, clock, sigma, detail, size_m);
    else
        intensity = evalRain(l, k, amp, clock, sigma, detail, size_m, freq_n);

    out_color = vec4(clamp(intensity, 0.0, 1.0), clamp((k + 1.0) * 0.5, 0.0, 1.0), 0.0, 1.0);
}
)";
}
