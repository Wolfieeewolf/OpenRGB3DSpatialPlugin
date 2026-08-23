// Ripple Ember — expanding fire rings / spiral (was ember_field; no audio).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * (2.0 * zoom);
    float r = length(p);
    float a = atan(p.y, p.x);
    float t = u_time;

    float rings = 2.0 + 5.0 * detail;
    float ring = abs(sin(r * rings * 3.14159 - t * 1.6));
    ring = pow(1.0 - ring, mix(1.2, 4.0, contrast * 0.35));
    float swirl = 0.5 + 0.5 * sin(a * (3.0 + 4.0 * detail) + t * 0.9 + r * 4.0);
    float ember = clamp(ring * (0.35 + 0.65 * swirl) * (1.15 - r), 0.0, 1.0);
    ember = pow(ember, contrast);

    float h = fract(hue + ember * 0.12 + t * 0.02);
    vec3 cool = vec3(0.02, 0.0, 0.03);
    vec3 hot = vec3(1.0, 0.28 + 0.25 * cos(h * 6.2831853), 0.04);
    vec3 col = mix(cool, hot, ember);
    out_color = vec4(col, 1.0);
}
