// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** GLSL volumeMain for LaserVector — ShowEditor-style Beam + Graphic sampling. */
inline const char* LaserVectorVolumeBody()
{
    return R"GLSL(
/* u_media: height = stroke count, width = 3
   texel (0,i): laser x0,y0,x1,y1 in [0,1] (from [-1,1])
   texel (1,i): laser z0,z1,track01,_
   texel (2,i): r,g,b,dwell01
   u_params:
     0 ox  1 oy  2 oz  3 thickness  4 width_dyn
     5 show_beams  6 crisp  7 inten_widen  8 dwell_widen
     9 stroke_count  10 machine  11 brightness  12 max_beams
     13 flatten_z  14 show_graphic
     15 fog  16 transparency  17 beam_gain  18 screen
     19 multi_projectors (0/1)
     20..55 projector origins for Spur/track 0..11 (ox,oy,oz × 12)
*/

float lvDecodeLaser(float u01) { return u01 * 2.0 - 1.0; }

vec3 lvFarUv(float lx, float ly)
{
    return vec3(clamp(0.5 + 0.5 * lx, 0.0, 1.0),
                clamp(0.5 + 0.5 * ly, 0.0, 1.0),
                1.0);
}

float lvSmoothstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-6), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

float lvAlongEnv(float along)
{
    float a = clamp(along, 0.0, 1.0);
    return lvSmoothstep(0.02, 0.10, a) * (1.0 - lvSmoothstep(0.78, 1.0, a));
}

float lvDistSeg3(vec3 p, vec3 a, vec3 b)
{
    vec3 d = b - a;
    float len2 = dot(d, d);
    float t = 0.0;
    if(len2 > 1e-12) t = clamp(dot(p - a, d) / len2, 0.0, 1.0);
    return length(p - (a + t * d));
}

float lvDistSeg2(vec2 p, vec2 a, vec2 b)
{
    vec2 d = b - a;
    float len2 = dot(d, d);
    float t = 0.0;
    if(len2 > 1e-12) t = clamp(dot(p - a, d) / len2, 0.0, 1.0);
    return length(p - (a + t * d));
}

void lvRayHit(vec3 p, vec3 o, vec3 f, out float dist, out float along)
{
    vec3 d = f - o;
    float len2 = dot(d, d);
    float t = 0.0;
    if(len2 > 1e-12) t = clamp(dot(p - o, d) / len2, 0.0, 1.0);
    dist = length(p - (o + t * d));
    along = t;
}

void lvWedgeHit(vec3 p, vec3 o, vec3 a, vec3 b, out float dist, out float along)
{
    vec3 ab = b - a;
    if(dot(ab, ab) < 1e-12)
    {
        lvRayHit(p, o, a, dist, along);
        return;
    }
    vec3 oa = a - o;
    vec3 ob = b - o;
    vec3 c = cross(oa, ob);
    float c2 = dot(c, c);
    if(c2 < 1e-16)
    {
        float d0, d1, a0, a1;
        lvRayHit(p, o, a, d0, a0);
        lvRayHit(p, o, b, d1, a1);
        if(d0 <= d1) { dist = d0; along = a0; }
        else { dist = d1; along = a1; }
        return;
    }
    vec3 n = normalize(c);
    float sd = dot(p - o, n);
    vec3 r = (p - o) - sd * n;
    float u = dot(cross(r, ob), c) / c2;
    float v = dot(cross(oa, r), c) / c2;
    float wo = 1.0 - u - v;
    if(u >= 0.0 && v >= 0.0 && wo >= 0.0)
    {
        dist = abs(sd);
        along = clamp(u + v, 0.0, 1.0);
        return;
    }
    float d0, d1, a0, a1;
    lvRayHit(p, o, a, d0, a0);
    lvRayHit(p, o, b, d1, a1);
    float dab = lvDistSeg3(p, a, b);
    dist = d0; along = a0;
    if(d1 < dist) { dist = d1; along = a1; }
    if(dab < dist) { dist = dab; along = 1.0; }
}

float lvPresence(float dist, float thick, float crisp)
{
    float t = max(0.001, thick);
    if(dist >= t) return 0.0;
    float linear = 1.0 - (dist / t);
    if(crisp > 0.5)
    {
        if(linear >= 0.30) return 1.0;
        return clamp(linear / 0.30, 0.0, 1.0);
    }
    return linear;
}

float lvThick(float inten, float dwell, float base, float dyn, float iw, float dw)
{
    float mul = 1.0;
    float dmax = max(1.0, dyn);
    if(iw > 0.5) mul *= (1.0 + (dmax - 1.0) * clamp(inten, 0.0, 1.0));
    if(dw > 0.5)
    {
        float d = clamp(log2(1.0 + max(dwell, 1.0)) / 5.0, 0.0, 1.0);
        mul *= (1.0 + (dmax - 1.0) * 0.75 * d);
    }
    return max(0.001, base * mul);
}

