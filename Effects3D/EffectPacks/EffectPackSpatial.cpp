// SPDX-License-Identifier: GPL-2.0-only
#include "EffectPack.h"
#include "EffectPackBlockEval.h"

#include <algorithm>
#include <cmath>

namespace EffectPack
{
namespace
{

RGBColor ScaleI(RGBColor c, float intensity)
{
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    return ToRGBColor(
        (int)std::lround(RGBGetRValue(c) * intensity),
        (int)std::lround(RGBGetGValue(c) * intensity),
        (int)std::lround(RGBGetBValue(c) * intensity));
}

float Hash01(unsigned int h)
{
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) / (float)0xFFFFFFu;
}

float ValueNoise3(float x, float y, float z)
{
    const int ix = (int)std::floor(x);
    const int iy = (int)std::floor(y);
    const int iz = (int)std::floor(z);
    const float fx = x - (float)ix;
    const float fy = y - (float)iy;
    const float fz = z - (float)iz;
    auto corner = [](int cx, int cy, int cz) {
        return Hash01((unsigned int)(cx * 73856093 ^ cy * 19349663 ^ cz * 83492791));
    };
    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float sz = fz * fz * (3.0f - 2.0f * fz);
    const float c000 = corner(ix, iy, iz);
    const float c100 = corner(ix + 1, iy, iz);
    const float c010 = corner(ix, iy + 1, iz);
    const float c110 = corner(ix + 1, iy + 1, iz);
    const float c001 = corner(ix, iy, iz + 1);
    const float c101 = corner(ix + 1, iy, iz + 1);
    const float c011 = corner(ix, iy + 1, iz + 1);
    const float c111 = corner(ix + 1, iy + 1, iz + 1);
    const float x00 = c000 + (c100 - c000) * sx;
    const float x10 = c010 + (c110 - c010) * sx;
    const float x01 = c001 + (c101 - c001) * sx;
    const float x11 = c011 + (c111 - c011) * sx;
    const float y0 = x00 + (x10 - x00) * sy;
    const float y1 = x01 + (x11 - x01) * sy;
    return y0 + (y1 - y0) * sz;
}

float ProjectOnUnitAxis(float x, float y, float z,
                        float min_x, float max_x,
                        float min_y, float max_y,
                        float min_z, float max_z,
                        float ux, float uy, float uz)
{
    const float cx = 0.5f * (min_x + max_x);
    const float cy = 0.5f * (min_y + max_y);
    const float cz = 0.5f * (min_z + max_z);
    float t_min = 1e9f, t_max = -1e9f;
    for(int i = 0; i < 8; ++i)
    {
        const float px = (i & 1) ? max_x : min_x;
        const float py = (i & 2) ? max_y : min_y;
        const float pz = (i & 4) ? max_z : min_z;
        const float t = (px - cx) * ux + (py - cy) * uy + (pz - cz) * uz;
        t_min = std::min(t_min, t);
        t_max = std::max(t_max, t);
    }
    const float span = std::max(1e-5f, t_max - t_min);
    const float t = (x - cx) * ux + (y - cy) * uy + (z - cz) * uz;
    return std::clamp((t - t_min) / span, 0.0f, 1.0f);
}

using block_eval::WorldNorm;

WorldNorm MakeNormSample(float x, float y, float z,
                         float min_x, float max_x,
                         float min_y, float max_y,
                         float min_z, float max_z)
{
    WorldNorm s;
    s.span_x = max_x - min_x;
    s.span_y = max_y - min_y;
    s.span_z = max_z - min_z;
    const float diag = std::max(1e-5f, std::sqrt(s.span_x * s.span_x + s.span_y * s.span_y + s.span_z * s.span_z));
    const float eps = diag * 0.02f;

    s.nx = (s.span_x > eps) ? std::clamp((x - min_x) / s.span_x, 0.0f, 1.0f) : 0.5f;
    s.ny = (s.span_y > eps) ? std::clamp((y - min_y) / s.span_y, 0.0f, 1.0f) : 0.5f;
    s.nz = (s.span_z > eps) ? std::clamp((z - min_z) / s.span_z, 0.0f, 1.0f) : 0.5f;

    const float dx = (s.span_x > eps) ? (s.nx - 0.5f) : 0.0f;
    const float dy = (s.span_y > eps) ? (s.ny - 0.5f) : 0.0f;
    const float dz = (s.span_z > eps) ? (s.nz - 0.5f) : 0.0f;
    float max_d2 = 0.0f;
    if(s.span_x > eps) { max_d2 += 0.25f; }
    if(s.span_y > eps) { max_d2 += 0.25f; }
    if(s.span_z > eps) { max_d2 += 0.25f; }
    const float d2 = dx * dx + dy * dy + dz * dz;
    s.radius = (max_d2 > 1e-8f) ? std::sqrt(d2 / max_d2) : 0.0f;

    if(s.span_y > eps)
    {
        s.height = s.ny;
    }
    else if(s.span_z > eps)
    {
        s.height = s.nz;
    }
    else
    {
        s.height = s.nx;
    }
    return s;
}

bool CurvesEqual(const std::vector<CurvePoint>& a, const std::vector<CurvePoint>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(size_t i = 0; i < a.size(); ++i)
    {
        if(std::fabs(a[i].pos - b[i].pos) > 1e-4f || std::fabs(a[i].value - b[i].value) > 1e-4f)
        {
            return false;
        }
    }
    return true;
}

} // namespace

