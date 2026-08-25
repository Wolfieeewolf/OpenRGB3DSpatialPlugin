// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QString>
#include <cstddef>

/** Compiled-in Shader Field presets so the dropdown never depends on qrc. */
namespace ShaderFieldPresets
{

struct Bundled
{
    const char* id;
    const char* title;
    const char* source;
};

inline const Bundled kBundled[] = {
    {"slow_waves", "Slow Waves — soft blue bands", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * zoom;
    float dens = 4.0 + 14.0 * detail;
    float wave = sin(p.x * dens + u_time) * 0.5 + sin(p.y * dens * 0.75 - u_time * 0.85) * 0.5;
    wave = pow(clamp(wave * 0.5 + 0.5, 0.0, 1.0), contrast);
    float ang = hue * 6.2831853;
    vec3 deep = vec3(0.04, 0.06, 0.18);
    vec3 crest = vec3(0.20 + 0.35 * cos(ang), 0.45 + 0.25 * sin(ang), 0.90);
    out_color = vec4(mix(deep, crest, smoothstep(0.28, 0.72, wave)), 1.0);
}
)"},
    {"room_plasma", "Room Plasma — colorful swirl", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.2 * zoom);
    float t = u_time * 0.9;
    float dens = 1.0 + 2.4 * detail;
    float v = sin(p.x * dens + t) + sin(p.y * dens * 1.15 - t * 0.95)
            + sin((p.x + p.y) * dens * 0.65 + t * 1.25) + cos(length(p) * dens * 1.4 - t);
    v = pow(clamp(v * 0.25 + 0.5, 0.0, 1.0), contrast);
    float h = fract(v * 0.85 + hue + t * 0.04);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.85) * (0.35 + 0.65 * v), 1.0);
}
)"},
    {"spectrum_glow", "Checker Drift — moving lattice", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    float cells = 3.0 + 10.0 * detail;
    vec2 p = (uv - 0.5) * zoom * cells + vec2(u_time * 0.35, -u_time * 0.22);
    vec2 g = abs(fract(p) - 0.5);
    float checker = step(0.5, mod(floor(p.x) + floor(p.y), 2.0));
    float grid = 1.0 - smoothstep(0.42, 0.48, max(g.x, g.y));
    float v = pow(clamp(mix(checker * 0.55, 1.0, grid), 0.0, 1.0), contrast);
    float h = fract(hue + 0.08 * floor(p.x) + 0.05 * floor(p.y));
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
)"},
    {"ember_field", "Ripple Ember — fire rings", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * (2.0 * zoom);
    float r = length(p);
    float a = atan(p.y, p.x);
    float rings = 2.0 + 5.0 * detail;
    float ring = pow(1.0 - abs(sin(r * rings * 3.14159 - u_time * 1.6)), mix(1.2, 4.0, contrast * 0.35));
    float swirl = 0.5 + 0.5 * sin(a * (3.0 + 4.0 * detail) + u_time * 0.9 + r * 4.0);
    float ember = pow(clamp(ring * (0.35 + 0.65 * swirl) * (1.15 - r), 0.0, 1.0), contrast);
    float h = fract(hue + ember * 0.12 + u_time * 0.02);
    vec3 hot = vec3(1.0, 0.28 + 0.25 * cos(h * 6.2831853), 0.04);
    out_color = vec4(mix(vec3(0.02, 0.0, 0.03), hot, ember), 1.0);
}
)"},
    {"soft_aurora", "Soft Aurora — curtain bands", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.6 * zoom);
    float t = u_time * 0.35;
    float v = 0.0;
    float band0 = 0.55 + 0.45 * sin(p.x * (1.2) + t * 0.6 + sin(p.y * 2.2 + t * 0.4));
    float curtain0 = exp(-abs(p.y + 0.15 * sin(p.x * 1.5 + t) + 0.36) * (3.0 + detail * 2.0));
    v += band0 * curtain0 * 0.45;
    float band1 = 0.55 + 0.45 * sin(p.x * (1.2 + 0.35 * detail) + t * 0.75 + sin(p.y * 2.2 + t * 0.4 + 1.0));
    float curtain1 = exp(-abs(p.y + 0.15 * sin(p.x * 1.5 + t + 1.0) + 0.18) * (3.0 + detail * 2.0));
    v += band1 * curtain1 * 0.33;
    float band2 = 0.55 + 0.45 * sin(p.x * (1.2 + 0.70 * detail) + t * 0.90 + sin(p.y * 2.2 + t * 0.4 + 2.0));
    float curtain2 = exp(-abs(p.y + 0.15 * sin(p.x * 1.5 + t + 2.0)) * (3.0 + detail * 2.0));
    v += band2 * curtain2 * 0.26;
    float band3 = 0.55 + 0.45 * sin(p.x * (1.2 + 1.05 * detail) + t * 1.05 + sin(p.y * 2.2 + t * 0.4 + 3.0));
    float curtain3 = exp(-abs(p.y + 0.15 * sin(p.x * 1.5 + t + 3.0) - 0.18) * (3.0 + detail * 2.0));
    v += band3 * curtain3 * 0.21;
    float band4 = 0.55 + 0.45 * sin(p.x * (1.2 + 1.40 * detail) + t * 1.20 + sin(p.y * 2.2 + t * 0.4 + 4.0));
    float curtain4 = exp(-abs(p.y + 0.15 * sin(p.x * 1.5 + t + 4.0) - 0.36) * (3.0 + detail * 2.0));
    v += band4 * curtain4 * 0.18;
    v = pow(clamp(v, 0.0, 1.0), mix(1.4, 0.7, contrast / 2.5));
    float h = fract(0.55 + hue + v * 0.12 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.55 + 0.35 * v) * (0.20 + 0.80 * v), 1.0);
}
)"},
    {"soft_ripples", "Soft Ripples — expanding rings", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float r = length(p);
    float t = u_time * 0.85;
    float freq = 2.2 + 3.5 * detail;
    float wave = sin(r * freq - t * 2.2) * 0.55 + sin(r * freq * 0.55 - t * 1.1 + 1.2) * 0.35;
    float v = clamp(pow(0.5 + 0.5 * wave, mix(2.8, 1.4, contrast / 2.5)) * exp(-r * 0.55), 0.0, 1.0);
    float h = fract(hue + r * 0.08 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.75) * (0.15 + 0.85 * v), 1.0);
}
)"},
    {"hex_drift", "Hex Drift — lattice wash", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (3.2 * zoom * (0.7 + detail));
    float t = u_time * 0.4;
    p += vec2(sin(t * 0.7), cos(t * 0.55)) * 0.15;
    vec2 r = vec2(1.0, 0.8660254);
    vec2 h1 = r * 0.5;
    vec2 a = mod(p, r) - h1;
    vec2 b = mod(p - h1, r) - h1;
    vec2 gv = (dot(a, a) < dot(b, b)) ? a : b;
    float d = max(abs(gv.x) * 0.866 + abs(gv.y) * 0.5, abs(gv.y));
    float edge = 1.0 - smoothstep(0.28, 0.42, d);
    float pulse = 0.5 + 0.5 * sin(t * 2.0 + (p.x + p.y) * 1.5);
    float v = pow(clamp(edge * (0.55 + 0.45 * pulse), 0.0, 1.0), mix(1.6, 0.85, contrast / 2.5));
    float hh = fract(hue + d * 0.35 + t * 0.05);
    vec3 rgb = clamp(abs(mod(hh * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.7) * (0.12 + 0.88 * v), 1.0);
}
)"},
};

inline constexpr int kBundledCount = int(sizeof(kBundled) / sizeof(kBundled[0]));

inline const Bundled* Find(const QString& id)
{
    for(int i = 0; i < kBundledCount; ++i)
    {
        if(id == QLatin1String(kBundled[i].id))
            return &kBundled[i];
    }
    return nullptr;
}

} // namespace ShaderFieldPresets
