// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "EffectListManager3D.h"
#include "ZoneGrid3D.h"
#include "Effects3D/Games/Minecraft/MinecraftGame.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "LogManager.h"
#include "ControllerLayout3D.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "SpatialLighting/EmitterRelayMirror.h"
#include "SpatialLighting/EmitterLocalSampling.h"
#include "Effects3D/SpatialLighting/RoomSpatialLightingUi.h"
#include "VirtualController3D.h"
#include "LEDPosition3D.h"
#include "SpatialRoom/SpatialRoomFrame.h"
#include "GridSpaceUtils.h"
#include "ZoneManager3D.h"
#include "SpatialTabLedHelpers.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "SettingsManager.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <cmath>
#include <algorithm>
#include <set>

namespace
{
struct EffectSlotGridOverride
{
    bool use_zone_grid = false;
    bool use_anchor_grid = false;
    std::unique_ptr<GridContext3D> room_grid_local;
    std::unique_ptr<GridContext3D> world_grid_local;
};

void ApplyZoneAnchorMetadata(GridContext3D& grid,
                             ReferenceMode origin_mode,
                             ZoneManager3D* zone_manager,
                             const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                             int zone_index,
                             const std::unordered_set<RGBController*>* skip_physical_controllers,
                             bool room_aligned)
{
    if(zone_index == -1)
    {
        return;
    }
    if(origin_mode == REF_MODE_LED_CENTROID)
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
                             const std::unordered_set<RGBController*>* skip_physical_controllers,
                             const GridContext3D& room_grid,
                             const GridContext3D& world_grid,
                             uint64_t render_sequence)
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
                                true);
        ApplyZoneAnchorMetadata(*slot_override.world_grid_local,
                                stack_origin_mode,
                                zone_manager,
                                controller_transforms,
                                zone_index,
                                skip_physical_controllers,
                                false);
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

const GridContext3D* ResolveActiveSlotGrid(const EffectSlotGridOverride& slot_override,
                                           bool use_world_bounds)
{
    if(slot_override.use_zone_grid || slot_override.use_anchor_grid)
    {
        return use_world_bounds ? slot_override.world_grid_local.get() : slot_override.room_grid_local.get();
    }
    return nullptr;
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
                                       int ctrl_idx)
{
    if(!effect)
    {
        return false;
    }
    if(has_relay_stack)
    {
        if(effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::Direct &&
           !SpatialLightingSceneProvider::instance()->isEmitterController(ctrl_idx))
        {
            return false;
        }
        if(effect_index < relay_stack_index &&
           !SpatialLightingSceneProvider::instance()->isEmitterController(ctrl_idx))
        {
            return false;
        }
    }
    if(!effect->appliesRoomOutputToController(ctrl_idx))
    {
        return false;
    }
    return true;
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

bool IsRelayOnlyReceiver(const SpatialEffect3D* relay_effect, int ctrl_idx)
{
    return relay_effect &&
           relay_effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay &&
           relay_effect->isRoomReceiverController(ctrl_idx) &&
           !relay_effect->isRoomEmitterController(ctrl_idx);
}

bool IsRelayEmitter(const SpatialEffect3D* relay_effect, int ctrl_idx)
{
    return relay_effect &&
           relay_effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay &&
           relay_effect->isRoomEmitterController(ctrl_idx);
}

} // namespace

