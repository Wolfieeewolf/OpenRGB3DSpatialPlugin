// SPDX-License-Identifier: GPL-2.0-only
#include "EffectPack.h"
#include "EffectPackBlockEval.h"
#include "EffectScript.h"

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
    if(id == "solid")
    {
        block->color = accent;
        block->color_from = accent;
        block->color_to = accent;
        block->gradient = {{0.0f, accent}};
    }
    else if(id == "rainbow")
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

bool BlockUsesSequenceAxis(const Block& block)
{
    if(block.axis_space != AxisSpace::Sequence)
    {
        return false;
    }
    return !script::UsesWorld(script::FileId(block));
}

bool BlockUsesSharedWorldBounds(const Block& block)
{
    if(block.axis_space == AxisSpace::Room)
    {
        return true;
    }
    return block.axis_space == AxisSpace::Sequence && !BlockUsesSequenceAxis(block);
}

using block_eval::WorldCtx;


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

    script::LedView led;
    led.block = &block;
    led.local_ms = local_ms;
    led.progress = ctx.progress;
    led.axis = ctx.s.nx;
    led.seed = twinkle_seed;
    led.intensity = ctx.intensity;
    led.color = ctx.color;
    led.x = x;
    led.y = y;
    led.z = z;
    led.min_x = min_x;
    led.max_x = max_x;
    led.min_y = min_y;
    led.max_y = max_y;
    led.min_z = min_z;
    led.max_z = max_z;
    led.nx = ctx.s.nx;
    led.ny = ctx.s.ny;
    led.nz = ctx.s.nz;
    led.radius = ctx.s.radius;
    led.height = ctx.s.height;
    led.span_x = ctx.s.span_x;
    led.span_y = ctx.s.span_y;
    led.span_z = ctx.s.span_z;
    led.dx = ctx.dx;
    led.dy = ctx.dy;
    led.dz = ctx.dz;
    const std::string id = script::FileId(block);
    if(!script::Run(id, &led))
    {
        return false;
    }
    ctx.color = led.color;
    ctx.intensity = led.intensity;

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
