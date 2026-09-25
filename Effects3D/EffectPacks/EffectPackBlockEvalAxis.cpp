// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"
#include "EffectPackBlockEval.h"
#include "EffectPackDetail.h"
#include "EffectScript.h"

#include <algorithm>
#include <cmath>

namespace EffectPack
{
using detail::ScaleIntensity;
using detail::AxisPos;
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

    if(script::UsesWorld(script::FileId(block)))
    {
        const float axis = std::clamp(axis_pos, 0.0f, 1.0f);
        return EvaluateBlockAtWorld(block, local_ms,
                                    axis, 0.5f, 0.5f,
                                    0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                                    twinkle_seed, out_color, out_intensity);
    }

    script::LedView led;
    led.block = &block;
    led.local_ms = local_ms;
    led.axis = std::clamp(axis_pos, 0.0f, 1.0f);
    led.progress = BlockProgress(block, local_ms);
    led.seed = twinkle_seed;
    led.intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    led.color = block.color;
    if(!script::Run(script::FileId(block), &led))
    {
        return false;
    }
    if(!block.intensity_curve.empty())
    {
        led.intensity *= SampleCurve(block.intensity_curve, led.progress);
    }

    if(out_color)
    {
        *out_color = ScaleIntensity(led.color, led.intensity);
    }
    if(out_intensity)
    {
        *out_intensity = led.intensity;
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
