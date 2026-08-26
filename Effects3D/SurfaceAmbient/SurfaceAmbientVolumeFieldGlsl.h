// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Surface Ambient volume field: R=shell intensity, G=plasma01.
 *  p01 = unit room [0,1]^3 (floor y=0, ceiling y=1).
 *  u_params: [0]=surf_mask [1]=style (0=None, 1..7=Fire..Steam)
 *            [2]=motion [3]=height_pct [4]=sigma
 *            [5]=freq [6]=speed [7]=band_speed_mul [8]=feature_size
 *  Clock is always u_time (params[7] only scales it). GLSL 1.10 — no int clamp().
 *  Globals: Speed→[6], Detail→[5], Size→[8], Scale→[3] (shell depth).
 */
inline const char* SurfaceAmbientVolumeFieldGlsl()
{
    return R"(
float saHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
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
    for(int i = 0; i < 4; ++i)
    {
        v += a * saNoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}
float saHasBit(float mask, float bit)
{
    return step(0.5, mod(floor(mask / bit), 2.0));
}

/* Returns plasma in x, sparse intensity mul in y. role: 0 floor, 1 ceiling, 2 wall.
 * feature > 1 → larger tongues/pools (from global Size). */
vec2 saEvalPreset(int style, int role, float alongA, float alongB, float up01,
                  float time, float freq, float speed, float feature)
{
    float t = time * max(0.08, speed);
    /* Fire keeps denser UV; other presets run coarser so features read larger on LEDs. */
    float feat = max(0.45, feature);
    float f_fire = max(0.18, freq) / max(0.7, feat * 0.85);
    float f_big = max(0.12, freq) / max(0.55, feat * 1.15);
    float f = (style == 0 || style == 4) ? f_fire : f_big;
    float plasma = 0.5;
    float sparse_mul = 1.0;

    if(style == 0)
    {
        if(role == 0)
        {
            float bed = saFbm(vec2(alongA, alongB) * (2.2 * f) + vec2(t * 0.35, -t * 0.2));
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            plasma = clamp(0.35 + 0.55 * bed + 0.25 * (1.0 - edge), 0.0, 1.0);
        }
        else if(role == 1)
        {
            float spark = saNoise(vec2(alongA, alongB) * (8.0 * f) + vec2(t * 1.4, t * 0.9));
            float flick = 0.5 + 0.5 * sin(t * 9.0 + spark * 12.0);
            plasma = clamp(pow(max(0.0, spark - 0.62), 1.4) * 2.8 * flick, 0.0, 1.0);
            sparse_mul = 0.45 + 0.55 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.55;
            float turb = saFbm(vec2(alongA * (3.0 * f), rise * (4.5 * f)));
            float tongue = saNoise(vec2(alongA * (5.5 * f) + turb * 0.4, rise * (6.0 * f)));
            float base_boost = 1.0 - up01;
            plasma = clamp((0.25 + 0.75 * tongue) * (0.35 + 0.65 * base_boost) * (0.55 + 0.45 * turb), 0.0, 1.0);
            plasma *= mix(1.0, 0.28, up01);
        }
    }
    else if(style == 1)
    {
        /* Water */
        if(role == 0)
        {
            float r = length(vec2(alongA - 0.5, alongB - 0.5));
            float ring = abs(sin(r * 11.0 * f - t * 5.5));
            float slosh = saFbm(vec2(alongA, alongB) * (2.0 * f) + vec2(t * 0.85, -t * 0.55));
            plasma = clamp(0.22 + 0.55 * slosh + 0.4 * (1.0 - ring) * (1.0 - clamp(r * 1.35, 0.0, 1.0)), 0.0, 1.0);
        }
        else if(role == 1)
        {
            float r = length(vec2(alongA - 0.5, alongB - 0.5));
            float pour = exp(-r * r * 8.0) * (0.5 + 0.5 * saNoise(vec2(alongA, alongB) * (3.8 * f) + vec2(t * 2.8, 0.0)));
            float outward = abs(sin(r * 14.0 * f - t * 4.5));
            plasma = clamp(pour * 1.55 + (1.0 - outward) * 0.35 * (1.0 - clamp(r, 0.0, 1.0)), 0.0, 1.0);
        }
        else
        {
            float sheet = saFbm(vec2(alongA * (1.5 * f), (1.0 - up01) * (3.2 * f) - t * 3.0));
            float streaks = abs(sin((alongA * 9.0 * f) + sheet * 2.0 - t * 4.2));
            float foam = saNoise(vec2(alongA * (4.0 * f), up01 * (3.0 * f) - t * 1.5));
            plasma = clamp((0.28 + 0.6 * sheet) * (0.35 + 0.65 * up01) * (0.4 + 0.6 * (1.0 - streaks)) + 0.18 * foam, 0.0, 1.0);
        }
    }
    else if(style == 2)
    {
        /* Slime — larger drips / thicker streams. */
        if(role == 0)
        {
            float pool = saFbm(vec2(alongA, alongB) * (1.5 * f) + vec2(t * 0.22, t * 0.16));
            float settle = 0.5 + 0.5 * sin(pool * 6.28318 + t * 0.7);
            plasma = clamp(0.28 + 0.6 * pool * settle, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float cell = saNoise(vec2(floor(alongA * 4.5 * f), floor(alongB * 4.5 * f)));
            float drip = fract(cell * 5.0 + t * (0.55 + 0.45 * cell));
            float blob = 1.0 - abs(drip * 2.0 - 1.0);
            float near = min(fract(alongA * 4.5 * f), fract(alongB * 4.5 * f));
            plasma = clamp(pow(blob, 1.35) * step(0.28, cell) * (0.55 + near), 0.0, 1.0);
            sparse_mul = 0.4 + 0.6 * plasma;
        }
        else
        {
            float slide = (1.0 - up01) - t * 0.7;
            float stream = saFbm(vec2(alongA * (1.8 * f), slide * (2.4 * f)));
            float thick = abs(sin(alongA * 6.0 * f + stream * 3.0));
            plasma = clamp((0.25 + 0.75 * stream) * (0.4 + 0.6 * (1.0 - thick * 0.65)), 0.0, 1.0);
        }
    }
    else if(style == 3)
    {
        /* Lava — hotter contrast, heavier flow. */
        if(role == 0)
        {
            float churn = saFbm(vec2(alongA, alongB) * (1.35 * f) + vec2(t * 0.4, -t * 0.28));
            float hot = saNoise(vec2(alongA, alongB) * (3.2 * f) + vec2(t * 1.1, t * 0.8));
            plasma = clamp(0.22 + 0.45 * churn + 0.45 * hot, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float cell = saNoise(vec2(floor(alongA * 3.5 * f), floor(alongB * 3.5 * f)));
            float drip = fract(cell * 3.0 + t * 0.75);
            plasma = clamp(pow(1.0 - abs(drip * 2.0 - 1.0), 1.7) * step(0.32, cell) * (0.55 + 0.45 * cell), 0.0, 1.0);
            sparse_mul = 0.45 + 0.55 * plasma;
        }
        else
        {
            float flow = (1.0 - up01) - t * 0.55;
            float heavy = saFbm(vec2(alongA * (1.35 * f), flow * (2.0 * f)));
            float flicker = 0.5 + 0.5 * sin(t * 9.0 + heavy * 12.0);
            float crack = abs(sin(alongA * 5.5 * f + heavy * 2.0));
            plasma = clamp((0.22 + 0.78 * heavy) * (0.5 + 0.5 * flicker) * (0.55 + 0.45 * (1.0 - crack * 0.5)), 0.0, 1.0);
        }
    }
    else if(style == 4)
    {
        /* Embers */
        if(role == 0)
        {
            float bed = saFbm(vec2(alongA, alongB) * (2.0 * f) + vec2(t * 0.28, -t * 0.16));
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            float coal = saNoise(vec2(alongA, alongB) * (5.5 * f) + vec2(t * 0.65, t * 0.45));
            float hot = pow(max(0.0, coal - 0.50), 1.5);
            float flick = 0.5 + 0.5 * sin(t * 6.5 + bed * 8.0);
            plasma = clamp((0.16 + 0.38 * bed + 0.12 * (1.0 - edge)) * (0.5 + 0.5 * flick) + hot * 0.42, 0.0, 1.0);
            sparse_mul = 0.52 + 0.48 * plasma;
        }
        else if(role == 1)
        {
            float spark = saNoise(vec2(alongA, alongB) * (7.0 * f) + vec2(t * 1.25, t * 0.85));
            float flick = 0.5 + 0.5 * sin(t * 8.5 + spark * 10.0);
            float ember = pow(max(0.0, spark - 0.56), 1.3) * 2.4 * flick;
            float drift = saFbm(vec2(alongA, alongB) * (3.2 * f) + vec2(t * 0.35, -t * 0.5));
            plasma = clamp(ember + drift * 0.32, 0.0, 1.0);
            sparse_mul = 0.32 + 0.68 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.68;
            float turb = saFbm(vec2(alongA * (3.2 * f), rise * (5.2 * f)));
            float tongue = saNoise(vec2(alongA * (6.2 * f) + turb * 0.32, rise * (7.2 * f)));
            float base_boost = 1.0 - up01;
            float wisp = clamp((0.14 + 0.58 * tongue) * (0.38 + 0.62 * base_boost) * (0.48 + 0.52 * turb), 0.0, 1.0);
            wisp *= mix(1.0, 0.12, clamp(up01 * 1.45, 0.0, 1.0));
            float spark = saNoise(vec2(alongA * (8.5 * f), rise * (9.5 * f)));
            float ember = pow(max(0.0, spark - 0.54), 1.4) * (0.35 + 0.65 * base_boost);
            float lick = abs(sin(alongA * 11.0 * f + turb * 2.5 - t * 4.5)) * base_boost * 0.45;
            plasma = clamp(max(wisp, max(ember * 0.9, lick)), 0.0, 1.0);
            sparse_mul = 0.36 + 0.64 * plasma;
        }
    }
    else if(style == 5)
    {
        /* Ocean — broader caustics, stronger bands. */
        float current = alongA + alongB * 0.35;
        if(role == 0)
        {
            float deep = saFbm(vec2(alongA, alongB) * (1.1 * f) + vec2(t * 0.38, t * 0.3));
            float slow = sin(current * 4.5 * f - t * 1.35);
            float shimmer = saNoise(vec2(alongA, alongB) * (3.0 * f) + vec2(t * 0.9, 0.0));
            plasma = clamp(0.22 + 0.45 * deep + 0.28 * (0.5 + 0.5 * slow) + 0.18 * shimmer, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float caus = saFbm(vec2(alongA, alongB) * (2.2 * f) + vec2(t * 0.85, -t * 0.6));
            float rip = abs(sin((alongA + alongB) * 7.5 * f - t * 3.2));
            plasma = clamp(0.28 + 0.5 * caus + 0.32 * (1.0 - rip), 0.0, 1.0);
        }
        else
        {
            float caus = saFbm(vec2(current * (1.8 * f) - t * 0.75, up01 * (1.4 * f)));
            float band = 0.5 + 0.5 * sin(current * 5.5 * f - t * 2.2 + up01 * 2.5);
            float sparkle = saNoise(vec2(current * (4.0 * f), up01 * 3.0 - t));
            plasma = clamp(0.22 + 0.45 * caus + 0.32 * band + 0.15 * sparkle, 0.0, 1.0);
        }
    }
    else
    {
        /* Steam — broader vents / haze, more motion. */
        if(role == 0)
        {
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            float vent = saFbm(vec2(alongA, alongB) * (2.2 * f) + vec2(0.0, -t * 1.35));
            plasma = clamp(pow(edge, 1.05) * (0.3 + 0.7 * vent), 0.0, 1.0);
            sparse_mul = 0.3 + 0.7 * plasma;
        }
        else if(role == 1)
        {
            float fog = saFbm(vec2(alongA, alongB) * (1.5 * f) + vec2(t * 0.5, t * 0.4));
            float blob = saNoise(vec2(alongA, alongB) * (2.8 * f) - vec2(t * 0.35, 0.0));
            plasma = clamp(0.22 + 0.5 * fog + 0.35 * blob, 0.0, 1.0);
            sparse_mul = 0.5 + 0.45 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.75;
            float haze = saFbm(vec2(alongA * (1.7 * f), rise * (2.2 * f)));
            float streak = abs(sin(alongA * 4.5 * f + haze * 2.0 - t * 2.2));
            float from_bot = 1.0 - up01;
            plasma = clamp((0.2 + 0.7 * haze) * (0.3 + 0.7 * from_bot) * (0.45 + 0.55 * (1.0 - streak * 0.55)), 0.0, 1.0);
            sparse_mul = 0.35 + 0.65 * plasma;
        }
    }
    return vec2(clamp(plasma, 0.0, 1.0), sparse_mul);
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
            m = mix(base, clamp(base * 0.5 + (1.0 - splash) * 0.55, 0.0, 1.0), 0.65);
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
            float sheet = saFbm(vec2(alongA * 3.0, flow * 4.0));
            m = mix(base, clamp(0.25 + 0.75 * sheet, 0.0, 1.0), 0.7);
        }
    }
    else if(motion == 4)
    {
        if(role == 1)
        {
            float spark = saNoise(vec2(alongA, alongB) * 9.0 + vec2(t * 1.5, t));
            m = mix(base, clamp(pow(max(0.0, spark - 0.6), 1.5) * 2.5, 0.0, 1.0), 0.6);
        }
        else if(role == 0)
        {
            float rise = saFbm(vec2(alongA, alongB) * 3.0 + vec2(0.0, -t * 0.8));
            m = mix(base, clamp(0.3 + 0.7 * rise, 0.0, 1.0), 0.55);
        }
        else
        {
            float rise = up01 - t * 0.9;
            float turb = saFbm(vec2(alongA * 3.5, rise * 5.0));
            m = mix(base, clamp(0.2 + 0.8 * turb * (1.0 - up01 * 0.5), 0.0, 1.0), 0.7);
        }
    }
    else if(motion == 5)
    {
        float crest = sin((alongA + alongB * 0.25) * 6.28318 * 1.5 - t * 2.0);
        if(role == 2)
            m = mix(base, clamp(0.45 + 0.45 * crest + 0.15 * up01, 0.0, 1.0), 0.65);
        else
            m = mix(base, clamp(0.4 + 0.5 * (0.5 + 0.5 * crest), 0.0, 1.0), 0.6);
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
    /* 0 = Preset None; 1..7 = Fire..Steam (matches CPU Style enum). */
    int style = int(floor(u_params[1] + 0.5));
    int motion = int(floor(u_params[2] + 0.5));
    float h_pct = max(0.05, u_params[3]);
    float sigma = max(0.02, u_params[4]);
    float freq = max(0.05, u_params[5]);
    float speed = max(0.15, u_params[6]);
    float band_mul = max(0.15, u_params[7]);
    float feature = max(0.45, u_params[8]);
    /* Always advance from engine clock — absolute time in a param can stall/desync. */
    float time_e = u_time * band_mul;

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
    float intensity;
    if(style <= 0)
    {
        float n = saFbm(vec2(best_a, best_b) * (2.2 * freq / feature) + vec2(time_e * 0.25, -time_e * 0.18));
        float base = clamp(0.35 + 0.55 * n + 0.15 * best_up, 0.0, 1.0);
        plasma = saApplyMotion(motion, best_role, best_a, best_b, best_up, time_e, speed, base);
        intensity = clamp(best_i * mix(0.28, 1.0, plasma), 0.0, 1.0);
    }
    else
    {
        /* style 1..7 → saEvalPreset 0..6 (Fire..Steam). Manual clamp: GLSL 1.10 has no int clamp. */
        int preset = style - 1;
        if(preset < 0) preset = 0;
        if(preset > 6) preset = 6;
        vec2 ps = saEvalPreset(preset, best_role, best_a, best_b, best_up, time_e, freq, speed, feature);
        plasma = clamp(ps.x, 0.0, 1.0);
        intensity = clamp(best_i * ps.y * mix(0.12, 1.0, plasma), 0.0, 1.0);
    }
    out_color = vec4(intensity, plasma, 0.0, 1.0);
}
)";
}
