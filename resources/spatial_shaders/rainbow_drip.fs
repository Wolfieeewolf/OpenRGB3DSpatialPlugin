// SPDX-License-Identifier: GPL-2.0-only
// Rainbow Drip — falling color streaks
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.8;
    float cols = 4.0 + 10.0 * detail;
    float x = p.x * cols;
    float id = floor(x);
    float f = fract(x) - 0.5;
    float drip = fract(p.y * (1.2 + 0.8 * detail) - t * (0.6 + 0.5 * fract(sin(id * 12.9898) * 43758.5)) + id * 0.17);
    float streak = exp(-abs(f) * mix(8.0, 18.0, contrast / 2.5)) * pow(1.0 - abs(drip * 2.0 - 1.0), 2.2);
    float v = clamp(streak, 0.0, 1.0);
    float h = fract(hue + id * 0.07 + drip * 0.1);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
