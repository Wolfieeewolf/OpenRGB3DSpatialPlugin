void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / u_resolution.y, 1.0) * 3.2;
    float t = u_time * 0.35;
    float v = sin(p.x + t) + sin(p.y * 1.1 - t * 0.9);
    v += sin((p.x + p.y) * 0.7 + t * 1.3);
    v = v * 0.33 + 0.5;
    vec3 col = vec3(0.15, 0.35 + 0.25 * v, 0.55 + 0.35 * sin(v * 6.28 + t));
    out_color = vec4(col * (0.55 + 0.45 * v), 1.0);
}
