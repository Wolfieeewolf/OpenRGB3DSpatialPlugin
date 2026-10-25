// SPDX-License-Identifier: GPL-2.0-only

#include "Action.h"
#include "ActionDetail.h"
#include "ActionScript.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace Action
{
using detail::LerpColor;


float ClipProgress(const Clip& clip, int local_ms)
{
    const float dur = (float)std::max(1, clip.end_ms - clip.start_ms);
    float t = (float)(local_ms - clip.start_ms) / dur;
    t = std::clamp(t, 0.0f, 1.0f);
    const float speed = std::max(0.05f, clip.speed);
    const float scaled = t * speed;
    float result = 0.0f;
    if(scaled <= 0.0f)
    {
        result = 0.0f;
    }
    else
    {
        // Keep fractional progress in [0, 1). Mapping near-zero wrap → 1.0 made wipes/chases
        // briefly light the far end at cycle seams (including just after t=0 with float noise).
        const float wrapped = scaled - std::floor(scaled);
        if(t >= 1.0f && wrapped <= 1e-6f)
        {
            // True clip end on an integer cycle: show completed (fully wiped), not restarted.
            result = 1.0f;
        }
        else
        {
            result = wrapped;
        }
    }
    if(clip.reverse)
    {
        result = 1.0f - result;
    }
    return result;
}

float ClipSpanProgress(const Clip& clip, int local_ms)
{
    const float dur = (float)std::max(1, clip.end_ms - clip.start_ms);
    float t = (float)(local_ms - clip.start_ms) / dur;
    t = std::clamp(t, 0.0f, 1.0f);
    if(clip.reverse)
    {
        t = 1.0f - t;
    }
    return t;
}

float ClipSweepProgress(const Clip& clip, int local_ms)
{
    const float dur = (float)std::max(1, clip.end_ms - clip.start_ms);
    float t = (float)(local_ms - clip.start_ms) / dur;
    t = std::clamp(t, 0.0f, 1.0f);
    t = std::clamp(t * std::max(0.05f, clip.speed), 0.0f, 1.0f);
    if(clip.reverse)
    {
        t = 1.0f - t;
    }
    return t;
}

namespace
{
std::mutex g_gpu_anim_mu;
int g_gpu_anim_ms = -1;
} // namespace

void SetGpuAnimMs(int anim_ms)
{
    std::lock_guard<std::mutex> lock(g_gpu_anim_mu);
    g_gpu_anim_ms = anim_ms;
}

int GpuAnimMsOr(int local_ms)
{
    std::lock_guard<std::mutex> lock(g_gpu_anim_mu);
    return (g_gpu_anim_ms >= 0) ? g_gpu_anim_ms : local_ms;
}

float ClipPeriodProgress(const Clip& clip, int local_ms)
{
    // Prefer continuous anim clock so period curves do not hitch when LocalMs wraps.
    const int anim_ms = GpuAnimMsOr(local_ms);
    const int elapsed = std::max(0, anim_ms - clip.start_ms);
    const float speed = std::max(0.05f, clip.speed);
    const float period = std::max(1.0f, (float)std::max(1, clip.period_ms) / speed);
    float phase = std::fmod((float)elapsed, period);
    if(phase < 0.0f)
    {
        phase += period;
    }
    return std::clamp(phase / period, 0.0f, 1.0f);
}

void ApplyClipUvTransform(const Clip& clip, float* u, float* v)
{
    if(!u || !v)
    {
        return;
    }
    float uu = std::clamp(*u, 0.0f, 1.0f);
    float vv = std::clamp(*v, 0.0f, 1.0f);
    int q = clip.rotate_quarters % 4;
    if(q < 0)
    {
        q += 4;
    }
    for(int i = 0; i < q; ++i)
    {
        // 90° CW around (0.5, 0.5): (u,v) → (v, 1-u)
        const float nu = vv;
        const float nv = 1.0f - uu;
        uu = nu;
        vv = nv;
    }
    if(clip.flip_h)
    {
        uu = 1.0f - uu;
    }
    if(clip.flip_v)
    {
        vv = 1.0f - vv;
    }
    *u = uu;
    *v = vv;
}

float ApplyClipAxisMirror(const Clip& clip, float axis)
{
    float a = std::clamp(axis, 0.0f, 1.0f);
    int q = clip.rotate_quarters % 4;
    if(q < 0)
    {
        q += 4;
    }
    // 180° reverses a 1D strip; 90/270 need a V axis (media path handles those).
    if(q == 2)
    {
        a = 1.0f - a;
    }
    if(clip.flip_h)
    {
        a = 1.0f - a;
    }
    return a;
}

void ModulateClipIntensity(const Clip& clip, float clip_progress, float period_progress, float* intensity)
{
    if(!intensity)
    {
        return;
    }
    if(!clip.intensity_curve.empty())
    {
        *intensity *= SampleCurve(clip.intensity_curve, clip_progress);
    }
    if(!clip.period_curve.empty())
    {
        *intensity *= SampleCurve(clip.period_curve, period_progress);
    }
    *intensity = std::clamp(*intensity, 0.0f, 1.0f);
}

Direction OppositeDirection(Direction dir)
{
    switch(dir)
    {
        case Direction::Left: return Direction::Right;
        case Direction::Right: return Direction::Left;
        case Direction::Up: return Direction::Down;
        case Direction::Down: return Direction::Up;
        case Direction::Forward: return Direction::Back;
        case Direction::Back: return Direction::Forward;
        case Direction::PosX: return Direction::NegX;
        case Direction::NegX: return Direction::PosX;
        case Direction::PosY: return Direction::NegY;
        case Direction::NegY: return Direction::PosY;
        case Direction::PosZ: return Direction::NegZ;
        case Direction::NegZ: return Direction::PosZ;
        default:
        {
            const Direction unused = dir;
            (void)unused;
            return Direction::Left;
        }
    }
}

bool DirectionInvertsAxis(Direction dir)
{
    switch(dir)
    {
        /* Named dirs aim at the matching room wall (Forward→Front Z=0, Back→+Z). */
        case Direction::Left:
        case Direction::Down:
        case Direction::Forward:
        case Direction::NegX:
        case Direction::NegY:
        case Direction::NegZ:
            return true;
        case Direction::Right:
        case Direction::Up:
        case Direction::Back:
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

void EnsureClipGradient(Clip* clip)
{
    if(!clip || !clip->gradient.empty())
    {
        return;
    }
    if(EffectColorEnds(ClipFileId(*clip)))
    {
        clip->gradient.push_back({0.0f, clip->color_from});
        clip->gradient.push_back({1.0f, clip->color_to});
        return;
    }
    // Single stop for solid / blink / etc. — alternating treats one colour as colour↔off.
    clip->gradient.push_back({0.0f, clip->color});
}

RGBColor SampleGradient(const Clip& clip, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if(clip.gradient.empty())
    {
        if(EffectColorEnds(ClipFileId(clip)))
        {
            return LerpColor(clip.color_from, clip.color_to, t);
        }
        return clip.color;
    }
    if(clip.gradient.size() == 1)
    {
        return clip.gradient.front().color;
    }
    if(t <= clip.gradient.front().pos)
    {
        return clip.gradient.front().color;
    }
    if(t >= clip.gradient.back().pos)
    {
        return clip.gradient.back().color;
    }
    for(size_t i = 1; i < clip.gradient.size(); ++i)
    {
        const GradientStop& a = clip.gradient[i - 1];
        const GradientStop& b = clip.gradient[i];
        if(t <= b.pos)
        {
            const float span = std::max(1e-6f, b.pos - a.pos);
            return LerpColor(a.color, b.color, (t - a.pos) / span);
        }
    }
    return clip.gradient.back().color;
}

RGBColor SampleGradientStop(const Clip& clip, int index)
{
    Clip sample = clip;
    EnsureClipGradient(&sample);
    if(sample.gradient.empty())
    {
        return clip.color;
    }
    const int n = (int)sample.gradient.size();
    index %= n;
    if(index < 0)
    {
        index += n;
    }
    return sample.gradient[(size_t)index].color;
}

int UniqueGradientStopCount(const Clip& clip)
{
    Clip sample = clip;
    EnsureClipGradient(&sample);
    if(sample.gradient.empty())
    {
        return 1;
    }
    int count = 0;
    for(size_t i = 0; i < sample.gradient.size(); ++i)
    {
        bool seen = false;
        for(size_t j = 0; j < i; ++j)
        {
            if(sample.gradient[j].color == sample.gradient[i].color)
            {
                seen = true;
                break;
            }
        }
        if(!seen)
        {
            ++count;
        }
    }
    return std::max(1, count);
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

const Clip* FindActiveClip(const Track& track, int local_ms)
{
    const Clip* top = nullptr;
    for(const Clip& clip : track.clips)
    {
        if(local_ms >= clip.start_ms && local_ms < clip.end_ms && clip.end_ms > clip.start_ms)
        {
            top = &clip;
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
    const Clip* top = FindActiveClip(track, local_ms);
    if(!top)
    {
        return false;
    }
    return EvaluateClipAtLed(*top, local_ms, led_index, led_count, out_color, out_intensity);
}

} // namespace Action
