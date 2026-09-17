// SPDX-License-Identifier: GPL-2.0-only
// Vortex Swirl — spiral field
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float r = length(p);
    float a = atan(p.y, p.x);
    float arms = 2.0 + floor(4.0 * detail + 0.5);
    float spiral = sin(a * arms + r * (6.0 + 8.0 * detail) - u_time * 1.6);
    float v = pow(clamp(0.5 + 0.5 * spiral, 0.0, 1.0), mix(2.0, 0.85, contrast / 2.5));
    v *= exp(-r * 0.65);
    float rings = 0.5 + 0.5 * sin(r * (10.0 + 12.0 * detail) - u_time * 2.0);
    v = max(v, rings * exp(-r * 1.1) * 0.55);
    float h = fract(hue + a * 0.08 + r * 0.12 + u_time * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
