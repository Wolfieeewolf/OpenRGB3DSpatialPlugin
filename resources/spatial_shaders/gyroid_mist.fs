// SPDX-License-Identifier: GPL-2.0-only
// Gyroid Mist — soft SDF wash
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.4 * zoom);
    float t = u_time * 0.4;
    float s = 2.2 + 4.0 * detail;
    float g = sin(p.x * s + t) * cos(p.y * s - t * 0.8)
            + sin((p.x + p.y) * s * 0.7 - t)
            + cos(length(p) * s * 0.9 + t * 1.1);
    float v = pow(clamp(0.5 + 0.5 * g * 0.55, 0.0, 1.0), mix(1.7, 0.8, contrast / 2.5));
    float mist = exp(-length(p) * 0.35) * (0.55 + 0.45 * v);
    float h = fract(hue + v * 0.15 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(0.04, 0.05, 0.09), rgb, mist), 1.0);
}
