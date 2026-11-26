// SPDX-License-Identifier: GPL-2.0-or-later

#include "OpenRGB3DSpatialTab.h"
#include "EffectListManager3D.h"
#include "ZoneGrid3D.h"
#include "Minecraft/MinecraftGame.h"
#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "ReactiveInputManager.h"
#include "ReactiveInputTypes.h"
#include "PluginLog.h"
#include "ControllerLayout3D.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "SpatialLighting/EmitterLocalSampling.h"
#include "RoomSpatialLightingUi.h"
#include "VirtualController3D.h"
#include "LEDPosition3D.h"
#include "GridSpaceUtils.h"
#include "ZoneManager3D.h"
#include "SpatialTabLedHelpers.h"
#include "PluginUiUtils.h"
#include "Game/RoomSampleConfigPublisher.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include "GpuRoomGridOverlay.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>

namespace
{
struct EffectSlotGridOverride
{
    bool use_zone_grid = false;
    bool use_anchor_grid = false;
    std::unique_ptr<GridContext3D> room_grid_local;
    std::unique_ptr<GridContext3D> world_grid_local;
};

struct RenderEffectSlot
{
    SpatialEffect3D* effect = nullptr;
    int zone_index = -1;
    BlendMode blend_mode = BlendMode::REPLACE;
};

bool EnsureStackEffectInstanceForOverlay(EffectInstance3D& inst)
{
    if(inst.effect_class_name.empty())
    {
        return false;
    }
    if(inst.effect)
    {
        return true;
    }
    SpatialEffect3D* created = EffectListManager3D::get()->CreateEffect(inst.effect_class_name);
    if(!created)
    {
        return false;
    }
    inst.effect.reset(created);
    if(inst.saved_settings && !inst.saved_settings->empty())
    {
        created->LoadSettings(*inst.saved_settings);
    }
    return true;
}

/** Room-grid GPU overlay (dense atlas preview) — uses same prepared grids as LED eval. */
void RefreshGpuRoomGridOverlay(LEDViewport3D* viewport,
                               const GridBounds& room_bounds,
                               float effect_time,
                               const std::vector<RenderEffectSlot>& overlay_slots,
                               const std::vector<EffectSlotGridOverride>& overlay_overrides,
                               const std::vector<const GridContext3D*>& prepared_grids)
{
    if(!viewport || !viewport->GetShowRoomGridOverlay())
    {
        if(viewport)
        {
            viewport->ClearGpuRoomGridOverlayLayers();
        }
        return;
    }
    /* Viewport freeze: keep the last baked overlay — do NOT clear.
       Do not gate on isVisible(): a momentarily non-visible GL widget still needs
       idle/stop re-bakes so layers are ready when paints resume. */
    if(!viewport->IsViewportPaintingEnabled())
    {
        return;
    }
    if(overlay_slots.empty())
    {
        viewport->ClearRoomGridOverlayBounds();
        viewport->ClearGpuRoomGridOverlayLayers();
        return;
    }

    SpatialLightingSceneProvider::instance()->SetShadingControllerIndex(-1);
    viewport->SetRoomGridOverlayBounds(room_bounds.min_x, room_bounds.max_x,
                                       room_bounds.min_y, room_bounds.max_y,
                                       room_bounds.min_z, room_bounds.max_z);
    /* Real light reads the LEDs' own colours in the viewport: no effect layers to bake. */
    if(viewport->GetRoomGridOverlayMode() == LEDViewport3D::OverlayRealLight)
    {
        viewport->ClearGpuRoomGridOverlayLayers();
        return;
    }

    std::vector<GpuRoomGridOverlayLayer> gpu_layers;
    gpu_layers.reserve(std::min(overlay_slots.size(), (size_t)GpuRoomGridOverlayLayer::kMaxLayers));
    for(size_t effect_idx = 0; effect_idx < overlay_slots.size(); ++effect_idx)
    {
        const RenderEffectSlot& slot = overlay_slots[effect_idx];
        if(!slot.effect || effect_idx >= prepared_grids.size() || !prepared_grids[effect_idx])
        {
            continue;
        }
        const EffectSlotGridOverride& grid_override = overlay_overrides[effect_idx];
        if(slot.effect->UseZoneGrid() && slot.zone_index != -1 && !grid_override.use_zone_grid)
        {
            continue;
        }
        if(gpu_layers.size() >= (size_t)GpuRoomGridOverlayLayer::kMaxLayers)
        {
            break;
        }
        GpuRoomGridOverlayLayer layer;
        /* Soft-fail per layer — one miss must not wipe a good previous overlay. */
        if(!slot.effect->TryFillGpuRoomGridOverlayLayer(layer,
                                                        *prepared_grids[effect_idx],
                                                        effect_time))
        {
            continue;
        }
        layer.blend_mode = (int)slot.blend_mode;
        gpu_layers.push_back(std::move(layer));
    }

    if(!gpu_layers.empty())
    {
        viewport->SetGpuRoomGridOverlayLayers(std::move(gpu_layers));
    }
    /* Empty bake: keep previous owned layers (Stop/idle GL miss). Never Clear here —
       clearing would drop back to the empty-stack guide lattice mid-playback. */
}

std::string PreferReactiveLedName(RGBControllerInterface* ctrl, const LEDPosition3D& led)
{
    if(!ctrl)
    {
        return {};
    }
    auto score = [](const std::string& n) -> int {
        if(n.empty())
        {
            return 0;
        }
        if(n.size() >= 4 && (n.compare(0, 4, "Key:") == 0 || n.compare(0, 4, "key:") == 0))
        {
            return 3;
        }
        return 1;
    };
    auto better = [&score](const std::string& a, const std::string& b) -> std::string {
        return (score(b) > score(a)) ? b : a;
    };
    auto named = [ctrl, &better](unsigned int idx) -> std::string {
        if(idx >= ctrl->GetLEDCount())
        {
            return {};
        }
        return better(ctrl->GetLEDName(idx), ctrl->GetLEDDisplayName(idx));
    };
    std::string as_global;
    std::string as_zone;
    as_global = named(led.led_idx);
    if(led.zone_idx < ctrl->GetZoneCount())
    {
        as_zone = named(ctrl->GetZoneStartIndex(led.zone_idx) + led.led_idx);
    }
    return (score(as_zone) >= score(as_global)) ? as_zone : as_global;
}

void ApplyZoneAnchorMetadata(GridContext3D& grid,
                             ReferenceMode origin_mode,
                             ZoneManager3D* zone_manager,
                             const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                             int zone_index,
                             const std::unordered_set<RGBControllerInterface*>* skip_physical_controllers,
                             bool room_aligned,
                             bool mapped_lights_baked_valid,
                             const Vector3D& mapped_lights_baked)
{
    if(zone_index == -1)
    {
        return;
    }
    if(origin_mode == REF_MODE_LED_CENTROID)
    {
        /* Same fixed Mapped lights hub as the stack (not a live zone chase). */
        if(mapped_lights_baked_valid)
        {
            grid.SetLedCentroid(mapped_lights_baked.x, mapped_lights_baked.y, mapped_lights_baked.z);
        }
        else
        {
            Vector3D centroid{};
            if(TryComputeZoneLedCentroid(zone_manager,
                                         controller_transforms,
                                         zone_index,
                                         skip_physical_controllers,
                                         true,
                                         room_aligned,
                                         &centroid))
            {
                grid.SetLedCentroid(centroid.x, centroid.y, centroid.z);
            }
        }
    }
    else if(origin_mode == REF_MODE_TARGET_ZONE_CENTER)
    {
        Vector3D center{};
        if(TryComputeZoneAnchorCenter(zone_manager,
                                      controller_transforms,
                                      zone_index,
                                      skip_physical_controllers,
                                      true,
                                      room_aligned,
                                      &center))
        {
            grid.SetAnchorOverride(center.x, center.y, center.z);
        }
    }
}

void PopulateEffectSlotGrids(EffectSlotGridOverride& slot_override,
                             SpatialEffect3D* effect,
                             int zone_index,
                             ReferenceMode stack_origin_mode,
                             ZoneManager3D* zone_manager,
                             const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                             const std::unordered_set<RGBControllerInterface*>* skip_physical_controllers,
                             const GridContext3D& room_grid,
                             const GridContext3D& world_grid,
                             uint64_t render_sequence,
                             bool mapped_lights_baked_valid,
                             const Vector3D& mapped_lights_baked)
{
    slot_override = EffectSlotGridOverride{};
    if(!effect)
    {
        return;
    }
    if(effect->UseZoneGrid())
    {
        if(TryMakeZoneGridContextPair(zone_manager,
                                      controller_transforms,
                                      zone_index,
                                      skip_physical_controllers,
                                      true,
                                      room_grid.grid_scale_mm,
                                      world_grid.grid_scale_mm,
                                      slot_override.room_grid_local,
                                      slot_override.world_grid_local))
        {
            slot_override.use_zone_grid = true;
            /* Zone grids bake a live LED average — Mapped lights must stay the
               stack hub (same as room-grid path), not a zone chase. */
            if(stack_origin_mode == REF_MODE_LED_CENTROID)
            {
                ApplyZoneAnchorMetadata(*slot_override.room_grid_local,
                                        stack_origin_mode,
                                        zone_manager,
                                        controller_transforms,
                                        zone_index,
                                        skip_physical_controllers,
                                        true,
                                        mapped_lights_baked_valid,
                                        mapped_lights_baked);
                ApplyZoneAnchorMetadata(*slot_override.world_grid_local,
                                        stack_origin_mode,
                                        zone_manager,
                                        controller_transforms,
                                        zone_index,
                                        skip_physical_controllers,
                                        false,
                                        mapped_lights_baked_valid,
                                        mapped_lights_baked);
            }
        }
    }
    else if(zone_index != -1 &&
            (stack_origin_mode == REF_MODE_LED_CENTROID || stack_origin_mode == REF_MODE_TARGET_ZONE_CENTER))
    {
        slot_override.room_grid_local = std::make_unique<GridContext3D>(
            room_grid.min_x, room_grid.max_x,
            room_grid.min_y, room_grid.max_y,
            room_grid.min_z, room_grid.max_z,
            room_grid.grid_scale_mm);
        slot_override.world_grid_local = std::make_unique<GridContext3D>(
            world_grid.min_x, world_grid.max_x,
            world_grid.min_y, world_grid.max_y,
            world_grid.min_z, world_grid.max_z,
            world_grid.grid_scale_mm);
        ApplyZoneAnchorMetadata(*slot_override.room_grid_local,
                                stack_origin_mode,
                                zone_manager,
                                controller_transforms,
                                zone_index,
                                skip_physical_controllers,
                                true,
                                mapped_lights_baked_valid,
                                mapped_lights_baked);
        ApplyZoneAnchorMetadata(*slot_override.world_grid_local,
                                stack_origin_mode,
                                zone_manager,
                                controller_transforms,
                                zone_index,
                                skip_physical_controllers,
                                false,
                                mapped_lights_baked_valid,
                                mapped_lights_baked);
        slot_override.use_anchor_grid = true;
    }
    if(slot_override.room_grid_local)
    {
        slot_override.room_grid_local->render_sequence = render_sequence;
    }
    if(slot_override.world_grid_local)
    {
        slot_override.world_grid_local->render_sequence = render_sequence;
    }
}

void BuildEffectSlotGridOverrides(std::vector<EffectSlotGridOverride>& out,
                                  const std::vector<RenderEffectSlot>& effect_slots,
                                  ReferenceMode stack_origin_mode,
                                  ZoneManager3D* zone_manager,
                                  const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                                  const std::unordered_set<RGBControllerInterface*>* skip_physical_controllers,
                                  const GridContext3D& room_grid,
                                  const GridContext3D& world_grid,
                                  uint64_t effect_render_sequence,
                                  bool mapped_lights_baked_valid,
                                  const Vector3D& mapped_lights_baked)
{
    out.resize(effect_slots.size());
    for(size_t effect_idx = 0; effect_idx < effect_slots.size(); effect_idx++)
    {
        PopulateEffectSlotGrids(out[effect_idx],
                                effect_slots[effect_idx].effect,
                                effect_slots[effect_idx].zone_index,
                                stack_origin_mode,
                                zone_manager,
                                controller_transforms,
                                skip_physical_controllers,
                                room_grid,
                                world_grid,
                                effect_render_sequence,
                                mapped_lights_baked_valid,
                                mapped_lights_baked);
    }
}

const GridContext3D* ResolveActiveSlotGrid(const EffectSlotGridOverride& slot_override,
                                           bool use_world_bounds)
{
    if(slot_override.use_zone_grid || slot_override.use_anchor_grid)
    {
        return use_world_bounds ? slot_override.world_grid_local.get() : slot_override.room_grid_local.get();
    }
    return nullptr;
}

int PackAmbientShadeSlot(int controller_index, int led_index)
{
    return (controller_index << 16) ^ (led_index & 0xFFFF);
}

}

