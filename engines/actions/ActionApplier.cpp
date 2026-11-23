// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionApplier.h"
#include "ActionBuiltinGpu.h"
#include "ActionMedia.h"
#include "ActionParticleField.h"
#include "ActionRibbonField.h"
#include "ActionScript.h"
#include "ActionVolumeField.h"
#include "ActionApplierDetail.h"
#include "ZoneManager3D.h"
#include "GpuRoomGridOverlay.h"
#include "SpatialEffect3D.h"
#include "SpatialOffscreenGlPool.h"
#include "EffectInstance3D.h"

#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Action
{

namespace applier_detail
{

void ApplyToControllerAll(RGBControllerInterface* c, RGBColor color, std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c)
    {
        return;
    }
    c->SetAllColors(color);
    touched->insert(c);
}

void ApplyToZone(RGBControllerInterface* c, int zone, RGBColor color, std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c || zone < 0 || (unsigned int)zone >= c->GetZoneCount())
    {
        return;
    }
    c->SetAllZoneColors(zone, color);
    touched->insert(c);
}

void ApplyToLeds(RGBControllerInterface* c, const std::vector<int>& indices, RGBColor color,
                 std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c)
    {
        return;
    }
    const unsigned int led_count = c->GetLEDCount();
    bool any = false;
    for(int idx : indices)
    {
        if(idx < 0 || (unsigned int)idx >= led_count)
        {
            continue;
        }
        c->SetColor((unsigned int)idx, color);
        any = true;
    }
    if(any)
    {
        touched->insert(c);
    }
}

} // namespace applier_detail

void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers)
{
    for(RGBControllerInterface* c : controllers)
    {
        if(!c)
        {
            continue;
        }
        try
        {
            c->SetCustomMode();
        }
        catch(...)
        {
        }
    }
}

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                          bool force_hw_update,
                          ZoneManager3D* zone_manager)
{
    return ApplyPackFrame(pack, local_ms, controllers, transforms, force_hw_update, zone_manager, nullptr);
}

