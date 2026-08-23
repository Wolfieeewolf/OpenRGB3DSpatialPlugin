// Room Plasma — saturated multi-lobe plasma (very different from Slow Waves).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.2 * zoom);
    float t = u_time * 0.9;
    float dens = 1.0 + 2.4 * detail;
    float v = 0.0;
    v += sin(p.x * dens + t);
    v += sin(p.y * dens * 1.15 - t * 0.95);
    v += sin((p.x + p.y) * dens * 0.65 + t * 1.25);
    v += cos(length(p) * dens * 1.4 - t);
    v = v * 0.25 + 0.5;
    v = pow(clamp(v, 0.0, 1.0), contrast);

    float h = fract(v * 0.85 + hue + t * 0.04);
    float s = 0.85;
    float vv = 0.35 + 0.65 * v;
    // HSV → RGB
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    vec3 col = mix(vec3(1.0), rgb, s) * vv;
    out_color = vec4(col, 1.0);
}
