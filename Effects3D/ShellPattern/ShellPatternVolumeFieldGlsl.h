// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** ShellPattern full volume: R=intensity, G=(k+1)/2.
 *  Compose after SpatialStripKernelEvalGlsl + StripUnfoldFieldGlsl.
 *  GLSL 1.10-safe (no inverted smoothstep edges; local smstep only).
 *  u_params: [0]=disp [1]=amp [2]=progress [3]=sigma [4]=detail [5]=size_m
 *            [6]=freq_n [7]=kid [8]=phase01 [9]=repeats [10]=unfold [11]=dir_deg
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
float wrapFade(float life)
{
    float a = smstep(0.0, 0.06, life);
    float b = 1.0 - smstep(0.90, 1.0, life);
    return a * b;
}
float evalBars(vec3 l, float k, float amp, float progress, float sigma, float detail)
{
    float n = 2.0 + floor(2.5 * detail + 0.5);
    float cell = 2.0 / max(n, 1.0);
    float cx = (floor((l.x + 1.0) / cell) + 0.5) * cell - 1.0;
    float cz = (floor((l.z + 1.0) / cell) + 0.5) * cell - 1.0;
    float dx = l.x - cx;
    float dz = l.z - cz;
    /* "half" is reserved in GLSL (half-precision type) — do not use as a name. */
    float half_w = cell * (0.34 + 0.12 * amp);
    if(abs(dx) > half_w || abs(dz) > half_w)
        return 0.0;
    float id = cubeHash11(cx * 12.7 + cz * 91.3);
    float breathe = 0.5 + 0.5 * sin(progress * 6.2831853 + id * 6.2831853);
    float h = clamp(0.22 + 0.58 * (0.5 + 0.5 * k) * amp + 0.32 * breathe, 0.12, 1.15);
    float y01 = (l.y + 1.0) * 0.5;
    if(y01 < 0.0 || y01 > h + 0.10)
        return 0.0;
    float edge = 1.0 - max(abs(dx), abs(dz)) / max(half_w, 0.001);
    float body = edge * (0.55 + 0.45 * clamp(y01 / max(h, 0.001), 0.0, 1.0));
    float tip = softBand(y01 - h, 0.12 + sigma * 0.5);
    return clamp(body + tip * 0.55, 0.0, 1.0);
}
float evalRipples(vec3 l, float k, float amp, float progress, float sigma, float detail, float size_m)
{
    float r = length(vec2(l.x, l.z));
    float freq = 1.6 + 2.4 * detail;
    float travel = progress * (3.2 + 2.8 * amp) * 6.2831853;
    float wave1 = sin(r * freq - travel);
    float crest1 = pow(max(0.0, 0.5 + 0.5 * wave1), 1.9 + 1.2 * (1.0 - clamp(sigma * 2.0, 0.0, 1.0)));
    float wave2 = sin(r * freq * 0.62 - travel * 0.55 + 1.3);
    float crest2 = pow(max(0.0, 0.5 + 0.5 * wave2), 2.4) * 0.48;
    float age = fractf(progress * (1.1 + 0.45 * amp) + 0.17);
    float R = age * (1.05 + 0.55 * size_m);
    float drop_ring = softBand(r - R, 0.06 + sigma * 0.35) * (1.0 - age) * wrapFade(age);
    float fall = exp(-r * (0.16 + 0.14 * (1.0 - clamp(size_m * 0.45, 0.0, 1.0))));
    float y_plane = softBand(l.y, 0.32 + 0.42 * size_m);
    float k_mod = 0.85 + 0.15 * (0.5 + 0.5 * k);
    float v = (crest1 + crest2) * fall + drop_ring * 0.95;
    return clamp(v * y_plane * k_mod * (0.85 + 0.25 * amp), 0.0, 1.0);
}
float evalDroplets(vec3 l, float amp, float progress, float sigma, float detail, float size_m)
{
    float cell = floor(l.x * (2.4 + 2.4 * detail) + 0.5) + floor(l.z * (2.4 + 2.4 * detail) + 0.5) * 7.0;
    float life = fractf(progress * (0.75 + 1.35 * amp) + cubeHash11(cell + 3.1));
    float py = 1.05 - life * 2.35;
    float rad = 0.10 + 0.08 * size_m;
    float d = abs(l.y - py);
    return clamp(softBand(d, rad + sigma * 0.35) * wrapFade(life), 0.0, 1.0);
}
float evalFireworks(vec3 l, float amp, float progress, float sigma, float detail, float size_m, float time_sec)
{
    float life = fractf(progress * (0.75 + 0.45 * amp) + 10.0);
    float fade = wrapFade(life);
    float expand = max(0.0, (life - 0.38) / 0.62);
    float R = expand * (0.45 + 0.65 * size_m);
    float d = length(l);
    float v = softBand(d - R, 0.07 + sigma * 0.4) * (1.0 - expand);
    float ang = atan(l.z, l.x);
    float spark = 0.55 + 0.45 * sin(ang * (5.0 + 3.0 * detail) + time_sec * 3.4);
    return clamp(v * spark * fade * (0.9 + 0.25 * amp), 0.0, 1.0);
}
float evalExplosion(vec3 l, float amp, float progress, float sigma, float detail, float size_m)
{
    float expand = fractf(progress * (0.95 + 0.65 * amp) + 10.0);
    float fade = wrapFade(expand);
    float R = expand * (0.7 + 0.85 * size_m);
    float d = length(l);
    float shell = softBand(d - R, 0.09 + sigma * 0.45) * (1.0 - expand * 0.85);
    float ang = atan(l.z, l.x);
    float rays = 0.5 + 0.5 * sin(ang * (6.0 + 6.0 * detail) + progress * 6.2831853);
    float streak = softBand(d - R * 0.7, 0.22) * rays * (1.0 - expand);
    float core = 0.0;
    if(expand < 0.22)
        core = softBand(d, 0.16) * (1.0 - expand / 0.22);
    return clamp(max(shell, max(streak * 0.8, core)) * fade, 0.0, 1.0);
}
float evalRain(vec3 l, float amp, float progress, float sigma, float detail, float size_m, float freq_n)
{
    float col = cubeHash11(floor(l.x * (3.2 + 3.2 * detail) + 0.5) + floor(l.z * 4.2 + 0.5) * 9.1);
    float life = fractf(progress * (1.05 + 1.65 * amp) + col);
    float py = 1.2 - life * 2.5;
    float slant = 0.4 + 0.3 * freq_n;
    float dx = l.x - slant * (l.y - py) * 0.18;
    float radial = length(vec2(dx, l.z));
    float along = softBand(l.y - py, 0.32 + 0.2 * size_m);
    float thin = softBand(radial, 0.055 + sigma * 0.28);
    return clamp(thin * along * wrapFade(life), 0.0, 1.0);
}

