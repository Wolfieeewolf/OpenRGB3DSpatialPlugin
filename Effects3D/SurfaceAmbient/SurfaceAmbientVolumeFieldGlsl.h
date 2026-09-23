// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Surface Ambient volume field: R=shell intensity, G=plasma01, B=hotness01.
 *  p01 = unit room [0,1]^3 (floor y=0, ceiling y=1).
 *  u_params: [0]=surf_mask [1]=style (0=None, 1..7=Fire..Steam)
 *            [2]=motion [3]=height_pct [4]=sigma
 *            [5]=freq [6]=speed [7]=band_speed_mul [8]=feature_size
 *  Clock is always u_time (params[7] only scales it). GLSL 1.10 — no int clamp().
 *  Globals: Speed→[6], Detail→[5], Size→[8], Scale→[3] (shell depth).
 *
 *  Material look borrows Shadertoy / voxel-field tricks: domain warp, ridge &
 *  cellular noise, caustic interference, sparse hot cores — still soft enough
 *  for LED shells, but with sharper material structure.
 */
inline const char* SurfaceAmbientVolumeFieldGlsl()
{
    return R"(
float saHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float saHash3(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}
float saNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = saHash(i);
    float b = saHash(i + vec2(1.0, 0.0));
    float c = saHash(i + vec2(0.0, 1.0));
    float d = saHash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
float saFbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for(int i = 0; i < 5; ++i)
    {
        v += a * saNoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}
/* iq-style ridge — cracks / crust. */
float saRidge(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for(int i = 0; i < 4; ++i)
    {
        float n = saNoise(p);
        v += a * (1.0 - abs(n * 2.0 - 1.0));
        p *= 2.07;
        a *= 0.5;
    }
    return clamp(v, 0.0, 1.0);
}
/* Cheap cellular (Worley-ish) for slime blobs / ember points. */
float saCellular(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float md = 8.0;
    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            vec2 g = vec2(float(x), float(y));
            vec2 o = vec2(saHash(i + g), saHash(i + g + vec2(19.1, 7.3)));
            vec2 r = g + o - f;
            md = min(md, dot(r, r));
        }
    }
    return sqrt(max(md, 0.0));
}
/* Domain warp — organic flow instead of flat noise wash. */
vec2 saWarp(vec2 p, float t, float amt)
{
    float n1 = saFbm(p * 1.35 + vec2(t * 0.21, -t * 0.17));
    float n2 = saFbm(p * 1.35 + vec2(3.7, 1.9) - vec2(t * 0.19, t * 0.23));
    return p + amt * vec2(n1 * 2.0 - 1.0, n2 * 2.0 - 1.0);
}
/* Dual-fbm caustic interference (water / ocean). */
float saCaustic(vec2 p, float t, float scale)
{
    float a = saFbm(p * scale + vec2(t * 0.55, -t * 0.35));
    float b = saFbm(p * scale * 1.27 + vec2(4.1, 2.3) - vec2(t * 0.42, t * 0.48));
    float c = abs(a - b);
    return pow(1.0 - clamp(c * 1.65, 0.0, 1.0), 2.4);
}
float saHasBit(float mask, float bit)
{
    return step(0.5, mod(floor(mask / bit), 2.0));
}

