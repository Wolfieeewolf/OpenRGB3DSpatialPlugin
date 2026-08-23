// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** ShellPattern cube displays (Bars..Rain): R=intensity.
 *  Classic shell/extrude/contour stay CPU (+ strip k).
 *  u_params: [0]=disp [1]=amp [2]=progress01 [3]=sigma [4]=detail [5]=size_m [6]=freq_n [7]=k_hint
 *  lx,ly,lz = p01*2-1  (room unit cube)
 */
inline const char* ShellPatternCubeVolumeFieldGlsl()
{
    return R"(
float hash11(float n)
{
    float x = sin(n * 127.1) * 43758.5453;
    return x - floor(x);
}
float softBand(float d, float sigma)
{
    float s = max(sigma, 0.02);
    return exp(-(d * d) / (s * s));
}
float wrapFade(float life)
{
    /* Soft birth/death so looping particles don't pop. */
    return smoothstep(0.0, 0.06, life) * smoothstep(1.0, 0.90, life);
}
float evalBars(vec3 l, float k, float amp, float progress, float sigma, float detail)
{
    float n = 3.0 + floor(3.0 * detail + 0.5);
    float cell = 2.0 / n;
    float cx = (floor((l.x + 1.0) / cell) + 0.5) * cell - 1.0;
    float cz = (floor((l.z + 1.0) / cell) + 0.5) * cell - 1.0;
    float dx = l.x - cx;
    float dz = l.z - cz;
    float half = cell * (0.28 + 0.08 * amp);
    if(abs(dx) > half || abs(dz) > half) return 0.0;
    float id = hash11(cx * 12.7 + cz * 91.3);
    /* Smooth height breathe — avoid hard tips. */
    float breathe = 0.5 + 0.5 * sin(progress * 6.2831853 + id * 6.2831853);
    float h = clamp(0.18 + 0.50 * (0.5 + 0.5 * k) * amp + 0.28 * breathe, 0.10, 1.10);
    float y01 = (l.y + 1.0) * 0.5;
    if(y01 < 0.0 || y01 > h + 0.08) return 0.0;
    float edge = 1.0 - max(abs(dx), abs(dz)) / half;
    float body = edge * (0.55 + 0.45 * clamp(y01 / max(h, 1e-3), 0.0, 1.0));
    float tip = softBand(y01 - h, 0.10 + sigma * 0.45);
    return clamp(body + tip * 0.55, 0.0, 1.0);
}
float evalRipples(vec3 l, float k, float amp, float progress, float sigma, float detail, float size_m)
{
    /* Traveling circular water crests (not a mid-plane pulse). */
    float r = length(l.xz);
    float freq = 2.4 + 3.6 * detail;
    float travel = progress * (2.0 + 2.2 * amp) * 6.2831853;

    float wave1 = sin(r * freq - travel);
    float crest1 = pow(0.5 + 0.5 * wave1, 2.2 + 1.6 * (1.0 - clamp(sigma * 2.0, 0.0, 1.0)));

    float wave2 = sin(r * freq * 0.62 - travel * 0.55 + 1.3);
    float crest2 = pow(0.5 + 0.5 * wave2, 2.8) * 0.42;

    /* Soft expanding ring births for extra "drop in water" feel. */
    float age = fract(progress * (0.85 + 0.35 * amp) + 0.17);
    float R = age * (0.95 + 0.35 * size_m);
    float drop_ring = softBand(r - R, 0.045 + sigma * 0.25) * (1.0 - age) * wrapFade(age);

    float fall = exp(-r * (0.22 + 0.18 * (1.0 - clamp(size_m * 0.5, 0.0, 1.0))));
    float y_plane = softBand(l.y, 0.24 + 0.30 * size_m);
    float k_mod = 0.88 + 0.12 * (0.5 + 0.5 * k);
    float v = (crest1 + crest2) * fall + drop_ring * 0.85;
    return clamp(v * y_plane * k_mod * (0.8 + 0.2 * amp), 0.0, 1.0);
}
float evalDroplets(vec3 l, float amp, float progress, float sigma, float detail, float size_m)
{
    float best = 0.0;
    int drops = int(clamp(4.0 + floor(4.0 * detail + 0.5), 4.0, 8.0));
    float fall = 0.55 + 1.1 * amp;
    for(int i = 0; i < 8; i++)
    {
        if(i >= drops) break;
        float seed = float(i) * 19.17 + 3.1;
        float px = hash11(seed) * 2.0 - 1.0;
        float pz = hash11(seed + 7.3) * 2.0 - 1.0;
        float life = fract(progress * fall + hash11(seed + 1.7) + 10.0);
        float py = 1.0 - life * 2.2;
        float rad = 0.06 + 0.05 * size_m + 0.03 * hash11(seed + 4.2);
        float d = length(l - vec3(px, py, pz));
        float blob = softBand(d, rad + sigma * 0.25) * wrapFade(life);
        if(life > 0.82)
        {
            float splash_r = (life - 0.82) * 4.0;
            float sr = length(l.xz - vec2(px, pz));
            blob = max(blob, softBand(sr - splash_r, 0.06) * softBand(l.y + 0.85, 0.12) * wrapFade(life));
        }
        best = max(best, blob);
    }
    return clamp(best, 0.0, 1.0);
}
float evalFireworks(vec3 l, float amp, float progress, float sigma, float detail, float size_m, float time_sec)
{
    float best = 0.0;
    int bursts = int(clamp(2.0 + floor(2.0 * detail + 0.5), 2.0, 4.0));
    for(int i = 0; i < 4; i++)
    {
        if(i >= bursts) break;
        float seed = float(i) * 31.7 + 11.0;
        float life = fract(progress * (0.55 + 0.35 * amp) + hash11(seed) + 10.0);
        float fade = wrapFade(life);
        float bx = (hash11(seed + 1.0) * 2.0 - 1.0) * 0.55;
        float bz = (hash11(seed + 2.0) * 2.0 - 1.0) * 0.55;
        float by = -0.85 + life * 1.5;
        float v = 0.0;
        if(life < 0.45)
        {
            float d = length(l - vec3(bx, by, bz));
            v = softBand(d, 0.05 + sigma * 0.2);
        }
        else
        {
            by = -0.85 + 0.45 * 1.5;
            float expand = (life - 0.45) / 0.55;
            float R = expand * (0.35 + 0.45 * size_m);
            float d = length(l - vec3(bx, by, bz));
            v = softBand(d - R, 0.055 + sigma * 0.35) * (1.0 - expand);
            float ang = atan(l.z - bz, l.x - bx);
            /* Slower spark shimmer — less flicker. */
            float spark = 0.55 + 0.45 * sin(ang * (5.0 + 3.0 * detail) + time_sec * 2.8);
            v *= 0.5 + 0.5 * spark;
        }
        best = max(best, v * fade);
    }
    return clamp(best * (0.85 + 0.2 * amp), 0.0, 1.0);
}
float evalExplosion(vec3 l, float amp, float progress, float sigma, float detail, float size_m)
{
    float expand = fract(progress * (0.7 + 0.5 * amp) + 10.0);
    float fade = wrapFade(expand);
    float R = expand * (0.55 + 0.65 * size_m);
    float d = length(l);
    float shell = softBand(d - R, 0.07 + sigma * 0.4) * (1.0 - expand * 0.85);
    float ang = atan(l.z, l.x);
    float rays = 0.5 + 0.5 * sin(ang * (6.0 + 6.0 * detail) + progress * 6.2831853);
    float streak = softBand(d - R * 0.7, 0.18) * rays * (1.0 - expand);
    float core = (expand < 0.18) ? softBand(d, 0.12) * (1.0 - expand / 0.18) : 0.0;
    return clamp(max(shell, max(streak * 0.75, core)) * fade, 0.0, 1.0);
}
float evalRain(vec3 l, float amp, float progress, float sigma, float detail, float size_m, float freq_n)
{
    float best = 0.0;
    int streaks = int(clamp(6.0 + floor(6.0 * detail + 0.5), 6.0, 12.0));
    float fall = 0.8 + 1.4 * amp;
    float slant = 0.35 + 0.25 * freq_n;
    for(int i = 0; i < 12; i++)
    {
        if(i >= streaks) break;
        float seed = float(i) * 13.91 + 2.4;
        float px = hash11(seed) * 2.0 - 1.0;
        float pz = hash11(seed + 5.5) * 2.0 - 1.0;
        float life = fract(progress * fall + hash11(seed + 0.7) + 10.0);
        float py = 1.15 - life * 2.4;
        float dx = l.x - (px + slant * (l.y - py) * 0.15);
        float dz = l.z - pz;
        float dy = l.y - py;
        float radial = length(vec2(dx, dz));
        float along = softBand(dy, 0.24 + 0.14 * size_m);
        float thin = softBand(radial, 0.04 + sigma * 0.22);
        best = max(best, thin * along * wrapFade(life));
    }
    return clamp(best, 0.0, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    int disp = int(clamp(u_params[0], 4.0, 9.0) + 0.5);
    float amp = clamp(u_params[1], 0.2, 2.0);
    float progress = fract(u_params[2]);
    float sigma = max(u_params[3], 0.02);
    float detail = clamp(u_params[4], 0.05, 1.0);
    float size_m = clamp(u_params[5], 0.2, 2.5);
    float freq_n = clamp(u_params[6], 0.05, 1.0);
    float k = clamp(u_params[7], -1.0, 1.0);
    vec3 l = p01 * 2.0 - 1.0;
    float intensity = 0.0;
    if(disp == 4) intensity = evalBars(l, k, amp, progress, sigma, detail);
    else if(disp == 5) intensity = evalRipples(l, k, amp, progress, sigma, detail, size_m);
    else if(disp == 6) intensity = evalDroplets(l, amp, progress, sigma, detail, size_m);
    else if(disp == 7) intensity = evalFireworks(l, amp, progress, sigma, detail, size_m, u_time);
    else if(disp == 8) intensity = evalExplosion(l, amp, progress, sigma, detail, size_m);
    else if(disp == 9) intensity = evalRain(l, amp, progress, sigma, detail, size_m, freq_n);
    out_color = vec4(clamp(intensity, 0.0, 1.0), 0.0, 0.0, 1.0);
}
)";
}
