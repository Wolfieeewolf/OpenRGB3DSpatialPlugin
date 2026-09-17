// SPDX-License-Identifier: GPL-2.0-only
// Dense Chroma — packed color plasma
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.4 * zoom);
    float t = u_time * 0.95;
    float dens = 3.5 + 8.0 * detail;
    float v = sin(p.x * dens + t) + sin(p.y * dens * 1.1 - t)
            + sin((p.x + p.y) * dens * 0.55 + t * 1.3)
            + cos(length(p) * dens * 0.9 - t * 0.8)
            + sin(dot(p, vec2(0.7, -0.5)) * dens + t * 0.6);
    v = pow(clamp(v * 0.18 + 0.5, 0.0, 1.0), mix(1.6, 0.7, contrast / 2.5));
    float h = fract(v * 1.4 + hue + t * 0.04);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(1.0), rgb, 0.9) * (0.25 + 0.75 * v), 1.0);
}
