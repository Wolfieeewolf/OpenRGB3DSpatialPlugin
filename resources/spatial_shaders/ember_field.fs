void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    vec2 p = (uv - 0.5) * 2.0;
    float r = length(p);
    float t = u_time;
    float beat = 0.0;
    for(int i = 0; i < 8; i++)
    {
        beat += u_audio[i];
    }
    beat = clamp(beat / 8.0, 0.0, 1.0);
    float ring = exp(-pow((r - 0.35 - beat * 0.15) * 6.0, 2.0));
    float swirl = sin(atan(p.y, p.x) * 5.0 + t * 0.6) * 0.5 + 0.5;
    float ember = ring * (0.4 + 0.6 * swirl);
    vec3 col = mix(vec3(0.02, 0.0, 0.01), vec3(1.0, 0.35, 0.05), ember);
    out_color = vec4(col * (0.35 + 0.65 * beat), 1.0);
}
