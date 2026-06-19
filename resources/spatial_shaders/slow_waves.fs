void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float t = u_time * 0.25;
    float wave = sin(uv.x * 12.0 + t) * 0.5 + sin(uv.y * 9.0 - t * 0.8) * 0.5;
    wave = wave * 0.5 + 0.5;
    float bands = smoothstep(0.35, 0.65, wave);
    vec3 deep = vec3(0.05, 0.08, 0.22);
    vec3 crest = vec3(0.25, 0.55, 0.95);
    vec3 col = mix(deep, crest, bands);
    out_color = vec4(col, 1.0);
}
