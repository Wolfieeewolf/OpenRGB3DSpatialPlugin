// SPDX-License-Identifier: GPL-2.0-only
// From dump 003Warpy — neon warp tunnels (clamped for LED-safe intensity).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
vec3 nwPalette(float t)
{
    vec3 a = vec3(0.960, 0.260, 0.580);
    vec3 b = vec3(0.900, 0.138, 0.450);
    vec3 c = vec3(0.520, 0.200, 0.520);
    vec3 d = vec3(-0.60, -0.90, -0.09);
    return a + b * cos(6.28318 * (c * t + d));
}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.8 * zoom);
    vec2 p0 = p;
    vec3 final_color = vec3(0.0);
    float dens = 3.5 + 3.0 * detail;

    vec2 u = p;
    u = u - 0.5;
    u *= sin(0.0 - length(u));
    float d = length(u) * exp(-length(p0));
    d = sin(d * dens + u_time * 0.3) * 0.6;
    d = abs(d);
    d = pow(0.03 / max(d, 0.02), mix(0.85, 1.25, contrast / 2.5));
    d = clamp(d, 0.0, 2.5);
    final_color += nwPalette(length(p0) - u_time * 0.25 + 10.0 + hue) * d;

    u = p;
    u.x = -u.x;
    u = u - 0.5 + 1.0;
    u *= sin(1.0 - length(u));
    d = length(u) * exp(-length(p0));
    d = sin(d * dens + u_time * 0.3) * 0.6;
    d = abs(d);
    d = pow(0.03 / max(d, 0.02), mix(0.85, 1.25, contrast / 2.5));
    d = clamp(d, 0.0, 2.5);
    final_color += nwPalette(length(p0) - u_time * 0.25 + 10.0 + hue) * d;

    u = p;
    u = u - 0.5 + 2.0;
    u *= sin(2.0 - length(u));
    d = length(u) * exp(-length(p0));
    d = sin(d * dens + u_time * 0.3) * 0.6;
    d = abs(d);
    d = pow(0.03 / max(d, 0.02), mix(0.85, 1.25, contrast / 2.5));
    d = clamp(d, 0.0, 2.5);
    final_color += nwPalette(length(p0) - u_time * 0.25 + 10.0 + hue) * d;

    float fall = 1.0 / max(length(p0), 0.35);
    final_color *= clamp(fall * 0.55, 0.0, 1.35);
    out_color = vec4(clamp(final_color, 0.0, 1.0), 1.0);
}