static float AverageAlongAxis(ControllerTransform* transform,
                              EffectAxis sort_axis,
                              const Vector3D& stack_ref_origin)
{
    if(!transform)
    {
        return 0.0f;
    }

    if(!transform->led_positions.empty())
    {
        if(transform->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(transform);
        }

        double accumulator = 0.0;
        for(size_t led_index = 0; led_index < transform->led_positions.size(); led_index++)
        {
            switch(sort_axis)
            {
                case AXIS_X:
                    accumulator += transform->led_positions[led_index].world_position.x;
                    break;
                case AXIS_Y:
                    accumulator += transform->led_positions[led_index].world_position.y;
                    break;
                case AXIS_Z:
                    accumulator += transform->led_positions[led_index].world_position.z;
                    break;
                case AXIS_RADIAL:
                default:
                {
                    float dx = transform->led_positions[led_index].world_position.x - stack_ref_origin.x;
                    float dy = transform->led_positions[led_index].world_position.y - stack_ref_origin.y;
                    float dz = transform->led_positions[led_index].world_position.z - stack_ref_origin.z;
                    accumulator += sqrtf(dx * dx + dy * dy + dz * dz);
                    break;
                }
            }
        }
        return (float)(accumulator / (double)transform->led_positions.size());
    }

    switch(sort_axis)
    {
        case AXIS_X:
            return transform->transform.position.x;
        case AXIS_Y:
            return transform->transform.position.y;
        case AXIS_Z:
            return transform->transform.position.z;
        case AXIS_RADIAL:
        default:
        {
            float dx = transform->transform.position.x - stack_ref_origin.x;
            float dy = transform->transform.position.y - stack_ref_origin.y;
            float dz = transform->transform.position.z - stack_ref_origin.z;
            return sqrtf(dx * dx + dy * dy + dz * dz);
        }
    }
}

