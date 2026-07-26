// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"
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

    // Preferred axis first; Up/Down fall back Y→Z→X (never X before Z).
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
    float ang = std::atan2(v, u); // −π..π
    float t = ang / (2.0f * 3.14159265358979323846f) + 0.5f;
    t -= std::floor(t);
    return invert ? (1.0f - t) : t;
}

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

    float intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    RGBColor color = block.color;
    const float axis = std::clamp(axis_pos, 0.0f, 1.0f);
    const float progress = BlockProgress(block, local_ms);

    switch(block.type)
    {
        case BlockType::Solid:
            color = SampleGradient(block, 0.0f);
            break;
        case BlockType::Fade:
        {
            const float t = (float)(local_ms - block.start_ms) / (float)(block.end_ms - block.start_ms);
            color = SampleGradient(block, t);
            break;
        }
        case BlockType::Pulse:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(1, (int)std::lround((float)std::max(1, block.period_ms) / speed));
            const float phase = (float)((local_ms - block.start_ms) % period) / (float)period;
            const float wave = 0.5f - 0.5f * std::cos(phase * 6.28318530718f);
            const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
            const float hi = std::clamp(block.max_intensity, 0.0f, 1.0f);
            intensity *= lo + (hi - lo) * wave;
            color = SampleGradient(block, phase);
            break;
        }
        case BlockType::Wipe:
        {
            const float edge = 0.08f;
            const float front = progress * (1.0f + 2.0f * edge) - edge;
            const float d = front - axis;
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
            intensity *= cover;
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Chase:
        {
            const float head = std::clamp(block.pulse_length, 0.02f, 1.0f);
            float delta = std::fabs(axis - progress);
            delta = std::min(delta, 1.0f - delta);
            if(delta > head)
            {
                return false;
            }
            intensity *= 1.0f - (delta / head);
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Twinkle:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(80, (int)std::lround((float)std::max(80, block.period_ms) / speed));
            const unsigned int h0 = HashLed(twinkle_seed, 0, 1);
            const float phase0 = (float)(h0 & 0xFFFF) / 65535.0f;
            const float density = 0.12f + 0.55f * std::clamp(block.intensity, 0.0f, 1.0f);
            const int local = std::max(0, local_ms - block.start_ms);
            const int epoch = local / period;
            const unsigned int he = HashLed(twinkle_seed ^ 0xA5A5, epoch * period, period);
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
            intensity *= lo + (hi - lo) * flash;
            const RGBColor base = SampleGradient(block, phase0);
            const RGBColor peak = SampleGradient(block, std::min(1.0f, phase0 + 0.35f));
            color = LerpColor(base, peak, flash);
            break;
        }
        case BlockType::Alternating:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(50, (int)std::lround((float)std::max(50, block.period_ms) / speed));
            const int phase_bit = ((local_ms - block.start_ms) / period) & 1;
            const int led_bit = (twinkle_seed ^ (int)std::lround(axis * 1024.0f)) & 1;
            const bool a = (led_bit ^ phase_bit) == 0;
            color = a ? SampleGradient(block, 0.0f) : SampleGradient(block, 1.0f);
            break;
        }
        case BlockType::Strobe:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(40, (int)std::lround((float)std::max(40, block.period_ms) / speed));
            const float phase = (float)((local_ms - block.start_ms) % period) / (float)period;
            const float duty = std::clamp(block.pulse_length, 0.05f, 0.95f);
            if(phase > duty)
            {
                return false;
            }
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Spin:
        {
            const float width = std::clamp(block.pulse_length, 0.04f, 0.55f);
            float delta = axis - progress;
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
            intensity *= cover;
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Candle:
        {
            const unsigned int h = HashLed(twinkle_seed, local_ms / 30, 30);
            const float n1 = (float)(h & 0xFF) / 255.0f;
            const float n2 = (float)((h >> 8) & 0xFF) / 255.0f;
            const float flicker = 0.55f + 0.45f * (0.65f * n1 + 0.35f * n2);
            const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
            const float hi = std::clamp(block.max_intensity, lo, 1.0f);
            intensity *= lo + (hi - lo) * flicker;
            color = SampleGradient(block, 0.15f + 0.7f * n1);
            break;
        }
        case BlockType::Dissolve:
        {
            const unsigned int h = HashLed(twinkle_seed ^ 0x5F3759DF, 0, 1);
            const float threshold = (float)(h & 0xFFFF) / 65535.0f;
            if(progress + 0.001f < threshold)
            {
                return false;
            }
            float cover = 1.0f;
            if(progress < threshold + 0.08f)
            {
                cover = (progress - threshold) / 0.08f;
            }
            intensity *= std::clamp(cover, 0.0f, 1.0f);
            color = SampleGradient(block, threshold);
            break;
        }
        case BlockType::Wave:
        {
            const float cycles = std::max(0.25f, block.speed);
            const float phase = progress * cycles * 6.2831853f;
            const float wave = 0.5f + 0.5f * std::sin((axis * 6.2831853f * std::max(0.5f, block.pulse_length * 4.0f)) - phase);
            intensity *= std::clamp(wave, 0.0f, 1.0f);
            color = SampleGradient(block, axis);
            break;
        }
        case BlockType::Scanner:
        {
            float head = progress * 2.0f;
            if(head > 1.0f)
            {
                head = 2.0f - head;
            }
            if(DirectionInvertsAxis(block.direction))
            {
                head = 1.0f - head;
            }
            const float half_w = std::max(0.02f, block.pulse_length * 0.5f);
            const float dist = std::fabs(axis - head);
            float cover = 0.0f;
            if(dist <= half_w)
            {
                cover = 1.0f - (dist / half_w);
            }
            if(cover <= 0.001f)
            {
                return false;
            }
            intensity *= cover;
            color = SampleGradient(block, head);
            break;
        }
        case BlockType::ColorWash:
        {
            float t = progress + axis * 0.35f;
            t -= std::floor(t);
            color = SampleGradient(block, t);
            break;
        }
        default:
            if(BlockNeedsWorldEval(block.type))
            {
                // Axis-only fallback: treat as a line along X in a unit cube.
                return EvaluateBlockAtWorld(block, local_ms,
                                            axis, 0.5f, 0.5f,
                                            0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                                            twinkle_seed, out_color, out_intensity);
            }
            break;
    }

    if(!block.intensity_curve.empty())
    {
        intensity *= SampleCurve(block.intensity_curve, progress);
    }

    if(out_color)
    {
        *out_color = ScaleIntensity(color, intensity);
    }
    if(out_intensity)
    {
        *out_intensity = intensity;
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