void volumeMain(out vec4 out_color, in vec3 p01)
{
    int disp = int(clamp(u_params[0], 0.0, 9.0) + 0.5);
    float amp = clamp(u_params[1], 0.2, 2.5);
    float progress = fractf(u_params[2]);
    float sigma = max(u_params[3], 0.03);
    float detail = clamp(u_params[4], 0.05, 1.0);
    float size_m = clamp(u_params[5], 0.35, 3.0);
    float freq_n = clamp(u_params[6], 0.05, 1.0);
    int kid = int(u_params[7] + 0.5);
    float phase01 = u_params[8];
    float repeats = max(u_params[9], 1.0);
    int unfold_mode = int(u_params[10] + 0.5);
    float dir_deg = u_params[11];

    /* Speed-scaled clock: Frequency boosts kernel chase; progress drives cube modes. */
    float anim_t = u_time * (0.65 + 2.2 * freq_n) + progress * 8.0;

    vec3 l = p01 * 2.0 - 1.0;
    float intensity = 0.0;
    float k = 0.0;

    vec3 uf = stripUnfoldKernelInputs(l.x, l.y, l.z, unfold_mode, dir_deg, phase01, anim_t);
    float s01 = uf.x;
    float phase_use = uf.y;
    float time_use = uf.z;
    k = evalStripKernelSigned(kid, s01, phase_use, repeats, time_use);

    if(disp == 0)
    {
        float surface_y = amp * k;
        intensity = shellIntensityGaussian(l.y, surface_y, max(sigma, 0.02), amp);
    }
    else if(disp == 1)
    {
        intensity = clamp((k + 1.0) * 0.5, 0.0, 1.0);
    }
    else if(disp == 2)
    {
        int shell_unfold = unfold_mode;
        if(shell_unfold != 1)
            shell_unfold = 4;
        vec3 uf_shell = stripUnfoldKernelInputs(l.x, l.y, l.z, shell_unfold, dir_deg, phase01, anim_t);
        k = evalStripKernelSigned(kid, uf_shell.x, uf_shell.y, repeats, uf_shell.z);
        float k01 = clamp((k + 1.0) * 0.5, 0.0, 1.0);
        float r_span = (0.65 + 0.55 * clamp(amp, 0.2, 2.5) / 2.0) * size_m;
        float surface_r = (0.15 + 0.85 * k01) * r_span;
        float lr = length(vec2(l.x, l.z));
        intensity = shellIntensityGaussian(lr, surface_r, sigma, max(1.0, amp));
    }
    else if(disp == 3)
    {
        float sig = max(0.035, 0.05 + sigma * 0.45);
        intensity = exp(-(k * k) / (sig * sig));
    }
    else if(disp == 4)
    {
        intensity = evalBars(l, k, amp, progress, sigma, detail);
        intensity = clamp(intensity * (0.82 + 0.18 * (0.5 + 0.5 * k)), 0.0, 1.0);
    }
    else if(disp == 5)
        intensity = evalRipples(l, k, amp, progress, sigma, detail, size_m);
    else if(disp == 6)
        intensity = evalDroplets(l, amp, progress, sigma, detail, size_m);
    else if(disp == 7)
        intensity = evalFireworks(l, amp, progress, sigma, detail, size_m, anim_t);
    else if(disp == 8)
        intensity = evalExplosion(l, amp, progress, sigma, detail, size_m);
    else
        intensity = evalRain(l, amp, progress, sigma, detail, size_m, freq_n);

    out_color = vec4(clamp(intensity, 0.0, 1.0), clamp((k + 1.0) * 0.5, 0.0, 1.0), 0.0, 1.0);
}
)";
}
