// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Audio Pulse volume field: R=shell energy, G=winning pulse index01, B=radial gradient.
 *  p01 = origin-local UV (0.5 = Spatial Anchor). CPU packs up to 5 active rings.
 *  u_params: [0]=wave_mode [1]=size_m [2]=detail [3]=falloff
 *            [4]=pulse_speed [5]=radius_basis [6]=half_w [7]=max_travel
 *            [8]=particle01 [9]=decay [10]=hw [11]=hh [12]=hd [13]=tight_mul
 *            [14..23]=pulse pairs (age, strength) × 5
 */
inline const char* AudioPulseVolumeFieldGlsl()
{
    return R"(
float pulseHash01(vec3 p, float salt)
{
    float n = dot(p, vec3(12.9898, 78.233, 37.719)) + salt * 19.19;
    return fract(sin(n) * 43758.5453);
}

float pulseSmoothstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(1e-5, e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

float pulseExpandingRing(float coord, float ring_radius, float half_w)
{
    float dist_from_ring = abs(coord - ring_radius);
    float band = 1.0 - pulseSmoothstep(0.0, half_w, dist_from_ring);
    band *= band;
    if(coord < ring_radius - half_w * 1.15)
        band = 0.0;
    return band;
}

float pulseClassicShell(float distance, float radius_basis, float age, float pulse_speed,
                        float falloff, float size_m, float detail, float tight_mul, float burst_cap)
{
    float tm = max(tight_mul, 0.25);
    float size_cl = clamp(size_m, 0.2, 2.0);
    float wave_thickness = radius_basis * (0.02 + 0.09 / max(0.2, falloff)) * size_cl;
    wave_thickness /= max(0.25, tm);
    wave_thickness *= clamp(0.85 + 0.15 * detail, 0.7, 1.15);

    float travel_speed = pulse_speed;
    float burst_phase = min(max(burst_cap, 0.1), age * travel_speed * 0.55);
    float explosion_radius = burst_phase * radius_basis * (0.14 + 0.86 * size_cl);

    float primary = 1.0 - pulseSmoothstep(explosion_radius - wave_thickness,
                                          explosion_radius + wave_thickness,
                                          distance);
    primary *= exp(-abs(distance - explosion_radius)
                   * (7.0 / max(wave_thickness, radius_basis * 0.0015)));

    float secondary_radius = explosion_radius * 0.68;
    float secondary = 1.0 - pulseSmoothstep(secondary_radius - wave_thickness * 0.45,
                                            secondary_radius + wave_thickness * 0.55,
                                            distance);
    secondary *= exp(-abs(distance - secondary_radius) * 0.11) * 0.62;

    float shock_age = exp(-age * 2.4);
    float freq_rip = detail * tm * 0.08 / max(0.08, size_cl);
    float shock = 0.09 * shock_age
        * sin(distance * freq_rip * 10.0 - burst_phase * 6.2831853);
    shock *= exp(-distance * 0.065);

    float core = 0.0;
    if(distance < explosion_radius * 0.24)
        core = (1.0 - distance / (explosion_radius * 0.24 + 1e-4)) * 0.4;

    return min(1.0, primary + secondary + shock + core);
}

float pulseDebris(vec3 p01, float burst_phase, float distance, float shell_radius, float particle01, float salt)
{
    if(particle01 <= 0.001)
        return 0.0;
    float h = pulseHash01(p01 * 17.0, salt + burst_phase * 10.0);
    if(h > particle01)
        return 0.0;
    float spread = shell_radius * (0.35 + 0.65 * min(1.0, burst_phase));
    float falloff = exp(-abs(distance - spread * 0.85) * 0.12);
    return falloff * (0.45 + 0.55 * h) * particle01;
}

float pulseBeatShell(int mode, float age, float distance, float ring_radius, float half_w, float expanding)
{
    float instant = exp(-age * 18.0);
    if(mode == 3)
        return instant;
    if(mode == 2)
        return expanding;
    if(mode == 4)
    {
        if(age < 0.10)
            return 0.0;
        return expanding;
    }
    if(mode == 5)
    {
        float lit_outside = pulseSmoothstep(0.0, half_w * 1.35, distance - ring_radius);
        return max(instant, lit_outside);
    }
    if(mode == 1)
        return clamp(expanding + instant * 0.9, 0.0, 1.0);
    return expanding;
}

float pulseContrib(int wave_mode, float age, float strength, float distance, float radius_basis,
                   float pulse_speed, float falloff, float size_m, float detail, float tight_mul,
                   float half_w, float max_travel_or_burst, float particle01, float decay, float height_fade,
                   vec3 p01)
{
    if(strength <= 0.001 || age < 0.0)
        return 0.0;

    if(wave_mode == 0)
    {
        float shell = pulseClassicShell(distance, radius_basis, age, pulse_speed,
                                        falloff, size_m, detail, tight_mul, max_travel_or_burst);
        float burst_phase = min(max(max_travel_or_burst, 0.1), age * pulse_speed * 0.55);
        float explosion_radius = burst_phase * radius_basis * (0.14 + 0.86 * size_m);
        float debris = max(
            pulseDebris(p01, burst_phase, distance, explosion_radius, particle01, 1.0),
            pulseDebris(p01, burst_phase * 0.97, distance, explosion_radius * 0.92, particle01, 2.0));
        shell = max(shell, debris);
        float fade = strength * exp(-decay * age);
        return fade * height_fade * shell;
    }

    float max_travel = max_travel_or_burst;
    float ring_age = age;
    if(wave_mode == 4)
        ring_age = max(0.0, age - 0.10);
    float ring_radius = ring_age * pulse_speed * max_travel;
    if(wave_mode != 3 && ring_radius > max_travel * 1.04)
        return 0.0;
    if(wave_mode == 3 && age > 0.35)
        return 0.0;

    float expanding = 0.0;
    if(wave_mode != 3)
        expanding = pulseExpandingRing(distance, ring_radius, half_w);
    float shell = pulseBeatShell(wave_mode, age, distance, ring_radius, half_w, expanding);

    if(particle01 > 0.001 && wave_mode != 3
       && abs(distance - ring_radius) < half_w * 2.0 && shell > 0.02)
    {
        float burst_phase = ring_radius / max(max_travel, 1e-4);
        float debris = max(
            pulseDebris(p01, burst_phase, distance, ring_radius, particle01, 1.0),
            pulseDebris(p01, burst_phase * 0.97, distance, ring_radius, particle01, 2.0));
        shell = max(shell, debris * shell);
    }

    float fade = strength * exp(-decay * age);
    return fade * height_fade * shell;
}

void volumeMain(out vec4 out_color, in vec3 p01)
{
    int wave_mode = int(floor(u_params[0] + 0.5));
    float size_m = clamp(u_params[1], 0.2, 2.0);
    float detail = max(u_params[2], 0.05);
    float falloff = max(u_params[3], 0.25);
    float pulse_speed = max(u_params[4], 0.01);
    float radius_basis = max(u_params[5], 1e-3);
    float half_w = max(u_params[6], 1e-4);
    float max_travel = max(u_params[7], 1e-3);
    float particle01 = clamp(u_params[8], 0.0, 1.0);
    float decay = max(u_params[9], 0.01);
    float hw = max(u_params[10], 1e-5);
    float hh = max(u_params[11], 1e-5);
    float hd = max(u_params[12], 1e-5);
    float tight_mul = max(u_params[13], 0.25);

    vec3 l = p01 * 2.0 - 1.0;
    float distance = length(vec3(l.x * hw, l.y * hh, l.z * hd));
    float height_fade = clamp(1.0 - p01.y, 0.15, 1.0);

    float c0 = pulseContrib(wave_mode, u_params[14], u_params[15], distance, radius_basis,
                            pulse_speed, falloff, size_m, detail, tight_mul,
                            half_w, max_travel, particle01, decay, height_fade, p01);
    float c1 = pulseContrib(wave_mode, u_params[16], u_params[17], distance, radius_basis,
                            pulse_speed, falloff, size_m, detail, tight_mul,
                            half_w, max_travel, particle01, decay, height_fade, p01);
    float c2 = pulseContrib(wave_mode, u_params[18], u_params[19], distance, radius_basis,
                            pulse_speed, falloff, size_m, detail, tight_mul,
                            half_w, max_travel, particle01, decay, height_fade, p01);
    float c3 = pulseContrib(wave_mode, u_params[20], u_params[21], distance, radius_basis,
                            pulse_speed, falloff, size_m, detail, tight_mul,
                            half_w, max_travel, particle01, decay, height_fade, p01);
    float c4 = pulseContrib(wave_mode, u_params[22], u_params[23], distance, radius_basis,
                            pulse_speed, falloff, size_m, detail, tight_mul,
                            half_w, max_travel, particle01, decay, height_fade, p01);

    float energy = 0.0;
    float best = c0;
    float best_idx = 0.0;
    if(wave_mode == 0)
    {
        energy = clamp(c0 + c1 + c2 + c3 + c4, 0.0, 1.0);
        if(c1 > best) { best = c1; best_idx = 1.0; }
        if(c2 > best) { best = c2; best_idx = 2.0; }
        if(c3 > best) { best = c3; best_idx = 3.0; }
        if(c4 > best) { best = c4; best_idx = 4.0; }
    }
    else
    {
        energy = c0;
        if(c1 > energy) { energy = c1; best_idx = 1.0; }
        if(c2 > energy) { energy = c2; best_idx = 2.0; }
        if(c3 > energy) { energy = c3; best_idx = 3.0; }
        if(c4 > energy) { energy = c4; best_idx = 4.0; }
        energy = clamp(energy * 1.28, 0.0, 1.0);
    }

    float gradient = clamp(distance / radius_basis, 0.0, 1.0);
    float pulse_idx01 = (best_idx + 0.5) / 5.0;
    out_color = vec4(energy, pulse_idx01, gradient, 1.0);
}
)";
}
