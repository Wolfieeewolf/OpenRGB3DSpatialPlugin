// SPDX-License-Identifier: GPL-2.0-only
// From dump 002Blobby — soft neon blobs (clamped).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
vec3 sbPalette(float t)
{
    vec3 a = vec3(0.660, 0.560, 0.680);
    vec3 b = vec3(0.718, 0.438, 0.720);
    vec3 c = vec3(0.520, 0.100, 0.520);
    vec3 d = vec3(-0.60, -0.30, -0.09);
    return a + b * cos(6.28318 * (c * t + d));
}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.7 * zoom);
    vec2 p0 = p;
    vec3 final_color = vec3(0.0);
    float dens = 6.0 + 6.0 * detail;

    vec2 u = p - 0.5;
    u *= sin(0.0 - length(p0));
    float d = length(u) * exp(-length(p0));
    d = abs(sin(d * dens + u_time * 0.25) * 0.5);
    d = pow(0.03 / max(d, 0.02), mix(1.0, 1.45, contrast / 2.5));
    d = clamp(d, 0.0, 2.2);
    final_color += sbPalette(length(p0) - u_time * 0.25 + hue) * d;

    u = p - 0.5 + 1.0;
    u *= sin(5.0 - length(p0));
    d = length(u) * exp(-length(p0));
    d = abs(sin(d * dens + u_time * 0.25) * 0.5);
    d = pow(0.03 / max(d, 0.02), mix(1.0, 1.45, contrast / 2.5));
    d = clamp(d, 0.0, 2.2);
    final_color += sbPalette(length(p0) - u_time * 0.25 + hue) * d;

    u = p - 0.5 + 2.0;
    u *= sin(10.0 - length(p0));
    d = length(u) * exp(-length(p0));
    d = abs(sin(d * dens + u_time * 0.25) * 0.5);
    d = pow(0.03 / max(d, 0.02), mix(1.0, 1.45, contrast / 2.5));
    d = clamp(d, 0.0, 2.2);
    final_color += sbPalette(length(p0) - u_time * 0.25 + hue) * d;

    final_color *= clamp(1.0 / max(length(p0), 0.4), 0.0, 1.4);
    out_color = vec4(clamp(final_color, 0.0, 1.0), 1.0);
}
