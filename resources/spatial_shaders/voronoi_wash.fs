// SPDX-License-Identifier: GPL-2.0-only
// Voronoi Wash — soft cellular lattice
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float vwHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    float scale = 2.2 + 5.0 * detail;
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (zoom * scale);
    p += 0.15 * vec2(sin(u_time * 0.35), cos(u_time * 0.28));
    vec2 g = floor(p);
    vec2 f = fract(p);
    float md = 8.0;
    for(int j = -1; j <= 1; j++)
    for(int i = -1; i <= 1; i++)
    {
        vec2 o = vec2(float(i), float(j));
        vec2 n = vec2(vwHash(g + o), vwHash(g + o + 7.1));
        n = 0.5 + 0.5 * sin(u_time * 0.6 + 6.2831853 * n);
        float d = length(o + n - f);
        md = min(md, d);
    }
    float v = pow(clamp(1.0 - md, 0.0, 1.0), mix(2.2, 0.9, contrast / 2.5));
    float rim = pow(clamp(1.0 - abs(md - 0.35) * 4.0, 0.0, 1.0), 2.0);
    v = max(v * 0.75, rim);
    float h = fract(hue + md * 0.4 + u_time * 0.025);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02), rgb, v), 1.0);
}
