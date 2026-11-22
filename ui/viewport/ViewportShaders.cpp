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
uniform int u_kind[4];       /* 0 volume atlas, 1 projected 2D frame (Shader Field) */
uniform int u_projection[4]; /* ShaderField::ProjectionMode for kind 1 */
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
/* KindScreenMirror (one layer): per display plane, mirrors ScreenMirror::CalculateColorGrid. */
uniform int u_sm_plane_count;
uniform float u_sm_grid_scale_mm;
uniform vec3 u_sm_frame[4];      /* w, h, row0 in the stacked texture */
uniform vec3 u_sm_pos[4];
uniform mat3 u_sm_rot[4];        /* use as delta * M (world -> plane local) */
uniform vec3 u_sm_inv_scale[4];
uniform vec3 u_sm_half[4];       /* half_w, half_h, zone_count */
uniform vec3 u_sm_crop_u[4];     /* lo, hi */
uniform vec3 u_sm_crop_v[4];
uniform vec3 u_sm_falloff[4];    /* max_mm, coverage, softness */
uniform vec3 u_sm_falloff2[4];   /* curve, inverted */
uniform vec3 u_sm_grade0[4];     /* brightness, threshold, white_rolloff */
uniform vec3 u_sm_grade1[4];     /* vibrance, white_rolloff_max */
uniform vec3 u_sm_gain[4];
uniform vec3 u_sm_zone_u[16];    /* plane*4 + zone: u_min, u_max */
uniform vec3 u_sm_zone_v[16];
/* KindReactive (one layer): wave rows in the layer texture, 4 RGBA32F texels each. */
uniform int u_rw_count;
uniform float u_rw_cycles;
uniform float u_rw_min_sigma; /* overlay-only ring width floor (~0.75 lattice step) */
/* KindMinecraft (one layer): slot map in MinecraftGame::FillOverlayLayer. */
uniform vec3 u_mc[18];
uniform int u_rainbow[4];     /* Rainbow vs palette for audio / spiral / hex-atlas finishes */
uniform int u_depth_shade[4]; /* finish_depth anchor-distance shade */
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

