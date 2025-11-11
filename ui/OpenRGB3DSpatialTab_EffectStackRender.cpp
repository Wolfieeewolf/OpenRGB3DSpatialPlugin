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
    /*---------------------------------------------------------*\
    | Safety: Check if we have any controllers                |
    \*---------------------------------------------------------*/
    if(controller_transforms.empty())
    {
        return; // No controllers to update
    }

    

    /*---------------------------------------------------------*\
    | Update effect time                                       |
    \*---------------------------------------------------------*/
    // effect_time is advanced in on_effect_timer_timeout()

    /*---------------------------------------------------------*\
    | Calculate room bounds for effects                        |
    | Origin: Front-Left-Floor Corner (0,0,0)                 |
    | Uses manual room dimensions if enabled, otherwise        |
    | auto-detects from LED positions                          |
    \*---------------------------------------------------------*/
    ManualRoomSettings room_settings = MakeManualRoomSettings(use_manual_room_size,
                                                              manual_room_width,
                                                              manual_room_height,
                                                              manual_room_depth);
    GridBounds bounds = ComputeGridBounds(room_settings, grid_scale_mm, controller_transforms);

    // Create grid context with room bounds
    GridContext3D grid_context(bounds.min_x, bounds.max_x,
                               bounds.min_y, bounds.max_y,
                               bounds.min_z, bounds.max_z,
                               grid_scale_mm);

    /*---------------------------------------------------------*\
    | Configure effect origin for all stack effects            |
    | Uses selected reference point (if any) or room center    |
    \*---------------------------------------------------------*/
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

    

    /*---------------------------------------------------------*\
    | Step 1: Calculate colors for ALL controllers first      |
    | This separates color calculation from hardware updates   |
    \*---------------------------------------------------------*/
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

                const Vector3D& world_pos = transform->led_positions[mapping_idx].effect_world_position;
                float x = world_pos.x;
                float y = world_pos.y;
                float z = world_pos.z;

                    /*---------------------------------------------------------*\
                    | Initialize with black (no color)                         |
                    \*---------------------------------------------------------*/
                    RGBColor final_color = ToRGBColor(0, 0, 0);

                    /*---------------------------------------------------------*\
                    | Iterate through effect stack and blend colors            |
                    \*---------------------------------------------------------*/
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

                        

                        /*---------------------------------------------------------*\
                        | Calculate effect color                                   |
                        \*---------------------------------------------------------*/
                        RGBColor effect_color = effect->CalculateColorGrid(x, y, z, effect_time, grid_context);
                        effect_color = effect->PostProcessColorGrid(x, y, z, effect_color, grid_context);

                        /*---------------------------------------------------------*\
                        | Blend with accumulated color                             |
                        \*---------------------------------------------------------*/
                        final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                    }
                    
                transform->led_positions[mapping_idx].preview_color = final_color;

                if(!mapping.controller || mapping.controller->zones.empty() || mapping.controller->colors.empty())
                {
                    continue;
                }

                if(mapping.zone_idx < mapping.controller->zones.size())
                {
                    unsigned int led_global_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                    if(led_global_idx < mapping.controller->colors.size())
                    {
                        mapping.controller->colors[led_global_idx] = final_color;
                    }
                }
            }

            // Note: Don't call UpdateLEDs() here - we'll do it in spatial order later
        }
        else
        {
            // Handle regular controllers
            RGBController* controller = transform->controller;
            if(!controller || controller->zones.empty() || controller->colors.empty())
            {
                continue;
            }

            // Calculate colors for each LED using cached positions
            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
            {
                LEDPosition3D& led_position = transform->led_positions[led_pos_idx];

                // Use pre-computed world position
                const Vector3D& world_pos = led_position.effect_world_position;
                float x = world_pos.x;
                float y = world_pos.y;
                float z = world_pos.z;

                // Validate zone index before accessing
                if(led_position.zone_idx >= controller->zones.size())
                {
                    continue; // Skip invalid zone
                }

                // Get the actual LED index for color updates
                unsigned int led_global_idx = controller->zones[led_position.zone_idx].start_idx + led_position.led_idx;

                /*---------------------------------------------------------*\
                | Initialize with black (no color)                         |
                \*---------------------------------------------------------*/
                RGBColor final_color = ToRGBColor(0, 0, 0);

                /*---------------------------------------------------------*\
                | Iterate through effect stack and blend colors            |
                \*---------------------------------------------------------*/
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

                    RGBColor effect_color = effect->CalculateColorGrid(x, y, z, effect_time, grid_context);
                    effect_color = effect->PostProcessColorGrid(x, y, z, effect_color, grid_context);

                    final_color = BlendColors(final_color, effect_color, slot.blend_mode);
                }

                transform->led_positions[led_pos_idx].preview_color = final_color;

                if(led_global_idx < controller->colors.size())
                {
                    controller->colors[led_global_idx] = final_color;
                }
            }

            // Note: Don't call UpdateLEDs() here - we'll do it in spatial order later
        }
    }

    /*---------------------------------------------------------*\
    | Step 2: Axis-aware UpdateLEDs ordering                  |
    | Sort controllers along the effect axis to reduce        |
    | perceived temporal skew between devices.                |
    \*---------------------------------------------------------*/

    // Select an axis and reverse flag from the first active effect
    EffectAxis sort_axis = AXIS_Y; // default: floor->ceiling (Y-up)
    bool sort_reverse = false;
    if(!active_effects.empty() && active_effects[0].effect)
    {
        sort_axis = active_effects[0].effect->GetAxis();
        sort_reverse = active_effects[0].effect->GetReverse();
    }

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

    /*---------------------------------------------------------*\
    | Update viewport to show changes                          |
    \*---------------------------------------------------------*/
    if(viewport)
    {
        viewport->UpdateColors();
    }
}
#include <algorithm>
