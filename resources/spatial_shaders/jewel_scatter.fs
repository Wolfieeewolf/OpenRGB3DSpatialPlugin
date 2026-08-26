// SPDX-License-Identifier: GPL-2.0-only
// Jewel Scatter — soft sparkle dots
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float jsHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (3.0 * zoom * (0.75 + detail));
    p += vec2(u_time * 0.12, -u_time * 0.08);
    vec2 g = floor(p);
    vec2 f = fract(p) - 0.5;
    float h0 = jsHash(g);
    float tw = 0.45 + 0.55 * sin(u_time * (3.0 + 5.0 * h0) + h0 * 30.0);
    float jewel = exp(-dot(f, f) * mix(14.0, 32.0, contrast / 2.5)) * step(0.62, h0) * tw;
    float trail = exp(-abs(f.y) * 10.0) * exp(-abs(f.x) * 4.0) * step(0.78, h0) * 0.45;
    float v = clamp(jewel + trail, 0.0, 1.0);
    float hh = fract(hue + h0 * 0.55 + u_time * 0.025);
    vec3 rgb = clamp(abs(mod(hh * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
