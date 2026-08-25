// Soft Aurora — layered curtains (Shader Field preset).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.6 * zoom);
    float t = u_time * 0.35;
    float v = 0.0;
    for(int i = 0; i < 5; i++)
    {
        float fi = float(i);
        float band = 0.55 + 0.45 * sin(p.x * (1.2 + fi * 0.35 * detail) + t * (0.6 + fi * 0.15)
                                       + sin(p.y * 2.2 + t * 0.4 + fi));
        float curtain = exp(-abs(p.y + 0.15 * sin(p.x * 1.5 + t + fi) - (fi - 2.0) * 0.18) * (3.0 + detail * 2.0));
        v += band * curtain * (0.45 / (1.0 + fi * 0.35));
    }
    v = pow(clamp(v, 0.0, 1.0), mix(1.4, 0.7, contrast / 2.5));

    float h = fract(0.55 + hue + v * 0.12 + t * 0.02);
    float s = 0.55 + 0.35 * v;
    float vv = 0.20 + 0.80 * v;
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, s) * vv, 1.0);
}
