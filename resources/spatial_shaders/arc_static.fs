// SPDX-License-Identifier: GPL-2.0-only
// Arc Static — soft electric wash
// u_params: [0]=zoom [1]=contrast [2]=hue01 [3]=detail
float asHash(vec2 p){return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.2 * zoom);
    float t = u_time * 1.1;
    float bolts = 0.0;
    for(int i = 0; i < 5; i++)
    {
        float fi = float(i);
        float ang = fi * 1.2566 + t * 0.15;
        vec2 dir = vec2(cos(ang), sin(ang));
        float along = dot(p, dir);
        float across = abs(dot(p, vec2(-dir.y, dir.x)));
        float jag = asHash(vec2(floor(along * (4.0 + 6.0 * detail) + t * 3.0), fi));
        float w = exp(-across * mix(10.0, 22.0, contrast / 2.5) / (0.35 + jag));
        bolts += w * (0.35 + 0.65 * jag);
    }
    float v = clamp(bolts * 0.55, 0.0, 1.0);
    float h = fract(0.55 + hue * 0.25 + v * 0.1 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.55, 0.75, 1.0), rgb, 0.35);
    out_color = vec4(mix(vec3(0.02, 0.03, 0.08), rgb, v), 1.0);
}
