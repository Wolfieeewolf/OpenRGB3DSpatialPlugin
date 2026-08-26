// SPDX-License-Identifier: GPL-2.0-only
// Petal Spin — rotating soft petals
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.9 * zoom);
    float t = u_time * 0.4;
    float r = length(p);
    float a = atan(p.y, p.x) + t;
    float petals = 4.0 + floor(6.0 * detail + 0.5);
    float flower = 0.5 + 0.5 * cos(a * petals);
    float ring = sin(r * (5.0 + 7.0 * detail) - t * 1.5);
    float v = pow(clamp(0.5 + 0.45 * flower * ring, 0.0, 1.0), mix(1.7, 0.8, contrast / 2.5));
    v *= exp(-r * 0.5);
    float h = fract(hue + flower * 0.1 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.9, 0.4, 0.65), rgb, 0.55);
    out_color = vec4(mix(vec3(0.05, 0.02, 0.06), rgb, v), 1.0);
}
