// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Note Sparkle volume field: R=energy, G=note hue01, B=radial gradient.
 *  u_params: [0]=drive [1]=particle01 [2]=turbulence [3]=hull_size
 *            [4]=low01 [5]=mid01 [6]=high01 [7]=falloff
 *            [8]=size_m [9]=detail [10]=tight_mul [11]=speed_mul
 *            [12..19]=note pairs (hue01, amp) × 4
 *            [20]=note_count [21]=time_e
 */
inline const char* NoteSparkleVolumeFieldGlsl()
{
    return R"(
float sparkHash01(vec3 p, float salt)
{
    float n = dot(p, vec3(12.9898, 78.233, 37.719)) + salt * 19.19;
    return fract(sin(n) * 43758.5453);
}

vec3 sparkHash3(vec3 p, float salt)
{
    return vec3(
        sparkHash01(p, salt),
        sparkHash01(p + vec3(17.1, 9.3, 3.7), salt + 1.3),
        sparkHash01(p + vec3(5.5, 21.2, 11.9), salt + 2.7));
}

float sparkSmoothstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(1e-5, e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

void volumeMain(out vec4 out_color, in vec3 p01)
{
    float drive = clamp(u_params[0], 0.0, 1.0);
    float particle01 = clamp(u_params[1], 0.0, 1.0);
    float turbulence = clamp(u_params[2], 0.0, 1.0);
    float hull_size = clamp(u_params[3], 0.08, 0.72);
    float low01 = clamp(u_params[4], 0.0, 1.0);
    float mid01 = clamp(u_params[5], 0.0, 1.0);
    float high01 = clamp(u_params[6], 0.0, 1.0);
    float falloff = max(u_params[7], 0.25);
    float size_m = max(u_params[8], 0.35);
    float detail = clamp(u_params[9], 0.05, 1.0);
    float tight = max(u_params[10], 0.25);
    float speed_mul = max(u_params[11], 0.15);
    float note_count = clamp(u_params[20], 0.0, 4.0);
    float time_e = u_params[21];

    float energy_gate = clamp(drive * 0.55 + mid01 * 0.25 + high01 * 0.35 + low01 * 0.12, 0.0, 1.0);
    if(energy_gate < 0.02)
    {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 c = p01 - vec3(0.5);
    float radial = length(c);
    float radial_n = clamp(radial / 0.8660254, 0.0, 1.0);

    /* Soft hull: sphere shell that breathes with bass + note energy. */
    float breathe = 1.0 + 0.28 * low01 + 0.18 * drive;
    float hull_r = hull_size * breathe * (0.55 + 0.45 * size_m);
    float shell_w = (0.035 + 0.055 / falloff) * size_m / tight;
    shell_w *= (0.85 + 0.25 * detail);
    float hull = 1.0 - sparkSmoothstep(0.0, shell_w, abs(radial - hull_r));
    hull *= hull;
    /* Thin cylinder accent so single-strip / ceiling zones still light. */
    float cyl = length(vec2(c.x, c.z));
    float cyl_hull = 1.0 - sparkSmoothstep(0.0, shell_w * 1.15, abs(cyl - hull_r * 0.92));
    cyl_hull *= cyl_hull * 0.65;
    float hull_layer = max(hull, cyl_hull) * (0.35 + 0.65 * energy_gate);

    /* Hash particle cloud near hull — curl / spiral drift. */
    float dens = mix(6.0, 14.0, particle01) * (0.7 + 0.5 * detail);
    dens = clamp(dens, 5.0, 16.0);
    vec3 cell = floor(p01 * dens);
    float best_p = 0.0;
    float best_hue = 0.0;
    float hue_w = 0.0;

    float tspin = time_e * speed_mul * (0.55 + 1.1 * turbulence);
    float ang = tspin * 1.7 + radial_n * 6.2831853 * (0.4 + 0.6 * turbulence);
    float ca = cos(ang);
    float sa = sin(ang);

    int i;
    for(i = 0; i < 8; i++)
    {
        float fi = float(i);
        vec3 off = vec3(mod(fi, 2.0), floor(mod(fi, 4.0) / 2.0), floor(fi / 4.0)) - vec3(0.5);
        vec3 ncell = cell + off;
        vec3 rnd = sparkHash3(ncell, 3.1 + fi * 0.17);
        vec3 local = (ncell + rnd) / dens;

        /* Spiral / outward push from bass. */
        float push = 1.0 + 0.22 * low01 + 0.12 * drive;
        vec3 d0 = local - vec3(0.5);
        float dx = d0.x * ca - d0.z * sa;
        float dz = d0.x * sa + d0.z * ca;
        vec3 drifted = vec3(0.5) + vec3(dx, d0.y + 0.04 * sin(tspin * 2.3 + rnd.y * 6.28), dz) * push;
        drifted.y += (rnd.y - 0.5) * 0.08 * turbulence;

        float pr = length(drifted - vec3(0.5));
        float near_hull = 1.0 - sparkSmoothstep(0.0, shell_w * 2.4, abs(pr - hull_r));
        float pd = length(p01 - drifted);
        float glow_r = mix(0.055, 0.028, detail) * size_m * (0.75 + 0.5 / tight);
        float glow = sparkSmoothstep(glow_r, 0.0, pd);
        glow *= glow;
        glow *= near_hull;

        /* Pick note hue from hash → active note slot. */
        float note_sel = rnd.x * max(note_count, 1.0);
        int ni = int(floor(note_sel));
        if(ni < 0)
            ni = 0;
        if(ni > 3)
            ni = 3;
        float n_hue = u_params[12 + ni * 2];
        float n_amp = u_params[13 + ni * 2];
        if(note_count < 0.5)
        {
            n_hue = 0.55;
            n_amp = energy_gate;
        }
        float contrib = glow * (0.35 + 0.65 * n_amp) * particle01;
        if(contrib > best_p)
            best_p = contrib;
        best_hue += n_hue * contrib;
        hue_w += contrib;
    }

    float particle_layer = best_p * (0.55 + 0.45 * high01 + 0.25 * mid01);
    float intensity = clamp(hull_layer * 0.55 + particle_layer * 1.35, 0.0, 1.0);
    intensity *= energy_gate;
    /* Quiet dark — no room wash when soft. */
    intensity *= sparkSmoothstep(0.03, 0.18, energy_gate);

    float hue01 = 0.55;
    if(hue_w > 1e-4)
        hue01 = clamp(best_hue / hue_w, 0.0, 1.0);
    else if(note_count >= 0.5)
        hue01 = clamp(u_params[12], 0.0, 1.0);

    float gradient = clamp(0.55 * (1.0 - radial_n) + 0.45 * intensity, 0.0, 1.0);
    out_color = vec4(intensity, hue01, gradient, 1.0);
}
)";
}
