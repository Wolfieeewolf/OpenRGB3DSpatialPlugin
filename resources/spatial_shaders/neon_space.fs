// SPDX-License-Identifier: GPL-2.0-only
// Neon Space — soft star haze
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float nsHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.6 * zoom * (0.8 + detail));
    p += 0.1 * vec2(sin(u_time * 0.2), cos(u_time * 0.17));
    vec2 g = floor(p);
    vec2 f = fract(p) - 0.5;
    float h0 = nsHash(g);
    float tw = 0.5 + 0.5 * sin(u_time * (2.0 + 4.0 * h0) + h0 * 20.0);
    float star = exp(-dot(f, f) * mix(18.0, 40.0, contrast / 2.5)) * step(0.72, h0) * tw;
    float haze = 0.15 + 0.25 * sin(length(p) * 2.0 - u_time * 0.4);
    float v = clamp(star + haze * 0.35, 0.0, 1.0);
    float hh = fract(hue + h0 * 0.3 + u_time * 0.02);
    vec3 rgb = clamp(abs(mod(hh * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.01, 0.02, 0.06), rgb, v), 1.0);
}
