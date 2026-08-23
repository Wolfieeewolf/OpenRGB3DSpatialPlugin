// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Hex lattice: R=intensity v, G=h01 palette driver.
 *  True honeycomb tiling (hex prisms in the horizontal plane): bright cell
 *  walls, dim pulsing interiors. Turbulence bends the sheet per height slice.
 *  u_params: [0]=flow_t (Speed only) [1]=hue_t (Frequency only)
 *            [2]=detail_n [3]=size [4]=breathing [5]=pulse [6]=turbulence
 *            [7]=flow_mul (Calm/Active/Aggressive)
 *            [8]=ox [9]=oy [10]=oz (effect/ref origin in unit UV)
 */
inline const char* HexLatticeVolumeFieldGlsl()
{
    return R"(
float wave01(float x)
{
    return 0.5 + 0.5 * sin(6.2831853 * x);
}
float hash12(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.547);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float flow_t = u_params[0];
    float hue_t = u_params[1];
    float detail_norm = clamp(u_params[2], 0.0, 1.0);
    float base_scale = max(u_params[3], 0.2);
    float breathing_amount = clamp(u_params[4], 0.0, 2.0);
    float pulse_amount = clamp(u_params[5], 0.0, 2.0);
    float turbulence = clamp(u_params[6], 0.0, 2.0);
    float flow_mul = max(u_params[7], 0.15);
    vec3 origin01 = clamp(vec3(u_params[8], u_params[9], u_params[10]), 0.0, 1.0);

    float breathe = 1.0 + (wave01(flow_t * 0.30) - 0.5) * 0.35 * breathing_amount;
    // Soft-cap cell density so Detail=200 does not explode shader cost / aliasing.
    float cells = min(12.0, (5.0 + 7.0 * detail_norm) / base_scale * breathe);

    vec3 local01 = clamp(p01 - origin01 + 0.5, 0.0, 1.0);
    vec2 uv = vec2(local01.x, local01.z);
    uv += turbulence * 0.07 * vec2(sin(6.2831853 * (local01.y * 0.8 + flow_t * 0.11)),
                                   cos(6.2831853 * (local01.y * 0.8 - flow_t * 0.09)));
    uv *= cells;
    uv += flow_t * flow_mul * vec2(0.22, 0.31);

    vec2 r = vec2(1.0, 1.7320508);
    vec2 hr = r * 0.5;
    vec2 a = mod(uv, r) - hr;
    vec2 b = mod(uv - hr, r) - hr;
    vec2 gv = (dot(a, a) < dot(b, b)) ? a : b;
    vec2 id = uv - gv;

    float hd = max(dot(abs(gv), vec2(0.5, 0.8660254)), abs(gv).x);
    float edge_w = 0.20 - 0.08 * detail_norm;
    float edge = smoothstep(0.5 - edge_w, 0.5 - edge_w * 0.15, hd);

    float hcell = hash12(id);
    float pulse = wave01(flow_t * (0.20 + 0.35 * hcell) * flow_mul + hcell);
    float cell_fill = (0.06 + 0.30 * pulse * pulse_amount) * (1.0 - edge);

    float v = clamp(edge * (0.80 + 0.20 * pulse) + cell_fill, 0.0, 1.0);

    float h01 = fract(id.x * 0.045 + id.y * 0.030 + hcell * 0.18 + (local01.y - 0.5) * 0.10 + hue_t);
    out_color = vec4(v, h01, 0.0, 1.0);
}
)";
}
