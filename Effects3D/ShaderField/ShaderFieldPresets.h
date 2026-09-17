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
    {"lobe_plasma", "Lobe Plasma — multi-center swirl", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.35 * zoom);
    float t = u_time;
    float n_its = 3.0 + floor(2.0 * detail + 0.5);
    float n_d_prod = 1.0;
    float i0 = 0.0;
    n_d_prod += sin(length(p - vec2(sin(0.0), cos(0.0)) * sin(t * 0.2 + 0.0)) * (8.0 + 4.0 * detail));
    i0 = 1.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    i0 = 2.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    i0 = 3.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    i0 = 4.0 / n_its;
    n_d_prod += sin(length(p - vec2(sin(6.2831 * i0), cos(6.2831 * i0)) * sin(t * 0.2 + i0)) * (8.0 + 4.0 * detail));
    float n1 = 0.5 + 0.5 * sin(n_d_prod * 4.0 + t * 2.2);
    float n2 = 0.5 + 0.5 * sin(n_d_prod * 2.0 + t * 2.2);
    float n3 = 0.5 + 0.5 * sin(n_d_prod * 1.0 + t * 2.2);
    float v = pow(clamp((n1 + n2 + n3) / 3.0, 0.0, 1.0), contrast);
    float h = fract(hue + n1 * 0.2 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    vec3 col = mix(vec3(n1, n2, n3), rgb, 0.55) * (0.25 + 0.75 * v);
    out_color = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)"},
    {"noise_contour", "Noise Contour — topo bands", R"(
float ncHash11(float t){return fract(sin(t*56789.0)*56789.0);}
float ncHash21(vec2 uv){return ncHash11(ncHash11(uv.x)+2.0*ncHash11(uv.y));}
vec2 ncGrad(vec2 uv){float t=ncHash21(uv);return vec2(cos(6.2831853*t),sin(6.2831853*t));}
float ncNoise(vec2 uv,float r){
    float ca=cos(r);float sa=sin(r);vec2 uvi=floor(uv);vec2 uvf=uv-uvi;
    vec2 g00=ncGrad(uvi);vec2 g10=ncGrad(uvi+vec2(1.0,0.0));
    vec2 g01=ncGrad(uvi+vec2(0.0,1.0));vec2 g11=ncGrad(uvi+vec2(1.0,1.0));
    g00=vec2(ca*g00.x-sa*g00.y,sa*g00.x+ca*g00.y);g10=vec2(ca*g10.x-sa*g10.y,sa*g10.x+ca*g10.y);
    g01=vec2(ca*g01.x-sa*g01.y,sa*g01.x+ca*g01.y);g11=vec2(ca*g11.x-sa*g11.y,sa*g11.x+ca*g11.y);
    float f00=dot(g00,uvf);float f10=dot(g10,uvf-vec2(1.0,0.0));
    float f01=dot(g01,uvf-vec2(0.0,1.0));float f11=dot(g11,uvf-vec2(1.0,1.0));
    float sx=uvf.x*uvf.x*(3.0-2.0*uvf.x);float sy=uvf.y*uvf.y*(3.0-2.0*uvf.y);
    return ((mix(mix(f00,f10,sx),mix(f01,f11,sx),sy)/0.7)+1.0)*0.5;
}
float ncFbm(vec2 uv,float r){return (ncNoise(uv,r)+ncNoise(uv*2.0,r)*0.5)/1.5;}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.4 * zoom * (0.7 + detail));
    float noise_fac = ncFbm(p * (2.0 + 2.5 * detail), u_time * 0.55);
    float contour = 0.5 * (1.0 - cos((18.0 + 28.0 * detail) * 3.14159265 * noise_fac));
    float clip = smoothstep(0.55, mix(0.85, 0.98, contrast / 2.5), contour);
    float h = fract(hue + noise_fac * 0.35 + u_time * 0.02);
    vec3 rgb = mix(vec3(0.85, 0.05, 0.75), vec3(0.05, 0.85, 0.90), noise_fac);
    vec3 tint = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(rgb, tint, 0.35) * clip * (0.35 + 0.65 * contour), 1.0);
}
)"},
    {"corner_waves", "Corner Waves — four-corner rings", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * zoom + 0.5;
    float vel = 1.6 + 2.2 * detail;
    float len = mix(18.0, 8.0, detail);
    float t = u_time * vel;
    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 q = (p - 0.5) * vec2(aspect, 1.0) + 0.5;
    float d0 = length((q - vec2(0.0, 0.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float d1 = length((q - vec2(1.0, 0.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float d2 = length((q - vec2(0.0, 1.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float d3 = length((q - vec2(1.0, 1.0)) * u_resolution.xy / max(u_resolution.y, 1.0));
    float w = sin(d0 / len - t) + sin(d1 / len - t) + sin(d2 / len - t) + sin(d3 / len - t);
    float v = pow(clamp(0.5 + 0.25 * w, 0.0, 1.0), contrast);
    float h = fract(hue + 0.08 * w + u_time * 0.02);
    vec3 crest = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    crest = crest * crest * (3.0 - 2.0 * crest);
    out_color = vec4(mix(vec3(0.02, 0.05, 0.14), crest, v), 1.0);
}
)"},
    {"aurora_ridge", "Aurora Ridge — horizon curtain", R"(
float arRnd(vec2 p,float n){return fract(abs(sin(p.x*123.4+p.y*432.1)*(p.x*3.7+p.x*p.y*4.5+256.7+n*654.3)+n*321.1));}
float arEase(float x){return x<0.5?2.0*x*x:1.0-pow(-2.0*x+2.0,2.0)/2.0;}
float arH(float x,float dx,float n,float div){return 0.5+(arRnd(vec2(floor(x*div)/div+dx/div,0.0),n)-0.5)/5.0;}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * zoom + 0.5;
    float div = 18.0 + 28.0 * detail;
    float t = u_time * 0.45;
    float n = floor(t);
    t = fract(t);
    float fx = fract(p.x * div);
    float v = mix(mix(arH(p.x,0.0,n,div),arH(p.x,1.0,n,div),arEase(fx)),
                  mix(arH(p.x,0.0,n+1.0,div),arH(p.x,1.0,n+1.0,div),arEase(fx)),t);
    float glow = pow(clamp(1.0 - abs(p.y - v) * mix(6.0, 14.0, contrast / 2.5), 0.0, 1.0), mix(1.6, 0.8, contrast / 2.5));
    vec3 base = mix(vec3(0.15, 0.75, 0.35 + 0.45 * p.y), vec3(0.75, 0.18, 0.85 * (1.0 - p.y)), clamp(abs(p.y - v) * 2.0, 0.0, 1.0));
    float h = fract(hue + glow * 0.08 + u_time * 0.015);
    vec3 tint = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(base, tint, 0.25) * (0.25 + 0.85 * glow), 1.0);
}
)"},
    {"neon_warp", "Neon Warp — tunnel glow", R"(
vec3 nwPalette(float t){vec3 a=vec3(0.960,0.260,0.580);vec3 b=vec3(0.900,0.138,0.450);vec3 c=vec3(0.520,0.200,0.520);vec3 d=vec3(-0.60,-0.90,-0.09);return a+b*cos(6.28318*(c*t+d));}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.8 * zoom);
    vec2 p0 = p;
    vec3 final_color = vec3(0.0);
    float dens = 3.5 + 3.0 * detail;
    vec2 u = p - 0.5; u *= sin(0.0 - length(u));
    float d = abs(sin(length(u) * exp(-length(p0)) * dens + u_time * 0.3) * 0.6);
    d = clamp(pow(0.03 / max(d, 0.02), mix(0.85, 1.25, contrast / 2.5)), 0.0, 2.5);
    final_color += nwPalette(length(p0) - u_time * 0.25 + 10.0 + hue) * d;
    u = p; u.x = -u.x; u = u - 0.5 + 1.0; u *= sin(1.0 - length(u));
    d = abs(sin(length(u) * exp(-length(p0)) * dens + u_time * 0.3) * 0.6);
    d = clamp(pow(0.03 / max(d, 0.02), mix(0.85, 1.25, contrast / 2.5)), 0.0, 2.5);
    final_color += nwPalette(length(p0) - u_time * 0.25 + 10.0 + hue) * d;
    u = p - 0.5 + 2.0; u *= sin(2.0 - length(u));
    d = abs(sin(length(u) * exp(-length(p0)) * dens + u_time * 0.3) * 0.6);
    d = clamp(pow(0.03 / max(d, 0.02), mix(0.85, 1.25, contrast / 2.5)), 0.0, 2.5);
    final_color += nwPalette(length(p0) - u_time * 0.25 + 10.0 + hue) * d;
    final_color *= clamp(0.55 / max(length(p0), 0.35), 0.0, 1.35);
    out_color = vec4(clamp(final_color, 0.0, 1.0), 1.0);
}
)"},
    {"soft_blobs", "Soft Blobs — neon metaballs", R"(
vec3 sbPalette(float t){vec3 a=vec3(0.660,0.560,0.680);vec3 b=vec3(0.718,0.438,0.720);vec3 c=vec3(0.520,0.100,0.520);vec3 d=vec3(-0.60,-0.30,-0.09);return a+b*cos(6.28318*(c*t+d));}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.7 * zoom);
    vec2 p0 = p;
    vec3 final_color = vec3(0.0);
    float dens = 6.0 + 6.0 * detail;
    vec2 u = p - 0.5; u *= sin(0.0 - length(p0));
    float d = abs(sin(length(u) * exp(-length(p0)) * dens + u_time * 0.25) * 0.5);
    d = clamp(pow(0.03 / max(d, 0.02), mix(1.0, 1.45, contrast / 2.5)), 0.0, 2.2);
    final_color += sbPalette(length(p0) - u_time * 0.25 + hue) * d;
    u = p - 0.5 + 1.0; u *= sin(5.0 - length(p0));
    d = abs(sin(length(u) * exp(-length(p0)) * dens + u_time * 0.25) * 0.5);
    d = clamp(pow(0.03 / max(d, 0.02), mix(1.0, 1.45, contrast / 2.5)), 0.0, 2.2);
    final_color += sbPalette(length(p0) - u_time * 0.25 + hue) * d;
    u = p - 0.5 + 2.0; u *= sin(10.0 - length(p0));
    d = abs(sin(length(u) * exp(-length(p0)) * dens + u_time * 0.25) * 0.5);
    d = clamp(pow(0.03 / max(d, 0.02), mix(1.0, 1.45, contrast / 2.5)), 0.0, 2.2);
    final_color += sbPalette(length(p0) - u_time * 0.25 + hue) * d;
    final_color *= clamp(1.0 / max(length(p0), 0.4), 0.0, 1.4);
    out_color = vec4(clamp(final_color, 0.0, 1.0), 1.0);
}
)"},
    {"atom_plasma", "Atom Plasma — compact lobes", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.85;
    float dens = 1.4 + 2.8 * detail;
    float v = 0.0;
    for(int i = 0; i < 4; i++)
    {
        float fi = float(i);
        vec2 c = vec2(cos(t * 0.4 + fi * 1.7), sin(t * 0.35 + fi * 2.1)) * (0.25 + 0.12 * fi);
        float d = length(p - c);
        v += sin(d * dens * 6.2831853 - t * (1.2 + 0.2 * fi)) * exp(-d * (2.2 + detail));
    }
    v = pow(clamp(v * 0.55 + 0.5, 0.0, 1.0), mix(1.8, 0.75, contrast / 2.5));
    float h = fract(v * 0.7 + hue + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.85) * (0.2 + 0.8 * v), 1.0);
}
)"},
    {"cell_bloom", "Cell Bloom — organic cells", R"(
float cbHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (3.0 * zoom * (0.7 + detail));
    p += vec2(u_time * 0.08, -u_time * 0.05);
    vec2 g = floor(p);
    vec2 f = fract(p);
    float md = 8.0;
    float mid = 8.0;
    for(int j = -1; j <= 1; j++)
    for(int i = -1; i <= 1; i++)
    {
        vec2 o = vec2(float(i), float(j));
        vec2 r = o + vec2(cbHash(g + o), cbHash(g + o + 19.0)) - f;
        float d = dot(r, r);
        if(d < md){mid = md; md = d;}
        else if(d < mid) mid = d;
    }
    float edge = clamp(sqrt(mid) - sqrt(md), 0.0, 1.0);
    float v = pow(clamp(1.0 - md * 1.8, 0.0, 1.0), mix(1.6, 0.7, contrast / 2.5));
    v *= 0.55 + 0.45 * smoothstep(0.02, 0.12, edge);
    float h = fract(hue + md * 0.35 + u_time * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.03, 0.04, 0.07), rgb, v), 1.0);
}
)"},
    {"voronoi_wash", "Voronoi Wash — soft cells", R"(
float vwHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    float scale = 2.2 + 5.0 * detail;
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (zoom * scale);
    p += 0.15 * vec2(sin(u_time * 0.35), cos(u_time * 0.28));
    vec2 g = floor(p);
    vec2 f = fract(p);
    float md = 8.0;
    for(int j = -1; j <= 1; j++)
    for(int i = -1; i <= 1; i++)
    {
        vec2 o = vec2(float(i), float(j));
        vec2 n = vec2(vwHash(g + o), vwHash(g + o + 7.1));
        n = 0.5 + 0.5 * sin(u_time * 0.6 + 6.2831853 * n);
        float d = length(o + n - f);
        md = min(md, d);
    }
    float v = pow(clamp(1.0 - md, 0.0, 1.0), mix(2.2, 0.9, contrast / 2.5));
    float rim = pow(clamp(1.0 - abs(md - 0.35) * 4.0, 0.0, 1.0), 2.0);
    v = max(v * 0.75, rim);
    float h = fract(hue + md * 0.4 + u_time * 0.025);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02), rgb, v), 1.0);
}
)"},
    {"gyroid_mist", "Gyroid Mist — soft SDF wash", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.4 * zoom);
    float t = u_time * 0.4;
    float s = 2.2 + 4.0 * detail;
    float g = sin(p.x * s + t) * cos(p.y * s - t * 0.8)
            + sin((p.x + p.y) * s * 0.7 - t)
            + cos(length(p) * s * 0.9 + t * 1.1);
    float v = pow(clamp(0.5 + 0.5 * g * 0.55, 0.0, 1.0), mix(1.7, 0.8, contrast / 2.5));
    float mist = exp(-length(p) * 0.35) * (0.55 + 0.45 * v);
    float h = fract(hue + v * 0.15 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(0.04, 0.05, 0.09), rgb, mist), 1.0);
}
)"},
    {"vortex_swirl", "Vortex Swirl — spiral field", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float r = length(p);
    float a = atan(p.y, p.x);
    float arms = 2.0 + floor(4.0 * detail + 0.5);
    float spiral = sin(a * arms + r * (6.0 + 8.0 * detail) - u_time * 1.6);
    float v = pow(clamp(0.5 + 0.5 * spiral, 0.0, 1.0), mix(2.0, 0.85, contrast / 2.5));
    v *= exp(-r * 0.65);
    float rings = 0.5 + 0.5 * sin(r * (10.0 + 12.0 * detail) - u_time * 2.0);
    v = max(v, rings * exp(-r * 1.1) * 0.55);
    float h = fract(hue + a * 0.08 + r * 0.12 + u_time * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
)"},
    {"wave_mesh", "Wave Mesh — interference lattice", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.2 * zoom);
    float t = u_time * 0.9;
    float dens = 3.0 + 7.0 * detail;
    float w = sin(p.x * dens + t) + sin(p.y * dens * 1.05 - t * 0.9)
            + sin((p.x + p.y) * dens * 0.7 + t * 1.15)
            + sin((p.x - p.y) * dens * 0.65 - t * 0.8);
    float v = pow(clamp(w * 0.22 + 0.5, 0.0, 1.0), mix(1.9, 0.75, contrast / 2.5));
    float h = fract(hue + v * 0.2 + t * 0.025);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.8) * (0.18 + 0.82 * v), 1.0);
}
)"},
    {"melt_petals", "Melt Petals — soft floral wash", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.8 * zoom);
    float t = u_time * 0.35;
    float r = length(p);
    float a = atan(p.y, p.x);
    float petals = 3.0 + floor(5.0 * detail + 0.5);
    float flower = 0.5 + 0.5 * cos(a * petals + sin(r * 4.0 - t) * 1.2);
    float melt = sin(r * (5.0 + 6.0 * detail) - t * 1.4 + flower * 2.0);
    float v = pow(clamp(0.5 + 0.45 * melt * flower, 0.0, 1.0), mix(1.8, 0.8, contrast / 2.5));
    v *= exp(-r * 0.55);
    float h = fract(hue + flower * 0.12 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.95, 0.55, 0.75), rgb, 0.65);
    out_color = vec4(mix(vec3(0.05, 0.03, 0.06), rgb, v), 1.0);
}
)"},
    {"oozy_flow", "Oozy Flow — soft molten wash", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.9 * zoom);
    float t = u_time * 0.45;
    float dens = 2.0 + 4.5 * detail;
    float v = 0.0;
    for(int i = 0; i < 5; i++)
    {
        float fi = float(i);
        vec2 q = p + vec2(sin(t * 0.7 + fi * 1.3), cos(t * 0.55 + fi * 0.9)) * (0.15 + 0.05 * fi);
        v += sin(q.x * dens + t + fi) * cos(q.y * dens * 0.85 - t * 0.8 + fi);
    }
    v = pow(clamp(v * 0.28 + 0.5, 0.0, 1.0), mix(1.7, 0.75, contrast / 2.5));
    float h = fract(hue + v * 0.18 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.85, 0.35, 0.15), rgb, 0.55);
    out_color = vec4(mix(vec3(0.06, 0.03, 0.02), rgb, v), 1.0);
}
)"},
    {"plasmic_ribbons", "Plasmic Ribbons — flowing bands", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.1 * zoom);
    float t = u_time * 0.7;
    float dens = 2.5 + 6.0 * detail;
    float ribbon = sin(p.x * dens + sin(p.y * 2.2 + t) * 1.8 + t)
                 + sin(p.y * dens * 0.8 - cos(p.x * 1.6 - t) * 1.4 - t * 0.9);
    float v = pow(clamp(ribbon * 0.35 + 0.5, 0.0, 1.0), mix(1.8, 0.8, contrast / 2.5));
    float h = fract(hue + ribbon * 0.08 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(1.0), rgb, 0.8) * (0.2 + 0.8 * v), 1.0);
}
)"},
    {"dense_chroma", "Dense Chroma — packed color plasma", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.4 * zoom);
    float t = u_time * 0.95;
    float dens = 3.5 + 8.0 * detail;
    float v = sin(p.x * dens + t) + sin(p.y * dens * 1.1 - t)
            + sin((p.x + p.y) * dens * 0.55 + t * 1.3)
            + cos(length(p) * dens * 0.9 - t * 0.8)
            + sin(dot(p, vec2(0.7, -0.5)) * dens + t * 0.6);
    v = pow(clamp(v * 0.18 + 0.5, 0.0, 1.0), mix(1.6, 0.7, contrast / 2.5));
    float h = fract(v * 1.4 + hue + t * 0.04);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(1.0), rgb, 0.9) * (0.25 + 0.75 * v), 1.0);
}
)"},
    {"rainbow_drip", "Rainbow Drip — falling color streaks", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.8;
    float cols = 4.0 + 10.0 * detail;
    float x = p.x * cols;
    float id = floor(x);
    float f = fract(x) - 0.5;
    float drip = fract(p.y * (1.2 + 0.8 * detail) - t * (0.6 + 0.5 * fract(sin(id * 12.9898) * 43758.5)) + id * 0.17);
    float streak = exp(-abs(f) * mix(8.0, 18.0, contrast / 2.5)) * pow(1.0 - abs(drip * 2.0 - 1.0), 2.2);
    float v = clamp(streak, 0.0, 1.0);
    float h = fract(hue + id * 0.07 + drip * 0.1);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
)"},
    {"neon_space", "Neon Space — soft star haze", R"(
float nsHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.6 * zoom * (0.8 + detail));
    p += 0.1 * vec2(sin(u_time * 0.2), cos(u_time * 0.17));
    vec2 g = floor(p);
    vec2 f = fract(p) - 0.5;
    float h0 = nsHash(g);
    float tw = 0.5 + 0.5 * sin(u_time * (2.0 + 4.0 * h0) + h0 * 20.0);
    float star = exp(-dot(f, f) * mix(18.0, 40.0, contrast / 2.5)) * step(0.72, h0) * tw;
    float haze = 0.15 + 0.25 * sin(length(p) * 2.0 - u_time * 0.4);
    float v = clamp(star + haze * 0.35, 0.0, 1.0);
    float hh = fract(hue + h0 * 0.3 + u_time * 0.02);
    vec3 rgb = clamp(abs(mod(hh * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.01, 0.02, 0.06), rgb, v), 1.0);
}
)"},
    {"fluid_swirl", "Fluid Swirl — soft current", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.5;
    vec2 q = p;
    q.x += 0.35 * sin(q.y * (2.0 + 3.0 * detail) + t);
    q.y += 0.35 * cos(q.x * (2.0 + 3.0 * detail) - t * 0.9);
    float v = sin(q.x * (3.0 + 5.0 * detail) + t) * cos(q.y * (3.0 + 4.0 * detail) - t);
    v = pow(clamp(v * 0.5 + 0.5, 0.0, 1.0), mix(1.7, 0.8, contrast / 2.5));
    float h = fract(hue + length(q) * 0.08 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.2, 0.55, 0.85), rgb, 0.65);
    out_color = vec4(mix(vec3(0.03, 0.05, 0.1), rgb, v), 1.0);
}
)"},
    {"psyche_grid", "Psyche Grid — soft 60s lattice", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.3 * zoom);
    float t = u_time * 0.35;
    float cells = 3.0 + 7.0 * detail;
    vec2 g = abs(fract(p * cells + vec2(t * 0.2, -t * 0.15)) - 0.5);
    float grid = 1.0 - smoothstep(0.38, 0.48, max(g.x, g.y));
    float swirl = 0.5 + 0.5 * sin((p.x + p.y) * cells * 0.5 + t * 2.0);
    float v = pow(clamp(mix(swirl * 0.55, 1.0, grid), 0.0, 1.0), mix(1.5, 0.75, contrast / 2.5));
    float h = fract(hue + floor(p.x * cells) * 0.08 + floor(p.y * cells) * 0.05 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.08, 0.02, 0.1), rgb, v), 1.0);
}
)"},
    {"blau_waves", "Blau Waves — soft blue bands", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.8 * zoom);
    float t = u_time * 0.55;
    float dens = 3.5 + 8.0 * detail;
    float w = sin(p.y * dens + t) + 0.55 * sin(p.x * dens * 0.45 - t * 0.7)
            + 0.35 * sin((p.x + p.y) * dens * 0.35 + t * 1.1);
    float v = pow(clamp(w * 0.4 + 0.5, 0.0, 1.0), mix(1.8, 0.8, contrast / 2.5));
    float h = fract(0.58 + hue * 0.15 + v * 0.08 + t * 0.015);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.15, 0.35, 0.85), rgb, 0.45);
    out_color = vec4(mix(vec3(0.02, 0.04, 0.12), rgb, v), 1.0);
}
)"},
    {"oil_slick", "Oil Slick — iridescent layers", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.0 * zoom);
    float t = u_time * 0.4;
    float dens = 2.2 + 5.0 * detail;
    float a = sin(p.x * dens + t) * cos(p.y * dens * 0.9 - t);
    float b = sin((p.x - p.y) * dens * 0.7 - t * 0.8);
    float c = cos(length(p) * dens * 1.1 + t * 0.6);
    float v = pow(clamp((a + b + c) * 0.28 + 0.5, 0.0, 1.0), mix(1.6, 0.75, contrast / 2.5));
    float h = fract(hue + a * 0.12 + b * 0.08 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb);
    out_color = vec4(mix(vec3(0.05, 0.06, 0.04), rgb, v), 1.0);
}
)"},
    {"jewel_scatter", "Jewel Scatter — soft sparkle dots", R"(
float jsHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (3.0 * zoom * (0.75 + detail));
    p += vec2(u_time * 0.12, -u_time * 0.08);
    vec2 g = floor(p);
    vec2 f = fract(p) - 0.5;
    float h0 = jsHash(g);
    float tw = 0.45 + 0.55 * sin(u_time * (3.0 + 5.0 * h0) + h0 * 30.0);
    float jewel = exp(-dot(f, f) * mix(14.0, 32.0, contrast / 2.5)) * step(0.62, h0) * tw;
    float trail = exp(-abs(f.y) * 10.0) * exp(-abs(f.x) * 4.0) * step(0.78, h0) * 0.45;
    float v = clamp(jewel + trail, 0.0, 1.0);
    float hh = fract(hue + h0 * 0.55 + u_time * 0.025);
    vec3 rgb = clamp(abs(mod(hh * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.02, 0.02, 0.05), rgb, v), 1.0);
}
)"},
    {"potential_rings", "Potential Rings — soft EM field", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.1 * zoom);
    float t = u_time * 0.65;
    vec2 c0 = vec2(sin(t * 0.4), cos(t * 0.35)) * 0.35;
    vec2 c1 = vec2(cos(t * 0.3), sin(t * 0.45)) * 0.4;
    float pot = 1.0 / max(length(p - c0), 0.08) - 1.0 / max(length(p - c1), 0.08);
    float rings = sin(pot * (2.5 + 5.0 * detail) - t);
    float v = pow(clamp(0.5 + 0.5 * rings, 0.0, 1.0), mix(1.9, 0.85, contrast / 2.5));
    v *= exp(-length(p) * 0.4);
    float h = fract(hue + pot * 0.05 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    out_color = vec4(mix(vec3(0.03, 0.04, 0.08), rgb, v), 1.0);
}
)"},
    {"fog_drift", "Fog Drift — soft rolling haze", R"(
float fdHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float fdNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = fdHash(i);
    float b = fdHash(i + vec2(1.0, 0.0));
    float c = fdHash(i + vec2(0.0, 1.0));
    float d = fdHash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.7 * zoom);
    float t = u_time * 0.25;
    float n = 0.0;
    float amp = 0.55;
    vec2 q = p * (1.5 + 2.5 * detail) + vec2(t, -t * 0.6);
    for(int i = 0; i < 4; i++)
    {
        n += fdNoise(q) * amp;
        q *= 2.05;
        amp *= 0.5;
    }
    float v = pow(clamp(n, 0.0, 1.0), mix(1.5, 0.7, contrast / 2.5));
    float h = fract(0.62 + hue * 0.2 + n * 0.1 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.45, 0.55, 0.7), rgb, 0.4);
    out_color = vec4(mix(vec3(0.06, 0.07, 0.1), rgb, v), 1.0);
}
)"},
    {"arc_static", "Arc Static — soft electric wash", R"(
float asHash(vec2 p){return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453);}
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (2.2 * zoom);
    float t = u_time * 1.1;
    float bolts = 0.0;
    for(int i = 0; i < 5; i++)
    {
        float fi = float(i);
        float ang = fi * 1.2566 + t * 0.15;
        vec2 dir = vec2(cos(ang), sin(ang));
        float along = dot(p, dir);
        float across = abs(dot(p, vec2(-dir.y, dir.x)));
        float jag = asHash(vec2(floor(along * (4.0 + 6.0 * detail) + t * 3.0), fi));
        float w = exp(-across * mix(10.0, 22.0, contrast / 2.5) / (0.35 + jag));
        bolts += w * (0.35 + 0.65 * jag);
    }
    float v = clamp(bolts * 0.55, 0.0, 1.0);
    float h = fract(0.55 + hue * 0.25 + v * 0.1 + t * 0.03);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.55, 0.75, 1.0), rgb, 0.35);
    out_color = vec4(mix(vec3(0.02, 0.03, 0.08), rgb, v), 1.0);
}
)"},
    {"petal_spin", "Petal Spin — rotating soft petals", R"(
void spatialMain(out vec4 out_color, in vec2 frag_coord)
{
    vec2 uv = frag_coord / u_resolution;
    float zoom = max(u_params[0], 0.25);
    float contrast = clamp(u_params[1], 0.35, 2.5);
    float hue = fract(u_params[2]);
    float detail = clamp(u_params[3], 0.05, 1.0);
    vec2 p = (uv - 0.5) * vec2(u_resolution.x / max(u_resolution.y, 1.0), 1.0) * (1.9 * zoom);
    float t = u_time * 0.4;
    float r = length(p);
    float a = atan(p.y, p.x) + t;
    float petals = 4.0 + floor(6.0 * detail + 0.5);
    float flower = 0.5 + 0.5 * cos(a * petals);
    float ring = sin(r * (5.0 + 7.0 * detail) - t * 1.5);
    float v = pow(clamp(0.5 + 0.45 * flower * ring, 0.0, 1.0), mix(1.7, 0.8, contrast / 2.5));
    v *= exp(-r * 0.5);
    float h = fract(hue + flower * 0.1 + t * 0.02);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    rgb = mix(vec3(0.9, 0.4, 0.65), rgb, 0.55);
    out_color = vec4(mix(vec3(0.05, 0.02, 0.06), rgb, v), 1.0);
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
