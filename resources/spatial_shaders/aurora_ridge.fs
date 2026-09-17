// SPDX-License-Identifier: GPL-2.0-only
// From dump AnotherAuroraBorealis — animated ridge / horizon (distinct from Soft Aurora).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float arRnd(vec2 p, float n)
{
    return fract(abs(sin(p.x * 123.4 + p.y * 432.1) * (p.x * 3.7 + p.x * p.y * 4.5 + 256.7 + n * 654.3)
                     + n * 321.1));
}
float arEase(float x)
{
    return x < 0.5 ? 2.0 * x * x : 1.0 - pow(-2.0 * x + 2.0, 2.0) / 2.0;
}
float arH(float x, float dx, float n, float div)
{
    return 0.5 + (arRnd(vec2(floor(x * div) / div + dx / div, 0.0), n) - 0.5) / 5.0;
}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * zoom + 0.5;
    float div = 18.0 + 28.0 * detail;
    float t = u_time * 0.45;
    float n = floor(t);
    t = fract(t);
    float fx = fract(p.x * div);
    float v = mix(
        mix(arH(p.x, 0.0, n, div), arH(p.x, 1.0, n, div), arEase(fx)),
        mix(arH(p.x, 0.0, n + 1.0, div), arH(p.x, 1.0, n + 1.0, div), arEase(fx)),
        t);
    float band = abs(p.y - v);
    float glow = pow(clamp(1.0 - band * mix(6.0, 14.0, contrast / 2.5), 0.0, 1.0), mix(1.6, 0.8, contrast / 2.5));
    vec3 cool = vec3(0.15, 0.75, 0.35 + 0.45 * p.y);
    vec3 warm = vec3(0.75, 0.18, 0.85 * (1.0 - p.y));
    vec3 base = mix(cool, warm, clamp(abs(p.y - v) * 2.0, 0.0, 1.0));
    float h = fract(hue + glow * 0.08 + u_time * 0.015);
    vec3 tint = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(base, tint, 0.25) * (0.25 + 0.85 * glow), 1.0);
}
