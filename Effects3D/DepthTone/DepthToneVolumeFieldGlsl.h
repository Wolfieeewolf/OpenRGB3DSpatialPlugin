// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Depth tone: R=(cos(hue)+1)/2, G=(sin(hue)+1)/2, B=value dim.
 *  Cos/sin avoids a one-voxel false seam from filtering wrapped hue01;
 *  range-encoded because the RGBA8 atlas clamps signed values to [0,1].
 *  u_params: [0]=pos [1]=hue_span [2]=percent_dim [3]=axis(0=X,1=Y,2=Z)
 *            [4]=layout(0=linear,1=center,2=ref) [5]=size_zoom [6]=detail
 *            [7]=ox [8]=oy [9]=oz (ref/origin in 01)
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
    int layout = int(clamp(u_params[4], 0.0, 2.0) + 0.5);
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
    else if(layout == 2)
    {
        vec3 c = p01 - vec3(0.5);
        d01 = clamp(length(c) / 0.8660254, 0.0, 1.0);
    }

    // Size zooms the depth mapping (larger = gentler gradient). Linear anchors
    // mid-axis; radial layouts anchor at the origin so the core keeps a gradient
    // instead of clamping to one flat ball around the centre.
    float d_mapped;
    if(layout == 0)
        d_mapped = clamp((d01 - 0.5) / size_zoom + 0.5, 0.0, 1.0);
    else
        d_mapped = clamp(d01 / size_zoom, 0.0, 1.0);

    // Detail sharpens tone steps. Quantize the scrolled phase (not the static
    // distance) so the bands travel with Speed instead of cycling in place.
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
