// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Harmonic pulse: R=intensity val, G=phase01 (color driver).
 *  u_params: [0]=beat_hz-ish motion [1]=spatial_freq [2]=wobble
 *            [3]=contrast [4]=size_density [5]=pulse_mix (0=global beat .. 1=spatial)
 *  p01 is origin-local unit UV (0.5 = Spatial Anchor); pair with SampleGpuVolumeOriginLocal01.
 */
inline const char* HarmonicPulseVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float motion = max(u_params[0], 0.02);
    float spatial_freq = max(u_params[1], 0.5);
    float wobble = clamp(u_params[2], 0.0, 3.0);
    float contrast = clamp(u_params[3], 0.35, 2.5);
    float size_density = max(u_params[4], 0.2);
    float pulse_mix = clamp(u_params[5], 0.0, 1.0);
    const float TWO_PI = 6.2831853;

    float beat = 0.5 + 0.5 * sin(u_time * motion * TWO_PI);
    float beat2 = 0.5 + 0.5 * sin(u_time * motion * 1.618 * TWO_PI + 1.2);
    float master = clamp(0.65 * beat + 0.35 * beat2, 0.0, 1.0);

    float zw = 0.5 + 0.5 * sin(u_time * motion * 0.55 * TWO_PI);
    float zoom = 1.0 + zw * wobble * 0.35;

    float xf = (p01.x - 0.5) * spatial_freq * zoom * size_density;
    float yf = (p01.y - 0.5) * spatial_freq * zoom * size_density;
    float zf = (p01.z - 0.5) * spatial_freq * zoom * size_density;

    float t1 = u_time * motion * TWO_PI;
    float t2 = u_time * motion * 0.73 * TWO_PI;
    float field = 0.5 + 0.5 * (
        0.45 * sin(xf * 6.2831853 + t1) +
        0.30 * cos(yf * 6.2831853 + t2) +
        0.25 * sin(zf * 6.2831853 + t1 - t2));
    field = clamp(field, 0.0, 1.0);

    float val = mix(master, field * (0.35 + 0.65 * master), pulse_mix);
    val = pow(clamp(val, 0.0, 1.0), contrast);
    val = clamp(0.12 + 0.88 * val, 0.0, 1.0);

    float phase01 = fract(master * 0.5 + field * 0.35 + u_time * motion * 0.15);
    out_color = vec4(val, phase01, master, 1.0);
}
)";
}
