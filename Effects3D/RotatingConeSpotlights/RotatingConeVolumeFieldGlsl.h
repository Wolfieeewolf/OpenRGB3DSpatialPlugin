// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/* R=sat G=val B=base_hue01. Hub at origin-local 0.5.
 * u_params: [0]=spin_t [1]=scale [2]=hue [3]=count [4]=motion [5]=surface
 *           [6]=wander [7]=elev [8]=hw [9]=hh [10]=hd
 *           [11..18]=u0,v0..u3,v3
 */
inline const char* RotatingConeVolumeFieldGlsl()
{
    return R"(
vec3 coneLocal(vec3 p, vec3 aim)
{
    vec3 z = aim;
    float len = length(z);
    if(len < 1e-5)
        z = vec3(0.0, 0.0, 1.0);
    else
        z /= len;
    vec3 up = (abs(z.y) < 0.92) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 x = normalize(cross(up, z));
    vec3 y = cross(z, x);
    return vec3(dot(p, x), dot(p, y), dot(p, z));
}
vec3 resolveApex(int surface, float u, float v, vec3 origin01, float hw, float hh, float hd)
{
    float uu = clamp(u, 0.0, 1.0);
    float vv = clamp(v, 0.0, 1.0);
    float dx = (uu - 0.5) * 2.0 * hw;
    float dz = (vv - 0.5) * 2.0 * hd;

    if(surface == 0 || surface == 1)
        return clamp(origin01 + vec3(dx, 0.0, dz), 0.02, 0.98);
    if(surface == 2)
        return clamp(vec3(origin01.x + dx, origin01.y + hh, origin01.z + dz), 0.02, 0.98);
    if(surface == 3)
        return clamp(vec3(origin01.x + dx, origin01.y - hh, origin01.z + dz), 0.02, 0.98);

    float ang = uu * 6.2831853;
    float wall_r = hd * 0.92;
    return clamp(vec3(origin01.x + wall_r * cos(ang),
                     origin01.y + (vv - 0.5) * 2.0 * hh,
                     origin01.z + wall_r * sin(ang)), 0.02, 0.98);
}
vec3 aimWander(int i, int count, int motion_mode, float spin_t, float wander, float elev_bias)
{
    int pair = (i / 2) * 2;
    int is_follower = 0;
    if(motion_mode > 0 && i != pair)
    {
        if(count == 3 && i == 2)
            is_follower = 0;
        else
            is_follower = 1;
    }
    int src = i;
    if(is_follower == 1)
        src = pair;

    float seed = float(src) * 1.6180339;
    float rate = 0.55 + 0.22 * float(src);
    float t = spin_t * rate + seed * 2.3999632;

    float yaw = t
              + wander * 0.85 * sin(t * 0.73 + seed * 4.1)
              + wander * 0.45 * sin(t * 1.37 + seed * 2.7)
              + wander * 0.25 * sin(t * 2.11 + seed);
    float pitch = elev_bias
                + wander * 0.55 * sin(t * 0.91 + seed * 3.3)
                + wander * 0.35 * sin(t * 1.67 + seed * 1.9);

    if(is_follower == 1)
    {
        yaw += 3.14159265;
        pitch = elev_bias - (pitch - elev_bias);
    }

    pitch = clamp(pitch, -1.35, 1.35);
    float cp = cos(pitch);
    return normalize(vec3(cos(yaw) * cp, sin(pitch), sin(yaw) * cp));
}
vec3 evalCone(int i, int count, int motion_mode, int surface,
              float u, float v, vec3 origin01, float hw, float hh, float hd,
              float spin_t, float wander, float elev_bias, float scale, float hue_static,
              vec3 p01)
{
    if(i >= count)
        return vec3(0.0);

    vec3 apex = resolveApex(surface, u, v, origin01, hw, hh, hd);
    vec3 aim = aimWander(i, count, motion_mode, spin_t, wander, elev_bias);

    if(surface == 4)
    {
        vec3 inward = normalize(origin01 - apex);
        if(length(inward) < 1e-5)
            inward = vec3(0.0, 0.0, -1.0);
        aim = normalize(mix(inward, aim, 0.65));
    }
    else if(surface == 2)
        aim = normalize(mix(vec3(0.0, -1.0, 0.0), aim, 0.70));
    else if(surface == 3)
        aim = normalize(mix(vec3(0.0, 1.0, 0.0), aim, 0.70));

    vec3 local = coneLocal(p01 - apex, aim);
    if(local.z <= 0.0)
        return vec3(0.0);

    float radial = sqrt((local.x * local.x + local.y * local.y) / max(scale, 0.00001));
    float dist = clamp(local.z - radial, -1.0, 1.0);
    float sat = clamp(1.0 - dist, 0.0, 1.0);
    float val = clamp(pow(max(0.0, 1.0 + dist), 4.0), 0.0, 1.0);
    float denom = max(float(count), 1.0);
    float h = fract(hue_static + float(i) / denom + 0.07 * apex.x);
    return vec3(sat, val, h);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float spin_t = u_params[0];
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

    if(surface == 2)
        elev_bias = -0.55;
    else if(surface == 3)
        elev_bias = 0.55;
    else if(surface == 4)
        elev_bias = 0.0;

    vec3 best = vec3(0.0);
    vec3 c0 = evalCone(0, count, motion_mode, surface, u_params[11], u_params[12], origin01, hw, hh, hd,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    vec3 c1 = evalCone(1, count, motion_mode, surface, u_params[13], u_params[14], origin01, hw, hh, hd,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    vec3 c2 = evalCone(2, count, motion_mode, surface, u_params[15], u_params[16], origin01, hw, hh, hd,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    vec3 c3 = evalCone(3, count, motion_mode, surface, u_params[17], u_params[18], origin01, hw, hh, hd,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    if(c0.x > best.x) best = c0;
    if(c1.x > best.x) best = c1;
    if(c2.x > best.x) best = c2;
    if(c3.x > best.x) best = c3;

    out_color = vec4(best, 1.0);
}
)";
}
