// SPDX-License-Identifier: GPL-2.0-only
// Atom Plasma — compact multi-lobe plasma
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.85;
    float dens = 1.4 + 2.8 * detail;
    float v = 0.0;
    for(int i = 0; i < 4; i++)
    {
        float fi = float(i);
        vec2 c = vec2(cos(t * 0.4 + fi * 1.7), sin(t * 0.35 + fi * 2.1)) * (0.25 + 0.12 * fi);
        float d = length(p - c);
        v += sin(d * dens * 6.2831853 - t * (1.2 + 0.2 * fi)) * exp(-d * (2.2 + detail));
    }
    v = pow(clamp(v * 0.55 + 0.5, 0.0, 1.0), mix(1.8, 0.75, contrast / 2.5));
    float h = fract(v * 0.7 + hue + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.85) * (0.2 + 0.8 * v), 1.0);
}
