// SPDX-License-Identifier: GPL-2.0-only
// Potential Rings — soft EM field
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.1 * zoom);
    float t = u_time * 0.65;
    vec2 c0 = vec2(sin(t * 0.4), cos(t * 0.35)) * 0.35;
    vec2 c1 = vec2(cos(t * 0.3), sin(t * 0.45)) * 0.4;
    float pot = 1.0 / max(length(p - c0), 0.08) - 1.0 / max(length(p - c1), 0.08);
    float rings = sin(pot * (2.5 + 5.0 * detail) - t);
    float v = pow(clamp(0.5 + 0.5 * rings, 0.0, 1.0), mix(1.9, 0.85, contrast / 2.5));
    v *= exp(-length(p) * 0.4);
    float h = fract(hue + pot * 0.05 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.03, 0.04, 0.08), rgb, v), 1.0);
}
