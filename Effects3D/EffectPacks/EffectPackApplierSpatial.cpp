// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackApplier.h"
#include "EffectPackApplierDetail.h"
#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include "ZoneManager3D.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace EffectPack
{
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

    const Block* top = FindActiveBlock(track, local_ms);
    const bool use_device = top && top->axis_space == AxisSpace::Device;
    const bool use_shared = top && BlockUsesSharedWorldBounds(*top) && shared_bounds && shared_bounds->valid;
    const bool use_sequence = top && BlockUsesSequenceAxis(*top)
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
                on = EvaluateBlockAtAxis(*top, local_ms, axis, seed, &color, &intensity);
            }
            else if(have_bounds && BlockNeedsWorldEval(top->type) && !BlockUsesSequenceAxis(*top))
            {
                on = EvaluateBlockAtWorld(*top, local_ms,
                                          p.x, p.y, p.z,
                                          min_x, max_x, min_y, max_y, min_z, max_z,
                                          seed, &color, &intensity);
            }
            else
            {
                float axis = 0.0f;
                if(have_bounds)
                {
                    if(top->type == BlockType::Spin)
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
                on = EvaluateBlockAtAxis(*top, local_ms, axis, seed, &color, &intensity);
            }
        }
        if(!on)
        {
            color = ToRGBColor(0, 0, 0);
        }
        led->preview_color = color;
        ApplyColorToMappedLed(*led, transform->controller, color, touched);
        if(on)
        {
            ++painted;
        }
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
                               const Block& sample,
                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                               std::vector<float>* out_axes,
                               std::vector<int>* out_seeds,
                               std::vector<float>* out_nx,
                               std::vector<float>* out_ny,
                               std::vector<float>* out_nz,
                               ZoneManager3D* zone_manager)
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
    const bool use_sequence = BlockUsesSequenceAxis(sample);
    const bool use_shared = BlockUsesSharedWorldBounds(sample);
    const bool angular = sample.type == BlockType::Spin && !use_sequence;

    std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
    applier_detail::BuildOrderedSequenceLeds(pack, probe, transforms, zone_manager, &ordered);

    applier_detail::SampleBounds shared_bounds;
    if(use_shared || !use_device)
    {
        for(const auto& pair : ordered)
        {
            applier_detail::ExpandBounds(&shared_bounds, pair.second->world_position);
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

    // Group by transform while preserving ordered LED appearance for nx/ny/nz.
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
        for(int i = 0; i < (int)matching.size(); ++i)
        {
            const Vector3D& p = sample_pos[(size_t)i];
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
            if(out_nx) { out_nx->push_back(flat_norm(p.x, min_x, span_x)); }
            if(out_ny) { out_ny->push_back(flat_norm(p.y, min_y, span_y)); }
            if(out_nz) { out_nz->push_back(flat_norm(p.z, min_z, span_z)); }
            out_seeds->push_back((int)(matching[(size_t)i]->zone_idx * 4096u
                                       + matching[(size_t)i]->led_idx));
        }
    }
}

} // namespace EffectPack
