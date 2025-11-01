/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_EffectStackRender.cpp                 |
|                                                           |
|   Effect Stack rendering implementation                  |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include <set>
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
    float grid_min_x = 0.0f, grid_max_x = 0.0f;
    float grid_min_y = 0.0f, grid_max_y = 0.0f;
    float grid_min_z = 0.0f, grid_max_z = 0.0f;

    if(use_manual_room_size)
    {
        /*---------------------------------------------------------*\
        | Use manually configured room dimensions                  |
        | Origin at front-left-floor corner                        |
        | IMPORTANT: Convert millimeters to grid units (/ 10.0f)   |
        \*---------------------------------------------------------*/
        grid_min_x = 0.0f;
        grid_max_x = manual_room_width / grid_scale_mm;
        grid_min_y = 0.0f;
        grid_max_y = manual_room_depth / grid_scale_mm;
        grid_min_z = 0.0f;
        grid_max_z = manual_room_height / grid_scale_mm;
    }
    else
    {
        /*---------------------------------------------------------*\
        | Auto-detect from LED positions                           |
        \*---------------------------------------------------------*/
        bool has_leds = false;

        // Update world positions first
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(transform && transform->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(transform);
            }
        }

        // Find min/max positions from ALL LEDs
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(!transform) continue;

            // Check all LED positions
            for(unsigned int led_idx = 0; led_idx < transform->led_positions.size(); led_idx++)
            {
                float x = transform->led_positions[led_idx].world_position.x;
                float y = transform->led_positions[led_idx].world_position.y;
                float z = transform->led_positions[led_idx].world_position.z;

                if(!has_leds)
                {
                    // Initialize bounds with first LED
                    grid_min_x = grid_max_x = x;
                    grid_min_y = grid_max_y = y;
                    grid_min_z = grid_max_z = z;
                    has_leds = true;
                }
                else
                {
                    // Expand bounds to include this LED
                    if(x < grid_min_x) grid_min_x = x;
                    if(x > grid_max_x) grid_max_x = x;
                    if(y < grid_min_y) grid_min_y = y;
                    if(y > grid_max_y) grid_max_y = y;
                    if(z < grid_min_z) grid_min_z = z;
                    if(z > grid_max_z) grid_max_z = z;
                }
            }
        }

        // Fallback if no LEDs found
        if(!has_leds)
        {
            // Convert default mm to grid units using current scale
            grid_min_x = 0.0f;
            grid_max_x = 1000.0f / grid_scale_mm;  // Default room width
            grid_min_y = 0.0f;
            grid_max_y = 1000.0f / grid_scale_mm;  // Default room depth
            grid_min_z = 0.0f;
            grid_max_z = 1000.0f / grid_scale_mm;  // Default room height
        }
    }

    // Create grid context with room bounds
    GridContext3D grid_context(grid_min_x, grid_max_x, grid_min_y, grid_max_y, grid_min_z, grid_max_z);

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

    

    /*---------------------------------------------------------*\
    | Step 1: Calculate colors for ALL controllers first      |
    | This separates color calculation from hardware updates   |
    \*---------------------------------------------------------*/
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
        }

        // removed controller 0 diagnostic logging

        // Handle virtual controllers
        if(transform->virtual_controller && !transform->controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            // Update cached world positions if dirty
            if(transform->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(transform);
            }

            // Apply effects to each virtual LED
            for(unsigned int mapping_idx = 0; mapping_idx < mappings.size(); mapping_idx++)
            {
                const GridLEDMapping& mapping = mappings[mapping_idx];
                if(!mapping.controller) continue;

                // Get world position
                if(mapping_idx < transform->led_positions.size())
                {
                    float x = transform->led_positions[mapping_idx].world_position.x;
                    float y = transform->led_positions[mapping_idx].world_position.y;
                    float z = transform->led_positions[mapping_idx].world_position.z;

                    

                    // Safety: Ensure controller is still valid
                    if(!mapping.controller || mapping.controller->zones.empty() || mapping.controller->colors.empty())
                    {
                        continue;
                    }

                    /*---------------------------------------------------------*\
                    | Initialize with black (no color)                         |
                    \*---------------------------------------------------------*/
                    RGBColor final_color = ToRGBColor(0, 0, 0);

                    /*---------------------------------------------------------*\
                    | Iterate through effect stack and blend colors            |
                    \*---------------------------------------------------------*/
                    for(unsigned int effect_idx = 0; effect_idx < effect_stack.size(); effect_idx++)
                    {
                        EffectInstance3D* instance = effect_stack[effect_idx].get();

                        /*---------------------------------------------------------*\
                        | Skip disabled effects or effects without object          |
                        \*---------------------------------------------------------*/
                        if(!instance->enabled || !instance->effect)
                        {
                            continue;
                        }
                        // Set origin for this effect instance (world coords provided below)
                        instance->effect->SetGlobalReferencePoint(stack_ref_origin);
                        instance->effect->SetReferenceMode(stack_origin_mode);

                        

                        /*---------------------------------------------------------*\
                        | Check if this effect targets this controller            |
                        | -1 = All Controllers                                     |
                        | 0-999 = Zone index                                       |
                        | -1000 and below = Individual controller (-idx - 1000)   |
                        \*---------------------------------------------------------*/
                        bool apply_to_this_controller = false;

                        if(instance->zone_index == -1)
                        {
                            /*---------------------------------------------------------*\
                            | Apply to all controllers                                 |
                            \*---------------------------------------------------------*/
                            apply_to_this_controller = true;
                        }
                        else if(instance->zone_index <= -1000)
                        {
                            /*---------------------------------------------------------*\
                            | Individual controller targeting                          |
                            \*---------------------------------------------------------*/
                            int target_ctrl_idx = -(instance->zone_index + 1000);
                            if(target_ctrl_idx >= 0 && target_ctrl_idx < (int)controller_transforms.size() && target_ctrl_idx == (int)ctrl_idx)
                            {
                                apply_to_this_controller = true;
                            }
                        }
                        else if(zone_manager && instance->zone_index >= 0)
                        {
                            /*---------------------------------------------------------*\
                            | Zone targeting                                           |
                            \*---------------------------------------------------------*/
                            Zone3D* zone = zone_manager->GetZone(instance->zone_index);
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
                        RGBColor effect_color = instance->effect->CalculateColorGrid(x, y, z, effect_time, grid_context);;
effect_color = instance->effect->PostProcessColorGrid(x, y, z, effect_color, grid_context);

                        /*---------------------------------------------------------*\
                        | Blend with accumulated color                             |
                        \*---------------------------------------------------------*/
                        final_color = BlendColors(final_color, effect_color, instance->blend_mode);
                    }
                    

                    /*---------------------------------------------------------*\
                    | Apply final blended color to LED                        |
                    \*---------------------------------------------------------*/
                    if(mapping.zone_idx < mapping.controller->zones.size())
                    {
                        unsigned int led_global_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                        if(led_global_idx < mapping.controller->colors.size())
                        {
                            mapping.controller->colors[led_global_idx] = final_color;
                        }
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

            // Update cached world positions if dirty
            if(transform->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(transform);
            }

            // Calculate colors for each LED using cached positions
            for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
            {
                LEDPosition3D& led_position = transform->led_positions[led_pos_idx];

                // Use pre-computed world position
                float x = led_position.world_position.x;
                float y = led_position.world_position.y;
                float z = led_position.world_position.z;

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
                for(unsigned int effect_idx = 0; effect_idx < effect_stack.size(); effect_idx++)
                {
                    EffectInstance3D* instance = effect_stack[effect_idx].get();

                    /*---------------------------------------------------------*\
                    | Skip disabled effects or effects without object          |
                    \*---------------------------------------------------------*/
                    if(!instance->enabled || !instance->effect)
                    {
                        continue;
                    }
                    // Set origin for this effect instance (world coords provided below)
                    instance->effect->SetGlobalReferencePoint(stack_ref_origin);
                    instance->effect->SetReferenceMode(stack_origin_mode);

                    /*---------------------------------------------------------*\
                    | Check if this effect targets this controller            |
                    | -1 = All Controllers                                     |
                    | 0-999 = Zone index                                       |
                    | -1000 and below = Individual controller (-idx - 1000)   |
                    \*---------------------------------------------------------*/
                    bool apply_to_this_controller = false;

                    if(instance->zone_index == -1)
                    {
                        /*---------------------------------------------------------*\
                        | Apply to all controllers                                 |
                        \*---------------------------------------------------------*/
                        apply_to_this_controller = true;
                    }
                    else if(instance->zone_index <= -1000)
                    {
                        /*---------------------------------------------------------*\
                        | Individual controller targeting                          |
                        \*---------------------------------------------------------*/
                        int target_ctrl_idx = -(instance->zone_index + 1000);
                        if(target_ctrl_idx >= 0 && target_ctrl_idx < (int)controller_transforms.size() && target_ctrl_idx == (int)ctrl_idx)
                        {
                            apply_to_this_controller = true;
                        }
                    }
                    else if(zone_manager && instance->zone_index >= 0)
                    {
                        /*---------------------------------------------------------*\
                        | Zone targeting                                           |
                        \*---------------------------------------------------------*/
                        Zone3D* zone = zone_manager->GetZone(instance->zone_index);
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
                    RGBColor effect_color = instance->effect->CalculateColorGrid(x, y, z, effect_time, grid_context);
                    effect_color = instance->effect->PostProcessColorGrid(x, y, z, effect_color, grid_context);

                    /*---------------------------------------------------------*\
                    | Blend with accumulated color                             |
                    \*---------------------------------------------------------*/
                    final_color = BlendColors(final_color, effect_color, instance->blend_mode);
                }

                /*---------------------------------------------------------*\
                | Apply final blended color to LED                        |
                \*---------------------------------------------------------*/
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

    // Select an axis and reverse flag from the first enabled stack effect
    EffectAxis sort_axis = AXIS_Y; // default: floor->ceiling (Y-up)
    bool sort_reverse = false;
    for(unsigned int ei = 0; ei < effect_stack.size(); ei++)
    {
        EffectInstance3D* inst = effect_stack[ei].get();
        if(inst && inst->enabled && inst->effect)
        {
            sort_axis = inst->effect->GetAxis();
            sort_reverse = inst->effect->GetReverse();
            break;
        }
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