namespace
{

bool EffectSlotAppliesToController(int slot_zone_index, int ctrl_idx, ZoneManager3D* zone_manager)
{
    if(slot_zone_index == -1)
    {
        return true;
    }
    if(slot_zone_index <= -1000)
    {
        const int target_ctrl_idx = -(slot_zone_index + 1000);
        return target_ctrl_idx >= 0 && target_ctrl_idx == ctrl_idx;
    }
    if(zone_manager && slot_zone_index >= 0)
    {
        if(Zone3D* zone = zone_manager->GetZone(slot_zone_index))
        {
            const std::vector<int>& zone_controllers = zone->GetControllers();
            return std::find(zone_controllers.begin(), zone_controllers.end(), ctrl_idx) != zone_controllers.end();
        }
    }
    return false;
}

bool ShouldApplyStackLayerToController(const SpatialEffect3D* effect,
                                       size_t effect_index,
                                       size_t relay_stack_index,
                                       bool has_relay_stack,
                                       int ctrl_idx,
                                       const std::unordered_set<int>* relay_emitter_indices = nullptr)
{
    if(!effect)
    {
        return false;
    }
    const auto is_relay_emitter = [&](int index) -> bool {
        if(relay_emitter_indices)
        {
            return relay_emitter_indices->find(index) != relay_emitter_indices->end();
        }
        return SpatialLightingSceneProvider::instance()->isEmitterController(index);
    };
    if(has_relay_stack)
    {
        if(effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::Direct &&
           !is_relay_emitter(ctrl_idx))
        {
            return false;
        }
        if(effect_index < relay_stack_index && !is_relay_emitter(ctrl_idx))
        {
            return false;
        }
    }
    if(!effect->appliesRoomOutputToController(ctrl_idx))
    {
        return false;
    }
    /* A relay layer paints only its own emitters: its receivers are lit by the relay pass,
       and on another relay's emitters it would only paint black over their pattern. */
    if(effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay
       && !effect->isRoomEmitterController(ctrl_idx))
    {
        return false;
    }
    return true;
}

SpatialLighting::OccluderBuildOptions MergeStackOccluderOptions(const std::vector<RenderEffectSlot>& effect_slots)
{
    SpatialLighting::OccluderBuildOptions merged{};
    for(const RenderEffectSlot& effect_slot : effect_slots)
    {
        if(!effect_slot.effect)
        {
            continue;
        }
        const RoomSpatialLightingUi::RoomSpatialLightParams& params = effect_slot.effect->roomRelayParams();
        if(effect_slot.effect->GetRoomOutputRole() != SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
        {
            continue;
        }
        if(!params.use_occlusion)
        {
            continue;
        }
        const SpatialLighting::OccluderBuildOptions layer_opts =
            RoomSpatialLightingUi::BuildOccluderOptions(params);
        merged.display_planes |= layer_opts.display_planes;
        merged.scene_props |= layer_opts.scene_props;
        merged.room_walls |= layer_opts.room_walls;
        merged.controllers |= layer_opts.controllers;
        merged.light_blockers |= layer_opts.light_blockers;
    }
    return merged;
}

const SpatialEffect3D* ResolveAmbientShadeSource(const std::vector<RenderEffectSlot>& effect_slots,
                                                 size_t relay_stack_index,
                                                 bool has_relay_stack,
                                                 int ctrl_idx,
                                                 ZoneManager3D* zone_manager)
{
    for(size_t effect_idx = effect_slots.size(); effect_idx-- > 0;)
    {
        const RenderEffectSlot& effect_slot = effect_slots[effect_idx];
        SpatialEffect3D* effect = effect_slot.effect;
        if(!effect)
        {
            continue;
        }
        if(effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
        {
            continue;
        }
        if(!effect->roomRelayParams().use_occlusion)
        {
            continue;
        }
        if(!EffectSlotAppliesToController(effect_slot.zone_index, ctrl_idx, zone_manager))
        {
            continue;
        }
        if(!ShouldApplyStackLayerToController(effect,
                                              effect_idx,
                                              relay_stack_index,
                                              has_relay_stack,
                                              ctrl_idx))
        {
            continue;
        }
        return effect;
    }
    return nullptr;
}

RGBColor SamplePatternOnEmitterCanvas(SpatialEffect3D* effect,
                                      float room_x,
                                      float room_y,
                                      float room_z,
                                      float time,
                                      const EmitterLocalSampling::CombinedEmitterCanvas& canvas)
{
    if(!effect || !canvas.valid || !canvas.grid)
    {
        return 0x00000000;
    }
    return effect->EvaluateColorGrid(room_x, room_y, room_z, time, *canvas.grid);
}

RGBColor SampleStackLayerColor(SpatialEffect3D* effect,
                               float x,
                               float y,
                               float z,
                               float time,
                               const GridContext3D& grid)
{
    if(!effect)
    {
        return 0x00000000;
    }
    return effect->EvaluateColorGrid(x, y, z, time, grid);
}

} // namespace

static bool TryGetGlobalLedIndex(RGBControllerInterface* controller,
                                 unsigned int zone_idx,
                                 unsigned int led_idx,
                                 unsigned int* global_idx)
{
    if(!controller || !global_idx)
    {
        return false;
    }
    if(zone_idx >= controller->GetZoneCount())
    {
        return false;
    }
    if(led_idx >= controller->GetZoneLEDsCount(zone_idx))
    {
        return false;
    }

    *global_idx = controller->GetZoneStartIndex(zone_idx) + led_idx;
    return (*global_idx < controller->GetLEDCount());
}

void OpenRGB3DSpatialTab::RefreshRoomGridOverlayPreview()
{
    if(!viewport)
    {
        return;
    }
    if(!viewport->GetShowRoomGridOverlay())
    {
        viewport->ClearRoomGridOverlayBounds();
        viewport->ClearGpuRoomGridOverlayLayers();
        return;
    }
    /* Tab/viewport freeze: leave frozen layers in place for the next paint. */
    if(!viewport->IsViewportPaintingEnabled())
    {
        return;
    }

    ManualRoomSettings room_settings = MakeManualRoomSettings(true,
                                                              manual_room_width,
                                                              manual_room_height,
                                                              manual_room_depth);
    GridBounds world_bounds = ComputeGridBounds(room_settings, grid_scale_mm, controller_transforms);
    const GridBounds& room_bounds = world_bounds;
    /* Always publish room bounds so the sample-lattice guide draws with an empty stack. */
    viewport->SetRoomGridOverlayBounds(room_bounds.min_x, room_bounds.max_x,
                                       room_bounds.min_y, room_bounds.max_y,
                                       room_bounds.min_z, room_bounds.max_z);

    GridContext3D world_grid(world_bounds.min_x, world_bounds.max_x,
                             world_bounds.min_y, world_bounds.max_y,
                             world_bounds.min_z, world_bounds.max_z,
                             grid_scale_mm);
    GridContext3D room_grid(room_bounds.min_x, room_bounds.max_x,
                            room_bounds.min_y, room_bounds.max_y,
                            room_bounds.min_z, room_bounds.max_z,
                            grid_scale_mm);
    ApplyMappedLightsAnchorToGrid(room_grid);
    ApplyMappedLightsAnchorToGrid(world_grid);

    ReferenceMode stack_origin_mode = REF_MODE_ROOM_CENTER;
    if(effectOriginCombo() && effectOriginCombo()->currentIndex() >= 0)
    {
        const int ref_idx = effectOriginCombo()->itemData(effectOriginCombo()->currentIndex()).toInt();
        if(ref_idx == -2)
        {
            stack_origin_mode = REF_MODE_TARGET_ZONE_CENTER;
        }
        else if(ref_idx == -3)
        {
            stack_origin_mode = REF_MODE_WORLD_ORIGIN;
        }
        else if(ref_idx == -4)
        {
            stack_origin_mode = REF_MODE_LED_CENTROID;
        }
        else if(ref_idx == -1)
        {
            stack_origin_mode = REF_MODE_ROOM_CENTER;
        }
        else if(ref_idx >= 0)
        {
            stack_origin_mode = REF_MODE_CUSTOM_POINT;
        }
    }

    std::vector<RenderEffectSlot> overlay_slots;
    overlay_slots.reserve(effect_stack.size() + 1);
    for(const std::unique_ptr<EffectInstance3D>& ptr : effect_stack)
    {
        EffectInstance3D* inst = ptr.get();
        if(!inst || !inst->enabled || inst->effect_class_name.empty())
        {
            continue;
        }
        if(!EnsureStackEffectInstanceForOverlay(*inst) || !inst->effect)
        {
            continue;
        }
        ApplyStackOriginComboToEffect(inst->effect.get());
        inst->effect->SetVolumeToolsUnlocked(inst->volume_tools_unlocked);
        RenderEffectSlot slot{};
        slot.effect = inst->effect.get();
        slot.zone_index = inst->zone_index;
        slot.blend_mode = inst->blend_mode;
        overlay_slots.push_back(slot);
    }
    /* Match RenderEffectStack: solo UI effect (no stack rows) still previews. */
    if(overlay_slots.empty() && current_effect_ui)
    {
        RenderEffectSlot slot{};
        slot.effect = current_effect_ui;
        slot.zone_index = ResolveZoneTargetSelection(effectZoneCombo());
        slot.blend_mode = BlendMode::REPLACE;
        ApplyStackOriginComboToEffect(current_effect_ui);
        overlay_slots.push_back(slot);
    }
    if(overlay_slots.empty())
    {
        /* No effect to shade — keep bounds, drop atlas layers so Draw shows the guide lattice. */
        viewport->ClearGpuRoomGridOverlayLayers();
        viewport->update();
        return;
    }

    std::unordered_set<RGBControllerInterface*> managed;
    for(const std::unique_ptr<ControllerTransform>& t : controller_transforms)
    {
        if(!t || !t->virtual_controller)
        {
            continue;
        }
        for(const GridLEDMapping& m : t->virtual_controller->GetMappings())
        {
            if(m.controller)
            {
                managed.insert(m.controller);
            }
        }
    }

    static uint64_t s_overlay_preview_seq = 0;
    const uint64_t seq = ++s_overlay_preview_seq;
    std::vector<EffectSlotGridOverride> overrides;
    BuildEffectSlotGridOverrides(overrides,
                                 overlay_slots,
                                 stack_origin_mode,
                                 zone_manager.get(),
                                 controller_transforms,
                                 &managed,
                                 room_grid,
                                 world_grid,
                                 seq,
                                 mapped_lights_anchor_valid_,
                                 mapped_lights_anchor_);

    std::vector<const GridContext3D*> prepared(overlay_slots.size(), nullptr);
    for(size_t i = 0; i < overlay_slots.size(); ++i)
    {
        SpatialEffect3D* eff = overlay_slots[i].effect;
        if(!eff)
        {
            continue;
        }
        const EffectSlotGridOverride& go = overrides[i];
        const bool use_world = eff->UseWorldGridBounds();
        const GridContext3D& global = use_world ? world_grid : room_grid;
        const GridContext3D* local = ResolveActiveSlotGrid(go, use_world);
        const GridContext3D& active = local ? *local : global;
        prepared[i] = &active;
        eff->PrepareGpuFields(seq, effect_time, active);
        eff->PrepareStripColormapAssist(seq, effect_time);
    }

    RefreshGpuRoomGridOverlay(viewport, room_bounds, effect_time, overlay_slots, overrides, prepared);
    viewport->update();
}

void OpenRGB3DSpatialTab::RenderEffectStack()
{
    MinecraftGame::ClearRenderSampleIndexContext();
    SyncReactiveInputSession();

    if(controller_transforms.empty())
    {
        return;
    }

    SpatialLightingSceneProvider::instance()->SetControllers(&controller_transforms);
    SpatialLightingSceneProvider::instance()->SetShadingControllerIndex(-1);

    static uint64_t s_effect_render_sequence = 0;
    const uint64_t effect_render_sequence = ++s_effect_render_sequence;

    ManualRoomSettings room_settings = MakeManualRoomSettings(true,
                                                              manual_room_width,
                                                              manual_room_height,
                                                              manual_room_depth);
    // room_position equals world_position today (Documentation/PluginSpatialMeasurement.md sec 4), so one bounds/centroid pass.
    GridBounds world_bounds = ComputeGridBounds(room_settings, grid_scale_mm, controller_transforms);
    const GridBounds& room_bounds = world_bounds;

    GridContext3D world_grid(world_bounds.min_x, world_bounds.max_x,
                             world_bounds.min_y, world_bounds.max_y,
                             world_bounds.min_z, world_bounds.max_z,
                             grid_scale_mm);
    GridContext3D room_grid(room_bounds.min_x, room_bounds.max_x,
                            room_bounds.min_y, room_bounds.max_y,
                            room_bounds.min_z, room_bounds.max_z,
                            grid_scale_mm);
    world_grid.render_sequence = effect_render_sequence;
    room_grid.render_sequence = effect_render_sequence;

    /* Mapped lights uses a baked hub (RefreshMappedLightsAnchor), not a live chase. */
    ApplyMappedLightsAnchorToGrid(room_grid);
    ApplyMappedLightsAnchorToGrid(world_grid);

    // LED-first Room Ambilight: publish against the global room grid + all layout LED room positions.
    // (Effect-local zone grids must not be used for room bounds - that mapped LEDs to the wrong side.)
    {
        RoomSampleConfigPublisher::SetPublishRoomGrid(room_grid);
        std::vector<float> led_xyz;
        led_xyz.reserve(4096);
        for(const std::unique_ptr<ControllerTransform>& transform : controller_transforms)
        {
            if(!transform || transform->hidden_by_virtual)
            {
                continue;
            }
            ControllerLayout3D::UpdateWorldPositions(transform.get());
            for(const LEDPosition3D& led : transform->led_positions)
            {
                led_xyz.push_back(led.room_position.x);
                led_xyz.push_back(led.room_position.y);
                led_xyz.push_back(led.room_position.z);
            }
        }
        RoomSampleConfigPublisher::SetFrameLedRoomPositions(
            led_xyz.empty() ? nullptr : led_xyz.data(),
            led_xyz.size() / 3u);
    }

    ReferenceMode stack_origin_mode = REF_MODE_USER_POSITION;
    Vector3D stack_ref_origin = {room_grid.center_x, room_grid.center_y, room_grid.center_z};
    if(effectOriginCombo())
    {
        int origin_index = effectOriginCombo()->currentIndex();
        if(origin_index >= 0)
        {
            int ref_idx = effectOriginCombo()->itemData(origin_index).toInt();
            if(ref_idx == -2)
            {
                stack_origin_mode = REF_MODE_TARGET_ZONE_CENTER;
            }
            else if(ref_idx == -3)
            {
                stack_origin_mode = REF_MODE_WORLD_ORIGIN;
            }
            else if(ref_idx == -1)
            {
                stack_origin_mode = REF_MODE_ROOM_CENTER;
            }
            else if(ref_idx == -4)
            {
                stack_origin_mode = REF_MODE_LED_CENTROID;
            }
            else if(ref_idx >= 0 && ref_idx < (int)reference_points.size() && reference_points[ref_idx])
            {
                stack_origin_mode = REF_MODE_CUSTOM_POINT;
                stack_ref_origin = reference_points[ref_idx]->GetPosition();
            }
        }
    }

    std::vector<RenderEffectSlot> active_effects;
    active_effects.reserve(effect_stack.size());

    for(const std::unique_ptr<EffectInstance3D>& instance_ptr : effect_stack)
    {
        EffectInstance3D* instance = instance_ptr.get();
        if(!instance || !instance->enabled || !instance->effect)
        {
            continue;
        }

        RenderEffectSlot slot;
        slot.effect = instance->effect.get();
        slot.zone_index = instance->zone_index;
        slot.blend_mode = instance->blend_mode;
        active_effects.push_back(slot);
    }

    if(active_effects.empty() && current_effect_ui && effect_running)
    {
        RenderEffectSlot slot;
        slot.effect = current_effect_ui;
        slot.zone_index = ResolveZoneTargetSelection(effectZoneCombo());
        slot.blend_mode = BlendMode::REPLACE;
        if(slot.effect)
        {
            active_effects.push_back(slot);
        }
    }

    if(active_effects.empty())
    {
        const RGBColor black = 0x00000000;
        std::unordered_set<RGBControllerInterface*> controllers_to_update;
        std::unordered_set<RGBControllerInterface*> controllers_managed_by_virtuals;
        for(const std::unique_ptr<ControllerTransform>& t : controller_transforms)
        {
            if(!t || !t->virtual_controller) continue;
            for(const GridLEDMapping& m : t->virtual_controller->GetMappings())
            {
                if(m.controller) controllers_managed_by_virtuals.insert(m.controller);
            }
        }

        for(const std::unique_ptr<ControllerTransform>& transform_ptr : controller_transforms)
        {
            ControllerTransform* transform = transform_ptr.get();
            if(!transform) continue;
            if(transform->hidden_by_virtual) continue;
            if(transform->controller &&
               controllers_managed_by_virtuals.find(transform->controller) != controllers_managed_by_virtuals.end())
                continue;

            if(transform->virtual_controller && !transform->controller)
            {
                for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); ++led_pos_idx)
                {
                    LEDPosition3D& led_position = transform->led_positions[led_pos_idx];
                    led_position.preview_color = black;
                    RGBControllerInterface* mapping_controller = led_position.controller;
                    if(!mapping_controller || mapping_controller->GetZoneCount() == 0 || mapping_controller->GetLEDCount() == 0)
                    {
                        continue;
                    }
                    if(led_position.zone_idx < mapping_controller->GetZoneCount())
                    {
                        unsigned int led_global_idx = 0;
                        if(TryGetGlobalLedIndex(mapping_controller,
                                                led_position.zone_idx,
                                                led_position.led_idx,
                                                &led_global_idx))
                        {
                            mapping_controller->SetColor(led_global_idx, black);
                            controllers_to_update.insert(mapping_controller);
                        }
                    }
                }
                continue;
            }

            RGBControllerInterface* controller = transform->controller;
            if(!controller || controller->GetZoneCount() == 0 || controller->GetLEDCount() == 0) continue;

            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
            {
                LEDPosition3D& led_position = transform->led_positions[led_pos_idx];
                led_position.preview_color = black;
                unsigned int led_global_idx = 0;
                if(TryGetGlobalLedIndex(controller, led_position.zone_idx, led_position.led_idx, &led_global_idx))
                    controller->SetColor(led_global_idx, black);
            }
            controllers_to_update.insert(controller);
        }

        for(RGBControllerInterface* ctrl : controllers_to_update)
        {
            if(ctrl) ctrl->UpdateLEDs();
        }

        if(viewport)
        {
            viewport->UpdateColors();
            RefreshRoomGridOverlayPreview();
        }
        return;
    }

