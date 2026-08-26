// SPDX-License-Identifier: GPL-2.0-only
// Oil Slick — iridescent layers
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.4;
    float dens = 2.2 + 5.0 * detail;
    float a = sin(p.x * dens + t) * cos(p.y * dens * 0.9 - t);
    float b = sin((p.x - p.y) * dens * 0.7 - t * 0.8);
    float c = cos(length(p) * dens * 1.1 + t * 0.6);
    float v = pow(clamp((a + b + c) * 0.28 + 0.5, 0.0, 1.0), mix(1.6, 0.75, contrast / 2.5));
    float h = fract(hue + a * 0.12 + b * 0.08 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(0.05, 0.06, 0.04), rgb, v), 1.0);
}
