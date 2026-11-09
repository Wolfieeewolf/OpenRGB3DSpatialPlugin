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

const char* MeshUnlitVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_texcoord;
uniform mat4 u_mvp;
uniform mat4 u_mv;
uniform mat4 u_light_mvp;
out vec3 v_color;
out vec2 v_texcoord;
out vec3 v_viewpos;
out vec4 v_shadow_coord;
void main()
{
    v_color = a_color;
    v_texcoord = a_texcoord;
    vec4 vp = u_mv * vec4(a_position, 1.0);
    v_viewpos = vp.xyz;
    v_shadow_coord = u_light_mvp * vec4(a_position, 1.0);
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* MeshUnlitFragment()
{
    return R"GLSL(
#version 410 core
in vec3 v_color;
in vec2 v_texcoord;
in vec3 v_viewpos;
in vec4 v_shadow_coord;
uniform sampler2D u_base_tex;
uniform sampler2D u_emis_tex;
uniform sampler2D u_normal_tex;
uniform sampler2D u_mr_tex;
uniform sampler2D u_occlusion_tex;
uniform sampler2D u_shadow_tex;
uniform int u_has_base_tex;
uniform int u_has_emis_tex;
uniform int u_has_normal_tex;
uniform int u_has_mr_tex;
uniform int u_has_occlusion_tex;
uniform int u_has_shadow;
uniform vec4 u_base_factor;
uniform vec3 u_emis_factor;
uniform float u_emis_strength;
uniform float u_bright;
uniform float u_alpha;
uniform int u_alpha_mode;
uniform float u_alpha_cutoff;
uniform vec2 u_base_uv_scale;
uniform vec2 u_base_uv_offset;
uniform float u_base_uv_rotation;
uniform vec2 u_emis_uv_scale;
uniform vec2 u_emis_uv_offset;
uniform float u_emis_uv_rotation;
uniform float u_normal_scale;
uniform float u_transmission;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_specular_factor;
uniform float u_occlusion_strength;
uniform vec3 u_tint;
out vec4 frag_color;

/* Assimp flips V on import. Convert to glTF UV, apply KHR_texture_transform,
   return glTF UV for sampling a top-down (unflipped) texture upload. */
vec2 applyUvXform(vec2 uv, vec2 scale, float rot, vec2 offset)
{
    vec2 g = vec2(uv.x, 1.0 - uv.y);
    g *= scale;
    float c = cos(rot);
    float s = sin(rot);
    g = mat2(c, s, -s, c) * g;
    g += offset;
    return g;
}

float sampleShadowPCF(vec3 ndc)
{
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float z = ndc.z * 0.5 + 0.5;
    if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || z > 1.0)
    {
        return 1.0;
    }
    float bias = 0.0025;
    float vis = 0.0;
    vec2 texel = 1.0 / vec2(textureSize(u_shadow_tex, 0));
    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            float d = texture(u_shadow_tex, uv + vec2(float(x), float(y)) * texel).r;
            vis += (z - bias <= d + 1e-4) ? 1.0 : 0.0;
        }
    }
    return vis / 9.0;
}

