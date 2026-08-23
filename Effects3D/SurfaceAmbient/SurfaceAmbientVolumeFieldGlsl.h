// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Surface Ambient volume field: R=shell intensity, G=plasma01.
 *  p01 = unit room [0,1]^3 (floor y=0, ceiling y=1).
 *  u_params: [0]=surf_mask [1]=style [2]=motion [3]=height_pct [4]=sigma
 *            [5]=freq [6]=speed [7]=time_e
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

/* Returns plasma in x, sparse intensity mul in y. role: 0 floor, 1 ceiling, 2 wall. */
vec2 saEvalPreset(int style, int role, float alongA, float alongB, float up01,
                  float time, float freq, float speed)
{
    float t = time * max(0.05, speed);
    float f = max(0.2, freq);
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
        if(role == 0)
        {
            float r = length(vec2(alongA - 0.5, alongB - 0.5));
            float ring = abs(sin(r * 18.0 * f - t * 4.0));
            float slosh = saFbm(vec2(alongA, alongB) * (3.0 * f) + vec2(t * 0.5, -t * 0.35));
            plasma = clamp(0.3 + 0.45 * slosh + 0.35 * (1.0 - ring) * (1.0 - clamp(r * 1.6, 0.0, 1.0)), 0.0, 1.0);
        }
        else if(role == 1)
        {
            float r = length(vec2(alongA - 0.5, alongB - 0.5));
            float pour = exp(-r * r * 14.0) * (0.55 + 0.45 * saNoise(vec2(alongA, alongB) * (6.0 * f) + vec2(t * 2.0, 0.0)));
            float outward = abs(sin(r * 22.0 * f - t * 3.5));
            plasma = clamp(pour * 1.4 + (1.0 - outward) * 0.25 * (1.0 - clamp(r, 0.0, 1.0)), 0.0, 1.0);
        }
        else
        {
            float sheet = saFbm(vec2(alongA * (2.2 * f), (1.0 - up01) * (5.0 * f) - t * 2.2));
            float streaks = abs(sin((alongA * 14.0 * f) + sheet * 2.0 - t * 3.0));
            plasma = clamp((0.35 + 0.55 * sheet) * (0.4 + 0.6 * up01) * (0.45 + 0.55 * (1.0 - streaks)), 0.0, 1.0);
        }
    }
    else if(style == 2)
    {
        if(role == 0)
        {
            float pool = saFbm(vec2(alongA, alongB) * (2.4 * f) + vec2(t * 0.12, t * 0.08));
            float settle = 0.55 + 0.45 * sin(pool * 6.28318 + t * 0.4);
            plasma = clamp(0.4 + 0.5 * pool * settle, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float cell = saNoise(vec2(floor(alongA * 7.0 * f), floor(alongB * 7.0 * f)));
            float drip = fract(cell * 5.0 + t * (0.35 + 0.4 * cell));
            float blob = 1.0 - abs(drip * 2.0 - 1.0);
            float near = min(fract(alongA * 7.0 * f), fract(alongB * 7.0 * f));
            plasma = clamp(pow(blob, 1.6) * step(0.35, cell) * (0.5 + near), 0.0, 1.0);
            sparse_mul = 0.5 + 0.5 * plasma;
        }
        else
        {
            float slide = (1.0 - up01) - t * 0.45;
            float stream = saFbm(vec2(alongA * (2.8 * f), slide * (3.5 * f)));
            float thick = abs(sin(alongA * 9.0 * f + stream * 3.0));
            plasma = clamp((0.3 + 0.7 * stream) * (0.45 + 0.55 * (1.0 - thick * 0.7)), 0.0, 1.0);
        }
    }
    else if(style == 3)
    {
        if(role == 0)
        {
            float churn = saFbm(vec2(alongA, alongB) * (2.0 * f) + vec2(t * 0.25, -t * 0.18));
            float hot = saNoise(vec2(alongA, alongB) * (5.0 * f) + vec2(t * 0.7, t * 0.5));
            plasma = clamp(0.35 + 0.4 * churn + 0.35 * hot, 0.0, 1.0);
        }
        else if(role == 1)
        {
            float cell = saNoise(vec2(floor(alongA * 5.0 * f), floor(alongB * 5.0 * f)));
            float drip = fract(cell * 3.0 + t * 0.5);
            plasma = clamp(pow(1.0 - abs(drip * 2.0 - 1.0), 2.0) * step(0.4, cell) * (0.6 + 0.4 * cell), 0.0, 1.0);
            sparse_mul = 0.55 + 0.45 * plasma;
        }
        else
        {
            float flow = (1.0 - up01) - t * 0.35;
            float heavy = saFbm(vec2(alongA * (2.0 * f), flow * (2.8 * f)));
            float flicker = 0.5 + 0.5 * sin(t * 7.0 + heavy * 10.0);
            plasma = clamp((0.3 + 0.7 * heavy) * (0.55 + 0.45 * flicker), 0.0, 1.0);
        }
    }
    else if(style == 4)
    {
        if(role == 0)
        {
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            float src = saNoise(vec2(alongA, alongB) * (10.0 * f) + vec2(t * 0.8, t * 0.6));
            plasma = clamp(pow(max(0.0, src - 0.55), 1.5) * (0.4 + 0.9 * edge), 0.0, 1.0);
            sparse_mul = 0.25 + 0.75 * plasma;
        }
        else if(role == 1)
        {
            float spark = saNoise(vec2(alongA, alongB) * (11.0 * f) + vec2(t * 1.6, -t * 1.1));
            plasma = clamp(pow(max(0.0, spark - 0.68), 1.8) * 3.0, 0.0, 1.0);
            sparse_mul = 0.2 + 0.8 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.7;
            float spark = saNoise(vec2(alongA * (9.0 * f), rise * (10.0 * f)));
            plasma = clamp(pow(max(0.0, spark - 0.58), 1.7) * (0.5 + 0.5 * (1.0 - up01)), 0.0, 1.0);
            sparse_mul = 0.2 + 0.8 * plasma;
        }
    }
    else if(style == 5)
    {
        float current = alongA + alongB * 0.35;
        if(role == 0)
        {
            float deep = saFbm(vec2(alongA, alongB) * (1.6 * f) + vec2(t * 0.22, t * 0.18));
            float slow = sin(current * 6.28318 * f - t * 0.8);
            plasma = clamp(0.35 + 0.4 * deep + 0.25 * (0.5 + 0.5 * slow), 0.0, 1.0);
        }
        else if(role == 1)
        {
            float caus = saFbm(vec2(alongA, alongB) * (3.5 * f) + vec2(t * 0.55, -t * 0.4));
            float rip = abs(sin((alongA + alongB) * 12.0 * f - t * 2.2));
            plasma = clamp(0.4 + 0.45 * caus + 0.25 * (1.0 - rip), 0.0, 1.0);
        }
        else
        {
            float caus = saFbm(vec2(current * (2.8 * f) - t * 0.45, up01 * (2.0 * f)));
            float band = 0.5 + 0.5 * sin(current * 8.0 * f - t * 1.4 + up01 * 2.0);
            plasma = clamp(0.35 + 0.4 * caus + 0.3 * band, 0.0, 1.0);
        }
    }
    else
    {
        if(role == 0)
        {
            float edge = max(abs(alongA - 0.5), abs(alongB - 0.5)) * 2.0;
            float vent = saFbm(vec2(alongA, alongB) * (3.5 * f) + vec2(0.0, -t * 0.9));
            plasma = clamp(pow(edge, 1.2) * (0.35 + 0.65 * vent), 0.0, 1.0);
            sparse_mul = 0.35 + 0.65 * plasma;
        }
        else if(role == 1)
        {
            float fog = saFbm(vec2(alongA, alongB) * (2.2 * f) + vec2(t * 0.3, t * 0.25));
            float blob = saNoise(vec2(alongA, alongB) * (4.0 * f) - vec2(t * 0.2, 0.0));
            plasma = clamp(0.3 + 0.45 * fog + 0.3 * blob, 0.0, 1.0);
            sparse_mul = 0.55 + 0.35 * plasma;
        }
        else
        {
            float rise = up01 - t * 0.5;
            float haze = saFbm(vec2(alongA * (2.5 * f), rise * (3.2 * f)));
            float streak = abs(sin(alongA * 7.0 * f + haze * 2.0 - t * 1.5));
            float from_bot = 1.0 - up01;
            plasma = clamp((0.25 + 0.65 * haze) * (0.35 + 0.65 * from_bot) * (0.5 + 0.5 * (1.0 - streak * 0.6)), 0.0, 1.0);
            sparse_mul = 0.4 + 0.6 * plasma;
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
    int style = int(u_params[1] + 0.5);
    int motion = int(u_params[2] + 0.5);
    float h_pct = max(0.05, u_params[3]);
    float sigma = max(0.02, u_params[4]);
    float freq = max(0.05, u_params[5]);
    float speed = max(0.0, u_params[6]);
    float time_e = u_params[7];

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

    vec2 ps = saEvalPreset(style, best_role, best_a, best_b, best_up, time_e, freq, speed);
    float plasma = saApplyMotion(motion, best_role, best_a, best_b, best_up, time_e, speed, ps.x);
    float intensity = clamp(best_i * ps.y, 0.0, 1.0);
    out_color = vec4(intensity, clamp(plasma, 0.0, 1.0), 0.0, 1.0);
}
)";
}
