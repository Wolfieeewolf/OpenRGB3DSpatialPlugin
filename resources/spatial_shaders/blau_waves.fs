// SPDX-License-Identifier: GPL-2.0-only
// Blau Waves — soft blue bands
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.8 * zoom);
    float t = u_time * 0.55;
    float dens = 3.5 + 8.0 * detail;
    float w = sin(p.y * dens + t) + 0.55 * sin(p.x * dens * 0.45 - t * 0.7)
            + 0.35 * sin((p.x + p.y) * dens * 0.35 + t * 1.1);
    float v = pow(clamp(w * 0.4 + 0.5, 0.0, 1.0), mix(1.8, 0.8, contrast / 2.5));
    float h = fract(0.58 + hue * 0.15 + v * 0.08 + t * 0.015);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.15, 0.35, 0.85), rgb, 0.45);
    out_color = vec4(mix(vec3(0.02, 0.04, 0.12), rgb, v), 1.0);
}
