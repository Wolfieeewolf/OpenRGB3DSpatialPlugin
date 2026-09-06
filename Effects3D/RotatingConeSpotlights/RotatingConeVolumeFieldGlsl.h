// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/* R=sat G=val B=base_hue01. Occupancy UV, Spatial Anchor at 0.5.
 * Each cone: Pixelblaze-style rotate-space + double cone through its apex.
 * dist = |rz| - sqrt(rx^2/scale + ry^2/scale); hsv(x, 1-dist, (1+dist)^4)
 * u_params: [0]=clock [1]=scale [2]=hue [3]=count [4]=motion [5]=surface
 *           [6]=wander [7]=elev [8]=hw [9]=hh [10]=hd
 *           [11..18]=u0,v0..u3,v3
 */
inline const char* RotatingConeVolumeFieldGlsl()
{
    return R"(
float rcTri(float v)
{
    v = fract(v);
    return (v < 0.5) ? (v * 2.0) : (2.0 - v * 2.0);
}
vec3 rcRotate(vec3 p, vec3 axis, float angle)
{
    float len = length(axis);
    if(len < 1e-5)
        return p;
    vec3 u = axis / len;
    float c = cos(angle);
    float s = sin(angle);
    return p * c + cross(u, p) * s + u * dot(u, p) * (1.0 - c);
}
vec3 resolveApex(int surface, float u, float v, vec3 origin01, float hw, float hh, float hd)
{
    float uu = clamp(u, 0.0, 1.0);
    float vv = clamp(v, 0.0, 1.0);
    float dx = (uu - 0.5) * 2.0 * hw;
    float dz = (vv - 0.5) * 2.0 * hd;

    if(surface == 1)
    {
        dx *= 0.45;
        dz *= 0.45;
        return origin01 + vec3(dx, 0.0, dz);
    }
    if(surface == 0)
        return origin01 + vec3(dx, 0.0, dz);
    if(surface == 2)
        return vec3(origin01.x + dx, origin01.y + hh, origin01.z + dz);
    if(surface == 3)
        return vec3(origin01.x + dx, origin01.y - hh, origin01.z + dz);

    float t = uu * 4.0;
    float wall = floor(t);
    float s = t - wall;
    if(wall >= 4.0)
    {
        wall = 0.0;
        s = 0.0;
    }
    float y = origin01.y + (vv - 0.5) * 2.0 * hh;
    if(wall < 0.5)
        return vec3(origin01.x - hw + s * 2.0 * hw, y, origin01.z - hd);
    if(wall < 1.5)
        return vec3(origin01.x + hw, y, origin01.z - hd + s * 2.0 * hd);
    if(wall < 2.5)
        return vec3(origin01.x + hw - s * 2.0 * hw, y, origin01.z + hd);
    return vec3(origin01.x - hw, y, origin01.z + hd - s * 2.0 * hd);
}
vec3 coneAxis(int src, float clock, float wander, float elev_bias)
{
    float seed = float(src) * 0.37;
    float t1 = 2.0 * rcTri(clock * 0.73 + seed) - 1.0;
    float t2 = 2.0 * rcTri(clock * 0.91 + seed * 1.7) - 1.0;
    float t3 = 2.0 * rcTri(clock * 1.11 + seed * 0.9) - 1.0;
    vec3 tumble = vec3(t1, t2, t3);
    vec3 rest = vec3(0.0, (elev_bias >= 0.0) ? 1.0 : -1.0, 0.0);
    if(abs(elev_bias) < 0.05)
        rest = vec3(t1 * 0.15, 1.0, t3 * 0.15);
    float w = clamp(wander, 0.15, 2.0);
    vec3 a = mix(rest, tumble, clamp(w * 0.55, 0.0, 1.0));
    float n = length(a);
    if(n < 1e-5)
        return rest;
    return a / n;
}
float coneAngle(int src, float clock)
{
    return fract(clock * 0.50 + float(src) * 0.19) * 6.2831853;
}
vec3 evalCone(int i, int count, int motion_mode, int surface,
              float u, float v, vec3 origin01, float hw, float hh, float hd,
              float clock, float wander, float elev_bias, float scale, float hue_static,
              vec3 p01)
{
    if(i >= count)
        return vec3(-2.0, hue_static, 0.0);

    int pair = (i / 2) * 2;
    int src = i;
    int opposite = 0;
    if(motion_mode > 0 && i != pair)
    {
        if(!(count == 3 && i == 2))
        {
            src = pair;
            opposite = 1;
        }
    }

    vec3 apex = resolveApex(surface, u, v, origin01, hw, hh, hd);
    vec3 axis = coneAxis(src, clock, wander, elev_bias);
    float ang = coneAngle(src, clock);
    if(opposite == 1)
        ang += 3.14159265;

    vec3 r = rcRotate(p01 - apex, axis, ang);
    float dist = abs(r.z) - sqrt(r.x * r.x / scale + r.y * r.y / scale);
    float h = fract(hue_static + (p01.x - 0.5) * 0.35 + float(i) / max(float(count), 1.0));
    return vec3(dist, h, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float clock = u_params[0];
    float scale = max(u_params[1], 0.00001);
    float hue_static = u_params[2];
    int count = int(clamp(u_params[3], 1.0, 4.0) + 0.5);
    int motion_mode = int(clamp(u_params[4], 0.0, 1.0) + 0.5);
    int surface = int(clamp(u_params[5], 0.0, 4.0) + 0.5);

    vec3 origin01 = vec3(0.5);
    float wander = clamp(u_params[6], 0.15, 2.0);
    float elev_bias = clamp(u_params[7], -1.2, 1.2);
    float hw = max(u_params[8], 0.02);
    float hh = max(u_params[9], 0.02);
    float hd = max(u_params[10], 0.02);

    vec3 best = vec3(-2.0, hue_static, 0.0);
    vec3 c0 = evalCone(0, count, motion_mode, surface, u_params[11], u_params[12], origin01, hw, hh, hd,
                       clock, wander, elev_bias, scale, hue_static, p01);
    vec3 c1 = evalCone(1, count, motion_mode, surface, u_params[13], u_params[14], origin01, hw, hh, hd,
                       clock, wander, elev_bias, scale, hue_static, p01);
    vec3 c2 = evalCone(2, count, motion_mode, surface, u_params[15], u_params[16], origin01, hw, hh, hd,
                       clock, wander, elev_bias, scale, hue_static, p01);
    vec3 c3 = evalCone(3, count, motion_mode, surface, u_params[17], u_params[18], origin01, hw, hh, hd,
                       clock, wander, elev_bias, scale, hue_static, p01);
    if(c0.z > 0.5 && c0.x > best.x) best = c0;
    if(c1.z > 0.5 && c1.x > best.x) best = c1;
    if(c2.z > 0.5 && c2.x > best.x) best = c2;
    if(c3.z > 0.5 && c3.x > best.x) best = c3;

    float dist = clamp(best.x, -1.0, 1.0);
    float sat = clamp(1.0 - dist, 0.0, 1.0);
    float val = clamp(pow(max(0.0, 1.0 + dist), 4.0), 0.0, 1.0);
    out_color = vec4(sat, val, best.y, 1.0);
}
)";
}
