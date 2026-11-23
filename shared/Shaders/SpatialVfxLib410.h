// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

/**
 * Shared VFX GLSL vocabulary injected ahead of author bodies (volume / particle /
 * ribbon / strip / shader-field). OpenGL 4.1 Core, versionless chunk — engines wrap
 * with #version 410 core.
 *
 * Opt-in: call vfx* helpers from volumeMain / spatialMain / stripMain. Prefix avoids
 * clashing with look-local helpers. Looks that ignore vfx* stay unchanged.
 *
 * Quality bar: vfxDetailWidth maps Detail 0..1 calm-soft → hologram-sharp
 * (mid ≈ older "Detail 100" softness). Prefer vfxCoreRim / vfxShell over one fat blob.
 */
namespace SpatialVfxLib410
{
inline QString GlslSource()
{
    /* Two chunks — stay under MSVC C2026 (~16 KiB) per literal. */
    return QStringLiteral(R"GLSL(
/* --- SpatialVfxLib410 (shared) --- */
float vfxSaturate(float x) { return clamp(x, 0.0, 1.0); }
float vfxHash11(float n) { return fract(sin(n) * 43758.5453123); }
float vfxHash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }
float vfxHash31(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123); }
vec2 vfxHash22(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}
float vfxNoise3(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n = i.x + i.y * 57.0 + i.z * 113.0;
    float a = mix(vfxHash11(n), vfxHash11(n + 1.0), f.x);
    float b = mix(vfxHash11(n + 57.0), vfxHash11(n + 58.0), f.x);
    float c = mix(vfxHash11(n + 113.0), vfxHash11(n + 114.0), f.x);
    float d = mix(vfxHash11(n + 170.0), vfxHash11(n + 171.0), f.x);
    return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}
float vfxFbm3(vec3 p, float octaves_f)
{
    float sum = 0.0;
    float amp = 0.5;
    float tot = 0.0;
    vec3 q = p;
    for(int o = 0; o < 5; ++o)
    {
        float on = 1.0 - step(octaves_f, float(o));
        sum += amp * vfxNoise3(q) * on;
        tot += amp * on;
        amp *= 0.5;
        q *= 2.03;
    }
    return sum / max(tot, 1e-4);
}
float vfxRidge3(vec3 p, float octaves_f)
{
    float sum = 0.0;
    float amp = 0.5;
    float tot = 0.0;
    vec3 q = p;
    for(int o = 0; o < 4; ++o)
    {
        float on = 1.0 - step(octaves_f, float(o));
        float n = vfxNoise3(q);
        sum += amp * (1.0 - abs(n * 2.0 - 1.0)) * on;
        tot += amp * on;
        amp *= 0.5;
        q *= 2.07;
    }
    return sum / max(tot, 1e-4);
}
vec3 vfxDomainWarp(vec3 p, float t, float amp)
{
    vec3 q = p;
    q.x += amp * (vfxNoise3(p + vec3(0.0, t, 1.7)) * 2.0 - 1.0);
    q.y += amp * (vfxNoise3(p + vec3(2.1, t * 1.1, 0.0)) * 2.0 - 1.0);
    q.z += amp * (vfxNoise3(p + vec3(t * 0.9, 4.3, 0.0)) * 2.0 - 1.0);
    return q;
}
float vfxDetailWidth(float detail01)
{
    float d = vfxSaturate(detail01);
    return mix(0.090, 0.010, pow(d, 0.50));
}
float vfxSoftBand(float dist, float soft)
{
    float s = max(soft, 0.008);
    return exp(-(dist * dist) / (s * s));
}
float vfxCoreRim(float dist, float soft)
{
    float s = max(soft, 0.008);
    float core = exp(-(dist * dist) / (s * s * 0.28));
    float rim = exp(-(dist * dist) / (s * s * 1.20)) * 0.42;
    return vfxSaturate(core + rim);
}
float vfxCoreRimDetail(float dist, float detail01)
{
    return vfxCoreRim(dist, vfxDetailWidth(detail01));
}
float vfxStreak(vec3 dlt, vec3 dir, float soft_along, float soft_side)
{
    float dn = length(dir);
    vec3 u = (dn > 1e-4) ? (dir / dn) : vec3(0.0, 0.0, 1.0);
    float along = dot(dlt, u);
    float side = length(dlt - u * along);
    float a = max(soft_along, 0.010);
    float b = max(soft_side, 0.008);
    float head = exp(-(along * along) / (a * a * 0.45));
    float trail = exp(-(along * along) / (a * a * 2.6)) * step(along, 0.0);
    return vfxSaturate(max(head, trail * 0.62) * exp(-(side * side) / (b * b)));
}
)GLSL")
           + QStringLiteral(R"GLSL(
float vfxSdSphere(vec3 p, float r) { return length(p) - r; }
float vfxSdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float vfxSdRoundBox(vec3 p, vec3 b, float r)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}
float vfxSdTorus(vec3 p, vec2 t)
{
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}
float vfxSdCapsule(vec3 p, vec3 a, vec3 b, float r)
{
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = vfxSaturate(dot(pa, ba) / max(dot(ba, ba), 1e-6));
    return length(pa - ba * h) - r;
}
float vfxSdPlane(vec3 p, vec3 n, float h)
{
    return dot(p, normalize(n)) + h;
}
float vfxSdOctahedron(vec3 p, float s)
{
    p = abs(p);
    return (p.x + p.y + p.z - s) * 0.57735027;
}
float vfxSdHexPrism(vec3 p, vec2 h)
{
    vec3 k = vec3(-0.8660254, 0.5, 0.57735);
    p = abs(p);
    p.xy -= 2.0 * min(dot(k.xy, p.xy), 0.0) * k.xy;
    vec2 d = vec2(length(p.xy - vec2(clamp(p.x, -k.z * h.x, k.z * h.x), h.x)) * sign(p.y - h.x),
                  p.z - h.y);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float vfxSmoothMin(float a, float b, float k)
{
    float h = vfxSaturate(0.5 + 0.5 * (b - a) / max(k, 1e-4));
    return mix(b, a, h) - k * h * (1.0 - h);
}
float vfxSmoothMax(float a, float b, float k)
{
    return -vfxSmoothMin(-a, -b, k);
}
float vfxShell(float sd, float soft) { return vfxCoreRim(abs(sd), soft); }
float vfxShellDetail(float sd, float detail01) { return vfxShell(sd, vfxDetailWidth(detail01)); }
vec3 vfxRotX(vec3 p, float a)
{
    float c = cos(a); float s = sin(a);
    return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}
vec3 vfxRotY(vec3 p, float a)
{
    float c = cos(a); float s = sin(a);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}
vec3 vfxRotZ(vec3 p, float a)
{
    float c = cos(a); float s = sin(a);
    return vec3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}
vec2 vfxCurl2(vec2 p, float t)
{
    float e = 0.12;
    float n0 = vfxNoise3(vec3(p, t));
    float nx = vfxNoise3(vec3(p + vec2(e, 0.0), t));
    float ny = vfxNoise3(vec3(p + vec2(0.0, e), t));
    return vec2(ny - n0, n0 - nx) / e;
}
float vfxPulse01(float t, float sharpness)
{
    float s = max(sharpness, 0.5);
    return pow(0.5 + 0.5 * sin(t), s);
}
float vfxEnvOnce(float prog01)
{
    float p = vfxSaturate(prog01);
    float attack = smoothstep(0.0, 0.12, p);
    float release = 1.0 - smoothstep(0.72, 1.0, p);
    return attack * release;
}
vec3 vfxHsv2rgb(vec3 c)
{
    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    return c.z * mix(vec3(1.0), rgb, c.y);
}
vec3 vfxPalette3(float t, vec3 a, vec3 b, vec3 c)
{
    float x = vfxSaturate(t);
    return (x < 0.5) ? mix(a, b, x * 2.0) : mix(b, c, (x - 0.5) * 2.0);
}
vec3 vfxHeatRgb(float heat01)
{
    float h = vfxSaturate(heat01);
    return vfxPalette3(h, vec3(0.15, 0.02, 0.01), vec3(1.0, 0.35, 0.05), vec3(1.0, 0.95, 0.75));
}
/* --- end SpatialVfxLib410 --- */
)GLSL");
}
} // namespace SpatialVfxLib410
