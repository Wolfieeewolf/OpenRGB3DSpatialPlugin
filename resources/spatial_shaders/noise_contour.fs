// SPDX-License-Identifier: GPL-2.0-only
// From dump 2dNoiseContour — gradient-noise contour bands (GLSL 1.10-safe).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float ncHash11(float t)
{
    return fract(sin(t * 56789.0) * 56789.0);
}
float ncHash21(vec2 uv)
{
    return ncHash11(ncHash11(uv.x) + 2.0 * ncHash11(uv.y));
}
vec2 ncGrad(vec2 uv)
{
    float t = ncHash21(uv);
    return vec2(cos(6.2831853 * t), sin(6.2831853 * t));
}
float ncNoise(vec2 uv, float r)
{
    float ca = cos(r);
    float sa = sin(r);
    vec2 uvi = floor(uv);
    vec2 uvf = uv - uvi;
    vec2 g00 = ncGrad(uvi);
    vec2 g10 = ncGrad(uvi + vec2(1.0, 0.0));
    vec2 g01 = ncGrad(uvi + vec2(0.0, 1.0));
    vec2 g11 = ncGrad(uvi + vec2(1.0, 1.0));
    g00 = vec2(ca * g00.x - sa * g00.y, sa * g00.x + ca * g00.y);
    g10 = vec2(ca * g10.x - sa * g10.y, sa * g10.x + ca * g10.y);
    g01 = vec2(ca * g01.x - sa * g01.y, sa * g01.x + ca * g01.y);
    g11 = vec2(ca * g11.x - sa * g11.y, sa * g11.x + ca * g11.y);
    float f00 = dot(g00, uvf);
    float f10 = dot(g10, uvf - vec2(1.0, 0.0));
    float f01 = dot(g01, uvf - vec2(0.0, 1.0));
    float f11 = dot(g11, uvf - vec2(1.0, 1.0));
    float sx = uvf.x * uvf.x * (3.0 - 2.0 * uvf.x);
    float sy = uvf.y * uvf.y * (3.0 - 2.0 * uvf.y);
    float a = mix(f00, f10, sx);
    float b = mix(f01, f11, sx);
    float t = mix(a, b, sy);
    return (t / 0.7 + 1.0) * 0.5;
}
float ncFbm(vec2 uv, float r)
{
    float n0 = ncNoise(uv, r);
    float n1 = ncNoise(uv * 2.0, r) * 0.5;
    return (n0 + n1) / 1.5;
}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0)
             * (2.4 * zoom * (0.7 + detail));
    float r = u_time * 0.55;
    float noise_fac = ncFbm(p * (2.0 + 2.5 * detail), r);
    float bands = 18.0 + 28.0 * detail;
    float contour = 0.5 * (1.0 - cos(bands * 3.14159265 * noise_fac));
    float clip = smoothstep(0.55, mix(0.85, 0.98, contrast / 2.5), contour);
    float h = fract(hue + noise_fac * 0.35 + u_time * 0.02);
    vec3 c0 = vec3(0.85, 0.05, 0.75);
    vec3 c1 = vec3(0.05, 0.85, 0.90);
    vec3 rgb = mix(c0, c1, noise_fac);
    vec3 tint = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(rgb, tint, 0.35);
    out_color = vec4(rgb * clip * (0.35 + 0.65 * contour), 1.0);
}