    struct RenderTickSnapshotGuard
    {
        ScreenCaptureManager& mgr;
        explicit RenderTickSnapshotGuard(ScreenCaptureManager& m)
            : mgr(m)
        {
            mgr.BeginRenderTickSnapshot();
        }
        ~RenderTickSnapshotGuard()
        {
            mgr.EndRenderTickSnapshot();
        }
        RenderTickSnapshotGuard(const RenderTickSnapshotGuard&) = delete;
        RenderTickSnapshotGuard& operator=(const RenderTickSnapshotGuard&) = delete;
    };
    RenderTickSnapshotGuard render_tick_snapshot_guard(ScreenCaptureManager::Instance());

    for(size_t idx = 0; idx < active_effects.size(); idx++)
    {
        SpatialEffect3D* effect = active_effects[idx].effect;
        if(!effect)
        {
            continue;
        }
        /* Unlocked volume boxes own their world origin — do not let the stack
           Spatial Anchor yank GetEffectOriginGrid off the gizmo every frame. */
        if(effect->GetVolumeToolsUnlocked() && effect->HasVolumeWorldOrigin())
        {
            continue;
        }
        /* Explicit stack-anchor exceptions: Screen Mirror Reach uses display plane
           centres; Reactive uses device/key space. Everything else that samples
           via SampleVolumeLocalUv01 / FillOverlayVolumeFrame (Volume incl. Grid Kit,
           Shader Field, Audio, Media, Movie Sync, Minecraft, and sample: room
           occupancy) gets the hub every LED frame. */
        const SpatialEffectType effect_type = effect->GetEffectInfo().effect_type;
        if(effect_type == SPATIAL_EFFECT_SCREEN_MIRROR
           || effect_type == SPATIAL_EFFECT_REACTIVE)
        {
            continue;
        }
        effect->SetGlobalReferencePoint(stack_ref_origin);
        effect->SetCustomReferencePoint(stack_ref_origin);
        effect->SetReferenceMode(stack_origin_mode);
    }

