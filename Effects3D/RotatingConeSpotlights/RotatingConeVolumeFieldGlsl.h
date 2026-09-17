// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/* R=sat G=val B=base_hue01. Occupancy UV, Spatial Anchor at 0.5.
 * Center: fixtures at the Anchor (occupancy mid-plane). Floor / Ceiling: room
 * slab Y, occupancy XZ. Walls: room occupancy faces. Mirror = opposite beam.
 * u_params: [0]=clock [1]=cone_scale [2]=hue [3]=count [4]=mirror [5]=surface
 *           [6]=wander [7]=floor_y [8]=ceil_y [9]=xmin [10]=xmax [11]=zmin [12]=zmax
 *           [13..20]=u0,v0..u3,v3
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
vec3 resolveApex(int surface, float u, float v,
                 float floor_y, float ceil_y,
                 float xmin, float xmax, float zmin, float zmax)
{
    float uu = clamp(u, 0.0, 1.0);
    float vv = clamp(v, 0.0, 1.0);

    if(surface == 2)
        return vec3(uu, ceil_y, vv);
    if(surface == 3)
        return vec3(uu, floor_y, vv);
    if(surface == 4)
    {
        float y = mix(floor_y, ceil_y, vv);
        float t = uu * 4.0;
        float wall = floor(t);
        float s = t - wall;
        if(wall >= 4.0)
        {
            wall = 0.0;
            s = 0.0;
        }
        if(wall < 0.5)
            return vec3(mix(xmin, xmax, s), y, zmin);
        if(wall < 1.5)
            return vec3(xmax, y, mix(zmin, zmax, s));
        if(wall < 2.5)
            return vec3(mix(xmax, xmin, s), y, zmax);
        return vec3(xmin, y, mix(zmax, zmin, s));
    }
    return vec3(uu, 0.5, vv);
}
vec3 restAim(int surface, vec3 apex)
{
    if(surface == 2)
        return vec3(0.0, -1.0, 0.0);
    if(surface == 3)
        return vec3(0.0, 1.0, 0.0);
    if(surface == 4)
    {
        vec3 inward = vec3(0.5, apex.y, 0.5) - vec3(apex.x, apex.y, apex.z);
        inward.y = 0.0;
        float n = length(inward);
        if(n < 1e-5)
            return vec3(0.0, 0.0, 1.0);
        return inward / n;
    }
    return vec3(0.0, 1.0, 0.0);
}
vec3 toAimFrame(vec3 p, vec3 aim)
{
    float len = length(aim);
    vec3 z = (len < 1e-5) ? vec3(0.0, 1.0, 0.0) : (aim / len);
    vec3 up = (abs(z.y) > 0.92) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 x = cross(up, z);
    float xn = length(x);
    if(xn < 1e-5)
        x = vec3(1.0, 0.0, 0.0);
    else
        x /= xn;
    vec3 y = cross(z, x);
    return vec3(dot(p, x), dot(p, y), dot(p, z));
}
vec3 coneAim(int src, float clock, float wander, vec3 rest)
{
    float seed = float(src) * 0.37;
    float t1 = 2.0 * rcTri(clock * 0.73 + seed) - 1.0;
    float t2 = 2.0 * rcTri(clock * 0.91 + seed * 1.7) - 1.0;
    float t3 = 2.0 * rcTri(clock * 1.11 + seed * 0.9) - 1.0;
    vec3 tumble = vec3(t1, t2, t3);
    float nrest = length(rest);
    vec3 r0 = (nrest < 1e-5) ? vec3(0.0, 1.0, 0.0) : (rest / nrest);
    float w = clamp(wander, 0.15, 2.0);
    vec3 a = mix(r0, tumble, clamp(w * 0.55, 0.0, 1.0));
    float n = length(a);
    if(n < 1e-5)
        return r0;
    return a / n;
}
float coneAngle(int src, float clock)
{
    return fract(clock * 0.50 + float(src) * 0.19) * 6.2831853;
}
vec3 evalCone(int i, int count, int mirror, int surface,
              float u, float v,
              float floor_y, float ceil_y,
              float xmin, float xmax, float zmin, float zmax,
              float clock, float wander, float scale, float hue_static,
              vec3 p01)
{
    if(i >= count)
        return vec3(-1.0e6, hue_static, 0.0);

    vec3 apex = resolveApex(surface, u, v, floor_y, ceil_y, xmin, xmax, zmin, zmax);
    vec3 aim = coneAim(i, clock, wander, restAim(surface, apex));
    vec3 r = toAimFrame(p01 - apex, aim);
    r = rcRotate(r, vec3(0.0, 0.0, 1.0), coneAngle(i, clock));
    float hypotv = sqrt(r.x * r.x / scale + r.y * r.y / scale);
    float axial = (mirror > 0) ? abs(r.z) : r.z;
    float dist = axial - hypotv;
    if(mirror == 0 && r.z < 0.0)
        dist = -1.0;
    float h = fract(hue_static + (p01.x - 0.5) * 0.35 + float(i) / max(float(count), 1.0));
    return vec3(dist, h, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float clock = u_params[0];
    float scale = max(u_params[1], 0.00001);
    float hue_static = u_params[2];
    int count = int(clamp(u_params[3], 1.0, 4.0) + 0.5);
    int mirror = int(clamp(u_params[4], 0.0, 1.0) + 0.5);
    int surface = int(clamp(u_params[5], 0.0, 4.0) + 0.5);
    if(surface == 1)
        surface = 0;

    float wander = clamp(u_params[6], 0.15, 2.0);
    float floor_y = u_params[7];
    float ceil_y = u_params[8];
    float xmin = u_params[9];
    float xmax = u_params[10];
    float zmin = u_params[11];
    float zmax = u_params[12];

    vec3 best = vec3(-1.0e6, hue_static, 0.0);
    vec3 c0 = evalCone(0, count, mirror, surface, u_params[13], u_params[14],
                       floor_y, ceil_y, xmin, xmax, zmin, zmax,
                       clock, wander, scale, hue_static, p01);
    vec3 c1 = evalCone(1, count, mirror, surface, u_params[15], u_params[16],
                       floor_y, ceil_y, xmin, xmax, zmin, zmax,
                       clock, wander, scale, hue_static, p01);
    vec3 c2 = evalCone(2, count, mirror, surface, u_params[17], u_params[18],
                       floor_y, ceil_y, xmin, xmax, zmin, zmax,
                       clock, wander, scale, hue_static, p01);
    vec3 c3 = evalCone(3, count, mirror, surface, u_params[19], u_params[20],
                       floor_y, ceil_y, xmin, xmax, zmin, zmax,
                       clock, wander, scale, hue_static, p01);
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
