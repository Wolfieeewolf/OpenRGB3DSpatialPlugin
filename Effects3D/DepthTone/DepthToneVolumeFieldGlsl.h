// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/* R=(cos(hue)+1)/2 G=(sin(hue)+1)/2 B=value
 * u_params: [0]=pos [1]=hue_span [2]=percent_dim [3]=axis
 *           [4]=layout(0=linear,1=center) [5]=size_zoom [6]=detail
 */
inline const char* DepthToneVolumeFieldGlsl()
{
    return R"(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float pos = u_params[0];
    float hue_span = u_params[1];
    float percent_dim = u_params[2];
    int axis = int(clamp(u_params[3], 0.0, 2.0) + 0.5);
    int layout = int(clamp(u_params[4], 0.0, 1.0) + 0.5);
    float size_zoom = max(u_params[5], 0.15);
    float detail = clamp(u_params[6], 0.05, 1.0);

    float linear01 = p01.z;
    if(axis == 0) linear01 = p01.x;
    else if(axis == 1) linear01 = p01.y;

    float d01 = linear01;
    if(layout == 1)
    {
        vec3 c = p01 - vec3(0.5);
        d01 = clamp(length(c) / 0.8660254, 0.0, 1.0);
    }

    float d_mapped;
    if(layout == 0)
        d_mapped = clamp((d01 - 0.5) / size_zoom + 0.5, 0.0, 1.0);
    else
        d_mapped = clamp(d01 / size_zoom, 0.0, 1.0);

    float tones = max(2.0, 2.0 + hue_span * 32.0);
    float soft = clamp(detail, 0.0, 1.0);
    float phase = pos + d_mapped * hue_span;
    float stepped = floor(phase * tones + 1e-4) / tones;
    float phase_use = mix(phase, mix(phase, stepped, 0.85), soft);

    float hue_turns = fract(phase_use + 1.0);
    float ang = hue_turns * 6.2831853;
    float center = 1.0 - abs(d_mapped - 0.5) * 2.0;
    float v = clamp(1.0 - percent_dim * center, 0.0, 1.0);
    out_color = vec4(cos(ang) * 0.5 + 0.5, sin(ang) * 0.5 + 0.5, v, 1.0);
}
)";
}
