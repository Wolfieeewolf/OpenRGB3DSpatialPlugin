void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    int idx = int(clamp(uv.x, 0.0, 0.999) * 127.0);
    float low = u_audio[0];
    float mid = u_audio[48];
    float high = u_audio[110];
    float band = u_audio[idx];
    float t = u_time * 0.2;
    float glow = band * 0.85 + mid * 0.25 + low * 0.15;
    glow = clamp(glow, 0.0, 1.0);
    vec3 bass_col = vec3(0.35, 0.12, 0.55);
    vec3 mid_col = vec3(0.12, 0.45, 0.75);
    vec3 high_col = vec3(0.85, 0.75, 0.35);
    vec3 base = mix(bass_col, mid_col, clamp(mid * 1.2, 0.0, 1.0));
    base = mix(base, high_col, clamp(high * 1.1, 0.0, 1.0));
    float vignette = 0.65 + 0.35 * sin(uv.y * 3.14159 + t);
    out_color = vec4(base * glow * vignette, 1.0);
}