namespace
{

bool LayerZoneIncludesTransform(int layer_zone_index,
                                int transform_index,
                                ZoneManager3D* zone_manager)
{
    if(layer_zone_index == -1)
    {
        return true;
    }
    if(layer_zone_index <= -1000)
    {
        return transform_index == -(layer_zone_index + 1000);
    }
    if(zone_manager && layer_zone_index >= 0)
    {
        if(Zone3D* zone = zone_manager->GetZone(layer_zone_index))
        {
            return zone->ContainsController(transform_index);
        }
        return false;
    }
    return true;
}

/** Overlay layer for one RoomBox field clip: the GPU field it just painted with, framed
    like RoomGridFieldUv (clip origin + MakeEffectGpuAtlasHalfExtents, clip size = Scale). */
void AppendClipFieldOverlayLayer(const Clip& clip,
                                 int local_ms,
                                 const ApplySpatialContext& ctx,
                                 std::vector<GpuRoomGridOverlayLayer>* out)
{
    const std::string id = script::FileId(clip);
    unsigned int src_tex = 0;
    int n = 0;
    bool ok_gpu = false;
    /* Same engine priority as PaintTransformTargetSpatial. Prefer overlay handoff. */
    if(script::UsesField(id))
    {
        ok_gpu = volume_field::ExportAtlasGpu(src_tex, n);
    }
    else if(script::UsesParticles(id))
    {
        ok_gpu = particle_field::ExportAtlasGpu(src_tex, n);
    }
    else if(script::UsesRibbons(id))
    {
        ok_gpu = ribbon_field::ExportAtlasGpu(src_tex, n);
    }
    else if(builtin_gpu::Uses(id) && script::FieldShader(id).empty())
    {
        ok_gpu = builtin_gpu::ExportAtlasGpu(src_tex, n);
    }

    GpuRoomGridOverlayLayer layer;
    layer.kind = GpuRoomGridOverlayLayer::KindVolumeAtlas;
    if(ok_gpu && src_tex != 0 && n >= 2)
    {
        layer.pool_src_tex = src_tex;
        layer.pool_src_w = n;
        layer.pool_src_h = n * n;
        layer.atlas_n = n;
    }
    else if(SpatialOffscreenGlPool::hostContextReady())
    {
        return; /* pool warm — fail closed, no CPU dual beside GPU handoff */
    }
    else
    {
        std::vector<unsigned char> rgba;
        bool ok = false;
        if(script::UsesField(id))
        {
            ok = volume_field::ExportAtlasRgba8(rgba, n);
        }
        else if(script::UsesParticles(id))
        {
            ok = particle_field::ExportAtlasRgba8(rgba, n);
        }
        else if(script::UsesRibbons(id))
        {
            ok = ribbon_field::ExportAtlasRgba8(rgba, n);
        }
        else if(builtin_gpu::Uses(id) && script::FieldShader(id).empty())
        {
            ok = builtin_gpu::ExportAtlasRgba8(rgba, n);
        }
        if(!ok || n < 2 || rgba.size() < (size_t)n * (size_t)n * (size_t)n * 4u)
        {
            return;
        }
        layer.atlas_rgba = std::move(rgba);
        layer.atlas_n = n;
    }
    layer.finish_mode = GpuRoomGridOverlayLayer::FinishRgb; /* same export + texel order as FolderVolume finish_rgb */
    layer.blend_mode = (int)BlendMode::MAX;                  /* later tracks only overwrite lit LEDs */
    layer.sample_room = false;
    layer.volume_tools = false;
    layer.volume_shape = 0;

    const Vector3D origin = ResolveClipOriginGrid(clip, ctx);
    const GridContext3D grid(ctx.grid_min_x, ctx.grid_max_x,
                             ctx.grid_min_y, ctx.grid_max_y,
                             ctx.grid_min_z, ctx.grid_max_z);
    const EffectGridAxisHalfExtents he =
        MakeEffectGpuAtlasHalfExtents(grid, origin, std::clamp(clip.effect_size, 0.05f, 2.0f));
    layer.origin[0] = origin.x;
    layer.origin[1] = origin.y;
    layer.origin[2] = origin.z;
    layer.half_ext[0] = he.hw;
    layer.half_ext[1] = he.hh;
    layer.half_ext[2] = he.hd;
    layer.room_min[0] = ctx.grid_min_x;
    layer.room_min[1] = ctx.grid_min_y;
    layer.room_min[2] = ctx.grid_min_z;
    layer.room_max[0] = ctx.grid_max_x;
    layer.room_max[1] = ctx.grid_max_y;
    layer.room_max[2] = ctx.grid_max_z;

    float intensity = std::clamp(clip.intensity, 0.0f, 1.0f);
    ModulateClipIntensity(clip, ClipSpanProgress(clip, local_ms), ClipPeriodProgress(clip, local_ms), &intensity);
    layer.brightness = intensity;
    out->push_back(std::move(layer));
}

} // namespace

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                          bool force_hw_update,
                          ZoneManager3D* zone_manager,
                          const ApplySpatialContext* spatial,
                          std::vector<GpuRoomGridOverlayLayer>* overlay_out)
{
    ApplyStats stats;
    std::unordered_set<RGBControllerInterface*> touched;
    const bool use_transforms = transforms && !transforms->empty();
    const RGBColor off = ToRGBColor(0, 0, 0);
    const int layer_zone = spatial ? spatial->layer_zone_index : -1;
    static std::uint64_t s_field_sequence = 0;
    const std::uint64_t field_sequence = ++s_field_sequence;

    if(use_transforms)
    {
        for(int ti = 0; ti < (int)transforms->size(); ++ti)
        {
            ControllerTransform* transform = (*transforms)[(size_t)ti].get();
            if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
            {
                continue;
            }
            if(!LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
            {
                continue;
            }
            for(LEDPosition3D& led : transform->led_positions)
            {
                led.preview_color = off;
            }
        }
    }
    else
    {
        for(RGBControllerInterface* c : controllers)
        {
            if(applier_detail::PackIncludesController(pack, c))
            {
                applier_detail::ApplyToControllerAll(c, off, &touched);
            }
        }
    }

    for(const Track& track : pack.tracks)
    {
        if(use_transforms)
        {
            const Clip* top = FindActiveClip(track, local_ms);
            if(top)
            {
                const std::string effect_id = script::FileId(*top);
                BindClipOrigin(*top, spatial);
                if(script::UsesField(effect_id))
                {
                    volume_field::PrepareForClip(*top, local_ms, field_sequence);
                }
                if(script::UsesParticles(effect_id))
                {
                    particle_field::PrepareForClip(*top, local_ms, field_sequence);
                }
                if(script::UsesRibbons(effect_id))
                {
                    ribbon_field::PrepareForClip(*top, local_ms, field_sequence);
                }
                if(IsMediaClip(*top))
                {
                    PrepareMediaForClip(*top, local_ms, field_sequence);
                }
                if(builtin_gpu::Uses(effect_id))
                {
                    builtin_gpu::PrepareForClip(*top, local_ms, field_sequence);
                }
            }
            applier_detail::SampleBounds shared_bounds;
            applier_detail::SeqAxesMap seq_map;
            const bool want_room_box = top && ClipUsesRoomBoxBounds(*top);
            const bool want_track_union = top && ClipUsesTrackWorldUnion(*top);
            const bool want_sequence = top && ClipUsesSequenceAxis(*top);

            if(want_room_box)
            {
                if(!spatial || !spatial->has_world_bounds)
                {
                    continue;
                }
                shared_bounds.min_x = spatial->min_x;
                shared_bounds.max_x = spatial->max_x;
                shared_bounds.min_y = spatial->min_y;
                shared_bounds.max_y = spatial->max_y;
                shared_bounds.min_z = spatial->min_z;
                shared_bounds.max_z = spatial->max_z;
                shared_bounds.valid = true;
                shared_bounds.room_ctx = spatial;
            }
            else if(want_track_union)
            {
                std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
                applier_detail::BuildOrderedSequenceLeds(pack, track, transforms, zone_manager, &ordered);
                for(const auto& pair : ordered)
                {
                    if(pair.second)
                    {
                        applier_detail::ExpandBounds(&shared_bounds, pair.second->world_position);
                    }
                }
            }
            if(want_sequence)
            {
                std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
                applier_detail::BuildOrderedSequenceLeds(pack, track, transforms, zone_manager, &ordered);
                // Sequence march must match the zone filter used when painting, or visible
                // LEDs get the wrong slice of the 0..1 wipe.
                if(layer_zone != -1)
                {
                    std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> filtered;
                    filtered.reserve(ordered.size());
                    for(const auto& pair : ordered)
                    {
                        int ti = -1;
                        for(int i = 0; i < (int)transforms->size(); ++i)
                        {
                            if((*transforms)[(size_t)i].get() == pair.first)
                            {
                                ti = i;
                                break;
                            }
                        }
                        if(LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
                        {
                            filtered.push_back(pair);
                        }
                    }
                    ordered.swap(filtered);
                }
                applier_detail::BuildSequenceAxesMap(ordered, &seq_map);
            }

            int painted = 0;
            for(int ti = 0; ti < (int)transforms->size(); ++ti)
            {
                ControllerTransform* transform = (*transforms)[(size_t)ti].get();
                if(!LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
                {
                    continue;
                }
                if(!TrackAppliesToTransform(pack, track, transform, ti, zone_manager))
                {
                    continue;
                }
                const std::vector<float>* seq_axes = nullptr;
                auto it = seq_map.find(transform);
                if(it != seq_map.end())
                {
                    seq_axes = &it->second;
                }
                painted += applier_detail::PaintTransformTargetSpatial(transform, track, local_ms, &touched,
                                                       shared_bounds.valid ? &shared_bounds : nullptr,
                                                       seq_axes);
            }
            if(painted > 0)
            {
                ++stats.tracks_applied;
                stats.viewport_leds_painted += painted;
            }
            /* Room-grid overlay: export the field this track just painted with (still the
               prepared program — the next track re-prepares it), framed exactly like the
               LED mapping in RoomGridFieldUv. */
            if(overlay_out && top && spatial && ClipFieldUsesRoomGrid(*top, spatial)
               && (int)overlay_out->size() < GpuRoomGridOverlayLayer::kMaxLayers)
            {
                AppendClipFieldOverlayLayer(*top, local_ms, *spatial, overlay_out);
            }
            continue;
        }

        RGBColor color = ToRGBColor(0, 0, 0);
        float intensity = 0.0f;
        if(!EvaluateTrackColor(track, local_ms, &color, &intensity))
        {
            continue;
        }
        ++stats.tracks_applied;

        switch(track.target.kind)
        {
            case TargetKind::All:
                for(RGBControllerInterface* c : controllers)
                {
                    if(applier_detail::PackIncludesController(pack, c))
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Device:
                for(RGBControllerInterface* c : controllers)
                {
                    if(applier_detail::PackIncludesController(pack, c)
                       && ControllerMatchesDevice(c, track.target.device_name))
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Zone:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!applier_detail::PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    const int zone = FindZoneIndex(c, track.target.zone_name);
                    if(zone >= 0)
                    {
                        applier_detail::ApplyToZone(c, zone, color, &touched);
                    }
                    else if(track.target.zone_name.empty())
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Leds:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!applier_detail::PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    applier_detail::ApplyToLeds(c, track.target.led_indices, color, &touched);
                }
                break;
            case TargetKind::SceneZone:
                break;
            default:
            {
                const TargetKind unused = track.target.kind;
                (void)unused;
                break;
            }
        }
    }

    if(use_transforms)
    {
        for(int ti = 0; ti < (int)transforms->size(); ++ti)
        {
            ControllerTransform* transform = (*transforms)[(size_t)ti].get();
            if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
            {
                continue;
            }
            if(!LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
            {
                continue;
            }
            for(LEDPosition3D& led : transform->led_positions)
            {
                if(led.preview_color != off)
                {
                    continue;
                }
                applier_detail::ApplyColorToMappedLed(led, transform->controller, off, &touched);
            }
        }
    }

    static std::int64_t s_last_hw_ms = 0;
    const std::int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool flush_hw = force_hw_update || (now_ms - s_last_hw_ms) >= 45;
    if(flush_hw)
    {
        s_last_hw_ms = now_ms;
        for(RGBControllerInterface* c : touched)
        {
            try
            {
                c->UpdateLEDs();
            }
            catch(...)
            {
            }
        }
    }
    stats.controllers_touched = (int)touched.size();
    return stats;
}


} // namespace Action