float SampleCurve(const std::vector<CurvePoint>& curve, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if(curve.empty())
    {
        return 1.0f;
    }
    if(curve.size() == 1)
    {
        return std::clamp(curve.front().value, 0.0f, 1.0f);
    }
    if(t <= curve.front().pos)
    {
        return std::clamp(curve.front().value, 0.0f, 1.0f);
    }
    if(t >= curve.back().pos)
    {
        return std::clamp(curve.back().value, 0.0f, 1.0f);
    }
    for(size_t i = 1; i < curve.size(); ++i)
    {
        const CurvePoint& a = curve[i - 1];
        const CurvePoint& b = curve[i];
        if(t <= b.pos)
        {
            const float span = std::max(1e-6f, b.pos - a.pos);
            const float u = (t - a.pos) / span;
            return std::clamp(a.value + (b.value - a.value) * u, 0.0f, 1.0f);
        }
    }
    return std::clamp(curve.back().value, 0.0f, 1.0f);
}

void ApplyBuiltinIntensityCurve(Block* block, const char* preset_id)
{
    if(!block || !preset_id)
    {
        return;
    }
    const std::string id(preset_id);
    block->intensity_curve.clear();
    if(id == "triangle" || id == "seesaw")
    {
        block->intensity_curve = {{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "ease_in")
    {
        block->intensity_curve = {{0.0f, 0.0f}, {0.6f, 0.25f}, {1.0f, 1.0f}};
    }
    else if(id == "ease_out")
    {
        block->intensity_curve = {{0.0f, 1.0f}, {0.4f, 0.75f}, {1.0f, 0.0f}};
    }
    else if(id == "pulse_curve")
    {
        block->intensity_curve = {{0.0f, 0.15f}, {0.2f, 1.0f}, {0.4f, 0.15f}, {1.0f, 0.15f}};
    }
    else if(id == "hold_peak")
    {
        block->intensity_curve = {{0.0f, 0.0f}, {0.2f, 1.0f}, {0.8f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "snap")
    {
        block->intensity_curve = {{0.0f, 0.0f}, {0.08f, 1.0f}, {0.92f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "flat")
    {
        return;
    }
    else
    {
        block->intensity_curve = {{0.0f, 1.0f}, {1.0f, 1.0f}};
    }
}

const char* MatchBuiltinIntensityCurve(const std::vector<CurvePoint>& curve)
{
    if(curve.empty())
    {
        return "flat";
    }
    static const char* kIds[] = {
        "triangle", "ease_in", "ease_out", "pulse_curve", "hold_peak", "snap"
    };
    for(const char* id : kIds)
    {
        Block probe;
        ApplyBuiltinIntensityCurve(&probe, id);
        if(CurvesEqual(curve, probe.intensity_curve))
        {
            return id;
        }
    }
    return nullptr;
}

bool ApplyGradientPresetId(Block* block, const char* preset_id, RGBColor accent)
{
    if(!block || !preset_id)
    {
        return false;
    }
    const std::string id(preset_id);
    if(id == "rainbow")
    {
        block->gradient = {
            {0.0f, ToRGBColor(255, 0, 0)},
            {0.2f, ToRGBColor(255, 128, 0)},
            {0.4f, ToRGBColor(255, 255, 0)},
            {0.6f, ToRGBColor(0, 255, 0)},
            {0.8f, ToRGBColor(0, 128, 255)},
            {1.0f, ToRGBColor(128, 0, 255)},
        };
    }
    else if(id == "red_blue")
    {
        block->gradient = {{0.0f, ToRGBColor(255, 0, 0)}, {1.0f, ToRGBColor(0, 80, 255)}};
    }
    else if(id == "white_color")
    {
        block->gradient = {{0.0f, ToRGBColor(255, 255, 255)}, {1.0f, accent}};
    }
    else if(id == "fire")
    {
        block->gradient = {
            {0.0f, ToRGBColor(20, 0, 0)},
            {0.35f, ToRGBColor(255, 40, 0)},
            {0.7f, ToRGBColor(255, 160, 0)},
            {1.0f, ToRGBColor(255, 255, 180)},
        };
    }
    else if(id == "ice")
    {
        block->gradient = {
            {0.0f, ToRGBColor(20, 40, 80)},
            {0.5f, ToRGBColor(120, 200, 255)},
            {1.0f, ToRGBColor(240, 250, 255)},
        };
    }
    else if(id == "forest")
    {
        block->gradient = {
            {0.0f, ToRGBColor(10, 40, 10)},
            {0.5f, ToRGBColor(40, 160, 60)},
            {1.0f, ToRGBColor(180, 255, 120)},
        };
    }
    else if(id == "sunset")
    {
        block->gradient = {
            {0.0f, ToRGBColor(40, 20, 80)},
            {0.4f, ToRGBColor(255, 80, 40)},
            {0.75f, ToRGBColor(255, 180, 60)},
            {1.0f, ToRGBColor(255, 240, 200)},
        };
    }
    else if(id == "cyber")
    {
        block->gradient = {
            {0.0f, ToRGBColor(0, 255, 180)},
            {0.5f, ToRGBColor(0, 120, 255)},
            {1.0f, ToRGBColor(200, 0, 255)},
        };
    }
    else
    {
        return false;
    }
    if(!block->gradient.empty())
    {
        block->color = block->gradient.front().color;
        block->color_from = block->gradient.front().color;
        block->color_to = block->gradient.back().color;
    }
    return true;
}

void AxisUnitVector(const Block& block, float* out_x, float* out_y, float* out_z)
{
    float x = 1.0f, y = 0.0f, z = 0.0f;
    if(block.axis_mode == AxisMode::Custom)
    {
        const float deg = 3.14159265358979323846f / 180.0f;
        const float yaw = block.axis_yaw_deg * deg;
        const float pitch = block.axis_pitch_deg * deg;
        const float cp = std::cos(pitch);
        x = std::cos(yaw) * cp;
        y = std::sin(pitch);
        z = std::sin(yaw) * cp;
    }
    else
    {
        const int axis = DirectionPreferredAxis(block.direction);
        const bool inv = DirectionInvertsAxis(block.direction);
        x = (axis == 0) ? (inv ? -1.0f : 1.0f) : 0.0f;
        y = (axis == 1) ? (inv ? -1.0f : 1.0f) : 0.0f;
        z = (axis == 2) ? (inv ? -1.0f : 1.0f) : 0.0f;
    }
    const float len = std::max(1e-6f, std::sqrt(x * x + y * y + z * z));
    if(out_x) { *out_x = x / len; }
    if(out_y) { *out_y = y / len; }
    if(out_z) { *out_z = z / len; }
}

float SampleAxisPos(const Block& block,
                    float x, float y, float z,
                    float min_x, float max_x,
                    float min_y, float max_y,
                    float min_z, float max_z)
{
    if(block.axis_mode == AxisMode::Custom)
    {
        float ux, uy, uz;
        AxisUnitVector(block, &ux, &uy, &uz);
        return ProjectOnUnitAxis(x, y, z, min_x, max_x, min_y, max_y, min_z, max_z, ux, uy, uz);
    }
    return WorldAxisPos(block.direction, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
}

float SampleSpinAngle(const Block& block,
                      float x, float y, float z,
                      float min_x, float max_x,
                      float min_y, float max_y,
                      float min_z, float max_z)
{
    if(block.axis_mode == AxisMode::Custom)
    {
        float ux, uy, uz;
        AxisUnitVector(block, &ux, &uy, &uz);
        float rx = 0.0f, ry = 1.0f, rz = 0.0f;
        if(std::fabs(uy) > 0.9f)
        {
            rx = 1.0f; ry = 0.0f; rz = 0.0f;
        }
        float tx = ry * uz - rz * uy;
        float ty = rz * ux - rx * uz;
        float tz = rx * uy - ry * ux;
        float tlen = std::max(1e-6f, std::sqrt(tx * tx + ty * ty + tz * tz));
        tx /= tlen; ty /= tlen; tz /= tlen;
        float bx = uy * tz - uz * ty;
        float by = uz * tx - ux * tz;
        float bz = ux * ty - uy * tx;
        const float cx = 0.5f * (min_x + max_x);
        const float cy = 0.5f * (min_y + max_y);
        const float cz = 0.5f * (min_z + max_z);
        const float dx = x - cx, dy = y - cy, dz = z - cz;
        const float u = dx * tx + dy * ty + dz * tz;
        const float v = dx * bx + dy * by + dz * bz;
        if(std::fabs(u) < 1e-6f && std::fabs(v) < 1e-6f)
        {
            return SampleAxisPos(block, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
        }
        float ang = std::atan2(v, u);
        float t = ang / (2.0f * 3.14159265358979323846f) + 0.5f;
        t -= std::floor(t);
        return t;
    }
    return WorldSpinAngle(block.direction, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
}

bool BlockNeedsWorldEval(BlockType t)
{
    switch(t)
    {
        case BlockType::SphereWipe:
        case BlockType::Orbit:
        case BlockType::Ripple:
        case BlockType::Meteor:
        case BlockType::Noise3D:
        case BlockType::Burst:
        case BlockType::Plasma:
        case BlockType::Snow:
        case BlockType::Fire:
        case BlockType::Balls:
        case BlockType::Bars:
            return true;
        default:
            return false;
    }
}

bool BlockNeedsDirection(BlockType t)
{
    switch(t)
    {
        case BlockType::Wipe:
        case BlockType::Chase:
        case BlockType::Wave:
        case BlockType::ColorWash:
        case BlockType::Spin:
        case BlockType::Alternating:
        case BlockType::Orbit:
        case BlockType::Meteor:
        case BlockType::Bars:
        case BlockType::Scanner:
            return true;
        default:
            return false;
    }
}

bool BlockUsesSequenceAxis(const Block& block)
{
    if(block.axis_space != AxisSpace::Sequence)
    {
        return false;
    }
    // Volume / field types keep room XYZ; everything else marches along order.
    switch(block.type)
    {
        case BlockType::SphereWipe:
        case BlockType::Orbit:
        case BlockType::Ripple:
        case BlockType::Noise3D:
        case BlockType::Burst:
        case BlockType::Plasma:
        case BlockType::Snow:
        case BlockType::Fire:
        case BlockType::Balls:
            return false;
        default:
            return true;
    }
}

bool BlockUsesSharedWorldBounds(const Block& block)
{
    if(block.axis_space == AxisSpace::Room)
    {
        return true;
    }
    // Sequence + volume/field → room XYZ
    return block.axis_space == AxisSpace::Sequence && !BlockUsesSequenceAxis(block);
}

namespace
{

using block_eval::WorldCtx;
using block_eval::WorldFn;

bool EvalSphereWipe(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float edge = 0.12f;
    const float front = ctx.progress * (1.0f + 2.0f * edge) - edge;
    const float d = front - ctx.s.radius;
    float cover = 0.0f;
    if(d >= edge) { cover = 1.0f; }
    else if(d > -edge) { cover = (d + edge) / (2.0f * edge); }
    if(cover <= 0.001f) { return false; }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalOrbit(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    float ux, uy, uz;
    AxisUnitVector(block, &ux, &uy, &uz);
    float rx = 0.0f, ry = 1.0f, rz = 0.0f;
    if(std::fabs(uy) > 0.9f) { rx = 1.0f; ry = 0.0f; }
    float tx = ry * uz - rz * uy;
    float ty = rz * ux - rx * uz;
    float tz = rx * uy - ry * ux;
    float tlen = std::max(1e-6f, std::sqrt(tx * tx + ty * ty + tz * tz));
    tx /= tlen; ty /= tlen; tz /= tlen;
    float bx = uy * tz - uz * ty;
    float by = uz * tx - ux * tz;
    float bz = ux * ty - uy * tx;
    const float u = ctx.dx * tx + ctx.dy * ty + ctx.dz * tz;
    const float v = ctx.dx * bx + ctx.dy * by + ctx.dz * bz;
    float ang = std::atan2(v, u) / (2.0f * 3.14159265358979323846f) + 0.5f;
    ang -= std::floor(ang);
    const float width = std::clamp(block.pulse_length, 0.06f, 0.5f);
    float delta = ang - ctx.progress;
    delta -= std::floor(delta + 0.5f);
    const float lead = width * 0.25f;
    const float trail = width;
    float cover = 0.0f;
    if(delta >= 0.0f && delta <= lead) { cover = 1.0f - (delta / lead); }
    else if(delta < 0.0f && -delta <= trail) { cover = 1.0f + (delta / trail); }
    cover *= 0.45f + 0.55f * (1.0f - std::clamp(ctx.s.radius, 0.0f, 1.0f) * 0.5f);
    if(cover <= 0.001f) { return false; }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalRipple(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float band = std::clamp(block.pulse_length, 0.06f, 0.4f);
    float cover = 0.0f;
    const float wave = std::fabs(ctx.s.radius - ctx.progress);
    if(wave <= band) { cover = 1.0f - (wave / band); }
    const float wave2 = std::fabs(ctx.s.radius - std::fmod(ctx.progress + 0.5f, 1.0f));
    if(wave2 <= band) { cover = std::max(cover, (1.0f - (wave2 / band)) * 0.75f); }
    if(cover <= 0.001f) { return false; }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, ctx.s.radius);
    return true;
}

bool EvalMeteor(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float along = SampleAxisPos(block, ctx.x, ctx.y, ctx.z,
                                      ctx.min_x, ctx.max_x, ctx.min_y, ctx.max_y, ctx.min_z, ctx.max_z);
    const float trail = std::clamp(block.pulse_length, 0.08f, 0.55f);
    auto hit = [&](float head) -> float {
        const float delta = head - along;
        if(delta < 0.0f || delta > trail) { return 0.0f; }
        return 1.0f - (delta / trail);
    };
    float cover = hit(ctx.progress);
    if(cover <= 0.001f)
    {
        const float h = Hash01((unsigned int)ctx.twinkle_seed * 2654435761u);
        cover = hit(std::fmod(ctx.progress + h * 0.85f, 1.0f)) * 0.85f;
    }
    if(cover <= 0.001f) { return false; }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, cover);
    return true;
}

bool EvalNoiseOrPlasma(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float t = ctx.progress * 3.0f;
    const float n = ValueNoise3(ctx.s.nx * 2.5f + t, ctx.s.ny * 2.5f - t * 0.6f, ctx.s.nz * 2.5f + t * 0.35f);
    const float n2 = ValueNoise3(ctx.s.nx * 5.0f - t * 0.5f, ctx.s.ny * 5.0f + t * 0.4f, ctx.s.nz * 5.0f);
    const float field = std::clamp(0.5f * n + 0.5f * n2, 0.0f, 1.0f);
    ctx.intensity *= 0.35f + 0.65f * field;
    ctx.color = SampleGradient(block, field);
    return true;
}

bool EvalSnow(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float h = Hash01((unsigned int)ctx.twinkle_seed * 9743197u + 17u);
    const float fall = std::fmod(ctx.progress + h, 1.0f);
    const float flake_h = 1.0f - fall;
    const float drift = 0.12f * std::sin(fall * 10.0f + h * 8.0f);
    float plane = ctx.s.nx;
    if(ctx.s.height == ctx.s.nx)
    {
        plane = (ctx.s.span_z >= ctx.s.span_y) ? ctx.s.nz : ctx.s.ny;
    }
    else if(ctx.s.height == ctx.s.ny)
    {
        plane = (ctx.s.span_x >= ctx.s.span_z) ? ctx.s.nx : ctx.s.nz;
    }
    else
    {
        plane = (ctx.s.span_x >= ctx.s.span_y) ? ctx.s.nx : ctx.s.ny;
    }
    const float fx = std::fmod(h + drift + 1.0f, 1.0f);
    const float d = std::fabs(plane - fx) * 1.2f + std::fabs(ctx.s.height - flake_h);
    if(d > 0.16f) { return false; }
    ctx.intensity *= 1.0f - d / 0.16f;
    ctx.color = SampleGradient(block, h);
    return true;
}

bool EvalFire(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float rise = 1.0f - ctx.s.height;
    const float flicker = ValueNoise3(ctx.s.nx * 4.0f, ctx.s.height * 3.0f + ctx.progress * 6.0f, ctx.s.nz * 4.0f);
    const float heat = std::clamp(rise * (0.4f + 0.6f * flicker) + 0.08f * flicker, 0.0f, 1.0f);
    if(heat < 0.08f) { return false; }
    ctx.intensity *= heat;
    ctx.color = SampleGradient(block, heat);
    return true;
}

bool EvalBalls(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float rad = std::clamp(block.pulse_length * 0.5f, 0.1f, 0.35f);
    float best = 1.0f;
    for(int i = 0; i < 4; ++i)
    {
        const float ph = (float)i / 4.0f;
        const float cx = 0.5f + 0.38f * std::sin((ctx.progress + ph) * 6.2831853f);
        const float cy = 0.5f + 0.38f * std::cos((ctx.progress * 1.2f + ph) * 6.2831853f);
        const float cz = 0.5f + 0.38f * std::sin((ctx.progress * 0.85f + ph * 1.7f) * 6.2831853f);
        float ddx = ctx.s.nx - cx;
        float ddy = ctx.s.ny - cy;
        float ddz = ctx.s.nz - cz;
        const float diag = std::max(1e-5f, std::sqrt(ctx.s.span_x * ctx.s.span_x + ctx.s.span_y * ctx.s.span_y + ctx.s.span_z * ctx.s.span_z));
        const float eps = diag * 0.02f;
        if(ctx.s.span_x <= eps) { ddx = 0.0f; }
        if(ctx.s.span_y <= eps) { ddy = 0.0f; }
        if(ctx.s.span_z <= eps) { ddz = 0.0f; }
        best = std::min(best, std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz));
    }
    if(best > rad) { return false; }
    ctx.intensity *= 1.0f - best / rad;
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalBars(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float axis = SampleAxisPos(block, ctx.x, ctx.y, ctx.z,
                                     ctx.min_x, ctx.max_x, ctx.min_y, ctx.max_y, ctx.min_z, ctx.max_z);
    const int bars = 5;
    const float pos = std::fmod(axis * (float)bars + ctx.progress * (float)bars + 1.0f, 1.0f);
    if(pos > 0.62f) { return false; }
    ctx.intensity *= 0.75f + 0.25f * (1.0f - pos / 0.62f);
    ctx.color = SampleGradient(block, axis);
    return true;
}

bool EvalBurst(WorldCtx& ctx)
{
    const Block& block = *ctx.block;
    const float edge = std::clamp(block.pulse_length, 0.06f, 0.45f);
    const float front = ctx.progress * (1.0f + 2.0f * edge);
    const float d = front - ctx.s.radius;
    float cover = 0.0f;
    if(d >= 0.0f && d <= edge)
    {
        cover = 1.0f - (d / edge);
    }
    else if(d < 0.0f && -d <= edge * 0.35f)
    {
        cover = 1.0f + (d / (edge * 0.35f));
    }
    if(ctx.progress < 0.2f)
    {
        cover = std::max(cover, (1.0f - ctx.progress / 0.2f) * (1.0f - std::clamp(ctx.s.radius, 0.0f, 1.0f)));
    }
    if(cover <= 0.001f) { return false; }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, std::clamp(ctx.s.radius, 0.0f, 1.0f));
    return true;
}

bool EvalWorldDefault(WorldCtx& ctx)
{
    float t = ctx.progress + ctx.s.nx * 0.25f + ctx.s.ny * 0.25f + ctx.s.nz * 0.25f;
    t -= std::floor(t);
    ctx.color = SampleGradient(*ctx.block, t);
    return true;
}

WorldFn WorldFnFor(BlockType type)
{
    switch(type)
    {
        case BlockType::SphereWipe: return &EvalSphereWipe;
        case BlockType::Orbit: return &EvalOrbit;
        case BlockType::Ripple: return &EvalRipple;
        case BlockType::Meteor: return &EvalMeteor;
        case BlockType::Noise3D:
        case BlockType::Plasma: return &EvalNoiseOrPlasma;
        case BlockType::Snow: return &EvalSnow;
        case BlockType::Fire: return &EvalFire;
        case BlockType::Balls: return &EvalBalls;
        case BlockType::Bars: return &EvalBars;
        case BlockType::Burst: return &EvalBurst;
        default: return &EvalWorldDefault;
    }
}

} // namespace

bool EvaluateBlockAtWorld(const Block& block,
                          int local_ms,
                          float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z,
                          int twinkle_seed,
                          RGBColor* out_color,
                          float* out_intensity)
{
    if(local_ms < block.start_ms || local_ms >= block.end_ms || block.end_ms <= block.start_ms)
    {
        return false;
    }

    WorldCtx ctx;
    ctx.block = &block;
    ctx.local_ms = local_ms;
    ctx.progress = BlockProgress(block, local_ms);
    ctx.twinkle_seed = twinkle_seed;
    ctx.x = x;
    ctx.y = y;
    ctx.z = z;
    ctx.min_x = min_x;
    ctx.max_x = max_x;
    ctx.min_y = min_y;
    ctx.max_y = max_y;
    ctx.min_z = min_z;
    ctx.max_z = max_z;
    ctx.s = MakeNormSample(x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
    ctx.dx = ctx.s.nx - 0.5f;
    ctx.dy = ctx.s.ny - 0.5f;
    ctx.dz = ctx.s.nz - 0.5f;
    ctx.intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    ctx.color = block.color;

    WorldFn fn = WorldFnFor(block.type);
    if(!fn(ctx))
    {
        return false;
    }

    if(!block.intensity_curve.empty())
    {
        ctx.intensity *= SampleCurve(block.intensity_curve, ctx.progress);
    }

    if(out_color)
    {
        *out_color = ScaleI(ctx.color, ctx.intensity);
    }
    if(out_intensity)
    {
        *out_intensity = ctx.intensity;
    }
    return true;
}

} // namespace EffectPack
