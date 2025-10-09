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
#include "LogManager.h"
#include <set>

void OpenRGB3DSpatialTab::RenderEffectStack()
{
    /*---------------------------------------------------------*\
    | Safety: Check if we have any controllers                |
    \*---------------------------------------------------------*/
    if(controller_transforms.empty())
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] RenderEffectStack: No controllers!");
        return; // No controllers to update
    }

    static int log_counter = 0;
    if(log_counter == 0)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] RenderEffectStack: %u controllers, %u effects",
                    (unsigned int)controller_transforms.size(), (unsigned int)effect_stack.size());
    }
    log_counter = (log_counter + 1) % 30; // Log every 30 frames (once per second)

    /*---------------------------------------------------------*\
    | Update effect time                                       |
    \*---------------------------------------------------------*/
    effect_time += 0.033f; // ~30 FPS

    /*---------------------------------------------------------*\
    | Calculate grid bounds for effect calculations            |
    \*---------------------------------------------------------*/
    if(custom_grid_x < 1) custom_grid_x = 10;
    if(custom_grid_y < 1) custom_grid_y = 10;
    if(custom_grid_z < 1) custom_grid_z = 10;

    int half_x = custom_grid_x / 2;
    int half_y = custom_grid_y / 2;
    int half_z = custom_grid_z / 2;

    float grid_min_x = -half_x;
    float grid_max_x = custom_grid_x - half_x - 1;
    float grid_min_y = -half_y;
    float grid_max_y = custom_grid_y - half_y - 1;
    float grid_min_z = -half_z;
    float grid_max_z = custom_grid_z - half_z - 1;

    GridContext3D grid_context(grid_min_x, grid_max_x, grid_min_y, grid_max_y, grid_min_z, grid_max_z);

    /*---------------------------------------------------------*\
    | Render each controller                                   |
    \*---------------------------------------------------------*/
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Controller %u is NULL!", ctrl_idx);
            continue;
        }

        /*---------------------------------------------------------*\
        | Log Controller 0 details for diagnosis                   |
        \*---------------------------------------------------------*/
        static bool logged_ctrl_0 = false;
        if(ctrl_idx == 0 && !logged_ctrl_0)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Controller 0 details: virtual=%d, regular=%d, name=%s",
                       transform->virtual_controller != nullptr,
                       transform->controller != nullptr,
                       transform->controller ? transform->controller->name.c_str() : "(no controller)");
            logged_ctrl_0 = true;
        }

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

                    // Check if LED is within grid bounds
                    if(x >= grid_min_x && x <= grid_max_x &&
                       y >= grid_min_y && y <= grid_max_y &&
                       z >= grid_min_z && z <= grid_max_z)
                    {
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
                        static int effect_log_counter = 0;
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

                            if(effect_log_counter == 0 && effect_idx == 0)
                            {
                                LOG_WARNING("[OpenRGB3DSpatialPlugin] Effect %u: zone_index=%d, enabled=%d, has_effect=%d",
                                           effect_idx, instance->zone_index, instance->enabled, instance->effect.get() != nullptr);
                            }

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
                                if(effect_log_counter == 0 && effect_idx == 0)
                                {
                                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Effect %u not applied to controller %u", effect_idx, ctrl_idx);
                                }
                                continue;
                            }

                            if(effect_log_counter == 0 && effect_idx == 0)
                            {
                                LOG_WARNING("[OpenRGB3DSpatialPlugin] Applying effect %u to controller %u", effect_idx, ctrl_idx);
                            }

                            /*---------------------------------------------------------*\
                            | Calculate effect color                                   |
                            \*---------------------------------------------------------*/
                            RGBColor effect_color = instance->effect->CalculateColorGrid(x, y, z, effect_time, grid_context);

                            /*---------------------------------------------------------*\
                            | Blend with accumulated color                             |
                            \*---------------------------------------------------------*/
                            final_color = BlendColors(final_color, effect_color, instance->blend_mode);
                        }
                        effect_log_counter = (effect_log_counter + 1) % 30;

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
            }

            /*---------------------------------------------------------*\
            | Update the virtual controller's physical controllers     |
            \*---------------------------------------------------------*/
            std::set<RGBController*> updated_controllers;

            for(unsigned int mapping_idx = 0; mapping_idx < mappings.size(); mapping_idx++)
            {
                if(mappings[mapping_idx].controller && updated_controllers.find(mappings[mapping_idx].controller) == updated_controllers.end())
                {
                    mappings[mapping_idx].controller->UpdateLEDs();
                    updated_controllers.insert(mappings[mapping_idx].controller);
                }
            }
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

                // Only apply effects to LEDs within the grid bounds
                if(x >= grid_min_x && x <= grid_max_x &&
                   y >= grid_min_y && y <= grid_max_y &&
                   z >= grid_min_z && z <= grid_max_z)
                {
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
            }

            /*---------------------------------------------------------*\
            | Update the controller                                    |
            \*---------------------------------------------------------*/
            controller->UpdateLEDs();
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
