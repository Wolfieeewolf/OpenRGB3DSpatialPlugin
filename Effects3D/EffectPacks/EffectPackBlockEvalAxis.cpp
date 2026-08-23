// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"
#include "EffectPackBlockEval.h"
#include "EffectPackDetail.h"

#include <algorithm>
#include <cmath>

namespace EffectPack
{
using detail::ScaleIntensity;
using detail::LerpColor;
using detail::AxisPos;
using detail::HashLed;
using detail::NormOnAxis;

float WorldAxisPos(Direction dir,
                   float x, float y, float z,
                   float min_x, float max_x,
                   float min_y, float max_y,
                   float min_z, float max_z)
{
    const float sx = max_x - min_x;
    const float sy = max_y - min_y;
    const float sz = max_z - min_z;
    const float diag = std::max(1e-5f, std::sqrt(sx * sx + sy * sy + sz * sz));
    const float eps = diag * 0.02f;
    const bool invert = DirectionInvertsAxis(dir);
    const int preferred = DirectionPreferredAxis(dir);

    auto span_of = [&](int axis) -> float {
        return (axis == 0) ? sx : ((axis == 1) ? sy : sz);
    };
    auto sample = [&](int axis) -> float {
        if(axis == 0)
        {
            return NormOnAxis(x, min_x, max_x, invert);
        }
        if(axis == 1)
        {
            return NormOnAxis(y, min_y, max_y, invert);
        }
        return NormOnAxis(z, min_z, max_z, invert);
    };

    int order[3] = {preferred, 0, 0};
    if(preferred == 0)
    {
        order[1] = 2;
        order[2] = 1;
    }
    else if(preferred == 1)
    {
        order[1] = 2;
        order[2] = 0;
    }
    else
    {
        order[1] = 0;
        order[2] = 1;
    }

    for(int i = 0; i < 3; ++i)
    {
        if(span_of(order[i]) > eps)
        {
            return sample(order[i]);
        }
    }
    return invert ? 1.0f : 0.0f;
}

float WorldSpinAngle(Direction dir,
                     float x, float y, float z,
                     float min_x, float max_x,
                     float min_y, float max_y,
                     float min_z, float max_z)
{
    const float sx = max_x - min_x;
    const float sy = max_y - min_y;
    const float sz = max_z - min_z;
    const float diag = std::max(1e-5f, std::sqrt(sx * sx + sy * sy + sz * sz));
    const float eps = diag * 0.02f;
    const bool invert = DirectionInvertsAxis(dir);
    const int axis = DirectionPreferredAxis(dir);

    const float cx = 0.5f * (min_x + max_x);
    const float cy = 0.5f * (min_y + max_y);
    const float cz = 0.5f * (min_z + max_z);
    float u = 0.0f;
    float v = 0.0f;
    float su = 0.0f;
    float sv = 0.0f;
    if(axis == 0)
    {
        u = y - cy;
        v = z - cz;
        su = sy;
        sv = sz;
    }
    else if(axis == 1)
    {
        u = x - cx;
        v = z - cz;
        su = sx;
        sv = sz;
    }
    else
    {
        u = x - cx;
        v = y - cy;
        su = sx;
        sv = sy;
    }

    if(su <= eps || sv <= eps)
    {
        return WorldAxisPos(dir, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
    }

    u /= std::max(su, eps);
    v /= std::max(sv, eps);
    float ang = std::atan2(v, u);
    float t = ang / (2.0f * 3.14159265358979323846f) + 0.5f;
    t -= std::floor(t);
    return invert ? (1.0f - t) : t;
}

namespace
{

using block_eval::AxisCtx;
using block_eval::AxisFn;

bool EvalSolid(AxisCtx& ctx)
{
    ctx.color = SampleGradient(*ctx.block, 0.0f);
    return true;
}

bool EvalFade(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float t = (float)(ctx.local_ms - block.start_ms) / (float)(block.end_ms - block.start_ms);
    ctx.color = SampleGradient(block, t);
    return true;
}

bool EvalPulse(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float speed = std::max(0.05f, block.speed);
    const int period = std::max(1, (int)std::lround((float)std::max(1, block.period_ms) / speed));
    const float phase = (float)((ctx.local_ms - block.start_ms) % period) / (float)period;
    const float wave = 0.5f - 0.5f * std::cos(phase * 6.28318530718f);
    const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
    const float hi = std::clamp(block.max_intensity, 0.0f, 1.0f);
    ctx.intensity *= lo + (hi - lo) * wave;
    ctx.color = SampleGradient(block, phase);
    return true;
}

bool EvalWipe(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float edge = 0.08f;
    const float front = ctx.progress * (1.0f + 2.0f * edge) - edge;
    const float d = front - ctx.axis;
    float cover = 0.0f;
    if(d >= edge)
    {
        cover = 1.0f;
    }
    else if(d > -edge)
    {
        cover = (d + edge) / (2.0f * edge);
    }
    if(cover <= 0.001f)
    {
        return false;
    }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalChase(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float head = std::clamp(block.pulse_length, 0.02f, 1.0f);
    float delta = std::fabs(ctx.axis - ctx.progress);
    delta = std::min(delta, 1.0f - delta);
    if(delta > head)
    {
        return false;
    }
    ctx.intensity *= 1.0f - (delta / head);
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalTwinkle(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float speed = std::max(0.05f, block.speed);
    const int period = std::max(80, (int)std::lround((float)std::max(80, block.period_ms) / speed));
    const unsigned int h0 = HashLed(ctx.twinkle_seed, 0, 1);
    const float phase0 = (float)(h0 & 0xFFFF) / 65535.0f;
    const float density = 0.12f + 0.55f * std::clamp(block.intensity, 0.0f, 1.0f);
    const int local = std::max(0, ctx.local_ms - block.start_ms);
    const int epoch = local / period;
    const unsigned int he = HashLed(ctx.twinkle_seed ^ 0xA5A5, epoch * period, period);
    const float roll = (float)((he >> 8) & 0xFF) / 255.0f;
    const bool active_cycle = (roll < density);
    const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
    const float hi = std::clamp(block.max_intensity, lo, 1.0f);
    float flash = 0.0f;
    if(active_cycle)
    {
        const int phase_ms = (local + (int)std::lround(phase0 * (float)period)) % period;
        const float phase = (float)phase_ms / (float)period;
        const float win = 0.30f;
        if(phase < win)
        {
            flash = std::sin((phase / win) * 3.14159265358979323846f);
        }
    }
    ctx.intensity *= lo + (hi - lo) * flash;
    const RGBColor base = SampleGradient(block, phase0);
    const RGBColor peak = SampleGradient(block, std::min(1.0f, phase0 + 0.35f));
    ctx.color = LerpColor(base, peak, flash);
    return true;
}

bool EvalAlternating(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float speed = std::max(0.05f, block.speed);
    const int period = std::max(50, (int)std::lround((float)std::max(50, block.period_ms) / speed));
    const int phase_bit = ((ctx.local_ms - block.start_ms) / period) & 1;
    const int led_bit = (ctx.twinkle_seed ^ (int)std::lround(ctx.axis * 1024.0f)) & 1;
    const bool a = (led_bit ^ phase_bit) == 0;
    ctx.color = a ? SampleGradient(block, 0.0f) : SampleGradient(block, 1.0f);
    return true;
}

bool EvalStrobe(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float speed = std::max(0.05f, block.speed);
    const int period = std::max(40, (int)std::lround((float)std::max(40, block.period_ms) / speed));
    const float phase = (float)((ctx.local_ms - block.start_ms) % period) / (float)period;
    const float duty = std::clamp(block.pulse_length, 0.05f, 0.95f);
    if(phase > duty)
    {
        return false;
    }
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalSpin(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float width = std::clamp(block.pulse_length, 0.04f, 0.55f);
    float delta = ctx.axis - ctx.progress;
    delta -= std::floor(delta + 0.5f);
    const float lead = width * 0.22f;
    const float trail = width;
    float cover = 0.0f;
    if(delta >= 0.0f && delta <= lead)
    {
        cover = 1.0f - (delta / lead);
    }
    else if(delta < 0.0f && -delta <= trail)
    {
        cover = 1.0f + (delta / trail);
    }
    if(width <= 0.28f)
    {
        float delta2 = delta - ((delta >= 0.0f) ? 0.5f : -0.5f);
        delta2 -= std::floor(delta2 + 0.5f);
        float cover2 = 0.0f;
        if(delta2 >= 0.0f && delta2 <= lead)
        {
            cover2 = 1.0f - (delta2 / lead);
        }
        else if(delta2 < 0.0f && -delta2 <= trail)
        {
            cover2 = 1.0f + (delta2 / trail);
        }
        cover = std::max(cover, cover2 * 0.85f);
    }
    if(cover <= 0.001f)
    {
        return false;
    }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, ctx.progress);
    return true;
}

bool EvalCandle(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const unsigned int h = HashLed(ctx.twinkle_seed, ctx.local_ms / 30, 30);
    const float n1 = (float)(h & 0xFF) / 255.0f;
    const float n2 = (float)((h >> 8) & 0xFF) / 255.0f;
    const float flicker = 0.55f + 0.45f * (0.65f * n1 + 0.35f * n2);
    const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
    const float hi = std::clamp(block.max_intensity, lo, 1.0f);
    ctx.intensity *= lo + (hi - lo) * flicker;
    ctx.color = SampleGradient(block, 0.15f + 0.7f * n1);
    return true;
}

bool EvalDissolve(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const unsigned int h = HashLed(ctx.twinkle_seed ^ 0x5F3759DF, 0, 1);
    const float threshold = (float)(h & 0xFFFF) / 65535.0f;
    if(ctx.progress + 0.001f < threshold)
    {
        return false;
    }
    float cover = 1.0f;
    if(ctx.progress < threshold + 0.08f)
    {
        cover = (ctx.progress - threshold) / 0.08f;
    }
    ctx.intensity *= std::clamp(cover, 0.0f, 1.0f);
    ctx.color = SampleGradient(block, threshold);
    return true;
}

bool EvalWave(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    const float cycles = std::max(0.25f, block.speed);
    const float phase = ctx.progress * cycles * 6.2831853f;
    const float wave = 0.5f + 0.5f * std::sin((ctx.axis * 6.2831853f * std::max(0.5f, block.pulse_length * 4.0f)) - phase);
    ctx.intensity *= std::clamp(wave, 0.0f, 1.0f);
    ctx.color = SampleGradient(block, ctx.axis);
    return true;
}

bool EvalScanner(AxisCtx& ctx)
{
    const Block& block = *ctx.block;
    float head = ctx.progress * 2.0f;
    if(head > 1.0f)
    {
        head = 2.0f - head;
    }
    if(DirectionInvertsAxis(block.direction))
    {
        head = 1.0f - head;
    }
    const float half_w = std::max(0.02f, block.pulse_length * 0.5f);
    const float dist = std::fabs(ctx.axis - head);
    float cover = 0.0f;
    if(dist <= half_w)
    {
        cover = 1.0f - (dist / half_w);
    }
    if(cover <= 0.001f)
    {
        return false;
    }
    ctx.intensity *= cover;
    ctx.color = SampleGradient(block, head);
    return true;
}

bool EvalColorWash(AxisCtx& ctx)
{
    float t = ctx.progress + ctx.axis * 0.35f;
    t -= std::floor(t);
    ctx.color = SampleGradient(*ctx.block, t);
    return true;
}

AxisFn AxisFnFor(BlockType type)
{
    switch(type)
    {
        case BlockType::Solid: return &EvalSolid;
        case BlockType::Fade: return &EvalFade;
        case BlockType::Pulse: return &EvalPulse;
        case BlockType::Wipe: return &EvalWipe;
        case BlockType::Chase: return &EvalChase;
        case BlockType::Twinkle: return &EvalTwinkle;
        case BlockType::Alternating: return &EvalAlternating;
        case BlockType::Strobe: return &EvalStrobe;
        case BlockType::Spin: return &EvalSpin;
        case BlockType::Candle: return &EvalCandle;
        case BlockType::Dissolve: return &EvalDissolve;
        case BlockType::Wave: return &EvalWave;
        case BlockType::Scanner: return &EvalScanner;
        case BlockType::ColorWash: return &EvalColorWash;
        default: return nullptr;
    }
}

} // namespace

bool EvaluateBlockAtAxis(const Block& block,
                         int local_ms,
                         float axis_pos,
                         int twinkle_seed,
                         RGBColor* out_color,
                         float* out_intensity)
{
    if(local_ms < block.start_ms || local_ms >= block.end_ms || block.end_ms <= block.start_ms)
    {
        return false;
    }

    AxisFn fn = AxisFnFor(block.type);
    if(!fn)
    {
        if(BlockNeedsWorldEval(block.type))
        {
            const float axis = std::clamp(axis_pos, 0.0f, 1.0f);
            return EvaluateBlockAtWorld(block, local_ms,
                                        axis, 0.5f, 0.5f,
                                        0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                                        twinkle_seed, out_color, out_intensity);
        }
        // Unknown axis type: keep block color / intensity.
    }

    AxisCtx ctx;
    ctx.block = &block;
    ctx.local_ms = local_ms;
    ctx.axis = std::clamp(axis_pos, 0.0f, 1.0f);
    ctx.progress = BlockProgress(block, local_ms);
    ctx.twinkle_seed = twinkle_seed;
    ctx.intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    ctx.color = block.color;

    if(fn && !fn(ctx))
    {
        return false;
    }

    if(!block.intensity_curve.empty())
    {
        ctx.intensity *= SampleCurve(block.intensity_curve, ctx.progress);
    }

    if(out_color)
    {
        *out_color = ScaleIntensity(ctx.color, ctx.intensity);
    }
    if(out_intensity)
    {
        *out_intensity = ctx.intensity;
    }
    return true;
}

bool EvaluateBlockAtLed(const Block& block,
                        int local_ms,
                        int led_index,
                        int led_count,
                        RGBColor* out_color,
                        float* out_intensity)
{
    led_count = std::max(1, led_count);
    led_index = std::clamp(led_index, 0, led_count - 1);
    const float axis = AxisPos(block.direction, led_index, led_count);
    return EvaluateBlockAtAxis(block, local_ms, axis, led_index, out_color, out_intensity);
}

} // namespace EffectPack
