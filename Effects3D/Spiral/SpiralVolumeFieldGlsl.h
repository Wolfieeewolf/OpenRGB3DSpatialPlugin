// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Spiral/spin scalar in R. p01 = unit room sample.
 *  u_params: [0]=progress [1]=freq_scale [2]=pattern [3]=num_arms
 *            [4]=gap01 [5]=detail [6]=coil01 [7]=height_coil01
 *            [8]=ox [9]=oy [10]=oz (effect/ref origin — spiral hub)
 *  coil01=0 → pure spin (straight radial arms). Higher → tighter spiral bend.
 */
inline const char* SpiralVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float progress_e = u_params[0];
    float freq_scale_e = max(u_params[1], 0.01);
    int pattern_type = int(u_params[2] + 0.5);
    float num_arms = max(u_params[3], 1.0);
    float gap_factor = clamp(u_params[4], 0.0, 0.95);
    float detail_e = max(u_params[5], 0.05);
    float coil01 = clamp(u_params[6], 0.0, 1.0);
    float height01 = clamp(u_params[7], 0.0, 1.0);
    vec3 origin01 = clamp(vec3(u_params[8], u_params[9], u_params[10]), 0.0, 1.0);

    float lx = (p01.x - origin01.x) * 2.0;
    float ly = clamp(p01.y - origin01.y + 0.5, 0.0, 1.0);
    float lz = (p01.z - origin01.z) * 2.0;
    float angle = atan(lz, lx);
    float norm_radius = clamp(length(vec2(lx, lz)), 0.0, 1.0);
    float norm_twist = ly;
    float two_pi = 6.2831853;
    // Radial coil: 0 = straight spin blades, 1 ≈ ~3.5 turns center→edge.
    float radial_coil = norm_radius * coil01 * two_pi * (2.2 + 1.4 * clamp(detail_e / 20.0, 0.0, 1.5));
    float z_twist = norm_twist * height01 * two_pi * (1.0 + 0.75 * clamp(detail_e / 20.0, 0.0, 1.5));
    float spiral_angle = angle * num_arms + radial_coil + z_twist - progress_e * 1.35;
    float spiral_value = 0.5;

    if(pattern_type == 0)
    {
        spiral_value = sin(spiral_angle) * (1.0 + 0.4 * cos(norm_twist * freq_scale_e * 3.0 + progress_e * 0.7));
        spiral_value += 0.3 * cos(spiral_angle * 0.5 + norm_twist * freq_scale_e * 4.5 + progress_e * 1.2);
        spiral_value = (spiral_value + 1.5) / 3.0;
        float thresh = 0.22 + 0.55 * gap_factor;
        spiral_value = clamp((spiral_value - thresh) / max(1e-3, 1.0 - thresh), 0.0, 1.0);
    }
    else if(pattern_type == 1)
    {
        float period = two_pi / num_arms;
        float arm_angle = mod(spiral_angle, period);
        if(arm_angle < 0.0) arm_angle += period;
        float blade_width = (1.0 - gap_factor) * period;
        if(arm_angle < blade_width)
        {
            float blade_position = arm_angle / max(blade_width, 1e-4);
            spiral_value = 0.5 + 0.5 * cos(blade_position * 3.14159);
        }
        else
        {
            spiral_value = 0.0;
        }
        float radial_fade = 0.4 + 0.6 * (1.0 - exp(-norm_radius * (detail_e * 0.8)));
        spiral_value = spiral_value * radial_fade + 0.1 * radial_fade;
    }
    else if(pattern_type == 2)
    {
        float period = two_pi / num_arms;
        float arm_angle = mod(spiral_angle, period);
        if(arm_angle < 0.0) arm_angle += period;
        float blade_width = (1.0 - gap_factor) * period;
        if(arm_angle < blade_width)
        {
            float blade_position = abs(arm_angle - blade_width * 0.5) / max(blade_width * 0.5, 1e-4);
            spiral_value = 1.0 - blade_position * blade_position;
        }
        else
        {
            spiral_value = 0.0;
        }
        float energy_pulse = 0.2 * sin(norm_radius * (detail_e * 1.2) - progress_e * 2.0);
        spiral_value = max(0.0, spiral_value + energy_pulse);
        float radial_fade = 0.4 + 0.6 * (1.0 - exp(-norm_radius * (detail_e * 0.8)));
        spiral_value *= radial_fade;
    }
    else if(pattern_type == 3)
    {
        spiral_value = 0.5 + 0.5 * sin(spiral_angle + norm_radius * (detail_e * 2.0)) * (1.0 - norm_radius * 0.3);
        float thresh = 0.18 + 0.50 * gap_factor;
        spiral_value = clamp((spiral_value - thresh) / max(1e-3, 1.0 - thresh), 0.0, 1.0);
    }
    else if(pattern_type == 4)
    {
        spiral_value = 0.5 + 0.5 * sin(spiral_angle - progress_e * 0.65) *
                             cos(norm_twist * freq_scale_e * 3.0 + progress_e);
        float thresh = 0.20 + 0.48 * gap_factor;
        spiral_value = clamp((spiral_value - thresh) / max(1e-3, 1.0 - thresh), 0.0, 1.0);
    }
    else
    {
        // Simple Spin: clean rotating blades; Coil bends them into a spiral.
        float period = two_pi / num_arms;
        float arm_angle = mod(spiral_angle, period);
        if(arm_angle < 0.0) arm_angle += period;
        float blade_width = (1.0 - gap_factor) * period * 0.85;
        float blade_core = (arm_angle < blade_width) ? (1.0 - arm_angle / max(blade_width, 1e-4)) : 0.0;
        float blade_glow = 0.0;
        if(arm_angle < blade_width * 1.5)
        {
            float glow_dist = abs(arm_angle - blade_width * 0.5) / max(blade_width * 0.5, 1e-4);
            blade_glow = 0.3 * (1.0 - glow_dist);
        }
        spiral_value = min(1.0, blade_core + blade_glow);
        float radial_fade = 0.35 + 0.65 * (1.0 - min(1.0, norm_radius) * 0.6);
        spiral_value = spiral_value * radial_fade + 0.08 * radial_fade;
    }

    out_color = vec4(clamp(spiral_value, 0.0, 1.0), 0.0, 0.0, 1.0);
}
)";
}