void lvMapMachine(inout vec3 rgb, float machine)
{
    int m = int(machine + 0.5);
    if(m == 1) { rgb.b = 0.0; }
    else if(m == 2)
    {
        float yellow = min(rgb.r, rgb.g) * (1.0 - rgb.b);
        if(yellow > 0.05)
        {
            rgb.r = min(1.0, rgb.r + yellow * 0.35);
            rgb.g = min(1.0, rgb.g + yellow * 0.45);
            rgb.b *= (1.0 - yellow * 0.6);
        }
    }
    else if(m == 3)
    {
        float lum = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
        rgb = vec3(0.0, lum, 0.0);
    }
    else if(m == 4)
    {
        float lum = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
        rgb = vec3(lum);
    }
    else if(m == 5)
    {
        if(rgb.r < 0.12) rgb.r = 0.0; else rgb.r = pow(rgb.r, 0.65);
        if(rgb.g < 0.12) rgb.g = 0.0; else rgb.g = pow(rgb.g, 0.65);
        if(rgb.b < 0.12) rgb.b = 0.0; else rgb.b = pow(rgb.b, 0.65);
    }
}

vec3 lvProjectorOrigin(int track)
{
    /* Single-head fallback: shared ox/oy/oz. */
    if(u_params[19] < 0.5)
    {
        return vec3(u_params[0], u_params[1], u_params[2]);
    }
    int t = clamp(track, 0, 11);
    int b = 20 + t * 3;
    return vec3(u_params[b], u_params[b + 1], u_params[b + 2]);
}

void volumeMain(out vec4 out_color, in vec3 p01)
{
    float thick0 = u_params[3];
    float wdyn = u_params[4];
    float show_beams = u_params[5];
    float crisp = u_params[6];
    float iw = u_params[7];
    float dw = u_params[8];
    int n = int(u_params[9] + 0.5);
    float machine = u_params[10];
    float bright = u_params[11];
    int maxb = int(u_params[12] + 0.5);
    float flatten = u_params[13];
    float show_graphic = u_params[14];
    float fog = clamp(u_params[15], 0.0, 1.0);
    float transparency = clamp(u_params[16], 0.0, 1.0);
    float beam_gain = clamp(u_params[17], 0.0, 1.0);
    float screen = clamp(u_params[18], 0.0, 1.0);

    out_color = vec4(0.0);
    if(n <= 0) return;
    if(maxb > 0) n = min(n, maxb);

    bool projector = (show_beams > 0.5) || (show_graphic > 0.5);
    float best_d = 1e9;
    float best_presence = 0.0;
    vec3 best_rgb = vec3(0.0);
    bool hit = false;

    for(int i = 0; i < 256; ++i)
    {
        if(i >= n) break;
        vec4 a = texelFetch(u_media, ivec2(0, i), 0);
        vec4 z = texelFetch(u_media, ivec2(1, i), 0);
        vec4 b = texelFetch(u_media, ivec2(2, i), 0);
        float x0 = lvDecodeLaser(a.r);
        float y0 = lvDecodeLaser(a.g);
        float x1 = lvDecodeLaser(a.b);
        float y1 = lvDecodeLaser(a.a);
        float z0 = lvDecodeLaser(z.r);
        float z1 = lvDecodeLaser(z.g);
        int track = int(z.b * 255.0 + 0.5);
        vec3 o = lvProjectorOrigin(track);
        vec3 rgb = b.rgb;
        float dwell = 1.0 + b.a * 15.0;
        float inten = max(rgb.r, max(rgb.g, rgb.b));
        float thick = lvThick(inten, dwell, thick0, wdyn, iw, dw);

        float dist = 1e9;
        float presence = 0.0;

        if(projector)
        {
            vec3 f0 = lvFarUv(x0, y0);
            vec3 f1 = lvFarUv(x1, y1);
            if(show_beams > 0.5)
            {
                float bd, ba;
                lvWedgeHit(p01, o, f0, f1, bd, ba);
                float bp = lvPresence(bd, thick, crisp) * lvAlongEnv(ba)
                           * fog * beam_gain * transparency;
                if(bp > presence)
                {
                    presence = bp;
                    dist = bd;
                }
            }
            if(show_graphic > 0.5)
            {
                float gd = lvDistSeg3(p01, f0, f1);
                float gp = lvPresence(gd, thick * 0.55, crisp) * screen * transparency;
                if(gp > presence)
                {
                    presence = gp;
                    dist = gd;
                }
            }
        }
        else if(flatten > 0.5)
        {
            vec2 lp = p01.xy * 2.0 - 1.0;
            dist = lvDistSeg2(lp, vec2(x0, y0), vec2(x1, y1));
            presence = lvPresence(dist, thick, crisp) * transparency;
        }
        else
        {
            vec3 lp = p01 * 2.0 - 1.0;
            dist = lvDistSeg3(lp, vec3(x0, y0, z0), vec3(x1, y1, z1));
            presence = lvPresence(dist, thick, crisp) * transparency;
        }

        if(presence <= 0.0) continue;
        if(dist < best_d || (dist == best_d && presence > best_presence))
        {
            vec3 c = rgb;
            lvMapMachine(c, machine);
            best_d = dist;
            best_presence = presence;
            best_rgb = c;
            hit = true;
        }
    }

    if(!hit) return;
    /* Presence already includes fog / beam / screen / transparency. */
    float gain = clamp(bright * clamp(best_presence, 0.0, 1.0), 0.0, 1.0);
    vec3 lit = clamp(best_rgb * gain, 0.0, 1.0);
    /* FinishRgb swaps B↔R on overlay — pack OpenRGB B,G,R. LED path unswaps. */
    out_color = vec4(lit.b, lit.g, lit.r, 1.0);
}
)GLSL";
}
