// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Rotating cone spotlights: R=sat, G=val, B=base_hue01 (Frequency scroll on CPU).
 *  Static per-cone apex from surface + U/V; aim wanders on a full sphere.
 *  Opposite motion locks cone i to pair-leader (i/2*2) at +pi yaw / mirrored pitch.
 *  When count==3, cone 2 stays independent even in Opposite mode.
 *
 *  GLSL 110 constraints honored here:
 *   - uniform float u_params[] only indexed with constant expressions
 *   - no inout parameters / no bool (use int flags)
 *
 *  u_params: [0]=spin_t [1]=scale [2]=hue_static [3]=count [4]=motion_mode
 *            [5]=surface [6..13]=u0,v0..u3,v3
 *            [14]=wander (or floor(wander*100)+ox when surface==Ref)
 *            [15]=elev_bias (or floor(oy*4095)+oz when surface==Ref)
 *  surface: 0 Center, 1 Ref, 2 Ceiling, 3 Floor, 4 Walls
 *  motion_mode: 0 Independent, 1 Opposite
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
vec3 resolveApex(int surface, float u, float v, vec3 ref_o)
{
    float uu = clamp(u, 0.0, 1.0);
    float vv = clamp(v, 0.0, 1.0);
    if(surface == 0)
        return vec3(mix(0.12, 0.88, uu), 0.5, mix(0.12, 0.88, vv));
    if(surface == 1)
    {
        vec3 o = clamp(ref_o, 0.05, 0.95);
        float dx = (uu - 0.5) * 0.70;
        float dz = (vv - 0.5) * 0.70;
        return clamp(vec3(o.x + dx, o.y, o.z + dz), 0.05, 0.95);
    }
    if(surface == 2)
        return vec3(mix(0.10, 0.90, uu), 0.92, mix(0.10, 0.90, vv));
    if(surface == 3)
        return vec3(mix(0.10, 0.90, uu), 0.08, mix(0.10, 0.90, vv));
    float ang = uu * 6.2831853;
    return vec3(0.5 + 0.46 * cos(ang), mix(0.12, 0.88, vv), 0.5 + 0.46 * sin(ang));
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
/* Returns vec3(sat, val, hue). sat==0 means this cone contributes nothing. */
vec3 evalCone(int i, int count, int motion_mode, int surface,
              float u, float v, vec3 ref_o,
              float spin_t, float wander, float elev_bias, float scale, float hue_static,
              vec3 p01)
{
    if(i >= count)
        return vec3(0.0);

    vec3 apex = resolveApex(surface, u, v, ref_o);
    vec3 aim = aimWander(i, count, motion_mode, spin_t, wander, elev_bias);

    if(surface == 4)
    {
        vec3 inward = normalize(vec3(0.5, 0.5, 0.5) - apex);
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

    float wander = 1.0;
    float elev_bias = 0.0;
    vec3 ref_o = vec3(0.5);
    if(surface == 1)
    {
        float packed_ox = u_params[14];
        float wander_i = floor(packed_ox);
        wander = clamp(wander_i / 100.0, 0.15, 2.0);
        float packed_oyoz = u_params[15];
        float oy = clamp(floor(packed_oyoz) / 4095.0, 0.0, 1.0);
        float oz = clamp(fract(packed_oyoz), 0.0, 1.0);
        ref_o = vec3(clamp(fract(packed_ox), 0.0, 1.0), oy, oz);
        elev_bias = 0.0;
    }
    else
    {
        wander = clamp(u_params[14], 0.15, 2.0);
        elev_bias = clamp(u_params[15], -1.2, 1.2);
    }
    if(surface == 2) elev_bias = -0.55;
    else if(surface == 3) elev_bias = 0.55;
    else if(surface == 4) elev_bias = 0.0;

    vec3 best = vec3(0.0);
    vec3 c0 = evalCone(0, count, motion_mode, surface, u_params[6],  u_params[7],  ref_o,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    vec3 c1 = evalCone(1, count, motion_mode, surface, u_params[8],  u_params[9],  ref_o,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    vec3 c2 = evalCone(2, count, motion_mode, surface, u_params[10], u_params[11], ref_o,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    vec3 c3 = evalCone(3, count, motion_mode, surface, u_params[12], u_params[13], ref_o,
                       spin_t, wander, elev_bias, scale, hue_static, p01);
    if(c0.x > best.x) best = c0;
    if(c1.x > best.x) best = c1;
    if(c2.x > best.x) best = c2;
    if(c3.x > best.x) best = c3;

    out_color = vec4(best, 1.0);
}
)";
}
