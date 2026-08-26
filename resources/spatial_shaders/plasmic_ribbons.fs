// SPDX-License-Identifier: GPL-2.0-only
// Plasmic Ribbons — flowing bands
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.1 * zoom);
    float t = u_time * 0.7;
    float dens = 2.5 + 6.0 * detail;
    float ribbon = sin(p.x * dens + sin(p.y * 2.2 + t) * 1.8 + t)
                 + sin(p.y * dens * 0.8 - cos(p.x * 1.6 - t) * 1.4 - t * 0.9);
    float v = pow(clamp(ribbon * 0.35 + 0.5, 0.0, 1.0), mix(1.8, 0.8, contrast / 2.5));
    float h = fract(hue + ribbon * 0.08 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.8) * (0.2 + 0.8 * v), 1.0);
}
