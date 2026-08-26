// SPDX-License-Identifier: GPL-2.0-only
// Fluid Swirl — soft current
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.5;
    vec2 q = p;
    q.x += 0.35 * sin(q.y * (2.0 + 3.0 * detail) + t);
    q.y += 0.35 * cos(q.x * (2.0 + 3.0 * detail) - t * 0.9);
    float v = sin(q.x * (3.0 + 5.0 * detail) + t) * cos(q.y * (3.0 + 4.0 * detail) - t);
    v = pow(clamp(v * 0.5 + 0.5, 0.0, 1.0), mix(1.7, 0.8, contrast / 2.5));
    float h = fract(hue + length(q) * 0.08 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.2, 0.55, 0.85), rgb, 0.65);
    out_color = vec4(mix(vec3(0.03, 0.05, 0.1), rgb, v), 1.0);
}
