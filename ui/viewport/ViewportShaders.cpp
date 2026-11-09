// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportShaders.h"
#include "GlProgram.h"

namespace ViewportShaders
{

const char* UnlitColorVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
uniform mat4 u_mvp;
out vec3 v_color;
void main()
{
    v_color = a_color;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* UnlitColorFragment()
{
    return R"GLSL(
#version 410 core
in vec3 v_color;
uniform float u_alpha;
out vec4 frag_color;
void main()
{
    frag_color = vec4(v_color, u_alpha);
}
)GLSL";
}

const char* UnlitPointVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
uniform mat4 u_mvp;
uniform float u_point_size;
out vec3 v_color;
void main()
{
    v_color = a_color;
    gl_PointSize = u_point_size;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* UnlitPointFragment()
{
    return UnlitColorFragment();
}

const char* TexturedUnlitVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_texcoord;
uniform mat4 u_mvp;
out vec3 v_color;
out vec2 v_texcoord;
void main()
{
    v_color = a_color;
    v_texcoord = a_texcoord;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* TexturedUnlitFragment()
{
    return R"GLSL(
#version 410 core
in vec3 v_color;
in vec2 v_texcoord;
uniform sampler2D u_texture;
uniform float u_alpha;
out vec4 frag_color;
void main()
{
    vec4 tex = texture(u_texture, v_texcoord);
    frag_color = vec4(tex.rgb * v_color, tex.a * u_alpha);
}
)GLSL";
}

bool CompileUnlitColor(GlProgram& out, QString* error_log)
{
    return out.Compile(UnlitColorVertex(), UnlitColorFragment(), error_log);
}

bool CompileUnlitPoint(GlProgram& out, QString* error_log)
{
    return out.Compile(UnlitPointVertex(), UnlitPointFragment(), error_log);
}

bool CompileTexturedUnlit(GlProgram& out, QString* error_log)
{
    return out.Compile(TexturedUnlitVertex(), TexturedUnlitFragment(), error_log);
}

const char* VolumeAtlasPointVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
uniform mat4 u_mvp;
uniform float u_point_size;
out vec3 v_world;
void main()
{
    v_world = a_position;
    gl_PointSize = u_point_size;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* VolumeAtlasPointFragment()
{
    return R"GLSL(
#version 410 core
in vec3 v_world;
uniform float u_alpha;
uniform float u_grid_brightness;
uniform int u_layer_count;
uniform sampler2D u_atlas0;
uniform sampler2D u_atlas1;
uniform sampler2D u_atlas2;
uniform sampler2D u_atlas3;
uniform int u_atlas_n[4];
uniform int u_atlas_nearest[4];
uniform int u_blend[4];
uniform int u_sample_room[4];
uniform int u_volume_tools[4];
uniform int u_volume_shape[4];
uniform vec3 u_origin[4];
uniform vec3 u_half_ext[4];
uniform vec3 u_room_min[4];
uniform vec3 u_room_max[4];
uniform vec3 u_shear[4];
uniform mat3 u_rot[4];
uniform float u_layer_brightness[4];
uniform int u_finish[4];
uniform float u_hue_scroll[4];
uniform int u_palette_count[4];
uniform vec3 u_palette[16]; /* 4 layers * 4 stops */
out vec4 frag_color;

float axis01(float v, float lo, float hi)
{
    float d = hi - lo;
    if(abs(d) < 1e-6) return 0.5;
    return clamp((v - lo) / d, 0.0, 1.0);
}

vec3 sampleAtlas(sampler2D atlas, int n, vec3 p01, int nearest)
{
    float nf = float(max(n, 2));
    if(nearest != 0)
    {
        /* Snap to voxel centers — keeps thin laser shafts from greying into fog. */
        float ix = min(floor(clamp(p01.x, 0.0, 1.0) * nf), nf - 1.0);
        float iy = min(floor(clamp(p01.y, 0.0, 1.0) * nf), nf - 1.0);
        float iz = min(floor(clamp(p01.z, 0.0, 1.0) * nf), nf - 1.0);
        float u = (ix + 0.5) / nf;
        float v = (iz * nf + iy + 0.5) / (nf * nf);
        return texture(atlas, vec2(u, v)).rgb;
    }
    float fx = clamp(p01.x, 0.0, 1.0) * (nf - 1.0);
    float fy = clamp(p01.y, 0.0, 1.0) * (nf - 1.0);
    float fz = clamp(p01.z, 0.0, 1.0) * (nf - 1.0);
    int z0 = int(floor(fz));
    int z1 = min(z0 + 1, n - 1);
    float tz = fz - float(z0);
    float u = (fx + 0.5) / nf;
    float v0 = (float(z0) * nf + fy + 0.5) / (nf * nf);
    float v1 = (float(z1) * nf + fy + 0.5) / (nf * nf);
    vec3 c0 = texture(atlas, vec2(u, v0)).rgb;
    vec3 c1 = texture(atlas, vec2(u, v1)).rgb;
    return mix(c0, c1, tz);
}

vec3 rainbowHue(float hue_deg)
{
    float hue = mod(hue_deg, 360.0);
    if(hue < 0.0) hue += 360.0;
    float c = 1.0;
    float x = c * (1.0 - abs(mod(hue / 60.0, 2.0) - 1.0));
    if(hue < 60.0) return vec3(c, x, 0.0);
    if(hue < 120.0) return vec3(x, c, 0.0);
    if(hue < 180.0) return vec3(0.0, c, x);
    if(hue < 240.0) return vec3(0.0, x, c);
    if(hue < 300.0) return vec3(x, 0.0, c);
    return vec3(c, 0.0, x);
}

vec3 hsv2rgb(float h, float s, float v)
{
    h = fract(h);
    s = clamp(s, 0.0, 1.0);
    v = clamp(v, 0.0, 1.0);
    float hf = h * 6.0;
    int i = int(mod(floor(hf), 6.0));
    float f = hf - floor(hf);
    float p = v * (1.0 - s);
    float q = v * (1.0 - f * s);
    float t = v * (1.0 - (1.0 - f) * s);
    if(i == 0) return vec3(v, t, p);
    if(i == 1) return vec3(q, v, p);
    if(i == 2) return vec3(p, v, t);
    if(i == 3) return vec3(p, q, v);
    if(i == 4) return vec3(t, p, v);
    return vec3(v, p, q);
}

vec3 samplePalette(int layer, float t)
{
    int n = clamp(u_palette_count[layer], 1, 4);
    float pos = clamp(t, 0.0, 1.0) * float(n - 1);
    int i0 = int(floor(pos));
    int i1 = min(i0 + 1, n - 1);
    float f = pos - float(i0);
    int base = layer * 4;
    return mix(u_palette[base + i0], u_palette[base + i1], f);
}

vec3 applyFinish(int layer, vec3 samp)
{
    int mode = u_finish[layer];
    float scroll = u_hue_scroll[layer];
    if(mode == 0)
    {
        /* FinishRgb: volume shaders pack like OpenRGB 0x00BBGGRR (texel rgb is B,G,R)
           — swap to linear/display RGB for the overlay. */
        return vec3(samp.b, samp.g, samp.r);
    }
    if(mode == 1)
    {
        /* finish_hsv: atlas (s, v, h) */
        if(samp.y <= 1e-5) return vec3(0.0);
        return hsv2rgb(samp.z + scroll / 360.0, samp.x, samp.y);
    }
    if(mode == 2)
    {
        /* finish_surface: r=intensity, g=plasma */
        float inten = samp.x;
        if(inten < 0.01) return vec3(0.0);
        vec3 c = (u_palette_count[layer] > 0)
            ? samplePalette(layer, samp.y)
            : rainbowHue(samp.y * 360.0 + scroll);
        return c * inten;
    }
    if(mode == 3)
    {
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        return rainbowHue(v * 360.0 + scroll) * v;
    }
    if(mode == 4)
    {
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        return samplePalette(layer, v) * v;
    }
    if(mode == 5)
    {
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        return rainbowHue(samp.y * 360.0 + scroll) * v;
    }
    if(mode == 6)
    {
        float hue = atan(samp.y * 2.0 - 1.0, samp.x * 2.0 - 1.0) / 6.2831853;
        float v = samp.z > 0.0 ? clamp(samp.z, 0.0, 1.0) : 1.0;
        if(v < 0.01) return vec3(0.0);
        return rainbowHue(fract(hue + scroll / 360.0) * 360.0) * v;
    }
    return samp;
}

float shapeNd(vec3 nrm, int shape)
{
    if(shape == 1) return length(nrm);
    if(shape == 2) return max(length(nrm.xz), abs(nrm.y));
    if(shape == 3)
    {
        float t = clamp(0.5 * (1.0 - nrm.y), 0.0, 1.0);
        return max(length(nrm.xz) / max(t, 1e-4), abs(nrm.y));
    }
    return max(max(abs(nrm.x), abs(nrm.y)), abs(nrm.z));
}

bool worldToP01(int i, vec3 world, out vec3 p01)
{
    if(u_sample_room[i] != 0)
    {
        p01 = vec3(
            axis01(world.x, u_room_min[i].x, u_room_max[i].x),
            axis01(world.y, u_room_min[i].y, u_room_max[i].y),
            axis01(world.z, u_room_min[i].z, u_room_max[i].z));
        return true;
    }
    vec3 delta = world - u_origin[i];
    vec3 local = u_rot[i] * delta;
    if(u_volume_tools[i] != 0)
    {
        local.y -= u_shear[i].z * local.z;
        local.x -= u_shear[i].x * local.y + u_shear[i].y * local.z;
    }
    vec3 he = max(u_half_ext[i], vec3(1e-4));
    vec3 nrm = local / he;
    if(shapeNd(nrm, u_volume_shape[i]) > 1.0)
    {
        return false;
    }
    p01 = clamp(0.5 + 0.5 * nrm, 0.0, 1.0);
    return true;
}

vec3 layerColor(int i, vec3 world)
{
    vec3 p01;
    if(!worldToP01(i, world, p01))
    {
        return vec3(0.0);
    }
    int nearest = u_atlas_nearest[i];
    vec3 samp;
    if(i == 0) samp = sampleAtlas(u_atlas0, u_atlas_n[0], p01, nearest);
    else if(i == 1) samp = sampleAtlas(u_atlas1, u_atlas_n[1], p01, nearest);
    else if(i == 2) samp = sampleAtlas(u_atlas2, u_atlas_n[2], p01, nearest);
    else samp = sampleAtlas(u_atlas3, u_atlas_n[3], p01, nearest);
    return applyFinish(i, samp) * u_layer_brightness[i];
}

vec3 blendRgb(vec3 base, vec3 over, int mode)
{
    if(mode <= 0) return over;
    if(mode == 1) return over;
    if(mode == 2) return min(base + over, vec3(1.0));
    if(mode == 3) return base * over;
    if(mode == 4) return vec3(1.0) - (vec3(1.0) - base) * (vec3(1.0) - over);
    if(mode == 5) return max(base, over);
    if(mode == 6) return min(base, over);
    return over;
}

void main()
{
    vec3 rgb = vec3(0.0);
    int n = clamp(u_layer_count, 0, 4);
    for(int i = 0; i < 4; ++i)
    {
        if(i >= n) break;
        rgb = blendRgb(rgb, layerColor(i, v_world), u_blend[i]);
    }
    frag_color = vec4(rgb * u_grid_brightness, u_alpha);
}
)GLSL";
}

bool CompileVolumeAtlasPoint(GlProgram& out, QString* error_log)
{
    return out.Compile(VolumeAtlasPointVertex(), VolumeAtlasPointFragment(), error_log);
}

} // namespace ViewportShaders