void main()
{
    vec2 buv = applyUvXform(v_texcoord, u_base_uv_scale, u_base_uv_rotation, u_base_uv_offset);
    vec2 euv = applyUvXform(v_texcoord, u_emis_uv_scale, u_emis_uv_rotation, u_emis_uv_offset);
    vec4 base = u_base_factor;
    if(u_has_base_tex != 0)
    {
        base *= texture(u_base_tex, buv);
    }
    vec3 emis = u_emis_factor * max(u_emis_strength, 0.0);
    vec4 emis_tex_s = vec4(0.0);
    if(u_has_emis_tex != 0)
    {
        emis_tex_s = texture(u_emis_tex, euv);
        emis *= emis_tex_s.rgb;
    }
    /* MASK: keep neon fragments when emissive is lit even if base alpha is cut. */
    if(u_alpha_mode == 1)
    {
        float mask_a = base.a;
        float emis_keep = max(max(emis.r, emis.g), emis.b);
        if(u_has_emis_tex != 0)
        {
            emis_keep = max(emis_keep, emis_tex_s.a);
        }
        if(mask_a < u_alpha_cutoff && emis_keep < 0.04)
        {
            discard;
        }
    }

    float metal = clamp(u_metallic, 0.0, 1.0);
    float rough = clamp(u_roughness, 0.04, 1.0);
    if(u_has_mr_tex != 0)
    {
        /* glTF metallicRoughness: G=roughness, B=metallic */
        vec4 mr = texture(u_mr_tex, buv);
        rough = clamp(rough * mr.g, 0.04, 1.0);
        metal = clamp(metal * mr.b, 0.0, 1.0);
    }

    float ao = 1.0;
    if(u_has_occlusion_tex != 0)
    {
        float occ = texture(u_occlusion_tex, buv).r;
        ao = mix(1.0, occ, clamp(u_occlusion_strength, 0.0, 1.0));
    }

    /* Sketchfab-ish on GL 4.1: albedo + cheap IBL/spec, HDR emissive (no tonemap —
       bloom composite tonemaps after halo extract). */
    vec3 tint = max(u_tint, vec3(0.05));
    vec3 albedo = base.rgb * v_color;
    float spec_w = clamp(u_specular_factor, 0.0, 1.0);

    vec3 dx = dFdx(v_viewpos);
    vec3 dy = dFdy(v_viewpos);
    vec3 n = normalize(cross(dx, dy));
    if(u_has_normal_tex != 0)
    {
        vec2 du1 = dFdx(buv);
        vec2 du2 = dFdy(buv);
        vec3 t = normalize(dx * du2.y - dy * du1.y);
        vec3 b = normalize(cross(n, t));
        t = normalize(cross(b, n));
        vec3 mapn = texture(u_normal_tex, buv).xyz * 2.0 - 1.0;
        mapn.xy *= max(u_normal_scale, 0.0);
        n = normalize(mat3(t, b, n) * mapn);
    }
    vec3 vdir = normalize(-v_viewpos);
    vec3 L = normalize(vec3(0.35, 0.85, 0.40));
    vec3 H = normalize(L + vdir);
    float ndotv = clamp(abs(dot(n, vdir)), 0.0, 1.0);
    float ndotl = clamp(dot(n, L), 0.0, 1.0);
    float ndoth = clamp(dot(n, H), 0.0, 1.0);
    float fres = pow(1.0 - ndotv, 2.4);

    float shadow = 1.0;
    if(u_has_shadow != 0)
    {
        vec3 sc = v_shadow_coord.xyz / max(v_shadow_coord.w, 1e-5);
        shadow = mix(0.55, 1.0, sampleShadowPCF(sc));
    }

    /* Cheap fake IBL: sky/ground hemisphere by normal.y (view space). */
    float hemi = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient = mix(vec3(0.10, 0.11, 0.14), vec3(0.42, 0.48, 0.58), hemi);
    ambient = mix(ambient, ambient * albedo, metal * 0.65);

    float shade = mix(0.45, 1.0, ndotl * shadow);
    vec3 diffuse = albedo * (1.0 - metal) * shade * ao;
    float spec_pow = mix(12.0, 128.0, 1.0 - rough);
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 specular = F0 * pow(ndoth, spec_pow) * (0.25 + 0.75 * (1.0 - rough))
                    * shadow * spec_w * (0.35 + 0.65 * fres);
    vec3 lit = (diffuse + ambient * albedo * (0.55 + 0.45 * (1.0 - metal)) + specular)
               * ao * u_bright;

    /* HDR emissive — strength can push well past 1 for bloom. */
    float emis_str = max(u_emis_strength, 0.0);
    vec3 emis_rgb = emis * max(u_bright, 0.35) * (1.0 + emis_str * 0.85);

    float albedo_lum = max(max(albedo.r, albedo.g), albedo.b);
    float emis_lum = max(max(emis_rgb.r, emis_rgb.g), emis_rgb.b);
    float emis_dom = clamp(emis_lum / max(albedo_lum + emis_lum, 1e-3), 0.0, 1.0);
    if(u_has_emis_tex != 0 || emis_str > 0.5)
    {
        lit = mix(lit, emis_rgb, emis_dom * 0.88);
    }

    vec3 rgb = (lit + emis_rgb * (0.65 + 0.55 * emis_dom)) * tint;
    float a = base.a * u_alpha;
    if(u_alpha_mode == 0)
    {
        a = u_alpha;
    }

    if(u_transmission > 0.05 || u_alpha_mode == 2)
    {
        float glass_a = max(base.a, 0.18);
        glass_a = mix(glass_a, min(1.0, glass_a + 0.45), fres * 0.35);
        float emis_a = clamp(emis_lum / (emis_lum + 0.25), 0.0, 1.0);
        a = clamp(max(glass_a, emis_a * 0.95) * max(u_alpha, 0.5), 0.0, 1.0);
        rgb = max(rgb, emis_rgb * tint);
        rgb += albedo * (0.10 + 0.22 * fres) * tint * ao;
        if(u_transmission > 0.05)
        {
            rgb += vec3(0.22, 0.65, 0.90) * fres * u_transmission * 0.4 * tint;
        }
    }

    /* No tonemap here — leave energy for HDR bloom FBO + composite. */
    frag_color = vec4(max(rgb, emis_rgb * 0.4 * tint), a);
}
)GLSL";
}