static bool TryGetGlobalLedIndex(RGBController* controller,
                                 unsigned int zone_idx,
                                 unsigned int led_idx,
                                 unsigned int* global_idx)
{
    if(!controller || !global_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_idx = controller->zones[zone_idx].start_idx + led_idx;
    return (*global_idx < controller->colors.size());
}

void OpenRGB3DSpatialTab::RenderEffectStack()
{
    MinecraftGame::ClearRenderSampleIndexContext();

    if(controller_transforms.empty())
    {
        return;
    }

    SyncDisplayPlaneManager();
    SpatialLightingSceneProvider::instance()->SetControllers(&controller_transforms);
    SpatialLightingSceneProvider::instance()->SetShadingControllerIndex(-1);

    static uint64_t s_effect_render_sequence = 0;
    const uint64_t effect_render_sequence = ++s_effect_render_sequence;

    SpatialRoom::BeginEffectRenderFrame(effect_render_sequence, SpatialRoom::SpatialRoomDepthPreset::Standard);

    ManualRoomSettings room_settings = MakeManualRoomSettings(use_manual_room_size,
                                                              manual_room_width,
                                                              manual_room_height,
                                                              manual_room_depth);
    GridBounds world_bounds = ComputeGridBounds(room_settings, grid_scale_mm, controller_transforms);
    GridBounds room_bounds  = ComputeRoomAlignedBounds(room_settings, grid_scale_mm, controller_transforms);

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

    Vector3D led_mu_room{};
    if(TryComputeLedCentroid(controller_transforms, true, &led_mu_room))
    {
        room_grid.SetLedCentroid(led_mu_room.x, led_mu_room.y, led_mu_room.z);
    }
    Vector3D led_mu_world{};
    if(TryComputeLedCentroid(controller_transforms, false, &led_mu_world))
    {
        world_grid.SetLedCentroid(led_mu_world.x, led_mu_world.y, led_mu_world.z);
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
                stack_origin_mode = REF_MODE_USER_POSITION;
                stack_ref_origin = reference_points[ref_idx]->GetPosition();
            }
        }
    }

    struct RenderEffectSlot
    {
        SpatialEffect3D* effect;
        int zone_index;
        BlendMode blend_mode;
    };

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
        std::unordered_set<RGBController*> controllers_to_update;
        std::unordered_set<RGBController*> controllers_managed_by_virtuals;
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
                    RGBController* mapping_controller = led_position.controller;
                    if(!mapping_controller || mapping_controller->zones.empty() || mapping_controller->colors.empty())
                    {
                        continue;
                    }
                    if(led_position.zone_idx < mapping_controller->zones.size())
                    {
                        unsigned int led_global_idx = 0;
                        if(TryGetGlobalLedIndex(mapping_controller,
                                                led_position.zone_idx,
                                                led_position.led_idx,
                                                &led_global_idx))
                        {
                            mapping_controller->colors[led_global_idx] = black;
                            controllers_to_update.insert(mapping_controller);
                        }
                    }
                }
                continue;
            }

            RGBController* controller = transform->controller;
            if(!controller || controller->zones.empty() || controller->colors.empty()) continue;

            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
            {
                LEDPosition3D& led_position = transform->led_positions[led_pos_idx];
                led_position.preview_color = black;
                unsigned int led_global_idx = 0;
                if(TryGetGlobalLedIndex(controller, led_position.zone_idx, led_position.led_idx, &led_global_idx))
                    controller->colors[led_global_idx] = black;
            }
            controllers_to_update.insert(controller);
        }

        for(RGBController* ctrl : controllers_to_update)
        {
            if(ctrl) ctrl->UpdateLEDs();
        }

        if(viewport)
        {
            viewport->UpdateColors();
            viewport->ClearRoomGridOverlayBounds();
            viewport->SetRoomGridColorCallback(nullptr);
            int nx = 0, ny = 0, nz = 0;
            viewport->GetRoomGridOverlayDimensions(&nx, &ny, &nz);
            const size_t grid_count = (size_t)nx * (size_t)ny * (size_t)nz;
            if(grid_count > 0 && grid_count <= 500000u)
            {
                std::vector<RGBColor> black_grid(grid_count, black);
                viewport->SetRoomGridColorBuffer(black_grid);
            }
            else
                viewport->SetRoomGridColorBuffer(std::vector<RGBColor>());
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
        if(active_effects[idx].effect)
        {
            active_effects[idx].effect->SetGlobalReferencePoint(stack_ref_origin);
            active_effects[idx].effect->SetReferenceMode(stack_origin_mode);
        }
    }

    std::unordered_set<RGBController*> controllers_managed_by_virtuals;
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
                                effect_render_sequence);
    }

    SpatialLightingSceneProvider::instance()->ClearEmitterRelayFrame();
    size_t relay_stack_index = active_effects.size();
    SpatialEffect3D* relay_layer_effect = nullptr;
    std::unordered_set<int> relay_emitter_controllers;
    EmitterLocalSampling::CombinedEmitterCanvas emitter_canvas{};
    {
        for(size_t i = 0; i < active_effects.size(); ++i)
        {
            SpatialEffect3D* effect = active_effects[i].effect;
            if(effect && effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
            {
                relay_stack_index = i;
                relay_layer_effect = effect;
                break;
            }
        }

        if(!relay_layer_effect && current_effect_ui &&
           current_effect_ui->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
        {
            for(size_t i = 0; i < active_effects.size(); ++i)
            {
                if(active_effects[i].effect == current_effect_ui)
                {
                    relay_stack_index = i;
                    relay_layer_effect = current_effect_ui;
                    break;
                }
            }
        }

        std::unordered_set<int>& emitter_set = relay_emitter_controllers;
        if(relay_layer_effect && relay_stack_index < active_effects.size())
        {
            for(int idx : relay_layer_effect->roomEmitterControllerIndices())
            {
                emitter_set.insert(idx);
            }
        }

        const size_t emitter_sample_through = relay_stack_index + 1;

        if(relay_layer_effect && !emitter_set.empty())
        {
            EmitterLocalSampling::TryBuildCombinedEmitterCanvas(controller_transforms,
                                                                emitter_set,
                                                                room_grid.grid_scale_mm,
                                                                effect_render_sequence,
                                                                emitter_canvas);
        }

        if(relay_layer_effect && relay_stack_index < active_effects.size() && !emitter_set.empty())
        {
            const RoomSpatialLightingUi::RoomSpatialLightParams& lp = relay_layer_effect->roomRelayParams();
            const float bright = std::max(0.15f, relay_layer_effect->GetBrightness() / 100.0f);

            const auto sample_emitter_led = [&](const LEDPosition3D& led_position, int ctrl_idx) -> RGBColor {
                ControllerTransform* emitter_transform = nullptr;
                if(ctrl_idx >= 0 && ctrl_idx < (int)controller_transforms.size())
                {
                    emitter_transform = controller_transforms[(size_t)ctrl_idx].get();
                }

                const Vector3D& world_pos = led_position.world_position;
                const float room_x = led_position.room_position.x;
                const float room_y = led_position.room_position.y;
                const float room_z = led_position.room_position.z;

                SpatialLightingSceneProvider* shade_provider = SpatialLightingSceneProvider::instance();
                const int prev_shade_ctrl = shade_provider->shadingControllerIndex();
                shade_provider->SetShadingControllerIndex(ctrl_idx);
                RGBColor blended = ToRGBColor(0, 0, 0);
                for(size_t effect_idx = 0; effect_idx < emitter_sample_through; ++effect_idx)
                {
                    const RenderEffectSlot& slot = active_effects[effect_idx];
                    SpatialEffect3D* effect = slot.effect;
                    if(!effect)
                    {
                        continue;
                    }
                    if(!EffectSlotAppliesToController(slot.zone_index, ctrl_idx, zone_manager.get()))
                    {
                        continue;
                    }

                    RGBColor effect_color = 0x00000000;
                    if(emitter_canvas.valid && emitter_canvas.grid)
                    {
                        effect_color = SamplePatternOnEmitterCanvas(effect,
                                                                    room_x,
                                                                    room_y,
                                                                    room_z,
                                                                    effect_time,
                                                                    emitter_canvas);
                        if(!effect->IsPointOnActiveSurface(room_x, room_y, room_z, *emitter_canvas.grid))
                        {
                            effect_color = 0x00000000;
                        }
                    }
                    else
                    {
                        const EffectSlotGridOverride& grid_override = slot_grid_overrides[effect_idx];
                        if(effect->UseZoneGrid() && slot.zone_index != -1 && !grid_override.use_zone_grid)
                        {
                            continue;
                        }
                        const bool requires_world = effect->RequiresWorldSpaceCoordinates();
                        const bool use_world_bounds = effect->UseWorldGridBounds();
                        const GridContext3D& global_grid = use_world_bounds ? world_grid : room_grid;
                        const GridContext3D* local_grid =
                            ResolveActiveSlotGrid(grid_override, use_world_bounds);
                        const GridContext3D& stack_grid = local_grid ? *local_grid : global_grid;

                        float sx = requires_world ? world_pos.x : room_x;
                        float sy = requires_world ? world_pos.y : room_y;
                        float sz = requires_world ? world_pos.z : room_z;
                        const GridContext3D& active_grid = stack_grid;

                        if(!effect->SkipsSpatialSampleWarp())
                        {
                            effect->ApplyAxisScale(sx, sy, sz, active_grid);
                            effect->ApplyEffectRotation(sx, sy, sz, active_grid);
                        }
                        effect_color = SampleStackLayerColor(effect,
                                                             sx,
                                                             sy,
                                                             sz,
                                                             effect_time,
                                                             active_grid);
                        if(!effect->IsPointOnActiveSurface(sx, sy, sz, active_grid))
                        {
                            effect_color = 0x00000000;
                        }
                    }
                    effect_color = effect->PostProcessColorGrid(effect_color);
                    blended = BlendColors(blended, effect_color, slot.blend_mode);
                }
                shade_provider->SetShadingControllerIndex(prev_shade_ctrl);
                return blended;
            };

            EmitterRelayMirror::MirrorFrame mirror{};
            mirror.reference_room = {room_grid.center_x, room_grid.center_y, room_grid.center_z};
            mirror.grid_scale_mm = room_grid.grid_scale_mm;
            const float room_diag_units = std::sqrt(room_grid.width * room_grid.width +
                                                    room_grid.height * room_grid.height +
                                                    room_grid.depth * room_grid.depth);
            mirror.reference_max_distance_mm =
                std::max(GridUnitsToMM(room_diag_units, room_grid.grid_scale_mm), lp.light_reach_mm);
            mirror.light_reach_mm = lp.light_reach_mm;
            mirror.glow_feather_percent = std::clamp(lp.glow_radius_mm * 0.35f, 5.0f, 80.0f);
            mirror.room_fill_strength = lp.room_fill / 100.0f;
            mirror.coverage = std::clamp(lp.light_reach_mm / std::max(mirror.reference_max_distance_mm * 0.45f, 1.0f),
                                         0.35f,
                                         3.0f);
            mirror.edge_softness = std::clamp(lp.glow_radius_mm * 0.35f, 5.0f, 50.0f);
            mirror.brightness = bright;

            for(const int emitter_ctrl : emitter_set)
            {
                if(emitter_ctrl < 0 || emitter_ctrl >= (int)controller_transforms.size())
                {
                    continue;
                }
                ControllerTransform* emitter_transform = controller_transforms[(size_t)emitter_ctrl].get();
                if(!emitter_transform || emitter_transform->hidden_by_virtual)
                {
                    continue;
                }
                ControllerLayout3D::UpdateWorldPositions(emitter_transform);

                Vector3D min_bounds{};
                Vector3D max_bounds{};
                ControllerLayout3D::CalculateControllerLocalBounds(emitter_transform, min_bounds, max_bounds);
                const float span_x = std::max(max_bounds.x - min_bounds.x, 0.01f);
                const float span_y = std::max(max_bounds.y - min_bounds.y, 0.01f);

                std::vector<EmitterRelayMirror::LedColorSample> led_samples;
                led_samples.reserve(emitter_transform->led_positions.size());

                const auto push_led_sample = [&](const LEDPosition3D& led_position) {
                    const RGBColor c = sample_emitter_led(led_position, emitter_ctrl);
                    const uint8_t r = static_cast<uint8_t>(c & 0xFF);
                    const uint8_t g = static_cast<uint8_t>((c >> 8) & 0xFF);
                    const uint8_t b = static_cast<uint8_t>((c >> 16) & 0xFF);
                    if(static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b) < 6)
                    {
                        return;
                    }
                    EmitterRelayMirror::LedColorSample sample{};
                    sample.u = (led_position.local_position.x - min_bounds.x) / span_x;
                    sample.v = (led_position.local_position.y - min_bounds.y) / span_y;
                    sample.r = r;
                    sample.g = g;
                    sample.b = b;
                    led_samples.push_back(sample);
                };

                for(const LEDPosition3D& led_position : emitter_transform->led_positions)
                {
                    push_led_sample(led_position);
                }

                if(led_samples.empty())
                {
                    continue;
                }

                EmitterRelayMirror::EmitterSurface surface{};
                EmitterRelayMirror::BuildSurfaceFromSamples(emitter_ctrl, emitter_transform, led_samples, surface);
                if(!surface.tex_rgb.empty())
                {
                    mirror.surfaces.push_back(std::move(surface));
                }
            }

            SpatialLightingSceneProvider::instance()->SetEmitterRelayMirrorFrame(std::move(mirror),
                                                                                 std::move(emitter_set));
        }
    }

    if(viewport && viewport->GetShowRoomGridOverlay() && active_effects.size() > 0)
    {
        SpatialLightingSceneProvider::instance()->SetShadingControllerIndex(-1);

        viewport->SetRoomGridOverlayBounds(room_bounds.min_x, room_bounds.max_x,
                                           room_bounds.min_y, room_bounds.max_y,
                                           room_bounds.min_z, room_bounds.max_z);
        int nx = 0, ny = 0, nz = 0;
        viewport->GetRoomGridOverlayDimensions(&nx, &ny, &nz);
        const size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
        if(count > 0 && count <= 500000u)
        {
            if(room_grid_overlay_buffer.size() != count)
            {
                room_grid_overlay_buffer.resize(count);
            }
            const float time_val = effect_time;
            SpatialRoom::BeginRoomGridOverlayPass();

            for(int ix = 0; ix < nx; ix++)
            {
                for(int iy = 0; iy < ny; iy++)
                {
                    for(int iz = 0; iz < nz; iz++)
                    {
                        RGBColor final_color = ToRGBColor(0, 0, 0);
                        float spx = 0.0f;
                        float spy = 0.0f;
                        float spz = 0.0f;
                        viewport->GetRoomGridOverlaySamplePosition(ix, iy, iz, spx, spy, spz);
                        for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                        {
                            const RenderEffectSlot& slot = active_effects[effect_idx];
                            if(!slot.effect)
                            {
                                continue;
                            }
                            const EffectSlotGridOverride& grid_override = slot_grid_overrides[effect_idx];
                            if(slot.effect->UseZoneGrid() && slot.zone_index != -1 && !grid_override.use_zone_grid)
                            {
                                continue;
                            }
                            const bool use_world_bounds = slot.effect->UseWorldGridBounds();
                            const GridContext3D& global_grid = use_world_bounds ? world_grid : room_grid;
                            const GridContext3D* local_grid =
                                ResolveActiveSlotGrid(grid_override, use_world_bounds);
                            const GridContext3D& active_grid = local_grid ? *local_grid : global_grid;
                            if(!slot.effect->SkipsSpatialSampleWarp())
                            {
                                slot.effect->ApplyAxisScale(spx, spy, spz, active_grid);
                                slot.effect->ApplyEffectRotation(spx, spy, spz, active_grid);
                            }
                            RGBColor effect_color =
                                slot.effect->EvaluateColorGrid(spx, spy, spz, time_val, active_grid);
                            if(!slot.effect->IsPointOnActiveSurface(spx, spy, spz, active_grid))
                                effect_color = 0x00000000;
                            effect_color = slot.effect->PostProcessColorGrid(effect_color);
                            final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                        }
                        room_grid_overlay_buffer[(size_t)ix * (size_t)ny * (size_t)nz + (size_t)iy * (size_t)nz + (size_t)iz] = final_color;
                    }
                }
            }
            SpatialRoom::EndRoomGridOverlayPass();
            viewport->SetRoomGridColorCallback(nullptr);
            viewport->SetRoomGridColorBuffer(room_grid_overlay_buffer);
        }
        viewport->update();
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

                if(IsRelayOnlyReceiver(relay_layer_effect, static_cast<int>(ctrl_idx)))
                {
                    const bool relay_use_world = relay_layer_effect->RequiresWorldSpaceCoordinates();
                    const bool relay_world_bounds = relay_layer_effect->UseWorldGridBounds();
                    const GridContext3D& relay_grid = relay_world_bounds ? world_grid : room_grid;
                    float sample_x = relay_use_world ? world_x : room_x;
                    float sample_y = relay_use_world ? world_y : room_y;
                    float sample_z = relay_use_world ? world_z : room_z;
                    RGBColor final_color =
                        relay_layer_effect->SampleRelayShadeAt(sample_x, sample_y, sample_z, relay_grid);
                    final_color = relay_layer_effect->PostProcessColorGrid(final_color);
                    transform->led_positions[led_pos_idx].preview_color = final_color;

                    RGBController* mapping_controller = led_position.controller;
                    if(mapping_controller && !mapping_controller->zones.empty() && !mapping_controller->colors.empty() &&
                       led_position.zone_idx < mapping_controller->zones.size())
                    {
                        unsigned int led_global_idx = 0;
                        if(TryGetGlobalLedIndex(mapping_controller,
                                                led_position.zone_idx,
                                                led_position.led_idx,
                                                &led_global_idx))
                        {
                            mapping_controller->colors[led_global_idx] = final_color;
                        }
                    }
                    continue;
                }

                if(IsRelayEmitter(relay_layer_effect, static_cast<int>(ctrl_idx)) &&
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
                    transform->led_positions[led_pos_idx].preview_color = final_color;

                    RGBController* mapping_controller = led_position.controller;
                    if(mapping_controller && !mapping_controller->zones.empty() && !mapping_controller->colors.empty() &&
                       led_position.zone_idx < mapping_controller->zones.size())
                    {
                        unsigned int led_global_idx = 0;
                        if(TryGetGlobalLedIndex(mapping_controller,
                                                led_position.zone_idx,
                                                led_position.led_idx,
                                                &led_global_idx))
                        {
                            mapping_controller->colors[led_global_idx] = final_color;
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
                        if(!effect->SkipsSpatialSampleWarp())
                        {
                            effect->ApplyAxisScale(sample_x, sample_y, sample_z, stack_grid);
                            effect->ApplyEffectRotation(sample_x, sample_y, sample_z, stack_grid);
                        }
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

                transform->led_positions[led_pos_idx].preview_color = final_color;

                RGBController* mapping_controller = led_position.controller;
                if(!mapping_controller || mapping_controller->zones.empty() || mapping_controller->colors.empty())
                {
                    continue;
                }

                if(led_position.zone_idx < mapping_controller->zones.size())
                {
                    unsigned int led_global_idx = 0;
                    if(TryGetGlobalLedIndex(mapping_controller,
                                            led_position.zone_idx,
                                            led_position.led_idx,
                                            &led_global_idx))
                    {
                        mapping_controller->colors[led_global_idx] = final_color;
                    }
                }
            }
        }
        else
        {
            RGBController* controller = transform->controller;
            if(!controller || controller->zones.empty() || controller->colors.empty())
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

                if(led_position.zone_idx >= controller->zones.size())
                {
                    continue;
                }
                unsigned int led_global_idx = 0;
                if(!TryGetGlobalLedIndex(controller, led_position.zone_idx, led_position.led_idx, &led_global_idx))
                {
                    continue;
                }

                if(IsRelayOnlyReceiver(relay_layer_effect, static_cast<int>(ctrl_idx)))
                {
                    const bool relay_use_world = relay_layer_effect->RequiresWorldSpaceCoordinates();
                    const bool relay_world_bounds = relay_layer_effect->UseWorldGridBounds();
                    const GridContext3D& relay_grid = relay_world_bounds ? world_grid : room_grid;
                    float sample_x = relay_use_world ? world_x : room_x;
                    float sample_y = relay_use_world ? world_y : room_y;
                    float sample_z = relay_use_world ? world_z : room_z;
                    RGBColor final_color =
                        relay_layer_effect->SampleRelayShadeAt(sample_x, sample_y, sample_z, relay_grid);
                    final_color = relay_layer_effect->PostProcessColorGrid(final_color);
                    transform->led_positions[led_pos_idx].preview_color = final_color;
                    if(led_global_idx < controller->colors.size())
                    {
                        controller->colors[led_global_idx] = final_color;
                    }
                    continue;
                }

                if(IsRelayEmitter(relay_layer_effect, static_cast<int>(ctrl_idx)) &&
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
                    transform->led_positions[led_pos_idx].preview_color = final_color;
                    if(led_global_idx < controller->colors.size())
                    {
                        controller->colors[led_global_idx] = final_color;
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
                    if(!effect->SkipsSpatialSampleWarp())
                    {
                        effect->ApplyAxisScale(sample_x, sample_y, sample_z, stack_grid);
                        effect->ApplyEffectRotation(sample_x, sample_y, sample_z, stack_grid);
                    }
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

                transform->led_positions[led_pos_idx].preview_color = final_color;

                if(led_global_idx < controller->colors.size())
                {
                    controller->colors[led_global_idx] = final_color;
                }
            }
        }
    }

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

    std::set<RGBController*> updated_physical_controllers;

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

    SpatialRoom::EndEffectRenderFrame();
}

SpatialEffectSettingsLayout OpenRGB3DSpatialTab::settingsLayoutForClass(const std::string& class_name) const
{
    if(class_name == "ScreenMirror")
    {
        return SpatialEffectSettingsLayout::CustomOnly;
    }

    (void)EffectListManager3D::get()->GetEffectInfo(class_name);
    return SpatialEffectSettingsLayout::FullWithTransport;
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


