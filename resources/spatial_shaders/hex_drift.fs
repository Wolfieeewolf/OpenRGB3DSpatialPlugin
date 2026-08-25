// Hex Drift — soft hexagonal lattice wash (WLED-matrix energy, room-friendly).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (3.2 * zoom * (0.7 + detail));
    float t = u_time * 0.4;
    p += vec2(sin(t * 0.7), cos(t * 0.55)) * 0.15;

    const float s3 = 0.8660254;
    vec2 r = vec2(1.0, s3);
    vec2 h1 = r * 0.5;
    vec2 a = mod(p, r) - h1;
    vec2 b = mod(p - h1, r) - h1;
    vec2 gv = (dot(a, a) < dot(b, b)) ? a : b;
    float d = max(abs(gv.x) * 0.866 + abs(gv.y) * 0.5, abs(gv.y));
    float edge = 1.0 - smoothstep(0.28, 0.42, d);
    float pulse = 0.5 + 0.5 * sin(t * 2.0 + (p.x + p.y) * 1.5);
    float v = pow(clamp(edge * (0.55 + 0.45 * pulse), 0.0, 1.0), mix(1.6, 0.85, contrast / 2.5));

    float hh = fract(hue + d * 0.35 + t * 0.05);
    float sat = 0.7;
    float vv = 0.12 + 0.88 * v;
    vec3 rgb = clamp(abs(mod(hh * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, sat) * vv, 1.0);
}