    std::unordered_set<RGBControllerInterface*> controllers_managed_by_virtuals;
    for(const std::unique_ptr<ControllerTransform>& transform_ptr : controller_transforms)
    {
        ControllerTransform* transform = transform_ptr.get();
        if(!transform || transform->virtual_controller == nullptr)
        {
            continue;
        }

        const std::vector<GridLEDMapping>& mappings = transform->virtual_controller->GetMappings();
        for(const GridLEDMapping& mapping : mappings)
        {
            if(mapping.controller)
            {
                controllers_managed_by_virtuals.insert(mapping.controller);
            }
        }
    }

    std::vector<EffectSlotGridOverride> slot_grid_overrides(active_effects.size());
    for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
    {
        const RenderEffectSlot& slot = active_effects[effect_idx];
        PopulateEffectSlotGrids(slot_grid_overrides[effect_idx],
                                slot.effect,
                                slot.zone_index,
                                stack_origin_mode,
                                zone_manager.get(),
                                controller_transforms,
                                &controllers_managed_by_virtuals,
                                room_grid,
                                world_grid,
                                effect_render_sequence,
                                mapped_lights_anchor_valid_,
                                mapped_lights_anchor_);
    }

    /* Emitter + relay: every relay layer's emitters show the stack's pattern on one emitter
       canvas; the first relay layer sets where the stack starts reaching non-emitters.
       Receivers are lit after every LED has its final colour, by ApplyRoomRelayReceivers
       (room light model, cached visibility). */
    size_t relay_stack_index = active_effects.size();
    SpatialEffect3D* relay_layer_effect = nullptr;
    std::unordered_set<int> relay_emitter_controllers;
    std::unordered_set<int> relay_receiver_controllers;
    EmitterLocalSampling::CombinedEmitterCanvas emitter_canvas{};
    {
        std::vector<SpatialEffect3D*> relays;
        for(size_t i = 0; i < active_effects.size(); ++i)
        {
            SpatialEffect3D* effect = active_effects[i].effect;
            if(effect && effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
            {
                if(!relay_layer_effect)
                {
                    relay_stack_index = i;
                    relay_layer_effect = effect;
                }
                relays.push_back(effect);
            }
        }

        for(SpatialEffect3D* relay : relays)
        {
            for(int idx : relay->roomEmitterControllerIndices())
            {
                relay_emitter_controllers.insert(idx);
            }
        }
        for(SpatialEffect3D* relay : relays)
        {
            for(int idx = 0; idx < (int)controller_transforms.size(); ++idx)
            {
                if(!relay_emitter_controllers.count(idx) && relay->isRoomReceiverController(idx)
                   && !relay->isRoomEmitterController(idx))
                {
                    relay_receiver_controllers.insert(idx);
                }
            }
        }
        if(relay_layer_effect && !relay_emitter_controllers.empty())
        {
            EmitterLocalSampling::TryBuildCombinedEmitterCanvas(controller_transforms,
                                                                relay_emitter_controllers,
                                                                room_grid.grid_scale_mm,
                                                                effect_render_sequence,
                                                                emitter_canvas);
        }
        SpatialLightingSceneProvider::instance()->SetRelayEmitterControllers(relay_emitter_controllers);
    }

    const SpatialLighting::OccluderBuildOptions frame_occluder_options = MergeStackOccluderOptions(active_effects);
    if(frame_occluder_options.display_planes || frame_occluder_options.scene_props ||
       frame_occluder_options.room_walls || frame_occluder_options.controllers ||
       frame_occluder_options.light_blockers)
    {
        SpatialLightingSceneProvider::instance()->EnsureFrameOccluders(room_grid, frame_occluder_options);
    }

    const float shade_cache_quant = MMToGridUnits(24.0f, room_grid.grid_scale_mm);
    SpatialLightingSceneProvider::instance()->BeginAmbientShadeCacheFrame(shade_cache_quant);

    if(ReactiveInputManager* reactive_mgr = ReactiveInputManager::instance())
    {
        if(reactive_mgr->isRunning())
        {
            std::vector<ReactiveLedSample> samples;
            samples.reserve(256);
            for(const std::unique_ptr<ControllerTransform>& transform : controller_transforms)
            {
                if(!transform || transform->hidden_by_virtual)
                {
                    continue;
                }
                ControllerLayout3D::UpdateWorldPositions(transform.get());
                for(const LEDPosition3D& led : transform->led_positions)
                {
                    RGBControllerInterface* ctrl = led.controller;
                    if(!ctrl)
                    {
                        continue;
                    }
                    ReactiveLedSample sample{};
                    sample.type = ctrl->GetDeviceType();
                    sample.name = PreferReactiveLedName(ctrl, led);
                    sample.room_position = led.room_position;
                    sample.local_position = led.local_position;
                    sample.device_id = reinterpret_cast<std::uintptr_t>(ctrl);
                    ParseHidVidPid(ctrl->GetLocation(), &sample.vid, &sample.pid);
                    samples.push_back(std::move(sample));
                }
            }
            reactive_mgr->SetLayoutSnapshot(std::move(samples));
        }
    }

    // Prepare GPU fields against the same per-slot active grid used for LED eval
    // (zone / target-bounds / world), not always the full room.
    std::vector<const GridContext3D*> prepared_grids(active_effects.size(), nullptr);
    bool any_true_volume = false;
    for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
    {
        SpatialEffect3D* effect = active_effects[effect_idx].effect;
        if(!effect)
        {
            continue;
        }
        if(effect->PrefersTrueGpuVolumeSamples())
        {
            any_true_volume = true;
        }
        const EffectSlotGridOverride& grid_override = slot_grid_overrides[effect_idx];
        const bool use_world_bounds = effect->UseWorldGridBounds();
        const GridContext3D& global_grid = use_world_bounds ? world_grid : room_grid;
        const GridContext3D* local_grid =
            ResolveActiveSlotGrid(grid_override, use_world_bounds);
        const GridContext3D& active_grid = local_grid ? *local_grid : global_grid;
        prepared_grids[effect_idx] = &active_grid;
        effect->PrepareGpuFields(effect_render_sequence, effect_time, active_grid);
        effect->PrepareStripColormapAssist(effect_render_sequence, effect_time);
    }

    RefreshGpuRoomGridOverlay(viewport,
                              room_bounds,
                              effect_time,
                              active_effects,
                              slot_grid_overrides,
                              prepared_grids);

    if(any_true_volume)
    {
        std::vector<std::vector<float>> true_xyz(active_effects.size());
        /* Emitter + relay emitters sample on their emitter canvas, not the slot grid: bake
           those points on that grid too, or every emitter LED misses the bake and pays a
           blocking GPU round trip of its own each frame. */
        std::vector<std::vector<float>> emitter_xyz(active_effects.size());
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ++ctrl_idx)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(!transform || transform->hidden_by_virtual)
            {
                continue;
            }
            if(transform->controller &&
               controllers_managed_by_virtuals.find(transform->controller)
                   != controllers_managed_by_virtuals.end())
            {
                continue;
            }
            if(relay_receiver_controllers.count(static_cast<int>(ctrl_idx)))
            {
                continue; /* lit by the relay pass; never samples the stack */
            }
            ControllerLayout3D::UpdateWorldPositions(transform);
            for(const LEDPosition3D& led_position : transform->led_positions)
            {
                for(size_t effect_idx = 0; effect_idx < active_effects.size(); ++effect_idx)
                {
                    const RenderEffectSlot& slot = active_effects[effect_idx];
                    SpatialEffect3D* effect = slot.effect;
                    if(!effect || !effect->PrefersTrueGpuVolumeSamples() || !prepared_grids[effect_idx])
                    {
                        continue;
                    }
                    if(!EffectSlotAppliesToController(slot.zone_index,
                                                      static_cast<int>(ctrl_idx),
                                                      zone_manager.get()))
                    {
                        continue;
                    }
                    if(!ShouldApplyStackLayerToController(effect,
                                                          effect_idx,
                                                          relay_stack_index,
                                                          relay_layer_effect != nullptr,
                                                          static_cast<int>(ctrl_idx)))
                    {
                        continue;
                    }
                    /* Mirror EvaluateColorGrid: a relay layer only shows a pattern on its
                       emitters (receivers are lit later) and samples them unquantized. */
                    const bool relay_role =
                        effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay;
                    if(relay_role && !effect->isRoomEmitterController(static_cast<int>(ctrl_idx)))
                    {
                        continue;
                    }
                    if(relay_emitter_controllers.count(static_cast<int>(ctrl_idx))
                       && emitter_canvas.valid && emitter_canvas.grid)
                    {
                        float ex = led_position.room_position.x;
                        float ey = led_position.room_position.y;
                        float ez = led_position.room_position.z;
                        if(!relay_role)
                        {
                            effect->PrepareTrueGpuSamplePoint(ex, ey, ez, *emitter_canvas.grid);
                        }
                        emitter_xyz[effect_idx].push_back(ex);
                        emitter_xyz[effect_idx].push_back(ey);
                        emitter_xyz[effect_idx].push_back(ez);
                        continue;
                    }
                    const EffectSlotGridOverride& grid_override = slot_grid_overrides[effect_idx];
                    if(effect->UseZoneGrid() && slot.zone_index != -1 && !grid_override.use_zone_grid)
                    {
                        continue;
                    }
                    const bool requires_world = effect->RequiresWorldSpaceCoordinates();
                    float sample_x = requires_world ? led_position.world_position.x
                                                    : led_position.room_position.x;
                    float sample_y = requires_world ? led_position.world_position.y
                                                    : led_position.room_position.y;
                    float sample_z = requires_world ? led_position.world_position.z
                                                    : led_position.room_position.z;
                    if(!relay_role)
                    {
                        effect->PrepareTrueGpuSamplePoint(sample_x, sample_y, sample_z,
                                                          *prepared_grids[effect_idx]);
                    }
                    true_xyz[effect_idx].push_back(sample_x);
                    true_xyz[effect_idx].push_back(sample_y);
                    true_xyz[effect_idx].push_back(sample_z);
                }
            }
        }
        for(size_t effect_idx = 0; effect_idx < active_effects.size(); ++effect_idx)
        {
            SpatialEffect3D* effect = active_effects[effect_idx].effect;
            if(!effect || !effect->PrefersTrueGpuVolumeSamples() || !prepared_grids[effect_idx]
               || (true_xyz[effect_idx].empty() && emitter_xyz[effect_idx].empty()))
            {
                continue;
            }
            effect->BakeTrueGpuVolumeSamples(true_xyz[effect_idx].data(),
                                             (int)(true_xyz[effect_idx].size() / 3u),
                                             effect_time,
                                             *prepared_grids[effect_idx]);
            if(!emitter_xyz[effect_idx].empty())
            {
                effect->BakeTrueGpuVolumeSamples(emitter_xyz[effect_idx].data(),
                                                 (int)(emitter_xyz[effect_idx].size() / 3u),
                                                 effect_time,
                                                 *emitter_canvas.grid,
                                                 true);
            }
        }
    }

    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform || transform->hidden_by_virtual)
        {
            continue;
        }

        if(transform->controller &&
           controllers_managed_by_virtuals.find(transform->controller) != controllers_managed_by_virtuals.end())
        {
            continue;
        }

        ControllerLayout3D::UpdateWorldPositions(transform);
        SpatialLightingSceneProvider::instance()->SetShadingControllerIndex(static_cast<int>(ctrl_idx));

        if(transform->virtual_controller && !transform->controller)
        {
            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); ++led_pos_idx)
            {
                const LEDPosition3D& led_position = transform->led_positions[led_pos_idx];
                if(!led_position.controller)
                {
                    continue;
                }

                const Vector3D& world_pos = led_position.world_position;
                float room_x = led_position.room_position.x;
                float room_y = led_position.room_position.y;
                float room_z = led_position.room_position.z;
                float world_x = world_pos.x;
                float world_y = world_pos.y;
                float world_z = world_pos.z;

                if(relay_receiver_controllers.count(static_cast<int>(ctrl_idx)))
                {
                    continue; /* lit by ApplyRoomRelayReceivers once the emitters are final */
                }

                if(relay_emitter_controllers.count(static_cast<int>(ctrl_idx)) &&
                   emitter_canvas.valid && emitter_canvas.grid)
                {
                    RGBColor final_color = ToRGBColor(0, 0, 0);
                    for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                    {
                        const RenderEffectSlot& slot = active_effects[effect_idx];
                        SpatialEffect3D* effect = slot.effect;
                        if(!effect)
                        {
                            continue;
                        }
                        if(!EffectSlotAppliesToController(slot.zone_index, static_cast<int>(ctrl_idx), zone_manager.get()))
                        {
                            continue;
                        }
                        if(!ShouldApplyStackLayerToController(effect,
                                                              effect_idx,
                                                              relay_stack_index,
                                                              relay_layer_effect != nullptr,
                                                              static_cast<int>(ctrl_idx)))
                        {
                            continue;
                        }
                        RGBColor effect_color = SamplePatternOnEmitterCanvas(effect,
                                                                             room_x,
                                                                             room_y,
                                                                             room_z,
                                                                             effect_time,
                                                                             emitter_canvas);
                        if(!effect->IsPointOnActiveSurface(room_x, room_y, room_z, *emitter_canvas.grid))
                        {
                            effect_color = 0x00000000;
                        }
                        effect_color = effect->PostProcessColorGrid(effect_color);
                        final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                    }
                    if(const SpatialEffect3D* shade_source = ResolveAmbientShadeSource(active_effects,
                                                                                         relay_stack_index,
                                                                                         relay_layer_effect != nullptr,
                                                                                         static_cast<int>(ctrl_idx),
                                                                                         zone_manager.get()))
                    {
                        final_color = shade_source->ApplyLayerRoomAmbientShading(room_x,
                                                                                   room_y,
                                                                                   room_z,
                                                                                   final_color,
                                                                                   room_grid,
                                                                                   PackAmbientShadeSlot(static_cast<int>(ctrl_idx),
                                                                                                        static_cast<int>(led_pos_idx)));
                    }
                    transform->led_positions[led_pos_idx].preview_color = final_color;

                    RGBControllerInterface* mapping_controller = led_position.controller;
                    if(mapping_controller && mapping_controller->GetZoneCount() != 0 && mapping_controller->GetLEDCount() != 0 &&
                       led_position.zone_idx < mapping_controller->GetZoneCount())
                    {
                        unsigned int led_global_idx = 0;
                        if(TryGetGlobalLedIndex(mapping_controller,
                                                led_position.zone_idx,
                                                led_position.led_idx,
                                                &led_global_idx))
                        {
                            mapping_controller->SetColor(led_global_idx, final_color);
                        }
                    }
                    continue;
                }

                RGBColor final_color = ToRGBColor(0, 0, 0);
                    for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                    {
                        const RenderEffectSlot& slot = active_effects[effect_idx];
                        SpatialEffect3D* effect = slot.effect;
                        if(!effect)
                        {
                            continue;
                        }

                        if(!EffectSlotAppliesToController(slot.zone_index,
                                                          static_cast<int>(ctrl_idx),
                                                          zone_manager.get()))
                        {
                            continue;
                        }
                        if(!ShouldApplyStackLayerToController(effect,
                                                              effect_idx,
                                                              relay_stack_index,
                                                              relay_layer_effect != nullptr,
                                                              static_cast<int>(ctrl_idx)))
                        {
                            continue;
                        }
                        const EffectSlotGridOverride& grid_override = slot_grid_overrides[effect_idx];
                        if(effect->UseZoneGrid() && slot.zone_index != -1 && !grid_override.use_zone_grid)
                        {
                            continue;
                        }

                        const bool requires_world = effect->RequiresWorldSpaceCoordinates();
                        float sample_x = requires_world ? world_x : room_x;
                        float sample_y = requires_world ? world_y : room_y;
                        float sample_z = requires_world ? world_z : room_z;
                        const bool use_world_bounds = effect->UseWorldGridBounds();
                        const GridContext3D& global_grid = use_world_bounds ? world_grid : room_grid;
                        const GridContext3D* local_grid =
                            ResolveActiveSlotGrid(grid_override, use_world_bounds);
                        const GridContext3D& stack_grid = local_grid ? *local_grid : global_grid;
                        MinecraftGame::SetRenderSampleIndexContext((int)led_pos_idx, (int)transform->led_positions.size());
                        RGBColor effect_color = SampleStackLayerColor(effect,
                                                                      sample_x,
                                                                      sample_y,
                                                                      sample_z,
                                                                      effect_time,
                                                                      stack_grid);
                        if(!effect->IsPointOnActiveSurface(sample_x, sample_y, sample_z, stack_grid))
                            effect_color = 0x00000000;
                        effect_color = effect->PostProcessColorGrid(effect_color);
                        final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                    }

                if(const SpatialEffect3D* shade_source = ResolveAmbientShadeSource(active_effects,
                                                                                     relay_stack_index,
                                                                                     relay_layer_effect != nullptr,
                                                                                     static_cast<int>(ctrl_idx),
                                                                                     zone_manager.get()))
                {
                    final_color = shade_source->ApplyLayerRoomAmbientShading(room_x,
                                                                               room_y,
                                                                               room_z,
                                                                               final_color,
                                                                               room_grid,
                                                                               PackAmbientShadeSlot(static_cast<int>(ctrl_idx),
                                                                                                    static_cast<int>(led_pos_idx)));
                }

                transform->led_positions[led_pos_idx].preview_color = final_color;

                RGBControllerInterface* mapping_controller = led_position.controller;
                if(!mapping_controller || mapping_controller->GetZoneCount() == 0 || mapping_controller->GetLEDCount() == 0)
                {
                    continue;
                }

                if(led_position.zone_idx < mapping_controller->GetZoneCount())
                {
                    unsigned int led_global_idx = 0;
                    if(TryGetGlobalLedIndex(mapping_controller,
                                            led_position.zone_idx,
                                            led_position.led_idx,
                                            &led_global_idx))
                    {
                        mapping_controller->SetColor(led_global_idx, final_color);
                    }
                }
            }
        }
        else
        {
            RGBControllerInterface* controller = transform->controller;
            if(!controller || controller->GetZoneCount() == 0 || controller->GetLEDCount() == 0)
            {
                continue;
            }

            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
            {
                LEDPosition3D& led_position = transform->led_positions[led_pos_idx];
                const Vector3D& world_pos = led_position.world_position;
                const Vector3D& room_pos = led_position.room_position;
                float room_x = room_pos.x;
                float room_y = room_pos.y;
                float room_z = room_pos.z;
                float world_x = world_pos.x;
                float world_y = world_pos.y;
                float world_z = world_pos.z;

                if(led_position.zone_idx >= controller->GetZoneCount())
                {
                    continue;
                }
                unsigned int led_global_idx = 0;
                if(!TryGetGlobalLedIndex(controller, led_position.zone_idx, led_position.led_idx, &led_global_idx))
                {
                    continue;
                }

                if(relay_receiver_controllers.count(static_cast<int>(ctrl_idx)))
                {
                    continue; /* lit by ApplyRoomRelayReceivers once the emitters are final */
                }

                if(relay_emitter_controllers.count(static_cast<int>(ctrl_idx)) &&
                   emitter_canvas.valid && emitter_canvas.grid)
                {
                    RGBColor final_color = ToRGBColor(0, 0, 0);
                    for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                    {
                        const RenderEffectSlot& slot = active_effects[effect_idx];
                        SpatialEffect3D* effect = slot.effect;
                        if(!effect)
                        {
                            continue;
                        }
                        if(!EffectSlotAppliesToController(slot.zone_index, static_cast<int>(ctrl_idx), zone_manager.get()))
                        {
                            continue;
                        }
                        if(!ShouldApplyStackLayerToController(effect,
                                                              effect_idx,
                                                              relay_stack_index,
                                                              relay_layer_effect != nullptr,
                                                              static_cast<int>(ctrl_idx)))
                        {
                            continue;
                        }
                        RGBColor effect_color = SamplePatternOnEmitterCanvas(effect,
                                                                             room_x,
                                                                             room_y,
                                                                             room_z,
                                                                             effect_time,
                                                                             emitter_canvas);
                        if(!effect->IsPointOnActiveSurface(room_x, room_y, room_z, *emitter_canvas.grid))
                        {
                            effect_color = 0x00000000;
                        }
                        effect_color = effect->PostProcessColorGrid(effect_color);
                        final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                    }
                    if(const SpatialEffect3D* shade_source = ResolveAmbientShadeSource(active_effects,
                                                                                         relay_stack_index,
                                                                                         relay_layer_effect != nullptr,
                                                                                         static_cast<int>(ctrl_idx),
                                                                                         zone_manager.get()))
                    {
                        final_color = shade_source->ApplyLayerRoomAmbientShading(room_x,
                                                                                   room_y,
                                                                                   room_z,
                                                                                   final_color,
                                                                                   room_grid,
                                                                                   PackAmbientShadeSlot(static_cast<int>(ctrl_idx),
                                                                                                        static_cast<int>(led_pos_idx)));
                    }
                    transform->led_positions[led_pos_idx].preview_color = final_color;
                    if(led_global_idx < controller->GetLEDCount())
                    {
                        controller->SetColor(led_global_idx, final_color);
                    }
                    continue;
                }

                RGBColor final_color = ToRGBColor(0, 0, 0);
                for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                {
                    const RenderEffectSlot& slot = active_effects[effect_idx];
                    SpatialEffect3D* effect = slot.effect;
                    if(!effect)
                    {
                        continue;
                    }

                    if(!EffectSlotAppliesToController(slot.zone_index,
                                                      static_cast<int>(ctrl_idx),
                                                      zone_manager.get()))
                    {
                        continue;
                    }
                    if(!ShouldApplyStackLayerToController(effect,
                                                          effect_idx,
                                                          relay_stack_index,
                                                          relay_layer_effect != nullptr,
                                                          static_cast<int>(ctrl_idx)))
                    {
                        continue;
                    }
                    const EffectSlotGridOverride& grid_override = slot_grid_overrides[effect_idx];
                    if(effect->UseZoneGrid() && slot.zone_index != -1 && !grid_override.use_zone_grid)
                    {
                        continue;
                    }

                    const bool requires_world = effect->RequiresWorldSpaceCoordinates();
                    float sample_x = requires_world ? world_x : room_x;
                    float sample_y = requires_world ? world_y : room_y;
                    float sample_z = requires_world ? world_z : room_z;
                    const bool use_world_bounds = effect->UseWorldGridBounds();
                    const GridContext3D& global_grid = use_world_bounds ? world_grid : room_grid;
                    const GridContext3D* local_grid =
                        ResolveActiveSlotGrid(grid_override, use_world_bounds);
                    const GridContext3D& stack_grid = local_grid ? *local_grid : global_grid;
                    MinecraftGame::SetRenderSampleIndexContext((int)led_pos_idx, (int)transform->led_positions.size());
                    RGBColor effect_color = SampleStackLayerColor(effect,
                                                                  sample_x,
                                                                  sample_y,
                                                                  sample_z,
                                                                  effect_time,
                                                                  stack_grid);
                    if(!effect->IsPointOnActiveSurface(sample_x, sample_y, sample_z, stack_grid))
                        effect_color = 0x00000000;
                    effect_color = effect->PostProcessColorGrid(effect_color);

                    final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                }

                if(const SpatialEffect3D* shade_source = ResolveAmbientShadeSource(active_effects,
                                                                                     relay_stack_index,
                                                                                     relay_layer_effect != nullptr,
                                                                                     static_cast<int>(ctrl_idx),
                                                                                     zone_manager.get()))
                {
                    final_color = shade_source->ApplyLayerRoomAmbientShading(room_x,
                                                                               room_y,
                                                                               room_z,
                                                                               final_color,
                                                                               room_grid,
                                                                               PackAmbientShadeSlot(static_cast<int>(ctrl_idx),
                                                                                                    static_cast<int>(led_pos_idx)));
                }

                transform->led_positions[led_pos_idx].preview_color = final_color;

                if(led_global_idx < controller->GetLEDCount())
                {
                    controller->SetColor(led_global_idx, final_color);
                }
            }
        }
    }

    /* Every emitter now shows its final colour: light the relay receivers from it. */
    ApplyRoomRelayReceivers(nullptr);

    EffectAxis sort_axis = AXIS_Y;
    std::vector<std::pair<float, unsigned int>> sorted_controllers;
    sorted_controllers.reserve(controller_transforms.size());

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* transform = controller_transforms[i].get();
        if(!transform) continue;
        float key = AverageAlongAxis(transform, sort_axis, stack_ref_origin);
        sorted_controllers.emplace_back(key, i);
    }

    std::sort(sorted_controllers.begin(), sorted_controllers.end(),
        [&](const std::pair<float, unsigned int>& a, const std::pair<float, unsigned int>& b){
        return a.first < b.first;
    });

    std::set<RGBControllerInterface*> updated_physical_controllers;

    for(unsigned int i = 0; i < sorted_controllers.size(); i++)
    {
        unsigned int ctrl_idx = sorted_controllers[i].second;
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();

        if(!transform) continue;

        if(transform->virtual_controller && !transform->controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            for(unsigned int mapping_idx = 0; mapping_idx < mappings.size(); mapping_idx++)
            {
                if(mappings[mapping_idx].controller &&
                   updated_physical_controllers.find(mappings[mapping_idx].controller) == updated_physical_controllers.end())
                {
                    mappings[mapping_idx].controller->UpdateLEDs();
                    updated_physical_controllers.insert(mappings[mapping_idx].controller);
                }
            }
        }
        else if(transform->controller)
        {
            if(updated_physical_controllers.find(transform->controller) == updated_physical_controllers.end())
            {
                transform->controller->UpdateLEDs();
                updated_physical_controllers.insert(transform->controller);
            }
        }
    }

    MinecraftGame::ClearRenderSampleIndexContext();

    if(viewport)
    {
        viewport->UploadDisplayPlaneCaptureTexturesDuringEffectTick();
        viewport->UpdateColors();
    }
}

SpatialEffectSettingsLayout OpenRGB3DSpatialTab::settingsLayoutForClass(const std::string& class_name) const
{
    return EffectListManager3D::get()->GetEffectInfo(class_name).settings_layout;
}

OpenRGB3DSpatialTab::EffectSettingsUiMount OpenRGB3DSpatialTab::createEffectSettingsUi(QWidget* parent,
                                                                                      QBoxLayout* target_layout,
                                                                                      const std::string& class_name,
                                                                                      SpatialEffectSettingsLayout layout)
{
    EffectSettingsUiMount mount;
    if(!parent || class_name.empty())
    {
        return mount;
    }

    mount.effect = EffectListManager3D::get()->CreateEffect(class_name);
    if(!mount.effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect UI: %s", class_name.c_str());
        return mount;
    }

    mount.container = new QWidget(parent);
    auto* body_layout = new QVBoxLayout(mount.container);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(4);

    mount.effect->setParent(mount.container);
    mount.effect->MountSettingsUi(mount.container, layout);

    if(target_layout)
    {
        target_layout->addWidget(mount.container);
    }

    return mount;
}


