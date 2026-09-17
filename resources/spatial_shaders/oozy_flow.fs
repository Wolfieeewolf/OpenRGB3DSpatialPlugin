// SPDX-License-Identifier: GPL-2.0-only
// Oozy Flow — soft molten wash
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.9 * zoom);
    float t = u_time * 0.45;
    float dens = 2.0 + 4.5 * detail;
    float v = 0.0;
    for(int i = 0; i < 5; i++)
    {
        float fi = float(i);
        vec2 q = p + vec2(sin(t * 0.7 + fi * 1.3), cos(t * 0.55 + fi * 0.9)) * (0.15 + 0.05 * fi);
        v += sin(q.x * dens + t + fi) * cos(q.y * dens * 0.85 - t * 0.8 + fi);
    }
    v = pow(clamp(v * 0.28 + 0.5, 0.0, 1.0), mix(1.7, 0.75, contrast / 2.5));
    float h = fract(hue + v * 0.18 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.85, 0.35, 0.15), rgb, 0.55);
    out_color = vec4(mix(vec3(0.06, 0.03, 0.02), rgb, v), 1.0);
}
