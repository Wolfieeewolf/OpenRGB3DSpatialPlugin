// SPDX-License-Identifier: GPL-2.0-only
// From dump 4RadialWave — waves from four corners (distinct from Soft Ripples).
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);

    vec2 p = (uv - 0.5) * zoom + 0.5;
    float vel = 1.6 + 2.2 * detail;
    float len = mix(18.0, 8.0, detail);
    float t = u_time * vel;
    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 q = (p - 0.5) * vec2(aspect, 1.0) + 0.5;

    float d0 = length((q - vec2(0.0, 0.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float d1 = length((q - vec2(1.0, 0.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float d2 = length((q - vec2(0.0, 1.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float d3 = length((q - vec2(1.0, 1.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float w = sin(d0 / len - t) + sin(d1 / len - t) + sin(d2 / len - t) + sin(d3 / len - t);
    float v = pow(clamp(0.5 + 0.25 * w, 0.0, 1.0), contrast);
    float h = fract(hue + 0.08 * w + u_time * 0.02);
    vec3 deep = vec3(0.02, 0.05, 0.14);
    vec3 crest = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    crest = crest * crest * (3.0 - 2.0 * crest);
    out_color = vec4(mix(deep, crest, v), 1.0);
}
