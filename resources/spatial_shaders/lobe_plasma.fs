// SPDX-License-Identifier: GPL-2.0-only
// From dump AnotherPlasma — multi-lobe product plasma (distinct from Room Plasma).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.35 * zoom);
    float t = u_time;
    float n_its = 3.0 + floor(2.0 * detail + 0.5);
    float n_d_prod = 1.0;
    float i0 = 0.0;
    n_d_prod += sin(length(p - vec2(sin(0.0), cos(0.0)) * sin(t * 0.2 + 0.0)) * (8.0 + 4.0 * detail));
    i0 = 1.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    i0 = 2.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    i0 = 3.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    i0 = 4.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));

    float n1 = 0.5 + 0.5 * sin(n_d_prod * 4.0 + t * 2.2);
    float n2 = 0.5 + 0.5 * sin(n_d_prod * 2.0 + t * 2.2);
    float n3 = 0.5 + 0.5 * sin(n_d_prod * 1.0 + t * 2.2);
    float v = pow(clamp((n1 + n2 + n3) / 3.0, 0.0, 1.0), contrast);
    float h = fract(hue + n1 * 0.2 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    vec3 col = mix(vec3(n1, n2, n3), rgb, 0.55) * (0.25 + 0.75 * v);
    out_color = vec4(clamp(col, 0.0, 1.0), 1.0);
}
