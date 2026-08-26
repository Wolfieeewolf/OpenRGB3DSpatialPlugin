// SPDX-License-Identifier: GPL-2.0-only
// Melt Petals — soft floral wash
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.8 * zoom);
    float t = u_time * 0.35;
    float r = length(p);
    float a = atan(p.y, p.x);
    float petals = 3.0 + floor(5.0 * detail + 0.5);
    float flower = 0.5 + 0.5 * cos(a * petals + sin(r * 4.0 - t) * 1.2);
    float melt = sin(r * (5.0 + 6.0 * detail) - t * 1.4 + flower * 2.0);
    float v = pow(clamp(0.5 + 0.45 * melt * flower, 0.0, 1.0), mix(1.8, 0.8, contrast / 2.5));
    v *= exp(-r * 0.55);
    float h = fract(hue + flower * 0.12 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.95, 0.55, 0.75), rgb, 0.65);
    out_color = vec4(mix(vec3(0.05, 0.03, 0.06), rgb, v), 1.0);
}
