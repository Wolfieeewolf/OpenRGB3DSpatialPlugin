// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionApplier.h"
#include "ActionScript.h"
#include "ActionApplierDetail.h"
#include "ActionBuiltinGpu.h"
#include "ActionMedia.h"
#include "ActionParticleField.h"
#include "ActionRibbonField.h"
#include "ActionVolumeField.h"
#include "ControllerLayout3D.h"
#include "EffectInstance3D.h"
#include "Geometry3DUtils.h"
#include "SpatialEffect3D.h"
#include "ZoneManager3D.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Action
{

/* ---- Room-grid field mapping (same contract as stack effects) ---------------------- */

static bool ClipIsFieldClip(const Clip& clip)
{
    const std::string id = script::FileId(clip);
    return script::UsesField(id) || script::UsesParticles(id) || script::UsesRibbons(id)
        || (builtin_gpu::Uses(id) && script::FieldShader(id).empty());
}

bool ClipFieldUsesRoomGrid(const Clip& clip, const ApplySpatialContext* ctx)
{
    return ctx && ctx->has_room_grid && ClipUsesRoomBoxBounds(clip)
        && !ClipUsesSequenceAxis(clip) && clip.axis_space != AxisSpace::Device
        && ClipIsFieldClip(clip);
}

Vector3D ResolveClipOriginGrid(const Clip& clip, const ApplySpatialContext& ctx)
{
    Vector3D origin{ctx.anchor_x, ctx.anchor_y, ctx.anchor_z};
    if(!clip.origin_ref.empty())
    {
        for(const NamedWorldPoint& p : ctx.reference_points)
        {
            if(p.name == clip.origin_ref)
            {
                origin = Vector3D{p.x, p.y, p.z};
                break;
            }
        }
    }
    /* Offset = percent of half room per axis, as effect_offset_* in GetEffectOriginGrid. */
    origin.x += (std::clamp(clip.origin_offset_x, -100.0f, 100.0f) / 100.0f) * 0.5f * (ctx.grid_max_x - ctx.grid_min_x);
    origin.y += (std::clamp(clip.origin_offset_y, -100.0f, 100.0f) / 100.0f) * 0.5f * (ctx.grid_max_y - ctx.grid_min_y);
    origin.z += (std::clamp(clip.origin_offset_z, -100.0f, 100.0f) / 100.0f) * 0.5f * (ctx.grid_max_z - ctx.grid_min_z);
    return origin;
}

bool RoomGridFieldUv(const Clip& clip,
                     const ApplySpatialContext& ctx,
                     const Vector3D& clip_origin,
                     const Vector3D& room_position,
                     float* u, float* v, float* w)
{
    const GridContext3D grid(ctx.grid_min_x, ctx.grid_max_x,
                             ctx.grid_min_y, ctx.grid_max_y,
                             ctx.grid_min_z, ctx.grid_max_z);
    /* Clip size plays the role of Scale (occupancy), same clamp as ResolveClipOrigin01. */
    const float scale = std::clamp(clip.effect_size, 0.05f, 2.0f);
    return SampleGpuVolumeOriginLocal01(room_position.x, room_position.y, room_position.z,
                                        grid, clip_origin, scale, u, v, w);
}

void BindClipOrigin(const Clip& clip, const ApplySpatialContext* ctx)
{
    ClipOrigin01 origin;
    if(ctx && ctx->has_world_bounds)
    {
        origin = ResolveClipOrigin01(clip,
                                     ctx->min_x, ctx->max_x,
                                     ctx->min_y, ctx->max_y,
                                     ctx->min_z, ctx->max_z,
                                     ctx->reference_points);
    }
    else
    {
        origin = ResolveClipOrigin01(clip, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, {});
    }
    SetActiveClipOrigin01(origin);

    /* Room-grid field clips already sample origin-local UV (anchor = 0.5, size in the
       half-extents), so the field samplers must not shift or rescale again. */
    ClipOrigin01 field = origin;
    if(ClipFieldUsesRoomGrid(clip, ctx))
    {
        field.ox = field.oy = field.oz = 0.5f;
        field.size = 1.0f;
    }
    volume_field::SetSampleOrigin01(field.ox, field.oy, field.oz, field.size);
    particle_field::SetSampleOrigin01(field.ox, field.oy, field.oz, field.size);
    ribbon_field::SetSampleOrigin01(field.ox, field.oy, field.oz, field.size);
}

namespace applier_detail
{

void ApplyColorToMappedLed(const LEDPosition3D& led,
                           RGBControllerInterface* fallback_controller,
                           RGBColor color,
                           std::unordered_set<RGBControllerInterface*>* touched)
{
    RGBControllerInterface* mapping = led.controller ? led.controller : fallback_controller;
    if(!mapping || !touched)
    {
        return;
    }
    unsigned int global_idx = 0;
    if(!TryGetGlobalLedIndex(mapping, led.zone_idx, led.led_idx, &global_idx))
    {
        return;
    }
    mapping->SetColor(global_idx, color);
    touched->insert(mapping);
}

namespace
{

BlendMode StackBlendFor(ClipBlend blend)
{
    switch(blend)
    {
        case ClipBlend::Add:      return BlendMode::ADD;
        case ClipBlend::Screen:   return BlendMode::SCREEN;
        case ClipBlend::Lighten:  return BlendMode::MAX;
        case ClipBlend::Multiply: return BlendMode::MULTIPLY;
        case ClipBlend::Darken:   return BlendMode::MIN;
        case ClipBlend::Normal:
        default:                  return BlendMode::REPLACE;
    }
}

RGBColor ScaleColor(RGBColor c, float k)
{
    k = std::clamp(k, 0.0f, 1.0f);
    return ToRGBColor((int)std::lround(RGBGetRValue(c) * k),
                      (int)std::lround(RGBGetGValue(c) * k),
                      (int)std::lround(RGBGetBValue(c) * k));
}

/* Add with a highlight roll-off: sums below the knee add exactly; above it they ease
   toward full instead of clipping flat, so stacked hits keep some shape and colour. */
int AddSoftChannel(int a, int b)
{
    constexpr float kKnee = 200.0f;
    const float sum = (float)a + (float)b;
    if(sum <= kKnee)
    {
        return (int)sum;
    }
    const float room = 255.0f - kKnee;
    const float over = sum - kKnee;
    return (int)std::lround(kKnee + room * (1.0f - std::exp(-over / room)));
}

RGBColor AddSoft(RGBColor below, RGBColor top)
{
    return ToRGBColor(AddSoftChannel(RGBGetRValue(below), RGBGetRValue(top)),
                      AddSoftChannel(RGBGetGValue(below), RGBGetGValue(top)),
                      AddSoftChannel(RGBGetBValue(below), RGBGetBValue(top)));
}

RGBColor MaxColor(RGBColor a, RGBColor b)
{
    return ToRGBColor(std::max(RGBGetRValue(a), RGBGetRValue(b)),
                      std::max(RGBGetGValue(a), RGBGetGValue(b)),
                      std::max(RGBGetBValue(a), RGBGetBValue(b)));
}

} // namespace

RGBColor TrailGlowAt(LEDPosition3D& led, int now_ms)
{
    if(led.trail_ms <= 0)
    {
        return ToRGBColor(0, 0, 0);
    }
    const int age = now_ms - led.trail_start_ms;
    /* Fully faded (or a clock reset): drop the trail. */
    if(age < 0 || age >= led.trail_ms)
    {
        led.trail_ms = 0;
        led.trail_color = ToRGBColor(0, 0, 0);
        return led.trail_color;
    }
    /* Squared falloff: bright right behind the head, a long soft tail. */
    const float k = 1.0f - (float)age / (float)led.trail_ms;
    return ScaleColor(led.trail_color, k * k);
}

void PaintClipLed(LEDPosition3D& led,
                  RGBControllerInterface* fallback_controller,
                  const Clip& clip,
                  RGBColor color,
                  int now_ms,
                  std::unordered_set<RGBControllerInterface*>* touched)
{
    if(clip.trail_ms > 0)
    {
        /* Keep whichever is brighter: the fading tail or this frame's colour. */
        const RGBColor glow = MaxColor(TrailGlowAt(led, now_ms), color);
        led.trail_color = glow;
        led.trail_start_ms = now_ms;
        led.trail_ms = std::clamp(clip.trail_ms, 1, kMaxTrailMs);
    }
    RGBColor out = color;
    if(clip.blend == ClipBlend::Add)
    {
        out = AddSoft(led.preview_color, color);
    }
    else if(clip.blend != ClipBlend::Normal)
    {
        out = BlendColors(led.preview_color, color, StackBlendFor(clip.blend));
    }
    led.preview_color = out;
    ApplyColorToMappedLed(led, fallback_controller, out, touched);
}

void ExpandBounds(SampleBounds* b, const Vector3D& p)
{
    if(!b)
    {
        return;
    }
    if(!b->valid)
    {
        b->min_x = b->max_x = p.x;
        b->min_y = b->max_y = p.y;
        b->min_z = b->max_z = p.z;
        b->valid = true;
        return;
    }
    b->min_x = std::min(b->min_x, p.x); b->max_x = std::max(b->max_x, p.x);
    b->min_y = std::min(b->min_y, p.y); b->max_y = std::max(b->max_y, p.y);
    b->min_z = std::min(b->min_z, p.z); b->max_z = std::max(b->max_z, p.z);
}

int PaintTransformTargetSpatial(ControllerTransform* transform,
                                const Track& track,
                                const Clip* clip,
                                int local_ms,
                                std::unordered_set<RGBControllerInterface*>* touched,
                                const SampleBounds* shared_bounds,
                                const std::vector<float>* sequence_axes)
{
    if(!transform)
    {
        return 0;
    }
    if(transform->world_positions_dirty)
    {
        ControllerLayout3D::UpdateWorldPositions(transform);
    }

    std::vector<LEDPosition3D*> matching;
    matching.reserve(transform->led_positions.size());
    for(LEDPosition3D& led : transform->led_positions)
    {
        if(LedMatchesTarget(led, transform->controller, track.target))
        {
            matching.push_back(&led);
        }
    }
    if(matching.empty())
    {
        return 0;
    }

    /* The caller picks the clip: one per audible layer lane, painted bottom to top. */
    const Clip* top = clip;
    const bool use_device = top && top->axis_space == AxisSpace::Device;
    const bool use_shared = top && shared_bounds && shared_bounds->valid && !use_device;
    const bool use_sequence = top && ClipUsesSequenceAxis(*top)
        && sequence_axes && sequence_axes->size() == matching.size();

    std::vector<Vector3D> sample_pos;
    sample_pos.resize(matching.size());
    float min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
    bool have_bounds = false;
    if(use_shared)
    {
        min_x = shared_bounds->min_x; max_x = shared_bounds->max_x;
        min_y = shared_bounds->min_y; max_y = shared_bounds->max_y;
        min_z = shared_bounds->min_z; max_z = shared_bounds->max_z;
        have_bounds = true;
        for(int i = 0; i < (int)matching.size(); ++i)
        {
            sample_pos[(size_t)i] = matching[(size_t)i]->world_position;
        }
    }
    else
    {
        SampleBounds local_bounds;
        for(int i = 0; i < (int)matching.size(); ++i)
        {
            LEDPosition3D* led = matching[(size_t)i];
            if(use_device)
            {
                sample_pos[(size_t)i] = Geometry3D::TransformWorldToLocalScaled(led->world_position, transform->transform);
            }
            else
            {
                sample_pos[(size_t)i] = led->world_position;
            }
            ExpandBounds(&local_bounds, sample_pos[(size_t)i]);
        }
        if(local_bounds.valid)
        {
            min_x = local_bounds.min_x; max_x = local_bounds.max_x;
            min_y = local_bounds.min_y; max_y = local_bounds.max_y;
            min_z = local_bounds.min_z; max_z = local_bounds.max_z;
            have_bounds = true;
        }
    }

    // GPU field engines: volumeMain at each LED. Fail closed — no CPU dual.
    if(top && have_bounds && !use_sequence)
    {
        const std::string effect_id = script::FileId(*top);
        const bool field_clip = script::UsesField(effect_id);
        const bool particle_clip = script::UsesParticles(effect_id);
        const bool ribbon_clip = script::UsesRibbons(effect_id);
        const bool builtin_clip = builtin_gpu::Uses(effect_id)
            && script::FieldShader(effect_id).empty();
        if(field_clip || particle_clip || ribbon_clip || builtin_clip)
        {
            const bool gpu_ok =
                (field_clip && volume_field::IsAvailable())
                || (particle_clip && particle_field::IsAvailable())
                || (ribbon_clip && ribbon_field::IsAvailable())
                || (builtin_clip && builtin_gpu::IsAvailable());
            if(!gpu_ok)
            {
                return 0;
            }

            const int n = (int)matching.size();
            const float span_x = max_x - min_x;
            const float span_y = max_y - min_y;
            const float span_z = max_z - min_z;
            const float diag = std::max(1e-5f, std::sqrt(span_x * span_x + span_y * span_y + span_z * span_z));
            const float eps = diag * 0.02f;
            auto flat_norm = [&](float v, float vmin, float span) -> float {
                if(span <= eps)
                {
                    return 0.5f;
                }
                return std::clamp((v - vmin) / span, 0.0f, 1.0f);
            };

            /* RoomBox field clips map exactly like stack effects: origin-local UV around
               the clip origin (grid units), unlit outside the clip box. Device / track
               bounds keep their own box on purpose. */
            const ApplySpatialContext* room_ctx = use_shared ? shared_bounds->room_ctx : nullptr;
            const bool room_grid = ClipFieldUsesRoomGrid(*top, room_ctx);
            const Vector3D clip_origin = room_grid ? ResolveClipOriginGrid(*top, *room_ctx) : Vector3D{};

            std::vector<float> xyz((size_t)n * 3u);
            std::vector<char> outside((size_t)n, 0);
            for(int i = 0; i < n; ++i)
            {
                float nx = 0.5f, ny = 0.5f, nz = 0.5f;
                if(room_grid)
                {
                    if(!RoomGridFieldUv(*top, *room_ctx, clip_origin, matching[(size_t)i]->room_position,
                                        &nx, &ny, &nz))
                    {
                        outside[(size_t)i] = 1;
                    }
                }
                else
                {
                    const Vector3D& p = sample_pos[(size_t)i];
                    nx = flat_norm(p.x, min_x, span_x);
                    ny = flat_norm(p.y, min_y, span_y);
                    nz = flat_norm(p.z, min_z, span_z);
                }
                ApplyClipUvTransform(*top, &nx, &ny);
                xyz[(size_t)i * 3u + 0u] = nx;
                xyz[(size_t)i * 3u + 1u] = ny;
                xyz[(size_t)i * 3u + 2u] = nz;
            }

            std::vector<float> rgb((size_t)n * 3u, 0.0f);
            bool ok = false;
            if(field_clip)
            {
                ok = volume_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
            }
            else if(particle_clip)
            {
                ok = particle_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
            }
            else if(ribbon_clip)
            {
                ok = ribbon_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
            }
            else
            {
                ok = builtin_gpu::EvaluateAtPoints(xyz.data(), n, rgb.data());
            }
            if(!ok)
            {
                return 0;
            }

            float intensity = std::clamp(top->intensity, 0.0f, 1.0f);
            ModulateClipIntensity(*top, ClipSpanProgress(*top, local_ms),
                                  ClipPeriodProgress(*top, local_ms), &intensity);
            int painted = 0;
            for(int i = 0; i < n; ++i)
            {
                if(outside[(size_t)i])
                {
                    continue;
                }
                const float r = rgb[(size_t)i * 3u + 0u];
                const float g = rgb[(size_t)i * 3u + 1u];
                const float b = rgb[(size_t)i * 3u + 2u];
                const float peak = std::max(r, std::max(g, b));
                if(peak < 0.02f)
                {
                    continue;
                }
                const RGBColor color = ToRGBColor(
                    (int)std::lround(std::clamp(r * intensity, 0.0f, 1.0f) * 255.0f),
                    (int)std::lround(std::clamp(g * intensity, 0.0f, 1.0f) * 255.0f),
                    (int)std::lround(std::clamp(b * intensity, 0.0f, 1.0f) * 255.0f));
                LEDPosition3D* led = matching[(size_t)i];
                PaintClipLed(*led, transform->controller, *top, color, TrailClockMs(), touched);
                ++painted;
            }
            return painted;
        }
    }

    /* Media clips: one GPU UV batch (NEAREST + alpha / column-max). Fail closed — no CPU dual. */
    if(top && have_bounds && !use_sequence && IsMediaClip(*top))
    {
        const int n = (int)matching.size();
        const float sx = max_x - min_x;
        const float sy = max_y - min_y;
        const float sz = max_z - min_z;
        const float diag = std::max(1e-5f, std::sqrt(sx * sx + sy * sy + sz * sz));
        const float eps = diag * 0.02f;
        const float spans[3] = {sx, sy, sz};
        int order[3] = {0, 1, 2};
        if(spans[order[1]] > spans[order[0]]) { std::swap(order[0], order[1]); }
        if(spans[order[2]] > spans[order[1]]) { std::swap(order[1], order[2]); }
        if(spans[order[1]] > spans[order[0]]) { std::swap(order[0], order[1]); }
        int u_axis = order[0];
        int v_axis = order[1];
        if(order[1] == 0)
        {
            u_axis = 0;
            v_axis = order[0];
        }
        const bool collapse_v = (spans[v_axis] <= eps);
        const float span_u = std::max(eps, spans[u_axis]);
        const float span_v = collapse_v ? span_u : std::max(eps, spans[v_axis]);
        const float uv_aspect = span_u / span_v;

        std::vector<float> uvs((size_t)n * 2u);
        std::vector<unsigned char> collapse((size_t)n, collapse_v ? 1 : 0);
        for(int i = 0; i < n; ++i)
        {
            const Vector3D& p = sample_pos[(size_t)i];
            const float coords[3] = {
                (sx > eps) ? std::clamp((p.x - min_x) / sx, 0.0f, 1.0f) : 0.5f,
                (sy > eps) ? std::clamp((p.y - min_y) / sy, 0.0f, 1.0f) : 0.5f,
                (sz > eps) ? std::clamp((p.z - min_z) / sz, 0.0f, 1.0f) : 0.5f
            };
            uvs[(size_t)i * 2u] = coords[u_axis];
            uvs[(size_t)i * 2u + 1u] = collapse_v ? 0.5f : coords[v_axis];
        }

        std::vector<float> rgb((size_t)n * 3u, 0.0f);
        std::vector<unsigned char> lit((size_t)n, 0);
        if(EvaluateMediaAtUvsGpu(*top, local_ms, uvs.data(), collapse.data(), n, uv_aspect,
                                 rgb.data(), lit.data()))
        {
            float intensity = std::clamp(top->intensity, 0.0f, 1.0f);
            ModulateClipIntensity(*top, ClipSpanProgress(*top, local_ms),
                                  ClipPeriodProgress(*top, local_ms), &intensity);
            int painted = 0;
            for(int i = 0; i < n; ++i)
            {
                if(!lit[(size_t)i])
                {
                    continue;
                }
                const RGBColor color = ToRGBColor(
                    (int)std::lround(std::clamp(rgb[(size_t)i * 3u + 0u] * intensity, 0.0f, 1.0f) * 255.0f),
                    (int)std::lround(std::clamp(rgb[(size_t)i * 3u + 1u] * intensity, 0.0f, 1.0f) * 255.0f),
                    (int)std::lround(std::clamp(rgb[(size_t)i * 3u + 2u] * intensity, 0.0f, 1.0f) * 255.0f));
                LEDPosition3D* led = matching[(size_t)i];
                PaintClipLed(*led, transform->controller, *top, color, TrailClockMs(), touched);
                ++painted;
            }
            return painted;
        }
        /* Fail closed — no CPU dual beside the GPU media sampler (same as field clips). */
        return 0;
    }

    /* Script effects: build each LED's inputs exactly as the CPU branch below does,
       then run the translated script for every LED in one GPU draw. Falls through to
       the CPU interpreter if the GPU path is unavailable. */
    if(top && !IsMediaClip(*top) && script::GpuCapable(script::FileId(*top)))
    {
        const std::string effect_id = script::FileId(*top);
        const bool world_script = script::UsesWorld(effect_id);
        std::vector<script::GpuLedInput> inputs(matching.size());
        for(int i = 0; i < (int)matching.size(); ++i)
        {
            LEDPosition3D* led = matching[(size_t)i];
            const int seed = (int)(led->zone_idx * 4096u + led->led_idx);
            const Vector3D& p = sample_pos[(size_t)i];
            script::GpuLedInput& in = inputs[(size_t)i];
            if(use_sequence)
            {
                float axis = (*sequence_axes)[(size_t)i];
                if(DirectionInvertsAxis(top->direction))
                {
                    axis = 1.0f - axis;
                }
                BuildScriptInputAtAxis(*top, axis, seed, &in);
            }
            else if(have_bounds && world_script)
            {
                BuildScriptInputAtWorld(*top, p.x, p.y, p.z, min_x, max_x, min_y, max_y, min_z, max_z, seed, &in);
            }
            else
            {
                float axis = 0.0f;
                if(have_bounds)
                {
                    axis = EffectAxisAngle(ClipFileId(*top))
                        ? SampleSpinAngle(*top, p.x, p.y, p.z, min_x, max_x, min_y, max_y, min_z, max_z)
                        : SampleAxisPos(*top, p.x, p.y, p.z, min_x, max_x, min_y, max_y, min_z, max_z);
                }
                else
                {
                    axis = (matching.size() <= 1) ? 0.0f : (float)i / (float)(matching.size() - 1);
                    if(DirectionInvertsAxis(top->direction))
                    {
                        axis = 1.0f - axis;
                    }
                }
                BuildScriptInputAtAxis(*top, axis, seed, &in);
            }
        }
        std::vector<RGBColor> colors;
        std::vector<unsigned char> lit;
        if(script::RunBatchGpu(effect_id, *top, local_ms, inputs, &colors, &lit))
        {
            int painted = 0;
            for(int i = 0; i < (int)matching.size(); ++i)
            {
                if(!lit[(size_t)i])
                {
                    continue;
                }
                PaintClipLed(*matching[(size_t)i], transform->controller, *top, colors[(size_t)i], TrailClockMs(), touched);
                ++painted;
            }
            return painted;
        }
    }

    int painted = 0;
    for(int i = 0; i < (int)matching.size(); ++i)
    {
        LEDPosition3D* led = matching[(size_t)i];
        RGBColor color = ToRGBColor(0, 0, 0);
        float intensity = 0.0f;
        bool on = false;
        if(top)
        {
            const int seed = (int)(led->zone_idx * 4096u + led->led_idx);
            const Vector3D& p = sample_pos[(size_t)i];
            if(use_sequence)
            {
                float axis = (*sequence_axes)[(size_t)i];
                if(DirectionInvertsAxis(top->direction))
                {
                    axis = 1.0f - axis;
                }
                on = EvaluateClipAtAxis(*top, local_ms, axis, seed, &color, &intensity);
            }
            else if(have_bounds && (IsMediaClip(*top) || script::UsesWorld(script::FileId(*top))))
            {
                on = EvaluateClipAtWorld(*top, local_ms,
                                          p.x, p.y, p.z,
                                          min_x, max_x, min_y, max_y, min_z, max_z,
                                          seed, &color, &intensity);
            }
            else
            {
                float axis = 0.0f;
                if(have_bounds)
                {
                    if(EffectAxisAngle(ClipFileId(*top)))
                    {
                        axis = SampleSpinAngle(*top, p.x, p.y, p.z, min_x, max_x, min_y, max_y, min_z, max_z);
                    }
                    else
                    {
                        axis = SampleAxisPos(*top, p.x, p.y, p.z, min_x, max_x, min_y, max_y, min_z, max_z);
                    }
                }
                else
                {
                    axis = (matching.size() <= 1) ? 0.0f : (float)i / (float)(matching.size() - 1);
                    if(DirectionInvertsAxis(top->direction))
                    {
                        axis = 1.0f - axis;
                    }
                }
                on = EvaluateClipAtAxis(*top, local_ms, axis, seed, &color, &intensity);
            }
        }
        // Off means "this clip does not contribute" — do not write black or later
        // device/zone tracks erase earlier LED-row (and vice versa) paints.
        if(!on)
        {
            continue;
        }
        PaintClipLed(*led, transform->controller, *top, color, TrailClockMs(), touched);
        ++painted;
    }
    return painted;
}

void AppendMatchingLeds(ControllerTransform* transform,
                        const Target& target,
                        std::vector<std::pair<ControllerTransform*, LEDPosition3D*>>* out)
{
    if(!transform || !out)
    {
        return;
    }
    for(LEDPosition3D& led : transform->led_positions)
    {
        if(LedMatchesTarget(led, transform->controller, target))
        {
            out->push_back({transform, &led});
        }
    }
}

void BuildOrderedSequenceLeds(const Pack& pack,
                              const Track& track,
                              std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                              ZoneManager3D* zone_manager,
                              std::vector<std::pair<ControllerTransform*, LEDPosition3D*>>* ordered)
{
    if(!transforms || !ordered)
    {
        return;
    }
    ordered->clear();

    auto append_index = [&](int idx) {
        if(idx < 0 || idx >= (int)transforms->size())
        {
            return;
        }
        ControllerTransform* transform = (*transforms)[(size_t)idx].get();
        if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
        {
            return;
        }
        if(!TrackAppliesToTransform(pack, track, transform, idx, zone_manager))
        {
            return;
        }
        if(transform->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(transform);
        }
        AppendMatchingLeds(transform, track.target, ordered);
    };

    if(track.target.kind == TargetKind::SceneZone && zone_manager)
    {
        Zone3D* zone = zone_manager->GetZoneByName(track.target.scene_zone_name);
        if(zone)
        {
            for(int idx : zone->GetControllers())
            {
                append_index(idx);
            }
            return;
        }
    }

    if(track.target.kind == TargetKind::All && !pack.devices.empty())
    {
        std::vector<bool> used(transforms->size(), false);
        for(const std::string& device : pack.devices)
        {
            for(int i = 0; i < (int)transforms->size(); ++i)
            {
                if(used[(size_t)i])
                {
                    continue;
                }
                ControllerTransform* transform = (*transforms)[(size_t)i].get();
                if(transform && TransformMatchesDevice(transform, device))
                {
                    used[(size_t)i] = true;
                    append_index(i);
                }
            }
        }
        for(int i = 0; i < (int)transforms->size(); ++i)
        {
            if(!used[(size_t)i])
            {
                append_index(i);
            }
        }
        return;
    }

    for(int i = 0; i < (int)transforms->size(); ++i)
    {
        append_index(i);
    }
}

void BuildSequenceAxesMap(const std::vector<std::pair<ControllerTransform*, LEDPosition3D*>>& ordered,
                          SeqAxesMap* out)
{
    if(!out)
    {
        return;
    }
    out->clear();
    const int n = (int)ordered.size();
    for(int i = 0; i < n; ++i)
    {
        ControllerTransform* t = ordered[(size_t)i].first;
        const float axis = (n <= 1) ? 0.0f : (float)i / (float)(n - 1);
        (*out)[t].push_back(axis);
    }
}


} // namespace applier_detail

void BuildSpatialAxesForTarget(const Pack& pack,
                               const Target& target,
                               const Clip& sample,
                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                               std::vector<float>* out_axes,
                               std::vector<int>* out_seeds,
                               std::vector<float>* out_nx,
                               std::vector<float>* out_ny,
                               std::vector<float>* out_nz,
                               ZoneManager3D* zone_manager,
                               const ApplySpatialContext* room_bounds)
{
    if(!out_axes || !out_seeds)
    {
        return;
    }
    out_axes->clear();
    out_seeds->clear();
    if(out_nx) { out_nx->clear(); }
    if(out_ny) { out_ny->clear(); }
    if(out_nz) { out_nz->clear(); }
    if(!transforms)
    {
        return;
    }

    Track probe;
    probe.target = target;
    const bool use_device = sample.axis_space == AxisSpace::Device;
    const bool use_sequence = ClipUsesSequenceAxis(sample);
    const bool use_room_box = ClipUsesRoomBoxBounds(sample);
    const bool angular = EffectAxisAngle(ClipFileId(sample)) && !use_sequence;

    std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
    applier_detail::BuildOrderedSequenceLeds(pack, probe, transforms, zone_manager, &ordered);

    applier_detail::SampleBounds shared_bounds;
    if(use_room_box)
    {
        if(!room_bounds || !room_bounds->has_world_bounds)
        {
            return;
        }
        shared_bounds.min_x = room_bounds->min_x;
        shared_bounds.max_x = room_bounds->max_x;
        shared_bounds.min_y = room_bounds->min_y;
        shared_bounds.max_y = room_bounds->max_y;
        shared_bounds.min_z = room_bounds->min_z;
        shared_bounds.max_z = room_bounds->max_z;
        shared_bounds.valid = true;
    }
    else if(!use_device)
    {
        for(const auto& pair : ordered)
        {
            if(pair.second)
            {
                applier_detail::ExpandBounds(&shared_bounds, pair.second->world_position);
            }
        }
    }

    if(use_sequence)
    {
        const int n = (int)ordered.size();
        for(int i = 0; i < n; ++i)
        {
            float axis = (n <= 1) ? 0.0f : (float)i / (float)(n - 1);
            if(DirectionInvertsAxis(sample.direction))
            {
                axis = 1.0f - axis;
            }
            out_axes->push_back(axis);
            LEDPosition3D* led = ordered[(size_t)i].second;
            out_seeds->push_back((int)(led->zone_idx * 4096u + led->led_idx));
            if(out_nx) { out_nx->push_back(axis); }
            if(out_ny) { out_ny->push_back(0.5f); }
            if(out_nz) { out_nz->push_back(0.5f); }
        }
        return;
    }

    for(int ti = 0; ti < (int)transforms->size(); ++ti)
    {
        ControllerTransform* transform = (*transforms)[(size_t)ti].get();
        if(!TrackAppliesToTransform(pack, probe, transform, ti, zone_manager))
        {
            continue;
        }
        if(transform->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(transform);
        }

        std::vector<LEDPosition3D*> matching;
        matching.reserve(transform->led_positions.size());
        for(LEDPosition3D& led : transform->led_positions)
        {
            if(applier_detail::LedMatchesTarget(led, transform->controller, target))
            {
                matching.push_back(&led);
            }
        }
        if(matching.empty())
        {
            continue;
        }

        std::vector<Vector3D> sample_pos(matching.size());
        applier_detail::SampleBounds bounds;
        for(int i = 0; i < (int)matching.size(); ++i)
        {
            if(use_device)
            {
                sample_pos[(size_t)i] = Geometry3D::TransformWorldToLocalScaled(
                    matching[(size_t)i]->world_position, transform->transform);
                applier_detail::ExpandBounds(&bounds, sample_pos[(size_t)i]);
            }
            else
            {
                sample_pos[(size_t)i] = matching[(size_t)i]->world_position;
            }
        }
        if(!use_device)
        {
            bounds = shared_bounds;
        }
        if(!bounds.valid)
        {
            continue;
        }
        const float min_x = bounds.min_x, max_x = bounds.max_x;
        const float min_y = bounds.min_y, max_y = bounds.max_y;
        const float min_z = bounds.min_z, max_z = bounds.max_z;
        const float span_x = max_x - min_x;
        const float span_y = max_y - min_y;
        const float span_z = max_z - min_z;
        const float diag = std::max(1e-5f, std::sqrt(span_x * span_x + span_y * span_y + span_z * span_z));
        const float eps = diag * 0.02f;
        auto flat_norm = [&](float v, float vmin, float span) -> float {
            if(span <= eps)
            {
                return 0.5f;
            }
            return std::clamp((v - vmin) / span, 0.0f, 1.0f);
        };
        /* Same field mapping as PaintTransformTargetSpatial so thumbnails match the LEDs. */
        const bool room_grid = ClipFieldUsesRoomGrid(sample, room_bounds);
        const Vector3D clip_origin = room_grid ? ResolveClipOriginGrid(sample, *room_bounds) : Vector3D{};
        for(int i = 0; i < (int)matching.size(); ++i)
        {
            const Vector3D& p = sample_pos[(size_t)i];
            if(room_grid)
            {
                float u = 0.5f, v = 0.5f, w = 0.5f;
                if(!RoomGridFieldUv(sample, *room_bounds, clip_origin, matching[(size_t)i]->room_position,
                                    &u, &v, &w))
                {
                    /* Outside the clip box: thumbnails have no unlit mask, so pin to the
                       nearest face (the LED itself stays dark in PaintTransformTargetSpatial). */
                    const GridContext3D grid(room_bounds->grid_min_x, room_bounds->grid_max_x,
                                             room_bounds->grid_min_y, room_bounds->grid_max_y,
                                             room_bounds->grid_min_z, room_bounds->grid_max_z);
                    const EffectGridAxisHalfExtents e = MakeEffectGpuAtlasHalfExtents(
                        grid, clip_origin, std::clamp(sample.effect_size, 0.05f, 2.0f));
                    const Vector3D& rp = matching[(size_t)i]->room_position;
                    EffectGpuAtlasUvFromGrid(rp.x, rp.y, rp.z, clip_origin, e, &u, &v, &w);
                    u = std::clamp(u, 0.0f, 1.0f);
                    v = std::clamp(v, 0.0f, 1.0f);
                    w = std::clamp(w, 0.0f, 1.0f);
                }
                if(out_nx) { out_nx->push_back(u); }
                if(out_ny) { out_ny->push_back(v); }
                if(out_nz) { out_nz->push_back(w); }
            }
            if(angular)
            {
                out_axes->push_back(SampleSpinAngle(sample, p.x, p.y, p.z,
                                                    min_x, max_x, min_y, max_y, min_z, max_z));
            }
            else
            {
                out_axes->push_back(SampleAxisPos(sample, p.x, p.y, p.z,
                                                  min_x, max_x, min_y, max_y, min_z, max_z));
            }
            if(!room_grid)
            {
                if(out_nx) { out_nx->push_back(flat_norm(p.x, min_x, span_x)); }
                if(out_ny) { out_ny->push_back(flat_norm(p.y, min_y, span_y)); }
                if(out_nz) { out_nz->push_back(flat_norm(p.z, min_z, span_z)); }
            }
            out_seeds->push_back((int)(matching[(size_t)i]->zone_idx * 4096u
                                       + matching[(size_t)i]->led_idx));
        }
    }
}

} // namespace Action
