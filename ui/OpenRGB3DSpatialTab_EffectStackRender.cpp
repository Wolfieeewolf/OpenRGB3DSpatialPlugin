// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "GridSpaceUtils.h"
#include <set>
#include <unordered_set>
#include <algorithm>

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

void OpenRGB3DSpatialTab::RenderEffectStack()
{
    if(controller_transforms.empty())
    {
        return; // No controllers to update
    }

    

    // effect_time is advanced in on_effect_timer_timeout()

    ManualRoomSettings room_settings = MakeManualRoomSettings(use_manual_room_size,
                                                              manual_room_width,
                                                              manual_room_height,
                                                              manual_room_depth);
    GridBounds world_bounds = ComputeGridBounds(room_settings, grid_scale_mm, controller_transforms);
    GridBounds room_bounds  = ComputeRoomAlignedBounds(room_settings, grid_scale_mm, controller_transforms);

    // Create grid contexts
    GridContext3D world_grid(world_bounds.min_x, world_bounds.max_x,
                             world_bounds.min_y, world_bounds.max_y,
                             world_bounds.min_z, world_bounds.max_z,
                             grid_scale_mm);
    GridContext3D room_grid(room_bounds.min_x, room_bounds.max_x,
                            room_bounds.min_y, room_bounds.max_y,
                            room_bounds.min_z, room_bounds.max_z,
                            grid_scale_mm);

    ReferenceMode stack_origin_mode = REF_MODE_ROOM_CENTER;
    Vector3D stack_ref_origin = {0.0f, 0.0f, 0.0f};
    if(effect_origin_combo)
    {
        int origin_index = effect_origin_combo->currentIndex();
        int ref_idx = effect_origin_combo->itemData(origin_index).toInt();
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
        {
            stack_origin_mode = REF_MODE_USER_POSITION;
            stack_ref_origin = reference_points[ref_idx]->GetPosition();
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

    if(active_effects.empty() && current_effect_ui)
    {
        RenderEffectSlot slot;
        slot.effect = current_effect_ui;
        slot.zone_index = ResolveZoneTargetSelection(effect_zone_combo);
        slot.blend_mode = BlendMode::REPLACE;
        active_effects.push_back(slot);
    }

    if(active_effects.empty())
    {
        if(viewport)
        {
            viewport->SetRoomGridColorCallback(nullptr);
            viewport->SetRoomGridColorBuffer(std::vector<RGBColor>());
        }
        return;
    }

    for(size_t idx = 0; idx < active_effects.size(); idx++)
    {
        if(active_effects[idx].effect)
        {
            active_effects[idx].effect->SetGlobalReferencePoint(stack_ref_origin);
            active_effects[idx].effect->SetReferenceMode(stack_origin_mode);
        }
    }

    // Room grid overlay: compute colors once per frame and pass buffer (faster than per-point callback)
    if(viewport && viewport->GetShowRoomGridOverlay())
    {
        int nx = 0, ny = 0, nz = 0;
        viewport->GetRoomGridOverlayDimensions(&nx, &ny, &nz);
        const int step = viewport->GetRoomGridStep();
        const size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
        if(count > 0 && count <= 500000u) // cap to avoid huge allocs
        {
            std::vector<RGBColor> buf(count);
            const float time_val = effect_time;
            for(int ix = 0; ix < nx; ix++)
            {
                const float px = (float)(ix * step);
                for(int iy = 0; iy < ny; iy++)
                {
                    const float py = (float)(iy * step);
                    for(int iz = 0; iz < nz; iz++)
                    {
                        const float pz = (float)(iz * step);
                        RGBColor final_color = ToRGBColor(0, 0, 0);
                        for(const RenderEffectSlot& slot : active_effects)
                        {
                            if(!slot.effect) continue;
                            const bool use_world_bounds = slot.effect->RequiresWorldSpaceCoordinates() && slot.effect->RequiresWorldSpaceGridBounds();
                            const GridContext3D& active_grid = use_world_bounds ? world_grid : room_grid;
                            RGBColor effect_color = slot.effect->CalculateColorGrid(px, py, pz, time_val, active_grid);
                            effect_color = slot.effect->PostProcessColorGrid(effect_color);
                            final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                        }
                        buf[(size_t)ix * (size_t)ny * (size_t)nz + (size_t)iy * (size_t)nz + (size_t)iz] = final_color;
                    }
                }
            }
            viewport->SetRoomGridColorBuffer(buf);
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

        // Handle virtual controllers
        if(transform->virtual_controller && !transform->controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            // Apply effects to each virtual LED
            for(unsigned int mapping_idx = 0; mapping_idx < mappings.size(); mapping_idx++)
            {
                const GridLEDMapping& mapping = mappings[mapping_idx];
                if(mapping_idx >= transform->led_positions.size())
                {
                    continue;
                }

                const LEDPosition3D& led_position = transform->led_positions[mapping_idx];
                const Vector3D& world_pos = led_position.world_position;
                float room_x = led_position.room_position.x;
                float room_y = led_position.room_position.y;
                float room_z = led_position.room_position.z;
                float world_x = world_pos.x;
                float world_y = world_pos.y;
                float world_z = world_pos.z;

                    RGBColor final_color = ToRGBColor(0, 0, 0);
                    for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                    {
                        const RenderEffectSlot& slot = active_effects[effect_idx];
                        SpatialEffect3D* effect = slot.effect;
                        if(!effect)
                        {
                            continue;
                        }

                        bool apply_to_this_controller = false;

                        if(slot.zone_index == -1)
                        {
                            apply_to_this_controller = true;
                        }
                        else if(slot.zone_index <= -1000)
                        {
                            int target_ctrl_idx = -(slot.zone_index + 1000);
                            if(target_ctrl_idx >= 0 && target_ctrl_idx < (int)controller_transforms.size() && target_ctrl_idx == (int)ctrl_idx)
                            {
                                apply_to_this_controller = true;
                            }
                        }
                        else if(zone_manager && slot.zone_index >= 0)
                        {
                            Zone3D* zone = zone_manager->GetZone(slot.zone_index);
                            if(zone)
                            {
                                std::vector<int> zone_controllers = zone->GetControllers();
                                if(std::find(zone_controllers.begin(), zone_controllers.end(), (int)ctrl_idx) != zone_controllers.end())
                                {
                                    apply_to_this_controller = true;
                                }
                            }
                        }

                        if(!apply_to_this_controller)
                        {
                            continue;
                        }

                        

                        const bool requires_world = effect->RequiresWorldSpaceCoordinates();
                        float sample_x = requires_world ? world_x : room_x;
                        float sample_y = requires_world ? world_y : room_y;
                        float sample_z = requires_world ? world_z : room_z;
                        // Coordinate selection:
                        // - World coords: include controller rotation/translation (room-locked field sampling)
                        // - Room coords: ignore controller rotation (controller-locked sampling)
                        //
                        // Normalization (grid bounds) selection:
                        // - For most room-locked effects, normalize against ROOM-ALIGNED bounds (stable)
                        // - Some effects (e.g. ambilight/screen) may prefer WORLD bounds
                        const bool use_world_bounds = requires_world && effect->RequiresWorldSpaceGridBounds();
                        const GridContext3D& active_grid = use_world_bounds ? world_grid : room_grid;

                        RGBColor effect_color = effect->CalculateColorGrid(sample_x, sample_y, sample_z, effect_time, active_grid);
                        effect_color = effect->PostProcessColorGrid(effect_color);

                        final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                    }
                    
                transform->led_positions[mapping_idx].preview_color = final_color;

                if(!mapping.controller)
                {
                    continue;
                }

                std::size_t zone_count = mapping.controller->GetZoneCount();
                std::size_t led_count = mapping.controller->GetLEDCount();
                if(zone_count == 0 || led_count == 0)
                {
                    continue;
                }

                if(mapping.zone_idx < zone_count)
                {
                    unsigned int zone_start = mapping.controller->GetZoneStartIndex(mapping.zone_idx);
                    unsigned int led_global_idx = zone_start + mapping.led_idx;
                    if(led_global_idx < led_count)
                    {
                        mapping.controller->SetColor(led_global_idx, final_color);
                    }
                }
            }

            // Note: Don't call UpdateLEDs() here - we'll do it in spatial order later
        }
        else
        {
            // Handle regular controllers
            RGBController* controller = transform->controller;
            if(!controller)
            {
                continue;
            }

            std::size_t zone_count = controller->GetZoneCount();
            std::size_t led_count = controller->GetLEDCount();
            if(zone_count == 0 || led_count == 0)
            {
                continue;
            }

            // Calculate colors for each LED using cached positions
            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
            {
                LEDPosition3D& led_position = transform->led_positions[led_pos_idx];

                // Use pre-computed positions
                const Vector3D& world_pos = led_position.world_position;
                const Vector3D& room_pos = led_position.room_position;
                float room_x = room_pos.x;
                float room_y = room_pos.y;
                float room_z = room_pos.z;
                float world_x = world_pos.x;
                float world_y = world_pos.y;
                float world_z = world_pos.z;

                // Validate zone index before accessing
                if(led_position.zone_idx >= controller->GetZoneCount())
                {
                    continue; // Skip invalid zone
                }

                // Get the actual LED index for color updates
                unsigned int zone_start = controller->GetZoneStartIndex(led_position.zone_idx);
                unsigned int led_global_idx = zone_start + led_position.led_idx;

                RGBColor final_color = ToRGBColor(0, 0, 0);
                for(size_t effect_idx = 0; effect_idx < active_effects.size(); effect_idx++)
                {
                    const RenderEffectSlot& slot = active_effects[effect_idx];
                    SpatialEffect3D* effect = slot.effect;
                    if(!effect)
                    {
                        continue;
                    }

                    bool apply_to_this_controller = false;

                    if(slot.zone_index == -1)
                    {
                        apply_to_this_controller = true;
                    }
                    else if(slot.zone_index <= -1000)
                    {
                        int target_ctrl_idx = -(slot.zone_index + 1000);
                        if(target_ctrl_idx >= 0 && target_ctrl_idx < (int)controller_transforms.size() && target_ctrl_idx == (int)ctrl_idx)
                        {
                            apply_to_this_controller = true;
                        }
                    }
                    else if(zone_manager && slot.zone_index >= 0)
                    {
                        Zone3D* zone = zone_manager->GetZone(slot.zone_index);
                        if(zone)
                        {
                            std::vector<int> zone_controllers = zone->GetControllers();
                            if(std::find(zone_controllers.begin(), zone_controllers.end(), (int)ctrl_idx) != zone_controllers.end())
                            {
                                apply_to_this_controller = true;
                            }
                        }
                    }

                    if(!apply_to_this_controller)
                    {
                        continue;
                    }

                    const bool requires_world = effect->RequiresWorldSpaceCoordinates();
                    float sample_x = requires_world ? world_x : room_x;
                    float sample_y = requires_world ? world_y : room_y;
                    float sample_z = requires_world ? world_z : room_z;
                    const bool use_world_bounds = requires_world && effect->RequiresWorldSpaceGridBounds();
                    const GridContext3D& active_grid = use_world_bounds ? world_grid : room_grid;

                    RGBColor effect_color = effect->CalculateColorGrid(sample_x, sample_y, sample_z, effect_time, active_grid);
                    effect_color = effect->PostProcessColorGrid(effect_color);

                    final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                }

                transform->led_positions[led_pos_idx].preview_color = final_color;

                if(led_global_idx < colors.size())
                {
                    colors[led_global_idx] = final_color;
                }
            }

            // Note: Don't call UpdateLEDs() here - we'll do it in spatial order later
        }
    }

    // Sort controllers along Y-axis for spatial UpdateLEDs order
    EffectAxis sort_axis = AXIS_Y;
    bool sort_reverse = false;

    // Build sortable keys per controller
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
        if(!sort_reverse) return a.first < b.first;  // ascending
        return a.first > b.first;                    // descending when reversed
    });

    // Update controllers in spatial order (front to back)
    std::set<RGBController*> updated_physical_controllers;

    for(unsigned int i = 0; i < sorted_controllers.size(); i++)
    {
        unsigned int ctrl_idx = sorted_controllers[i].second;
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();

        if(!transform) continue;

        if(transform->virtual_controller && !transform->controller)
        {
            // Update virtual controller's physical controllers
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
            // Update regular controller
            if(updated_physical_controllers.find(transform->controller) == updated_physical_controllers.end())
            {
                transform->controller->UpdateLEDs();
                updated_physical_controllers.insert(transform->controller);
            }
        }
    }

    if(viewport)
    {
        viewport->UpdateColors();
    }
}