bool CompileMeshUnlit(GlProgram& out, QString* error_log)
{
    return out.Compile(MeshUnlitVertex(), MeshUnlitFragment(), error_log);
}

const char* MeshDepthVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
uniform mat4 u_mvp;
void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* MeshDepthFragment()
{
    return R"GLSL(
#version 410 core
out vec4 frag_color;
void main()
{
    float z = gl_FragCoord.z;
    frag_color = vec4(z, z, z, 1.0);
}
)GLSL";
}

bool CompileMeshDepth(GlProgram& out, QString* error_log)
{
    return out.Compile(MeshDepthVertex(), MeshDepthFragment(), error_log);
}

const char* FullscreenVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
out vec2 v_uv;
void main()
{
    v_uv = a_position.xy * 0.5 + 0.5;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
)GLSL";
}

const char* BloomBrightFragment()
{
    return R"GLSL(
#version 410 core
in vec2 v_uv;
uniform sampler2D u_scene;
uniform float u_threshold;
out vec4 frag_color;
void main()
{
    vec3 c = texture(u_scene, v_uv).rgb;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float knee = max(lum - u_threshold, 0.0);
    /* Soft knee keeps neon hue instead of clipping to white. */
    frag_color = vec4(c * (knee / (knee + 1.0)), 1.0);
}
)GLSL";
}

const char* BloomBlurFragment()
{
    return R"GLSL(
#version 410 core
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2 u_direction;
out vec4 frag_color;
void main()
{
    vec2 texel = u_direction;
    vec3 c = texture(u_tex, v_uv).rgb * 0.227027;
    c += texture(u_tex, v_uv + texel * 1.384615).rgb * 0.316216;
    c += texture(u_tex, v_uv - texel * 1.384615).rgb * 0.316216;
    c += texture(u_tex, v_uv + texel * 3.230769).rgb * 0.070270;
    c += texture(u_tex, v_uv - texel * 3.230769).rgb * 0.070270;
    frag_color = vec4(c, 1.0);
}
)GLSL";
}

const char* BloomCompositeFragment()
{
    return R"GLSL(
#version 410 core
in vec2 v_uv;
uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_intensity;
out vec4 frag_color;
void main()
{
    vec3 scene = texture(u_scene, v_uv).rgb;
    vec3 bloom = texture(u_bloom, v_uv).rgb * u_intensity;
    vec3 hdr = scene + bloom;
    /* Peak tonemap after bloom — keeps neon hue (luma scale crushed to white). */
    float peak = max(max(hdr.r, hdr.g), hdr.b);
    float mapped = peak / (1.0 + peak * 0.35);
    vec3 ldr = (peak > 1e-5) ? hdr * (mapped / peak) : hdr;
    frag_color = vec4(clamp(ldr, 0.0, 1.0), 1.0);
}
)GLSL";
}

bool CompileBloomBright(GlProgram& out, QString* error_log)
{
    return out.Compile(FullscreenVertex(), BloomBrightFragment(), error_log);
}

bool CompileBloomBlur(GlProgram& out, QString* error_log)
{
    return out.Compile(FullscreenVertex(), BloomBlurFragment(), error_log);
}

bool CompileBloomComposite(GlProgram& out, QString* error_log)
{
    return out.Compile(FullscreenVertex(), BloomCompositeFragment(), error_log);
}

} // namespace ViewportShaders
