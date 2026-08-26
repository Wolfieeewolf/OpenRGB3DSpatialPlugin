// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Texture projection: atlas stores final RGB (media + ambience).
 *  u_params: [0]=mode [1]=tile [2]=scroll_rate [3]=phase_mul [4]=amp [5]=detail_s
 *            [6]=fd [7]=curve [8]=edge [9]=prop [10]=steps_u
 *            [11]=steps_v_and_q (v + use_q*1000) [12]=wrap_mode
 *  sampler2D u_media — V flipped to match CPU SampleImageBilinear.
 */
inline const char* TextureProjectionVolumeFieldGlsl()
{
    return R"(
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
float frac01(float x)
{
    return x - floor(x);
}
float ambienceGain(float dist_n, float d_face, float fd, float c, float es)
{
    float g = 1.0;
    float amount = clamp(fd * 0.95 + c * 0.55, 0.0, 1.0);
    if(amount > 1e-4)
    {
        float t = clamp(dist_n, 0.0, 1.0);
        float linear = 1.0 - t * (0.12 + 0.88 * amount);
        float exponent = 0.70 + 3.8 * c;
        g *= pow(clamp(linear, 0.0, 1.0), exponent);
    }
    if(es > 1e-4)
    {
        float feather = 0.035 + 0.55 * es;
        g *= smstep(0.0, feather, d_face);
    }
    return clamp(g, 0.0, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    int mode = int(clamp(u_params[0], 0.0, 3.0) + 0.5);
    float tile = max(u_params[1], 0.12);
    float scroll_rate = u_params[2];
    float phase_mul = clamp(u_params[3], 0.0, 2.0);
    float amp = u_params[4];
    float detail_s = max(u_params[5], 0.05);
    float fd = clamp(u_params[6], 0.0, 1.0);
    float curve = clamp(u_params[7], 0.0, 1.0);
    float edge = clamp(u_params[8], 0.0, 1.0);
    float prop = clamp(u_params[9], 0.0, 1.0);
    float steps_u = max(u_params[10], 2.0);
    float packed_v = u_params[11];
    float steps_v = max(mod(packed_v, 1000.0), 2.0);
    float use_q = step(500.0, packed_v);
    float wrap_mode = u_params[12];

    float u = 0.5;
    float v = 0.5;
    if(mode == 0)
    {
        u = p01.x;
        v = p01.z;
    }
    else if(mode == 1)
    {
        u = p01.x;
        v = p01.y;
    }
    else if(mode == 2)
    {
        u = p01.y;
        v = p01.z;
    }
    else
    {
        vec3 d = p01 - vec3(0.5);
        float len = length(d);
        if(len < 1e-5)
        {
            u = 0.5;
            v = 0.5;
        }
        else
        {
            d /= len;
            u = atan(d.z, d.x) / 6.2831853 + 0.5;
            v = asin(clamp(d.y, -1.0, 1.0)) / 3.14159265 + 0.5;
        }
    }

    u = (u - 0.5) * tile + 0.5;
    v = (v - 0.5) * tile + 0.5;

    float dist_n = length(p01 - vec3(0.5)) * 1.7320508;
    float t_eff = u_time - prop * dist_n * 3.2;
    float v_ratio = 0.12 + 0.88 * clamp(phase_mul, 0.0, 1.0);
    float su = t_eff * scroll_rate;
    float sv = t_eff * scroll_rate * v_ratio;
    u += su;
    v += sv;

    float warp_ph = t_eff * (0.55 + 3.8 * phase_mul);
    u += sin(warp_ph + u * 9.0 * detail_s * 0.1 + v * 6.0 * detail_s * 0.08) * amp;
    v += cos(warp_ph * 0.93 + u * 7.0 * detail_s * 0.08 - v * 8.5 * detail_s * 0.1) * amp;

    /* Motion always loops the image; idle clamp only when wrap_mode off and no motion. */
    float do_wrap = 0.0;
    if(wrap_mode > 0.5)
        do_wrap = 1.0;
    if(abs(scroll_rate) > 1e-5)
        do_wrap = 1.0;
    if(amp > 1e-4)
        do_wrap = 1.0;
    if(do_wrap > 0.5)
    {
        u = frac01(u);
        v = frac01(v);
    }
    else
    {
        u = clamp(u, 0.0, 1.0);
        v = clamp(v, 0.0, 1.0);
    }

    if(use_q > 0.5)
    {
        u = floor(u * steps_u) / steps_u;
        v = floor(v * steps_v) / steps_v;
    }

    vec3 rgb = texture2D(u_media, vec2(u, 1.0 - v)).rgb;

    float d_face = min(min(min(p01.x, 1.0 - p01.x), min(p01.y, 1.0 - p01.y)), min(p01.z, 1.0 - p01.z));
    float ag = ambienceGain(clamp(dist_n, 0.0, 1.0), d_face, fd, curve, edge);
    out_color = vec4(rgb * ag, 1.0);
}
)";
}
