// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Spiral / spin volume: R=intensity.
 *  p01 = origin-local unit UV (0.5 = Spatial Anchor hub).
 *  u_params: [0]=progress [1]=freq_scale [2]=pattern [3]=num_arms
 *            [4]=gap01 [5]=detail [6]=coil01 [7]=height_coil01
 *  coil01=0 → straight spin rays; higher → tighter spiral bend.
 *  height_coil01=0 → flat; higher → vertical helix / tornado lift.
 *
 *  Every recipe keeps a soft body PLUS a crisp ridge so arms read on LED grids.
 *  patterns: 0 Smooth Spiral, 1 Pinwheel, 2 Saw Blade, 3 Swirl Circles,
 *            4 Hypnotic, 5 Simple Spin, 6 Tornado / Twister
 */
inline const char* SpiralVolumeFieldGlsl()
{
    return R"(
float spSaturate(float v) { return clamp(v, 0.0, 1.0); }
float spSoft(float d, float w)
{
    float s = max(w, 0.012);
    return exp(-(d * d) / (s * s));
}
/* Soft fill + bright crisp core — spiral arms stay legible. */
float spRidge(float d, float soft_w, float crisp_w)
{
    float body = spSoft(d, soft_w) * 0.58;
    float core = spSoft(d, max(crisp_w, 0.008));
    return spSaturate(max(body, core));
}
float spPeriod(float arms)
{
    return 6.2831853 / max(arms, 1.0);
}
/* Angular distance to nearest arm center (radians). */
float spArmDist(float spiral_angle, float arms)
{
    float period = spPeriod(arms);
    float aa = mod(spiral_angle + period * 0.5, period) - period * 0.5;
    return abs(aa);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress_e = u_params[0];
    float freq_scale_e = max(u_params[1], 0.01);
    int pattern_type = int(clamp(u_params[2], 0.0, 6.0) + 0.5);
    float num_arms = max(u_params[3], 1.0);
    float gap_factor = clamp(u_params[4], 0.0, 0.95);
    float detail_e = max(u_params[5], 0.05);
    float coil01 = clamp(u_params[6], 0.0, 1.0);
    float height01 = clamp(u_params[7], 0.0, 1.0);

    float lx = p01.x * 2.0 - 1.0;
    float ly = clamp(p01.y, 0.0, 1.0);
    float lz = p01.z * 2.0 - 1.0;
    float angle = atan(lz, lx);
    float norm_radius = clamp(length(vec2(lx, lz)), 0.0, 1.4142135);
    float r01 = clamp(norm_radius / 1.05, 0.0, 1.0);
    float norm_twist = ly;
    float two_pi = 6.2831853;
    float detail_n = clamp(detail_e / 20.0, 0.0, 1.5);

    /* Coil: 0 = straight rays; 1 = tight log-ish spiral. Height coil = helix / funnel. */
    float radial_coil = r01 * coil01 * two_pi * (2.4 + 1.6 * detail_n);
    float z_twist = norm_twist * height01 * two_pi * (1.15 + 0.85 * detail_n);
    float spiral_angle = angle * num_arms + radial_coil + z_twist - progress_e * 1.45;

    float soft_w = mix(0.55, 0.22, clamp(detail_n, 0.0, 1.0)) * (0.55 + 0.45 * (1.0 - gap_factor));
    float crisp_w = mix(0.16, 0.055, clamp(detail_n, 0.0, 1.0));
    float half_arm = spPeriod(num_arms) * 0.5 * (1.0 - gap_factor * 0.92);
    float spiral_value = 0.0;

    if(pattern_type == 0)
    {
        /* Smooth Spiral — soft ribbons with a crisp centerline. */
        float d = spArmDist(spiral_angle, num_arms);
        float ridge = spRidge(d, soft_w, crisp_w);
        float secondary = spRidge(spArmDist(spiral_angle * 0.5 + norm_twist * freq_scale_e, num_arms),
                                  soft_w * 1.35, crisp_w * 1.4) * 0.28;
        float lift = 0.5 + 0.5 * cos(norm_twist * freq_scale_e * 3.0 + progress_e * 0.7);
        spiral_value = spSaturate(ridge * (0.75 + 0.25 * lift) + secondary);
        spiral_value *= 0.55 + 0.45 * (1.0 - r01 * 0.35);
    }
    else if(pattern_type == 1)
    {
        /* Pinwheel — filled wedges; Coil bends straight rays into a spiral. */
        float d = spArmDist(spiral_angle, num_arms);
        float fill = 1.0 - smoothstep(half_arm * 0.55, half_arm * 1.05, d);
        float edge = spSoft(d - half_arm * 0.85, crisp_w * 0.85);
        float core = spSoft(d, crisp_w);
        spiral_value = spSaturate(fill * 0.72 + edge * 0.55 + core * 0.95);
        spiral_value *= 0.40 + 0.60 * (1.0 - exp(-r01 * (1.2 + detail_n)));
    }
    else if(pattern_type == 2)
    {
        /* Saw Blade — radial blades with serrated teeth on the leading edge. */
        float period = spPeriod(num_arms);
        float aa = mod(spiral_angle, period);
        if(aa < 0.0) aa += period;
        float blade_w = period * (1.0 - gap_factor) * 0.92;
        /* Triangular teeth along radius — more teeth with more arms/detail. */
        float teeth_n = mix(5.0, 11.0, clamp(detail_n, 0.0, 1.0));
        float tooth = abs(fract(r01 * teeth_n * (0.85 + 0.15 * num_arms)) - 0.5) * 2.0;
        float tooth_depth = 0.38 * (0.55 + 0.45 * (1.0 - gap_factor));
        float leading = blade_w * (1.0 - tooth_depth * (1.0 - tooth));
        float trailing = blade_w * 0.12;

        float in_blade = 0.0;
        if(aa < leading && aa > trailing * 0.25)
        {
            float across = aa / max(leading, 1e-4);
            /* Flat body with a hard leading face. */
            float body = 1.0 - smoothstep(0.82, 1.0, across);
            float face = spSoft(aa - leading, crisp_w * 0.7) * 1.15;
            float trail_edge = spSoft(aa - trailing, crisp_w);
            in_blade = spSaturate(body * 0.85 + face + trail_edge * 0.35);
        }
        /* Outer rim of the disk — saw tip ring. */
        float rim = spSoft(r01 - 0.92, 0.045) * 0.35 * (0.5 + 0.5 * tooth);
        spiral_value = spSaturate(in_blade * (0.45 + 0.55 * r01) + rim);
    }
    else if(pattern_type == 3)
    {
        /* Swirl Circles — concentric rings that Coil twists into spirals. */
        float rings = mix(3.0, 8.0, clamp(detail_n, 0.0, 1.0));
        float ring_phase = r01 * rings * two_pi - progress_e * 0.85 + spiral_angle * (0.15 + 0.85 * coil01);
        float d_ring = abs(sin(ring_phase * 0.5));
        float soft_ring = spSoft(d_ring, soft_w * 0.55);
        float crisp_ring = spSoft(d_ring, crisp_w * 0.45);
        float arm_hint = spRidge(spArmDist(spiral_angle, num_arms), soft_w * 1.2, crisp_w) * 0.35 * coil01;
        float thresh = 0.12 + 0.45 * gap_factor;
        float ring_v = spSaturate(max(soft_ring * 0.65, crisp_ring) - thresh);
        spiral_value = spSaturate(ring_v + arm_hint);
        spiral_value *= 0.50 + 0.50 * (1.0 - r01 * 0.25);
    }
    else if(pattern_type == 4)
    {
        /* Hypnotic — multi-frequency twist with crisp interference ridges. */
        float a0 = spiral_angle;
        float a1 = spiral_angle * 0.5 + norm_twist * freq_scale_e * 2.5 + progress_e * 0.55;
        float a2 = spiral_angle * 1.5 - r01 * (detail_e * 0.35) - progress_e * 0.9;
        float d0 = abs(sin(a0));
        float d1 = abs(sin(a1));
        float d2 = abs(sin(a2));
        float body = spSoft(d0, soft_w * 0.70) * 0.55
                   + spSoft(d1, soft_w * 0.85) * 0.30
                   + spSoft(d2, soft_w * 1.00) * 0.20;
        float ridge = max(spSoft(d0, crisp_w * 0.55),
                      max(spSoft(d1, crisp_w * 0.70) * 0.75,
                          spSoft(d2, crisp_w * 0.80) * 0.55));
        float thresh = 0.08 + 0.40 * gap_factor;
        spiral_value = spSaturate(max(body, ridge) - thresh);
    }
    else if(pattern_type == 5)
    {
        /* Simple Spin — clean rotating rays; Coil = swirl, Height = helix. */
        float d = spArmDist(spiral_angle, num_arms);
        float fill = 1.0 - smoothstep(half_arm * 0.35, half_arm * 0.95, d);
        float core = spSoft(d, crisp_w * 0.75);
        float glow = spSoft(d, soft_w) * 0.45;
        spiral_value = spSaturate(fill * 0.70 + core + glow);
        spiral_value *= 0.38 + 0.62 * (1.0 - min(1.0, r01) * 0.55);
        spiral_value = spSaturate(spiral_value + 0.06 * (1.0 - r01));
    }
    else
    {
        /* Tornado / Twister — funnel wall + wind ribbons from floor→ceiling. */
        float funnel = mix(0.98, 0.10, pow(ly, 0.85));
        float wall_w = mix(0.14, 0.055, clamp(detail_n, 0.0, 1.0));
        float wall = spRidge(abs(r01 - funnel), wall_w * 1.35, wall_w * 0.40);

        /* Wind ribbons wrap tighter as they climb (height coil) and as Coil rises. */
        float wind_twist = angle * num_arms
                         + ly * two_pi * (1.2 + 3.2 * height01 + 1.5 * coil01)
                         + r01 * coil01 * two_pi * 1.8
                         - progress_e * 1.8;
        float d_wind = spArmDist(wind_twist, num_arms);
        float ribbon = spRidge(d_wind, soft_w * 0.95, crisp_w * 0.85);
        /* Keep ribbons near the funnel surface. */
        float near_wall = spSoft(abs(r01 - funnel), wall_w * 2.2);
        ribbon *= 0.35 + 0.65 * near_wall;

        /* Debris / dust orbiting inside. */
        float debris_r = funnel * (0.35 + 0.45 * abs(sin(ly * 9.0 + progress_e * 2.2 + angle * 2.0)));
        float debris = spSoft(abs(r01 - debris_r), 0.07) * 0.40
                     * (0.4 + 0.6 * abs(sin(angle * num_arms * 2.0 - progress_e * 3.0)));

        float spine = (1.0 - smoothstep(0.0, funnel * 0.35, r01)) * 0.18 * (0.5 + 0.5 * ly);

        spiral_value = spSaturate(wall * 0.95 + ribbon * 1.05 + debris + spine);
        spiral_value *= 0.55 + 0.45 * (1.0 - gap_factor * 0.5);
    }

    out_color = vec4(clamp(spiral_value, 0.0, 1.0), 0.0, 0.0, 1.0);
}
)";
}