/* x=plasma, y=sparse intensity mul, z=hotness (cores / specular). */
vec3 saEvalPreset(int style, int role, float alongA, float alongB, float up01,
                  float time, float freq, float speed, float feature)
{
    float t = time * max(0.08, speed);
    float feat = max(0.45, feature);
    float f_fire = max(0.18, freq) / max(0.7, feat * 0.85);
    float f_big = max(0.12, freq) / max(0.55, feat * 1.15);
    float f = (style == 0 || style == 4) ? f_fire : f_big;
    float plasma = 0.5;
    float sparse_mul = 1.0;
    float hot = 0.0;

    if(style == 0)
    {
        /* Fire — domain-warped rising tongues + sparse white-hot cores. */
        if(role == 0)
        {
            vec2 q = saWarp(vec2(alongA, alongB) * (2.4 * f), t * 0.6, 0.28);
            float bed = saFbm(q + vec2(t * 0.22, -t * 0.16));
            float coal = saNoise(q * 2.4 + vec2(t * 0.7, t * 0.5));
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            float glow = pow(max(0.0, coal - 0.48), 1.6);
            float flick = 0.55 + 0.45 * sin(t * 7.5 + bed * 10.0);
            plasma = clamp((0.22 + 0.48 * bed + 0.18 * (1.0 - edge)) * flick + glow * 0.55, 0.0, 1.0);
            hot = clamp(glow * 1.4 * flick, 0.0, 1.0);
            sparse_mul = 0.55 + 0.45 * plasma;
        }
        else if(role == 1)
        {
            vec2 q = vec2(alongA, alongB) * (7.5 * f) + vec2(t * 1.5, t * 1.1);
            float spark = saNoise(q);
            float cell = saCellular(vec2(alongA, alongB) * (5.5 * f) + vec2(t * 0.4, -t * 0.3));
            float flick = 0.5 + 0.5 * sin(t * 11.0 + spark * 14.0);
            float ember = pow(max(0.0, spark - 0.58), 1.55) * 2.6 * flick;
            float pop = pow(max(0.0, 0.42 - cell), 2.2) * 1.8 * flick;
            plasma = clamp(max(ember, pop), 0.0, 1.0);
            hot = clamp(plasma * 1.1, 0.0, 1.0);
            sparse_mul = 0.28 + 0.72 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.62;
            vec2 q0 = vec2(alongA * (2.8 * f), rise * (3.8 * f));
            vec2 q = saWarp(q0, t, 0.42);
            float turb = saFbm(q);
            float tongue = saFbm(q * 1.55 + vec2(turb * 0.6, 0.0));
            float base_boost = pow(1.0 - up01, 1.15);
            float body = pow(max(0.0, tongue - 0.28), 1.25) * (0.40 + 0.60 * base_boost);
            float core = pow(max(0.0, tongue - 0.52), 2.4) * base_boost;
            float lick = abs(sin(alongA * 10.0 * f + turb * 3.0 - t * 5.0)) * base_boost * 0.35;
            plasma = clamp((0.55 * body + 1.15 * core + lick) * (0.55 + 0.45 * turb), 0.0, 1.0);
            plasma *= mix(1.0, 0.18, clamp(up01 * 1.35, 0.0, 1.0));
            hot = clamp(core * 1.6 + lick * 0.4, 0.0, 1.0);
            sparse_mul = 0.40 + 0.60 * plasma;
        }
    }
    else if(style == 1)
    {
        /* Water — caustics, specular glints, pouring sheets. */
        if(role == 0)
        {
            vec2 p = vec2(alongA, alongB);
            float r = length(p - vec2(0.5));
            float caus = saCaustic(saWarp(p * (2.2 * f), t, 0.22), t, 1.0);
            float ring = abs(sin(r * 12.0 * f - t * 5.8));
            float slosh = saFbm(saWarp(p * (1.8 * f), t * 0.8, 0.18));
            float glint = pow(max(0.0, saNoise(p * (9.0 * f) + vec2(t * 3.0, 0.0)) - 0.72), 2.0);
            plasma = clamp(0.18 + 0.42 * slosh + 0.48 * caus + 0.28 * (1.0 - ring) * (1.0 - clamp(r * 1.4, 0.0, 1.0)) + glint, 0.0, 1.0);
            hot = clamp(glint * 1.5 + caus * 0.35, 0.0, 1.0);
        }
        else if(role == 1)
        {
            vec2 p = vec2(alongA, alongB);
            float r = length(p - vec2(0.5));
            float pour = exp(-r * r * 7.5) * (0.45 + 0.55 * saFbm(p * (3.2 * f) + vec2(t * 2.6, 0.0)));
            float caus = saCaustic(p * (3.0 * f), t * 1.2, 1.15);
            float outward = abs(sin(r * 15.0 * f - t * 4.8));
            plasma = clamp(pour * 1.6 + caus * 0.55 * (1.0 - clamp(r, 0.0, 1.0)) + (1.0 - outward) * 0.22, 0.0, 1.0);
            hot = clamp(pour * 0.7 + caus * 0.5, 0.0, 1.0);
        }
        else
        {
            vec2 q = saWarp(vec2(alongA * (1.6 * f), (1.0 - up01) * (2.8 * f) - t * 2.8), t, 0.25);
            float sheet = saFbm(q);
            float caus = saCaustic(q, t, 1.3);
            float streaks = abs(sin(alongA * 10.0 * f + sheet * 2.5 - t * 4.5));
            float foam = pow(max(0.0, saNoise(vec2(alongA, up01) * (5.0 * f) - vec2(0.0, t * 1.8)) - 0.55), 1.6);
            plasma = clamp((0.22 + 0.50 * sheet + 0.40 * caus) * (0.30 + 0.70 * up01) * (0.40 + 0.60 * (1.0 - streaks * 0.7)) + foam * 0.35, 0.0, 1.0);
            hot = clamp(caus * 0.65 + foam * 0.9, 0.0, 1.0);
        }
    }
    else if(style == 2)
    {
        /* Slime — cellular blobs, viscous drip, glossy highlights. */
        if(role == 0)
        {
            vec2 q = saWarp(vec2(alongA, alongB) * (2.0 * f), t * 0.35, 0.20);
            float cell = saCellular(q * 2.2);
            float pool = 1.0 - clamp(cell * 1.35, 0.0, 1.0);
            float settle = 0.55 + 0.45 * sin(pool * 6.28318 + t * 0.65);
            float gloss = pow(max(0.0, 0.38 - cell), 2.8);
            plasma = clamp(0.20 + 0.70 * pool * settle + gloss * 0.55, 0.0, 1.0);
            hot = clamp(gloss * 1.4, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float cell_id = saNoise(vec2(floor(alongA * 4.2 * f), floor(alongB * 4.2 * f)));
            float drip = fract(cell_id * 5.0 + t * (0.45 + 0.50 * cell_id));
            float blob = 1.0 - abs(drip * 2.0 - 1.0);
            float near = min(fract(alongA * 4.2 * f), fract(alongB * 4.2 * f));
            float shape = pow(blob, 1.45) * step(0.30, cell_id) * (0.50 + near);
            float gloss = pow(max(0.0, blob - 0.55), 2.2) * step(0.30, cell_id);
            plasma = clamp(shape + gloss * 0.45, 0.0, 1.0);
            hot = clamp(gloss * 1.5, 0.0, 1.0);
            sparse_mul = 0.35 + 0.65 * plasma;
        }
        else
        {
            float slide = (1.0 - up01) - t * 0.65;
            vec2 q = saWarp(vec2(alongA * (1.7 * f), slide * (2.2 * f)), t * 0.4, 0.22);
            float stream = saFbm(q);
            float cell = saCellular(q * 1.8);
            float thick = abs(sin(alongA * 5.5 * f + stream * 3.0));
            float body = (0.22 + 0.78 * stream) * (0.40 + 0.60 * (1.0 - thick * 0.55));
            float blob = pow(max(0.0, 0.55 - cell), 1.8);
            float gloss = pow(max(0.0, 0.32 - cell), 2.6);
            plasma = clamp(body * 0.75 + blob * 0.85 + gloss * 0.4, 0.0, 1.0);
            hot = clamp(gloss * 1.5, 0.0, 1.0);
        }
    }
    else if(style == 3)
    {
        /* Lava — crust ridges, glowing veins, heavy flow. */
        if(role == 0)
        {
            vec2 q = saWarp(vec2(alongA, alongB) * (1.5 * f), t * 0.45, 0.16);
            float crust = saRidge(q * 1.6);
            float churn = saFbm(q + vec2(t * 0.32, -t * 0.24));
            float vein = pow(max(0.0, 1.0 - crust * 1.35), 2.2);
            float hot_spot = pow(max(0.0, saNoise(q * 2.8 + vec2(t * 0.9, t * 0.7)) - 0.45), 1.7);
            plasma = clamp(0.15 + 0.35 * churn + 0.55 * vein + hot_spot * 0.55, 0.0, 1.0);
            hot = clamp(vein * 0.7 + hot_spot * 1.2, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float cell = saNoise(vec2(floor(alongA * 3.2 * f), floor(alongB * 3.2 * f)));
            float drip = fract(cell * 3.0 + t * 0.70);
            float blob = pow(1.0 - abs(drip * 2.0 - 1.0), 1.8) * step(0.30, cell);
            float glow = pow(max(0.0, cell - 0.45), 1.5) * blob;
            plasma = clamp(blob * (0.55 + 0.45 * cell) + glow * 0.5, 0.0, 1.0);
            hot = clamp(glow * 1.4 + blob * 0.35, 0.0, 1.0);
            sparse_mul = 0.40 + 0.60 * plasma;
        }
        else
        {
            float flow = (1.0 - up01) - t * 0.50;
            vec2 q = saWarp(vec2(alongA * (1.3 * f), flow * (1.9 * f)), t * 0.35, 0.18);
            float heavy = saFbm(q);
            float crust = saRidge(q * 1.4);
            float vein = pow(max(0.0, 1.0 - crust * 1.4), 2.0);
            float flicker = 0.55 + 0.45 * sin(t * 8.5 + heavy * 11.0);
            plasma = clamp((0.18 + 0.55 * heavy + 0.55 * vein) * flicker, 0.0, 1.0);
            hot = clamp(vein * flicker * 1.2, 0.0, 1.0);
        }
    }
    else if(style == 4)
    {
        /* Embers — sparse point-cloud sparks over a coal bed. */
        if(role == 0)
        {
            vec2 q = saWarp(vec2(alongA, alongB) * (2.1 * f), t * 0.4, 0.14);
            float bed = saFbm(q);
            float coal = saNoise(q * 2.6 + vec2(t * 0.55, t * 0.4));
            float cell = saCellular(q * 3.2);
            float spark = pow(max(0.0, 0.36 - cell), 2.4) * (0.5 + 0.5 * sin(t * 9.0 + coal * 12.0));
            float glow = pow(max(0.0, coal - 0.48), 1.55);
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            plasma = clamp((0.14 + 0.35 * bed + 0.10 * (1.0 - edge)) + glow * 0.45 + spark, 0.0, 1.0);
            hot = clamp(glow * 0.9 + spark * 1.4, 0.0, 1.0);
            sparse_mul = 0.45 + 0.55 * plasma;
        }
        else if(role == 1)
        {
            float cell = saCellular(vec2(alongA, alongB) * (6.5 * f) + vec2(t * 0.55, -t * 0.4));
            float spark_n = saNoise(vec2(alongA, alongB) * (8.0 * f) + vec2(t * 1.4, t * 1.0));
            float flick = 0.5 + 0.5 * sin(t * 10.0 + spark_n * 12.0);
            float ember = pow(max(0.0, 0.40 - cell), 2.2) * 2.2 * flick;
            float pop = pow(max(0.0, spark_n - 0.60), 1.5) * 2.0 * flick;
            plasma = clamp(max(ember, pop), 0.0, 1.0);
            hot = clamp(plasma, 0.0, 1.0);
            sparse_mul = 0.25 + 0.75 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.72;
            vec2 q = saWarp(vec2(alongA * (3.0 * f), rise * (4.8 * f)), t, 0.35);
            float turb = saFbm(q);
            float tongue = saFbm(q * 1.4);
            float base_boost = 1.0 - up01;
            float wisp = pow(max(0.0, tongue - 0.32), 1.4) * (0.35 + 0.65 * base_boost) * (0.45 + 0.55 * turb);
            wisp *= mix(1.0, 0.10, clamp(up01 * 1.5, 0.0, 1.0));
            float cell = saCellular(vec2(alongA * (7.0 * f), rise * (8.0 * f)));
            float ember = pow(max(0.0, 0.38 - cell), 2.3) * (0.35 + 0.65 * base_boost);
            plasma = clamp(max(wisp, ember), 0.0, 1.0);
            hot = clamp(ember * 1.3 + pow(max(0.0, tongue - 0.55), 2.5) * base_boost, 0.0, 1.0);
            sparse_mul = 0.32 + 0.68 * plasma;
        }
    }
    else if(style == 5)
    {
        /* Ocean — layered caustics + slow swell. */
        float current = alongA + alongB * 0.35;
        if(role == 0)
        {
            vec2 p = saWarp(vec2(alongA, alongB) * (1.3 * f), t * 0.5, 0.20);
            float deep = saFbm(p);
            float caus = saCaustic(p, t * 0.7, 1.1);
            float slow = sin(current * 4.2 * f - t * 1.25);
            plasma = clamp(0.16 + 0.40 * deep + 0.42 * caus + 0.22 * (0.5 + 0.5 * slow), 0.0, 1.0);
            hot = clamp(caus * 0.85, 0.0, 1.0);
        }
        else if(role == 1)
        {
            vec2 p = saWarp(vec2(alongA, alongB) * (2.0 * f), t * 0.75, 0.24);
            float caus = saCaustic(p, t, 1.25);
            float rip = abs(sin((alongA + alongB) * 8.0 * f - t * 3.4));
            float sparkle = pow(max(0.0, saNoise(p * 3.5 + vec2(t * 2.0, 0.0)) - 0.70), 2.0);
            plasma = clamp(0.22 + 0.55 * caus + 0.28 * (1.0 - rip) + sparkle, 0.0, 1.0);
            hot = clamp(caus * 0.5 + sparkle * 1.4, 0.0, 1.0);
        }
        else
        {
            vec2 q = saWarp(vec2(current * (1.7 * f) - t * 0.7, up01 * (1.35 * f)), t * 0.6, 0.22);
            float caus = saCaustic(q, t, 1.2);
            float band = 0.5 + 0.5 * sin(current * 5.2 * f - t * 2.0 + up01 * 2.8);
            float sparkle = pow(max(0.0, saNoise(q * 4.0) - 0.68), 2.0);
            plasma = clamp(0.18 + 0.48 * caus + 0.28 * band + sparkle * 0.4, 0.0, 1.0);
            hot = clamp(caus * 0.55 + sparkle * 1.2, 0.0, 1.0);
        }
    }
    else
    {
        /* Steam — soft volumetric blobs + wispy rising edges. */
        if(role == 0)
        {
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            vec2 q = saWarp(vec2(alongA, alongB) * (2.0 * f), t * 0.9, 0.30);
            float vent = saFbm(q + vec2(0.0, -t * 1.2));
            float cell = saCellular(q * 1.6);
            float plume = pow(max(0.0, 0.55 - cell), 1.6);
            plasma = clamp(pow(edge, 1.05) * (0.25 + 0.55 * vent + 0.45 * plume), 0.0, 1.0);
            hot = clamp(plume * 0.6, 0.0, 1.0);
            sparse_mul = 0.28 + 0.72 * plasma;
        }
        else if(role == 1)
        {
            vec2 q = saWarp(vec2(alongA, alongB) * (1.4 * f), t * 0.55, 0.28);
            float fog = saFbm(q);
            float cell = saCellular(q * 1.5);
            float blob = pow(max(0.0, 0.58 - cell), 1.7);
            plasma = clamp(0.18 + 0.45 * fog + 0.50 * blob, 0.0, 1.0);
            hot = clamp(blob * 0.55, 0.0, 1.0);
            sparse_mul = 0.45 + 0.50 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.80;
            vec2 q = saWarp(vec2(alongA * (1.5 * f), rise * (2.0 * f)), t, 0.38);
            float haze = saFbm(q);
            float cell = saCellular(q * 1.7);
            float wisp = pow(max(0.0, 0.52 - cell), 1.8);
            float streak = abs(sin(alongA * 4.2 * f + haze * 2.2 - t * 2.4));
            float from_bot = 1.0 - up01;
            plasma = clamp((0.16 + 0.55 * haze + 0.55 * wisp) * (0.28 + 0.72 * from_bot) * (0.45 + 0.55 * (1.0 - streak * 0.5)), 0.0, 1.0);
            hot = clamp(wisp * 0.7, 0.0, 1.0);
            sparse_mul = 0.32 + 0.68 * plasma;
        }
    }
    return vec3(clamp(plasma, 0.0, 1.0), sparse_mul, clamp(hot, 0.0, 1.0));
}

float saApplyMotion(int motion, int role, float alongA, float alongB, float up01,
                    float time, float speed, float base)
{
    if(motion <= 0)
        return base;
    float t = time * max(0.05, speed);
    float m = base;

    if(motion == 1 || motion == 2 || motion == 3)
    {
        if(role == 0)
        {
            float r = length(vec2(alongA - 0.5, alongB - 0.5));
            float splash = abs(sin(r * 20.0 - t * (motion == 2 ? 5.0 : 3.5)));
            float caus = saCaustic(vec2(alongA, alongB) * 2.5, t, 1.0);
            m = mix(base, clamp(base * 0.45 + (1.0 - splash) * 0.45 + caus * 0.35, 0.0, 1.0), 0.70);
        }
        else if(role == 1)
        {
            float cell = saNoise(vec2(floor(alongA * 8.0), floor(alongB * 8.0)));
            float drip = fract(cell * 4.0 + t * (motion == 3 ? 0.55 : 1.2));
            float hit = pow(1.0 - abs(drip * 2.0 - 1.0), 2.2) * step(0.3, cell);
            m = mix(base, clamp(base * 0.35 + hit, 0.0, 1.0), 0.7);
        }
        else
        {
            float flow_spd = 0.7;
            if(motion == 1) flow_spd = 1.4;
            else if(motion == 2) flow_spd = 2.0;
            float flow = (1.0 - up01) - t * flow_spd;
            vec2 q = saWarp(vec2(alongA * 2.8, flow * 3.6), t, 0.22);
            float sheet = saFbm(q);
            m = mix(base, clamp(0.22 + 0.78 * sheet, 0.0, 1.0), 0.72);
        }
    }
    else if(motion == 4)
    {
        if(role == 1)
        {
            float spark = saNoise(vec2(alongA, alongB) * 9.0 + vec2(t * 1.5, t));
            float cell = saCellular(vec2(alongA, alongB) * 6.0 + vec2(t * 0.4));
            float pop = pow(max(0.0, 0.40 - cell), 2.2);
            m = mix(base, clamp(pow(max(0.0, spark - 0.58), 1.5) * 2.4 + pop, 0.0, 1.0), 0.65);
        }
        else if(role == 0)
        {
            float rise = saFbm(saWarp(vec2(alongA, alongB) * 2.8, t, 0.2) + vec2(0.0, -t * 0.8));
            m = mix(base, clamp(0.28 + 0.72 * rise, 0.0, 1.0), 0.58);
        }
        else
        {
            float rise = up01 - t * 0.9;
            vec2 q = saWarp(vec2(alongA * 3.2, rise * 4.6), t, 0.35);
            float turb = saFbm(q);
            m = mix(base, clamp(0.18 + 0.82 * turb * (1.0 - up01 * 0.5), 0.0, 1.0), 0.72);
        }
    }
    else if(motion == 5)
    {
        float crest = sin((alongA + alongB * 0.25) * 6.28318 * 1.5 - t * 2.0);
        float caus = saCaustic(vec2(alongA, alongB) * 2.0, t, 1.0);
        if(role == 2)
            m = mix(base, clamp(0.40 + 0.40 * crest + 0.15 * up01 + caus * 0.25, 0.0, 1.0), 0.68);
        else
            m = mix(base, clamp(0.35 + 0.45 * (0.5 + 0.5 * crest) + caus * 0.30, 0.0, 1.0), 0.65);
    }
    else if(motion == 6)
    {
        float breathe = 0.55 + 0.45 * sin(t * 2.2);
        m = clamp(base * breathe, 0.0, 1.0);
    }
    return clamp(m, 0.0, 1.0);
}

float saShellIntensity(float dist, float extent, float h_pct, float sigma)
{
    float height_ext = max(0.02, h_pct) * max(0.001, extent);
    if(dist < 0.0 || dist > height_ext)
        return 0.0;
    float d_sigma = max(1e-4, sigma * extent);
    return exp(-dist * dist / (d_sigma * d_sigma));
}

void volumeMain(out vec4 out_color, in vec3 p01)
{
    float mask = u_params[0];
    int style = int(floor(u_params[1] + 0.5));
    int motion = int(floor(u_params[2] + 0.5));
    float h_pct = max(0.05, u_params[3]);
    float sigma = max(0.02, u_params[4]);
    float freq = max(0.05, u_params[5]);
    float speed = max(0.0, u_params[6]);
    float band_mul = max(0.0, u_params[7]);
    float feature = max(0.45, u_params[8]);
    float time_e = u_time * speed * band_mul;

    float nx = p01.x;
    float ny = p01.y;
    float nz = p01.z;

    float best_i = 0.0;
    float best_a = 0.0;
    float best_b = 0.0;
    float best_up = 0.0;
    int best_role = 0;

    float i0 = (saHasBit(mask, 1.0) > 0.5) ? saShellIntensity(ny, 1.0, h_pct, sigma) : 0.0;
    if(i0 > best_i) { best_i = i0; best_role = 0; best_a = nx; best_b = nz; best_up = 0.0; }
    float i1 = (saHasBit(mask, 2.0) > 0.5) ? saShellIntensity(1.0 - ny, 1.0, h_pct, sigma) : 0.0;
    if(i1 > best_i) { best_i = i1; best_role = 1; best_a = nx; best_b = nz; best_up = 1.0; }
    float i2 = (saHasBit(mask, 4.0) > 0.5) ? saShellIntensity(nx, 1.0, h_pct, sigma) : 0.0;
    if(i2 > best_i) { best_i = i2; best_role = 2; best_a = nz; best_b = ny; best_up = ny; }
    float i3 = (saHasBit(mask, 8.0) > 0.5) ? saShellIntensity(1.0 - nx, 1.0, h_pct, sigma) : 0.0;
    if(i3 > best_i) { best_i = i3; best_role = 2; best_a = nz; best_b = ny; best_up = ny; }
    float i4 = (saHasBit(mask, 16.0) > 0.5) ? saShellIntensity(nz, 1.0, h_pct, sigma) : 0.0;
    if(i4 > best_i) { best_i = i4; best_role = 2; best_a = nx; best_b = ny; best_up = ny; }
    float i5 = (saHasBit(mask, 32.0) > 0.5) ? saShellIntensity(1.0 - nz, 1.0, h_pct, sigma) : 0.0;
    if(i5 > best_i) { best_i = i5; best_role = 2; best_a = nx; best_b = ny; best_up = ny; }

    if(best_i < 0.004)
    {
        out_color = vec4(0.0);
        return;
    }

    float plasma;
    float hotness = 0.0;
    float intensity;
    if(style <= 0)
    {
        vec2 qw = saWarp(vec2(best_a, best_b) * (2.2 * freq / feature), time_e * 0.3, 0.20);
        float n = saFbm(qw + vec2(time_e * 0.25, -time_e * 0.18));
        float base = clamp(0.32 + 0.55 * n + 0.15 * best_up, 0.0, 1.0);
        plasma = saApplyMotion(motion, best_role, best_a, best_b, best_up, time_e, speed, base);
        hotness = clamp(pow(max(0.0, plasma - 0.55), 1.8) * 1.4, 0.0, 1.0);
        intensity = clamp(best_i * mix(0.28, 1.0, plasma), 0.0, 1.0);
    }
    else
    {
        int preset = style - 1;
        if(preset < 0) preset = 0;
        if(preset > 6) preset = 6;
        vec3 ps = saEvalPreset(preset, best_role, best_a, best_b, best_up, time_e, freq, speed, feature);
        plasma = clamp(ps.x, 0.0, 1.0);
        hotness = clamp(ps.z, 0.0, 1.0);
        intensity = clamp(best_i * ps.y * mix(0.10, 1.0, plasma) * (1.0 + hotness * 0.35), 0.0, 1.0);
    }
    out_color = vec4(intensity, plasma, hotness, 1.0);
}
)";
}
