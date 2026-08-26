// SPDX-License-Identifier: GPL-2.0-only
// Cell Bloom — organic cells
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float cbHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (3.0 * zoom * (0.7 + detail));
    p += vec2(u_time * 0.08, -u_time * 0.05);
    vec2 g = floor(p);
    vec2 f = fract(p);
    float md = 8.0;
    float mid = 8.0;
    for(int j = -1; j <= 1; j++)
    for(int i = -1; i <= 1; i++)
    {
        vec2 o = vec2(float(i), float(j));
        vec2 r = o + vec2(cbHash(g + o), cbHash(g + o + 19.0)) - f;
        float d = dot(r, r);
        if(d < md){mid = md; md = d;}
        else if(d < mid) mid = d;
    }
    float edge = clamp(sqrt(mid) - sqrt(md), 0.0, 1.0);
    float v = pow(clamp(1.0 - md * 1.8, 0.0, 1.0), mix(1.6, 0.7, contrast / 2.5));
    v *= 0.55 + 0.45 * smoothstep(0.02, 0.12, edge);
    float h = fract(hue + md * 0.35 + u_time * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.03, 0.04, 0.07), rgb, v), 1.0);
}
