// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Color wheel: R=(cos(hue_ang)+1)/2, G=(sin(hue_ang)+1)/2 — the RGBA8 atlas clamps
 *  to [0,1], so signed cos/sin must be range-encoded. Decode to signed and reconstruct
 *  with atan2 after sample so atlas filtering does not paint a fake seam across ±π.
 *  u_params: [0]=progress [1]=dir(+1/-1) [2]=hue_repeats [3]=plane [4]=geom [5]=freq_spin
 *  geom: 0 Radial, 1 Shear, 2 Rings, 3 Pie
 */
inline const char* ColorWheelVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress = u_params[0];
    float dir = u_params[1];
    float wrap = clamp(u_params[2], 0.1, 3.0);
    int pl = int(u_params[3] + 0.5);
    int geom = int(u_params[4] + 0.5);
    float freq_spin = u_params[5];

    float lx = p01.x * 2.0 - 1.0;
    float ly = p01.y * 2.0 - 1.0;
    float lz = p01.z * 2.0 - 1.0;

    float u = lx;
    float v = lz;
    if(pl == 1) { u = lx; v = ly; }
    else if(pl == 2) { u = lz; v = ly; }

    float angle = 0.0;
    if(geom == 1)
    {
        float spin = progress * 6.2831855 * dir + freq_spin;
        float cu = cos(spin);
        float su = sin(spin);
        angle = (u * cu + v * su) * 3.14159265 * wrap;
    }
    else if(geom == 2)
    {
        float rad = length(vec2(u, v));
        angle = rad * 6.2831855 * wrap - progress * 6.2831855 * dir - freq_spin;
    }
    else if(geom == 3)
    {
        float slices = max(2.0, floor(wrap * 6.0 + 0.5));
        float a = atan(v, u);
        float spin = progress * 6.2831855 * dir + freq_spin;
        a = a - spin;
        float sector = floor((a / 6.2831855 + 1.0) * slices);
        angle = (sector + 0.5) / slices * 6.2831855;
    }
    else
    {
        angle = atan(v, u) * wrap;
    }

    // Rings animate via the progress term inside angle; adding the global spin
    // for geom 2 would cancel it exactly (speed would do nothing).
    float hue_turns = angle / 6.2831855 + freq_spin * 0.02;
    if(geom != 2)
        hue_turns += progress * dir;
    float ang = hue_turns * 6.2831855;
    out_color = vec4(cos(ang) * 0.5 + 0.5, sin(ang) * 0.5 + 0.5, 0.0, 1.0);
}
)";
}
