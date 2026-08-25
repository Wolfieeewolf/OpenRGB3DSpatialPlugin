// Soft Ripples — expanding rings for room projection.
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
    float t = u_time * 0.85;
    float freq = 2.2 + 3.5 * detail;
    float wave = sin(r * freq - t * 2.2) * 0.55 + sin(r * freq * 0.55 - t * 1.1 + 1.2) * 0.35;
    float crest = pow(0.5 + 0.5 * wave, mix(2.8, 1.4, contrast / 2.5));
    float fall = exp(-r * 0.55);
    float v = clamp(crest * fall, 0.0, 1.0);

    float h = fract(hue + r * 0.08 + t * 0.03);
    float s = 0.75;
    float vv = 0.15 + 0.85 * v;
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, s) * vv, 1.0);
}