vec3 sampleAtlas(sampler2D atlas, int n, vec3 p01)
{
    float nf = float(max(n, 2));
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

/* Palette (user colours) unless Rainbow is on or there are no colours — the same
   GetColorAtPosition / GetRainbowColor choice the LED finishes make. */
vec3 paletteOrRainbow(int layer, float t01, float scroll_deg)
{
    if(u_rainbow[layer] != 0 || u_palette_count[layer] <= 0)
    {
        return rainbowHue(t01 * 360.0 + scroll_deg);
    }
    return samplePalette(layer, fract(t01 + scroll_deg / 360.0));
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
        return paletteOrRainbow(layer, fract(hue + 1.0), scroll) * v;
    }
    if(mode == 7)
    {
        /* finish_audio: r=intensity, g=gradient; AudioEffectDisplayBrightness curve. */
        float inten = clamp(samp.x, 0.0, 1.0);
        if(inten < 0.04) return vec3(0.0);
        float lift = clamp(0.08 + 0.92 * pow(inten, 0.88), 0.0, 1.15);
        return paletteOrRainbow(layer, clamp(samp.y, 0.0, 1.0), scroll) * lift;
    }
    if(mode == 8)
    {
        /* finish_spiral: value drives colour and the value^0.85 mask. */
        float v = clamp(samp.x, 0.0, 1.0);
        if(v < 0.01) return vec3(0.0);
        return paletteOrRainbow(layer, v, scroll) * pow(v, 0.85);
    }
    if(mode == 9)
    {
        /* finish_hex with Rainbow off: palette at the hue channel, times value. */
        float v = samp.x;
        if(v < 0.01) return vec3(0.0);
        return paletteOrRainbow(layer, clamp(samp.y, 0.0, 1.0), 0.0) * v;
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

/* Mirror of ShaderField::SampleUv — keep in sync. Returns false when the LED path
   would leave the point unlit (uv outside [0,1] on non-wrapping projections). */
bool projectUv(int mode, vec3 n, out vec2 uv)
{
    const float TWO_PI = 6.2831853;
    float u = 0.5;
    float v = 0.5;
    bool wrap_u = false;
    if(mode == 0)      { u = n.x; v = n.z; }              /* floor */
    else if(mode == 1) { u = n.x; v = n.y; }              /* front wall */
    else if(mode == 2) { u = 1.0 - n.z; v = n.y; }        /* left wall */
    else if(mode == 4) { u = n.x; v = 1.0 - n.z; }        /* ceiling */
    else if(mode == 5) { u = 1.0 - n.x; v = n.y; }        /* back wall */
    else if(mode == 6) { u = n.z; v = n.y; }              /* right wall */
    else if(mode == 7)                                     /* cylinder around Y */
    {
        u = atan(n.z * 2.0 - 1.0, n.x * 2.0 - 1.0) / TWO_PI + 0.5;
        v = n.y;
        wrap_u = true;
    }
    else if(mode == 8)                                     /* radial floor */
    {
        vec2 r = vec2(n.x, n.z) * 2.0 - 1.0;
        u = atan(r.y, r.x) / TWO_PI + 0.5;
        v = length(r);
        wrap_u = true;
    }
    else if(mode == 9 || mode == 10)                       /* triplanar / cube face */
    {
        vec3 a = abs(n - 0.5);
        if(a.x >= a.y && a.x >= a.z)      { u = (n.x >= 0.5) ? n.z : (1.0 - n.z); v = n.y; }
        else if(a.y >= a.x && a.y >= a.z) { u = n.x; v = (n.y >= 0.5) ? (1.0 - n.z) : n.z; }
        else                              { u = (n.z >= 0.5) ? (1.0 - n.x) : n.x; v = n.y; }
    }
    else if(mode == 3)                                     /* sphere from origin */
    {
        vec3 r = n * 2.0 - 1.0;
        float len = length(r);
        if(len >= 1e-4)
        {
            r /= len;
            u = atan(r.z, r.x) / TWO_PI + 0.5;
            v = asin(clamp(r.y, -1.0, 1.0)) / 3.14159265 + 0.5;
            wrap_u = true;
        }
    }
    if(wrap_u) u = fract(u);
    uv = vec2(u, v);
    return !(u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0);
}

vec3 sampleFrame(int i, vec2 uv)
{
    /* Frame rows are uploaded top-first; the LED path reads row (1 - v). */
    vec2 t = vec2(uv.x, 1.0 - uv.y);
    if(i == 0) return texture(u_atlas0, t).rgb;
    if(i == 1) return texture(u_atlas1, t).rgb;
    if(i == 2) return texture(u_atlas2, t).rgb;
    return texture(u_atlas3, t).rgb;
}

/* ---- Screen Mirror (mirror of ScreenMirror_Render.cpp / Geometry3D::SpatialMapToScreen) ---- */

vec2 smMapToScreen(int k, vec3 world)
{
    vec3 local = ((world - u_sm_pos[k]) * u_sm_rot[k]) * u_sm_inv_scale[k];
    float hw = max(u_sm_half[k].x, 1e-4);
    float hh = max(u_sm_half[k].y, 1e-4);
    vec3 nn = local / vec3(hw, hh, max(hw, hh));
    float len = length(nn);
    if(len < 1e-6) return vec2(0.5);
    vec3 p = nn / len;
    float tz = p.z - 1.0;
    float denom = p.x * p.x + p.y * p.y + tz * tz;
    if(denom < 1e-8) return vec2(0.5);
    float m = 2.0 * sqrt(denom);
    return clamp(vec2(0.5 + p.x / m, 0.5 + p.y / m), 0.0, 1.0);
}

bool smInZones(int k, vec2 uv)
{
    int n = int(u_sm_half[k].z + 0.5);
    if(n <= 0) return true;
    for(int z = 0; z < 4; ++z)
    {
        if(z >= n) break;
        vec3 zu = u_sm_zone_u[k * 4 + z];
        vec3 zv = u_sm_zone_v[k * 4 + z];
        if(uv.x >= zu.x && uv.x <= zu.y && uv.y >= zv.x && uv.y <= zv.y) return true;
    }
    return false;
}

float smFalloff(int k, float dist_mm)
{
    float max_mm = u_sm_falloff[k].x;
    float coverage = max(u_sm_falloff[k].y, 0.0);
    float softness = u_sm_falloff[k].z;
    float curve = clamp(u_sm_falloff2[k].x, 0.25, 4.0);
    bool inverted = u_sm_falloff2[k].y > 0.5;
    if(coverage <= 0.0001 || max_mm <= 0.0) return 0.0;
    if(inverted)
    {
        float range = max(max_mm * coverage, 10.0);
        if(dist_mm >= range) return 0.0;
        float nd = pow(clamp(dist_mm / range, 0.0, 1.0), curve);
        float plateau = clamp(softness / 100.0, 0.0, 0.95);
        if(nd <= plateau) return 1.0;
        float t = clamp((nd - plateau) / max(1.0 - plateau, 1e-4), 0.0, 1.0);
        t = t * t * (3.0 - 2.0 * t);
        return 1.0 - t;
    }
    if(coverage >= 0.999) return 1.0;
    float nd = pow(clamp(dist_mm / max(max_mm, 1.0), 0.0, 1.0), curve);
    float boundary = max(0.0, 1.0 - coverage);
    if(boundary <= 0.0005) return 1.0;
    float feather = clamp(softness / 100.0, 0.0, 0.95) * 0.5;
    float fade_start = max(0.0, boundary - feather);
    float t = clamp((nd - fade_start) / max(boundary - fade_start, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

vec3 smGrade(int k, vec3 c) /* 0..255, mirror of GradeRgb */
{
    float brightness = u_sm_grade0[k].x;
    float threshold = u_sm_grade0[k].y;
    float wr_max = max(u_sm_grade1[k].y, 1.0);
    if(threshold > 0.0)
    {
        float thr = min(255.0, threshold);
        float lum0 = dot(c, vec3(0.299, 0.587, 0.114));
        float level = max(lum0, max(c.r, max(c.g, c.b)));
        if(level <= thr)
        {
            float t = (thr <= 0.0) ? 1.0 : max(0.0, level / thr);
            c *= t * t;
        }
    }
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float sat = (mx > 0.001) ? ((mx - mn) / mx) : 0.0;
    float wr_full = clamp(u_sm_grade0[k].z, 0.0, wr_max);
    float wr_sub = min(1.0, wr_full);
    if(lum > 242.0 && sat < 0.20)
    {
        float t = clamp((lum - 242.0) / 13.0, 0.0, 1.0) * (1.0 - wr_sub);
        c = mix(c, vec3(lum), t);
    }
    if(wr_sub > 0.0)
    {
        c -= vec3(wr_sub * min(c.r, min(c.g, c.b)));
    }
    float ultra = (wr_max > 1.0) ? clamp((wr_full - 1.0) / (wr_max - 1.0), 0.0, 1.0) : 0.0;
    if(ultra > 0.0)
    {
        float gray = (c.r + c.g + c.b) / 3.0;
        c = clamp(vec3(gray) + (c - vec3(gray)) * (1.0 + ultra * 0.95), 0.0, 255.0);
    }
    c *= brightness;
    float vib = clamp(u_sm_grade1[k].x, 0.0, 2.0);
    if(abs(vib - 1.0) > 0.001)
    {
        float gray = (c.r + c.g + c.b) / 3.0;
        c = clamp(vec3(gray) + (c - vec3(gray)) * vib, 0.0, 255.0);
    }
    return clamp(c * u_sm_gain[k], 0.0, 255.0);
}

vec3 smFetch(int i, int k, vec2 uv) /* nearest, like SampleNearestRgba; returns 0..255 */
{
    int w = max(int(u_sm_frame[k].x + 0.5), 1);
    int h = max(int(u_sm_frame[k].y + 0.5), 1);
    int row0 = int(u_sm_frame[k].z + 0.5);
    float u = clamp(uv.x, 0.0, 1.0);
    float v = clamp(1.0 - uv.y, 0.0, 1.0);
    ivec2 px = ivec2(clamp(int(u * float(w - 1) + 0.5), 0, w - 1),
                     row0 + clamp(int(v * float(h - 1) + 0.5), 0, h - 1));
    vec3 t;
    if(i == 0) t = texelFetch(u_atlas0, px, 0).rgb;
    else if(i == 1) t = texelFetch(u_atlas1, px, 0).rgb;
    else if(i == 2) t = texelFetch(u_atlas2, px, 0).rgb;
    else t = texelFetch(u_atlas3, px, 0).rgb;
    return t * 255.0;
}

vec3 screenMirrorColor(int i, vec3 world)
{
    vec3 acc = vec3(0.0);
    float acc_w = 0.0;
    int n = clamp(u_sm_plane_count, 0, 4);
    for(int k = 0; k < 4; ++k)
    {
        if(k >= n) break;
        vec2 uv = smMapToScreen(k, world);
        if(!smInZones(k, uv)) continue;
        uv.x = clamp(uv.x, u_sm_crop_u[k].x, u_sm_crop_u[k].y);
        uv.y = clamp(uv.y, u_sm_crop_v[k].x, u_sm_crop_v[k].y);
        float dist_mm = length(world - u_sm_pos[k]) * u_sm_grid_scale_mm;
        float wgt = smFalloff(k, dist_mm);
        if(wgt <= 0.01) continue;
        acc += wgt * smGrade(k, smFetch(i, k, uv));
        acc_w += wgt;
    }
    if(acc_w <= 0.01) return vec3(0.0);
    return clamp(acc / acc_w, 0.0, 255.0) / 255.0;
}

/* ---- Reactive (mirror of engines/reactive/Reactive.cpp; per-wave scalars come from
   ComputeWaveFrame on the C++ side) ---- */

float rwPrism(float dx, float dz, int sides)
{
    float r = length(vec2(dx, dz));
    if(r < 1e-8 || sides < 3) return r;
    float step = 6.28318530718 / float(sides);
    float p = mod(atan(dz, dx) + 3.14159265359, step);
    if(p < 0.0) p += step;
    p -= step * 0.5;
    return r * cos(p);
}

float rwShell(vec3 d, int shape)
{
    if(shape == 1) return max(abs(d.x), max(abs(d.y), abs(d.z)));
    if(shape == 2) return (abs(d.x) + abs(d.y) + abs(d.z)) * 0.57735027;
    if(shape == 3) return length(d.xz);
    if(shape >= 4 && shape <= 7)
    {
        int sides = (shape == 4) ? 3 : (shape == 5) ? 4 : (shape == 6) ? 5 : 6;
        float radial = rwPrism(d.x, d.z, sides);
        return sqrt(radial * radial + d.y * d.y);
    }
    return length(d);
}

float rwPlanar(float u, float v, int shape)
{
    if(shape == 1 || shape == 5) return max(abs(u), abs(v));
    if(shape == 2) return (abs(u) + abs(v)) * 0.70710678;
    if(shape == 4) return rwPrism(u, v, 3);
    if(shape == 6) return rwPrism(u, v, 5);
    if(shape == 7) return rwPrism(u, v, 6);
    return length(vec2(u, v));
}

float rwAxis(vec3 d, int axis)
{
    return (axis == 0) ? d.x : (axis == 1) ? d.y : d.z;
}

vec3 rwPlaneAxes(int orient, vec3 d) /* (u, v, normal) */
{
    if(orient == 0) return vec3(d.x, d.y, d.z);
    if(orient == 1) return vec3(d.x, d.z, d.y);
    return vec3(d.y, d.z, d.x);
}

ivec2 rwCrossAxes(int orient)
{
    if(orient == 0) return ivec2(0, 2);
    if(orient == 1) return ivec2(0, 1);
    return ivec2(1, 2);
}

float rwLook(float along, float radius, float sigma, int look, float core, float age, float expand)
{
    if(look == 1)
    {
        float et = (along - radius) / sigma;
        float front = exp(-2.0 * et * et);
        if(along > radius) return max(front, core);
        float inner = 0.42 + 0.58 * clamp(along / max(radius, 1e-4), 0.0, 1.0);
        float clear_ = exp(-((radius - along) / max(expand, 1e-6)) / 0.22);
        return max(front, max(inner * clear_, core));
    }
    if(look == 2)
    {
        float bs = max(sigma * 1.65, sigma + 1e-5);
        float et = (along - radius) / bs;
        float shock = exp(-2.2 * et * et);
        float u = clamp(along / max(radius, 1e-4), 0.0, 1.0);
        float fire = (1.0 - u) * exp(-age / 0.08);
        if(along <= radius) return max(shock, max(fire * fire * 1.25, core));
        return shock * 0.55;
    }
    if(look == 3)
    {
        float flash = exp(-age / 0.06);
        float q = along / max(sigma * 1.8, 1e-5);
        return max(flash * exp(-0.5 * q * q), core);
    }
    if(look == 4)
    {
        float sep = sigma * 2.8;
        float e0 = (along - radius) / sigma;
        float e1 = (along - max(0.0, radius - sep)) / sigma;
        return max(exp(-2.0 * e0 * e0), exp(-2.0 * e1 * e1));
    }
    float et = (along - radius) / sigma;
    return exp(-2.0 * et * et);
}

float rwAlong(vec3 d, int spread, int orient, int shape) /* SpreadAlongDistance */
{
    if(spread == 1) { vec3 p = rwPlaneAxes(orient, d); return rwPlanar(p.x, p.y, shape); }
    if(spread == 2) return abs(rwAxis(d, orient));
    if(spread == 3) { ivec2 a = rwCrossAxes(orient); return max(abs(rwAxis(d, a.x)), abs(rwAxis(d, a.y))); }
    if(spread == 4) return max(abs(d.x), max(abs(d.y), abs(d.z)));
    return rwShell(d, shape);
}

vec4 rwFetch(int i, int row, int col)
{
    ivec2 px = ivec2(col, row);
    if(i == 0) return texelFetch(u_atlas0, px, 0);
    if(i == 1) return texelFetch(u_atlas1, px, 0);
    if(i == 2) return texelFetch(u_atlas2, px, 0);
    return texelFetch(u_atlas3, px, 0);
}

vec3 reactiveColor(int i, vec3 world)
{
    float e_or = 0.0;
    float wsum = 0.0;
    float along_acc = 0.0;
    float travel_acc = 0.0;
    float slot_acc = 0.0;
    int n = clamp(u_rw_count, 0, 32);
    for(int r = 0; r < 32; ++r)
    {
        if(r >= n) break;
        vec4 a = rwFetch(i, r, 0); /* origin.xyz, packed modes */
        vec4 b = rwFetch(i, r, 1); /* radius, sigma, expand, gain */
        vec4 c = rwFetch(i, r, 2); /* core_amp, core_sigma, age, glow_amp */
        vec4 d = rwFetch(i, r, 3); /* glow_sigma, travel, slot_phase, valid */
        int modes = int(a.w + 0.5);
        int spread = modes / 1000;
        int orient = (modes / 100) % 10;
        int look = (modes / 10) % 10;
        int shape = modes % 10;
        vec3 dv = world - a.xyz;
        float dist3 = length(dv);
        float energy = 0.0;
        /* Lattice-safe widths: thin shells / core / glow would only hit axis points. */
        b.y = max(b.y, u_rw_min_sigma);
        c.y = max(c.y, u_rw_min_sigma);
        d.x = max(d.x, u_rw_min_sigma);
        if(b.w > 0.0)
        {
            float q = dist3 / max(c.y, 1e-5);
            float core = c.x * exp(-0.5 * q * q);
            if(spread == 1)
            {
                vec3 p = rwPlaneAxes(orient, dv);
                float slab = exp(-0.5 * ((p.z * p.z) / (b.y * b.y * 6.0)));
                energy = rwLook(rwPlanar(p.x, p.y, shape), b.x, b.y, look, core, c.z, b.z) * slab;
            }
            else if(spread == 2)
            {
                energy = rwLook(abs(rwAxis(dv, orient)), b.x, b.y, look, core, c.z, b.z);
            }
            else if(spread == 3)
            {
                ivec2 ax = rwCrossAxes(orient);
                energy = max(rwLook(abs(rwAxis(dv, ax.x)), b.x, b.y, look, core, c.z, b.z),
                             rwLook(abs(rwAxis(dv, ax.y)), b.x, b.y, look, core, c.z, b.z));
            }
            else if(spread == 4)
            {
                energy = max(rwLook(abs(dv.x), b.x, b.y, look, core, c.z, b.z),
                             max(rwLook(abs(dv.y), b.x, b.y, look, core, c.z, b.z),
                                 rwLook(abs(dv.z), b.x, b.y, look, core, c.z, b.z)));
            }
            else
            {
                energy = rwLook(rwShell(dv, shape), b.x, b.y, look, core, c.z, b.z);
            }
            energy *= b.w;
        }
        if(c.w > 0.0)
        {
            float g = dist3 / max(d.x, 1e-4);
            float glow = c.w * exp(-0.5 * g * g);
            energy = energy + glow - energy * glow;
        }
        if(energy <= 1e-4) continue;
        e_or = e_or + energy - e_or * energy;
        wsum += energy;
        along_acc += energy * rwAlong(dv, spread, orient, shape);
        travel_acc += energy * d.y;
        slot_acc += energy * d.z;
    }
    if(e_or <= 0.02 || wsum <= 1e-5) return vec3(0.0);

    float intensity = min(1.0, e_or * clamp(u_layer_brightness[i], 0.0, 1.0));
    float radial01 = clamp((along_acc / wsum) / max(travel_acc / wsum, 1e-4), 0.0, 1.0);
    float slot_phase = slot_acc / wsum;
    float scroll = u_hue_scroll[i];
    vec3 col;
    if(u_rainbow[i] != 0)
    {
        float hue = mod(radial01 * 360.0 * u_rw_cycles + slot_phase * 8.0 * 47.0 + scroll + 720.0, 360.0);
        col = rainbowHue(hue);
    }
    else
    {
        float driver = fract(radial01 * u_rw_cycles + slot_phase + scroll / 360.0 + 2.0);
        int pn = u_palette_count[i];
        if(pn <= 0)
        {
            col = rainbowHue(driver * 360.0);
        }
        else if(pn == 1)
        {
            col = u_palette[i * 4];
        }
        else
        {
            /* BlendPaletteAlong01: cyclic blend through the palette. */
            float scaled = driver * float(pn);
            int i0 = int(floor(scaled)) % pn;
            int i1 = (i0 + 1) % pn;
            col = mix(u_palette[i * 4 + i0], u_palette[i * 4 + i1], scaled - floor(scaled));
        }
    }
    return col * intensity;
}

/* ---- Minecraft (mirror of MinecraftGame::RenderColor + RoomSampleMapping) ---- */

float mcStripBr(int i, vec3 world, float fill_end, float total_slots)
{
    int flags = int(u_mc[0].x + 0.5);
    if(total_slots < 0.01 || (flags & 1) == 0) return -1.0;
    int axis = int(u_mc[0].y + 0.5);
    float lo = (axis == 1) ? u_room_min[i].x : (axis == 2) ? u_room_min[i].y : u_room_min[i].z;
    float hi = (axis == 1) ? u_room_max[i].x : (axis == 2) ? u_room_max[i].y : u_room_max[i].z;
    float p = (axis == 1) ? world.x : (axis == 2) ? world.y : world.z;
    float span = hi - lo;
    float u = (span < 1e-6) ? 0.5 : clamp((p - lo) / span, 0.0, 1.0);
    if((flags & 2) != 0) u = 1.0 - u;
    float center = (total_slots <= 1.0) ? 0.5 * total_slots : 0.5 + u * (total_slots - 1.0);
    float overlap = max(0.0, min(center + 0.5, fill_end) - max(center - 0.5, 0.0));
    return clamp(overlap, 0.0, 1.0);
}

vec4 mcCubeTexel(int i, int S, int face, int iu, int iv)
{
    ivec2 px = ivec2(iu, face * S + iv);
    if(i == 0) return texelFetch(u_atlas0, px, 0);
    if(i == 1) return texelFetch(u_atlas1, px, 0);
    if(i == 2) return texelFetch(u_atlas2, px, 0);
    return texelFetch(u_atlas3, px, 0);
}

bool mcRoomSample(int i, vec3 world, out vec3 rgb)
{
    rgb = vec3(0.0);
    if(u_mc[16].z < 0.5) return false;
    float sc = u_mc[16].x;
    int S = max(int(u_mc[16].y + 0.5), 1);
    vec3 d = vec3((world.x - u_mc[15].x) * sc, (world.y - u_mc[15].y) * sc, (u_mc[15].z - world.z) * sc);
    float len = length(d);
    if(len <= 1e-5) return false;
    d /= len;
    /* RoomSampleFrameProtocol::DirectionToCubemapUv */
    vec3 a = abs(d);
    int face;
    float u;
    float v;
    if(a.x >= a.y && a.x >= a.z)
    {
        if(d.x >= 0.0) { face = 0; u = 0.5 + (-d.z / a.x) * 0.5; v = 0.5 + (-d.y / a.x) * 0.5; }
        else           { face = 1; u = 0.5 + ( d.z / a.x) * 0.5; v = 0.5 + (-d.y / a.x) * 0.5; }
    }
    else if(a.y >= a.x && a.y >= a.z)
    {
        if(d.y >= 0.0) { face = 2; u = 0.5 + (d.x / a.y) * 0.5; v = 0.5 + ( d.z / a.y) * 0.5; }
        else           { face = 3; u = 0.5 + (d.x / a.y) * 0.5; v = 0.5 + (-d.z / a.y) * 0.5; }
    }
    else
    {
        if(d.z >= 0.0) { face = 4; u = 0.5 + ( d.x / a.z) * 0.5; v = 0.5 + (-d.y / a.z) * 0.5; }
        else           { face = 5; u = 0.5 + (-d.x / a.z) * 0.5; v = 0.5 + (-d.y / a.z) * 0.5; }
    }
    /* NearestFaceSample: nearest texel, then nearest filled neighbour (rings 1..3). */
    int iu0 = clamp(int(floor(clamp(u, 0.0, 1.0) * float(S) - 0.5 + 0.5)), 0, S - 1);
    int iv0 = clamp(int(floor(clamp(v, 0.0, 1.0) * float(S) - 0.5 + 0.5)), 0, S - 1);
    vec4 t = mcCubeTexel(i, S, face, iu0, iv0);
    if(t.a > 0.02) { rgb = t.rgb; return true; }
    for(int r = 1; r <= 3; ++r)
    {
        for(int dv = -r; dv <= r; ++dv)
        {
            for(int du = -r; du <= r; ++du)
            {
                if(max(abs(du), abs(dv)) != r) continue;
                int iu = iu0 + du;
                int iv = iv0 + dv;
                if(iu < 0 || iv < 0 || iu >= S || iv >= S) continue;
                t = mcCubeTexel(i, S, face, iu, iv);
                if(t.a > 0.02) { rgb = t.rgb; return true; }
            }
        }
    }
    return false;
}

vec3 minecraftColor(int i, vec3 world)
{
    int flags = int(u_mc[0].x + 0.5);
    int health_mode = int(u_mc[0].z + 0.5);
    vec3 col = vec3(0.0);
    if(health_mode == 1)
    {
        col = u_mc[1];
    }
    else if(health_mode == 2)
    {
        float br = mcStripBr(i, world, u_mc[3].x, u_mc[3].y);
        if(br < 0.0) col = u_mc[1];
        else col = (br > 1e-3) ? u_mc[2] * br : vec3(0.0);
    }
    for(int k = 0; k < 3; ++k)
    {
        vec3 p = u_mc[7 + k];
        if(p.z < 0.5) continue;
        float m = p.x;
        if(p.y > 0.5)
        {
            float br = mcStripBr(i, world, u_mc[10 + k].x, u_mc[10 + k].y);
            m = (br < 0.0) ? m : clamp(br * m, 0.0, 1.0);
        }
        col = mix(col, u_mc[4 + k], m);
    }
    vec3 room;
    if(mcRoomSample(i, world, room))
    {
        col = room;
    }
    float flash = u_mc[13].x;
    if(flash > 0.0)
    {
        float factor = 1.0;
        if((flags & 4) != 0)
        {
            vec3 o = world - u_mc[17];
            float ol = length(o);
            if(ol > 1e-6)
            {
                float align = clamp(dot(o / ol, u_mc[14]), -1.0, 1.0);
                float shaped = pow(clamp(0.5 * (align + 1.0), 0.0, 1.0), u_mc[13].z);
                factor = (1.0 - u_mc[13].y) + u_mc[13].y * (0.10 + 0.90 * shaped);
            }
        }
        col = mix(col, vec3(1.0, 0.0, 0.0), clamp(flash * factor, 0.0, 1.0));
    }
    return col;
}

vec3 layerColor(int i, vec3 world)
{
    if(u_kind[i] < 0)
    {
        return vec3(0.0);
    }
    vec3 p01;
    if(!worldToP01(i, world, p01))
    {
        return vec3(0.0);
    }
    if(u_kind[i] == 2)
    {
        return screenMirrorColor(i, world) * u_layer_brightness[i];
    }
    if(u_kind[i] == 4)
    {
        return minecraftColor(i, world) * u_layer_brightness[i];
    }
    if(u_kind[i] == 3)
    {
        /* Brightness is folded into intensity inside, like the LED path. */
        return reactiveColor(i, world);
    }
    if(u_kind[i] == 1)
    {
        vec2 uv;
        if(!projectUv(u_projection[i], p01, uv))
        {
            return vec3(0.0);
        }
        return sampleFrame(i, uv) * u_layer_brightness[i];
    }
    vec3 samp;
    if(i == 0) samp = sampleAtlas(u_atlas0, u_atlas_n[0], p01);
    else if(i == 1) samp = sampleAtlas(u_atlas1, u_atlas_n[1], p01);
    else if(i == 2) samp = sampleAtlas(u_atlas2, u_atlas_n[2], p01);
    else samp = sampleAtlas(u_atlas3, u_atlas_n[3], p01);
    float shade = 1.0;
    if(u_depth_shade[i] != 0)
    {
        /* FolderVolume finish_depth: radial distance (volume-local) vs median half-extent × √3. */
        vec3 he = max(u_half_ext[i], vec3(1e-4));
        float lo = min(he.x, min(he.y, he.z));
        float hi = max(he.x, max(he.y, he.z));
        float med = he.x + he.y + he.z - lo - hi;
        float max_r = max(med, 0.2 * hi) * 1.7320508;
        vec3 local = (p01 - 0.5) * 2.0 * he;
        float nd = min(1.0, length(local) / max(max_r, 1e-3));
        shade = 0.45 + 0.55 * (1.0 - nd * 0.6);
    }
    return applyFinish(i, samp) * shade * u_layer_brightness[i];
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
