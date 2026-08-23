// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** DNA helix: R=intensity, G=palette01, B=rung_hint.
 *  Proper double-helix SDF in unit cube (axis = Y), origin/ref via sample remap on CPU.
 *  u_params: [0]=progress [1]=twists [2]=radius01 [3]=thickness
 *            [4]=rung_amount [5]=shape(0=helix,1=rope,2=ribbons,3=ladder)
 *            [6]=ox [7]=oy [8]=oz (axis origin in 01)
 */
inline const char* DNAHelixVolumeFieldGlsl()
{
    return R"(
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress = u_params[0];
    float twists = max(u_params[1], 0.35);
    float radius01 = clamp(u_params[2], 0.06, 0.85);
    float thickness = clamp(u_params[3], 0.03, 0.45);
    float rung_amount = clamp(u_params[4], 0.0, 1.0);
    int shape = int(clamp(u_params[5], 0.0, 3.0) + 0.5);
    float lx = p01.x * 2.0 - 1.0;
    float ly = p01.y;
    float lz = p01.z * 2.0 - 1.0;

    float phase = ly * twists * 6.2831853 + progress * 6.2831853;
    float c1 = cos(phase);
    float s1 = sin(phase);
    float c2 = -c1;
    float s2 = -s1;

    float hx1 = radius01 * c1;
    float hz1 = radius01 * s1;
    float hx2 = radius01 * c2;
    float hz2 = radius01 * s2;

    float d1 = length(vec2(lx - hx1, lz - hz1));
    float d2 = length(vec2(lx - hx2, lz - hz2));
    float d_strand = min(d1, d2);

    float core_w = thickness * ((shape == 1) ? 1.65 : 1.0);
    float glow_w = core_w * ((shape == 2) ? 2.8 : 2.2);
    float strand = 1.0 - smstep(0.0, core_w, d_strand);
    float glow = (1.0 - smstep(core_w, glow_w, d_strand)) * 0.55;
    float intensity = strand + glow;

    if(shape == 2)
    {
        // Soft angular ribbons instead of tubes — more LEDs catch light.
        float a = atan(lz, lx);
        float da1 = abs(mod(a - phase + 3.14159265, 6.2831853) - 3.14159265);
        float da2 = abs(mod(a - phase - 3.14159265 + 3.14159265, 6.2831853) - 3.14159265);
        float ang = min(da1, da2);
        float rad = length(vec2(lx, lz));
        float ribbon = (1.0 - smstep(0.0, 0.55 + thickness, ang)) *
                       (1.0 - smstep(radius01 * 0.35, radius01 * 1.35, abs(rad - radius01)));
        intensity = max(intensity, ribbon);
    }

    float rung = 0.0;
    if(rung_amount > 0.02)
    {
        float rung_phase = fract(ly * twists * 2.0 + progress * 0.5);
        float rung_gate = 1.0 - smstep(0.0, 0.12 + 0.08 * (1.0 - rung_amount), abs(rung_phase - 0.5));
        float rad = length(vec2(lx, lz));
        float along = abs(rad - radius01 * 0.5) ; // mid-span between strands
        // Bridge between the two strand angles: light a segment across the diameter.
        float bridge = 1.0 - smstep(0.0, thickness * 1.1, abs(rad - radius01 * 0.55));
        float between = 1.0 - smstep(radius01 * 0.15, radius01 * 1.05, rad);
        rung = bridge * between * rung_gate * rung_amount;
        if(shape == 3)
            rung *= 1.45;
        intensity = max(intensity, rung);
    }

    // Soft axis glow so sparse grids still read motion.
    float axis_glow = 0.10 * (1.0 - smstep(0.0, radius01 * 1.4, length(vec2(lx, lz))));
    intensity = clamp(intensity + axis_glow, 0.0, 1.0);

    float palette01 = fract(ly * twists * 0.35 + progress + (d1 < d2 ? 0.0 : 0.5));
    float rung_hint = (rung > strand * 0.45) ? 1.0 : 0.0;
    out_color = vec4(intensity, palette01, rung_hint, 1.0);
}
)";
}
