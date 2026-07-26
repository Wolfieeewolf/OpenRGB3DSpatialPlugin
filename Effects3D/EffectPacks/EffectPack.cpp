// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"
#include "EffectPackDetail.h"

#include <algorithm>
#include <cmath>

namespace EffectPack
{
using detail::LerpColor;


float BlockProgress(const Block& block, int local_ms)
{
    const float dur = (float)std::max(1, block.end_ms - block.start_ms);
    float t = (float)(local_ms - block.start_ms) / dur;
    t = std::clamp(t, 0.0f, 1.0f);
    const float speed = std::max(0.05f, block.speed);
    const float scaled = t * speed;
    if(scaled <= 0.0f)
    {
        return 0.0f;
    }
    const float wrapped = scaled - std::floor(scaled);
    if(wrapped <= 1e-6f)
    {
        return 1.0f;
    }
    return wrapped;
}

bool DirectionInvertsAxis(Direction dir)
{
    switch(dir)
    {
        case Direction::Left:
        case Direction::Down:
        case Direction::Back:
        case Direction::NegX:
        case Direction::NegY:
        case Direction::NegZ:
            return true;
        case Direction::Right:
        case Direction::Up:
        case Direction::Forward:
        case Direction::PosX:
        case Direction::PosY:
        case Direction::PosZ:
            return false;
        default:
        {
            const Direction unused = dir;
            (void)unused;
            return false;
        }
    }
}

/** Preferred room axis: 0=X, 1=Y, 2=Z. */
int DirectionPreferredAxis(Direction dir)
{
    switch(dir)
    {
        case Direction::Left:
        case Direction::Right:
        case Direction::PosX:
        case Direction::NegX:
            return 0;
        case Direction::Up:
        case Direction::Down:
        case Direction::PosY:
        case Direction::NegY:
            return 1;
        case Direction::Forward:
        case Direction::Back:
        case Direction::PosZ:
        case Direction::NegZ:
            return 2;
        default:
        {
            const Direction unused = dir;
            (void)unused;
            return 0;
        }
    }
}

const char* BlockTypeDisplayName(BlockType t)
{
    switch(t)
    {
        case BlockType::Solid: return "Set Level";
        case BlockType::Fade: return "Fade";
        case BlockType::Pulse: return "Pulse";
        case BlockType::Wipe: return "Wipe";
        case BlockType::Chase: return "Chase";
        case BlockType::Twinkle: return "Twinkle";
        case BlockType::ColorWash: return "ColorWash";
        case BlockType::Alternating: return "Alternating";
        case BlockType::Strobe: return "Strobe";
        case BlockType::Spin: return "Spin";
        case BlockType::Candle: return "Candle Flicker";
        case BlockType::Dissolve: return "Dissolve";
        case BlockType::Wave: return "Wave";
        case BlockType::Plasma: return "Plasma";
        case BlockType::Snow: return "Snow";
        case BlockType::Fire: return "Fire";
        case BlockType::Balls: return "Balls";
        case BlockType::Bars: return "Bars";
        case BlockType::Scanner: return "Scanner";
        case BlockType::SphereWipe: return "Sphere Wipe";
        case BlockType::Orbit: return "Orbit";
        case BlockType::Ripple: return "Ripple";
        case BlockType::Meteor: return "Meteor";
        case BlockType::Noise3D: return "Noise 3D";
        case BlockType::Burst: return "Burst";
        default: return "Effect";
    }
}

void EnsureBlockGradient(Block* block)
{
    if(!block || !block->gradient.empty())
    {
        return;
    }
    if(block->type == BlockType::Fade)
    {
        block->gradient.push_back({0.0f, block->color_from});
        block->gradient.push_back({1.0f, block->color_to});
        return;
    }
    block->gradient.push_back({0.0f, block->color});
    block->gradient.push_back({1.0f, block->color});
}

RGBColor SampleGradient(const Block& block, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if(block.gradient.empty())
    {
        if(block.type == BlockType::Fade)
        {
            return LerpColor(block.color_from, block.color_to, t);
        }
        return block.color;
    }
    if(block.gradient.size() == 1)
    {
        return block.gradient.front().color;
    }
    if(t <= block.gradient.front().pos)
    {
        return block.gradient.front().color;
    }
    if(t >= block.gradient.back().pos)
    {
        return block.gradient.back().color;
    }
    for(size_t i = 1; i < block.gradient.size(); ++i)
    {
        const GradientStop& a = block.gradient[i - 1];
        const GradientStop& b = block.gradient[i];
        if(t <= b.pos)
        {
            const float span = std::max(1e-6f, b.pos - a.pos);
            return LerpColor(a.color, b.color, (t - a.pos) / span);
        }
    }
    return block.gradient.back().color;
}

bool EvaluateBlock(const Block& block, int local_ms, RGBColor* out_color, float* out_intensity)
{
    return EvaluateBlockAtLed(block, local_ms, 0, 1, out_color, out_intensity);
}

bool MapPlaybackTime(const Pack& pack, int elapsed_ms, bool event_active, int* out_local_ms)
{
    if(!out_local_ms || pack.duration_ms <= 0)
    {
        return false;
    }
    if(elapsed_ms < 0)
    {
        elapsed_ms = 0;
    }

    switch(pack.loop)
    {
        case LoopMode::Once:
            if(elapsed_ms >= pack.duration_ms)
            {
                return false;
            }
            *out_local_ms = elapsed_ms;
            return true;
        case LoopMode::Forever:
            *out_local_ms = elapsed_ms % pack.duration_ms;
            return true;
        case LoopMode::WhileActive:
            if(!event_active)
            {
                return false;
            }
            *out_local_ms = elapsed_ms % pack.duration_ms;
            return true;
        default:
        {
            const LoopMode unused = pack.loop;
            (void)unused;
            return false;
        }
    }
}

bool EvaluateTrackColor(const Track& track, int local_ms, RGBColor* out_color, float* out_intensity)
{
    return EvaluateTrackColorAtLed(track, local_ms, 0, 1, out_color, out_intensity);
}

const Block* FindActiveBlock(const Track& track, int local_ms)
{
    const Block* top = nullptr;
    for(const Block& block : track.blocks)
    {
        if(local_ms >= block.start_ms && local_ms < block.end_ms && block.end_ms > block.start_ms)
        {
            top = &block;
        }
    }
    return top;
}

bool EvaluateTrackColorAtLed(const Track& track,
                             int local_ms,
                             int led_index,
                             int led_count,
                             RGBColor* out_color,
                             float* out_intensity)
{
    const Block* top = FindActiveBlock(track, local_ms);
    if(!top)
    {
        return false;
    }
    return EvaluateBlockAtLed(*top, local_ms, led_index, led_count, out_color, out_intensity);
}

} // namespace EffectPack
