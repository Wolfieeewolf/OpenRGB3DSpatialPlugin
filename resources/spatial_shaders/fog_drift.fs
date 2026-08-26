// SPDX-License-Identifier: GPL-2.0-only
// Fog Drift — soft rolling haze
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float fdHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float fdNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = fdHash(i);
    float b = fdHash(i + vec2(1.0, 0.0));
    float c = fdHash(i + vec2(0.0, 1.0));
    float d = fdHash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.7 * zoom);
    float t = u_time * 0.25;
    float n = 0.0;
    float amp = 0.55;
    vec2 q = p * (1.5 + 2.5 * detail) + vec2(t, -t * 0.6);
    for(int i = 0; i < 4; i++)
    {
        n += fdNoise(q) * amp;
        q *= 2.05;
        amp *= 0.5;
    }
    float v = pow(clamp(n, 0.0, 1.0), mix(1.5, 0.7, contrast / 2.5));
    float h = fract(0.62 + hue * 0.2 + n * 0.1 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.45, 0.55, 0.7), rgb, 0.4);
    out_color = vec4(mix(vec3(0.06, 0.07, 0.1), rgb, v), 1.0);
}
