// SPDX-License-Identifier: GPL-2.0-only
// Psyche Grid — soft 60s lattice
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.3 * zoom);
    float t = u_time * 0.35;
    float cells = 3.0 + 7.0 * detail;
    vec2 g = abs(fract(p * cells + vec2(t * 0.2, -t * 0.15)) - 0.5);
    float grid = 1.0 - smoothstep(0.38, 0.48, max(g.x, g.y));
    float swirl = 0.5 + 0.5 * sin((p.x + p.y) * cells * 0.5 + t * 2.0);
    float v = pow(clamp(mix(swirl * 0.55, 1.0, grid), 0.0, 1.0), mix(1.5, 0.75, contrast / 2.5));
    float h = fract(hue + floor(p.x * cells) * 0.08 + floor(p.y * cells) * 0.05 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.08, 0.02, 0.1), rgb, v), 1.0);
}
