/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_ObjectCreator.cpp                   |
|                                                         |
|   Controller/object creator and display management      |
|                                                         |
|   Date: 2025-10-07                                      |
|                                                         |
|   This file is part of the OpenRGB project              |
|   SPDX-License-Identifier: GPL-2.0-only                 |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"
#include "DisplayPlaneManager.h"
#include "ScreenCaptureManager.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "SettingsManager.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
#include <nlohmann/json.hpp>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <set>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <system_error>

namespace filesystem = std::filesystem;


void OpenRGB3DSpatialTab::SetObjectCreatorStatus(const QString& message, bool is_error)
{
    if(!object_creator_status_label)
    {
        return;
    }

    if(message.isEmpty())
    {
        object_creator_status_label->clear();
        object_creator_status_label->setVisible(false);
        return;
    }

    object_creator_status_label->setVisible(true);
    QString color = is_error ? "#c0392b" : "#2d9cdb";
    object_creator_status_label->setStyleSheet(QString("color: %1; font-size: 11px;").arg(color));
    object_creator_status_label->setText(message);
}


void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager)
    {
        return;
    }

    UpdateAvailableControllersList();

    viewport->SetControllerTransforms(&controller_transforms);
}

void OpenRGB3DSpatialTab::UpdateAvailableControllersList()
{
    available_controllers_list->clear();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        int unassigned_zones = GetUnassignedZoneCount(controllers[i]);
        int unassigned_leds = GetUnassignedLEDCount(controllers[i]);

        if(unassigned_leds > 0)
        {
            QString display_text = QString::fromStdString(controllers[i]->name) +
                                   QString(" [%1 zones, %2 LEDs available]").arg(unassigned_zones).arg(unassigned_leds);
            available_controllers_list->addItem(display_text);
        }
    }

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_controllers[i]->GetName()));
    }

    // Also update the custom controllers list
    UpdateCustomControllersList();
}

void OpenRGB3DSpatialTab::UpdateCustomControllersList()
{
    custom_controllers_list->clear();

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        custom_controllers_list->addItem(QString::fromStdString(virtual_controllers[i]->GetName()));
    }
}

void OpenRGB3DSpatialTab::UpdateDeviceList()
{
    LoadDevices();
}

void OpenRGB3DSpatialTab::on_controller_selected(int index)
{
    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->clearSelection();
    }
    current_display_plane_index = -1;
    if(viewport) viewport->SelectDisplayPlane(-1);

    if(index >= 0 && index < (int)controller_transforms.size())
    {
        controller_list->setCurrentRow(index);

        ControllerTransform* ctrl = controller_transforms[index].get();

        // Block signals to prevent feedback loops
        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);
        rot_x_spin->blockSignals(true);
        rot_y_spin->blockSignals(true);
        rot_z_spin->blockSignals(true);
        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);
        rot_x_slider->blockSignals(true);
        rot_y_slider->blockSignals(true);
        rot_z_slider->blockSignals(true);

        pos_x_spin->setValue(ctrl->transform.position.x);
        pos_y_spin->setValue(ctrl->transform.position.y);
        pos_z_spin->setValue(ctrl->transform.position.z);
        rot_x_spin->setValue(ctrl->transform.rotation.x);
        rot_y_spin->setValue(ctrl->transform.rotation.y);
        rot_z_spin->setValue(ctrl->transform.rotation.z);

        pos_x_slider->setValue((int)(ctrl->transform.position.x * 10));
        float constrained_y = std::max(ctrl->transform.position.y, (float)0.0f);
        pos_y_slider->setValue((int)(constrained_y * 10));
        pos_z_slider->setValue((int)(ctrl->transform.position.z * 10));
        rot_x_slider->setValue((int)(ctrl->transform.rotation.x));
        rot_y_slider->setValue((int)(ctrl->transform.rotation.y));
        rot_z_slider->setValue((int)(ctrl->transform.rotation.z));

        // Unblock signals
        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);
        rot_x_spin->blockSignals(false);
        rot_y_spin->blockSignals(false);
        rot_z_spin->blockSignals(false);
        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);
        rot_x_slider->blockSignals(false);
        rot_y_slider->blockSignals(false);
        rot_z_slider->blockSignals(false);

        // Clear reference point selection when controller is selected
        reference_points_list->blockSignals(true);
        reference_points_list->clearSelection();
        reference_points_list->blockSignals(false);

        // Enable rotation controls - controllers have rotation
        rot_x_slider->setEnabled(true);
        rot_y_slider->setEnabled(true);
        rot_z_slider->setEnabled(true);
        rot_x_spin->setEnabled(true);
        rot_y_spin->setEnabled(true);
        rot_z_spin->setEnabled(true);

        // Update LED spacing controls
        if(edit_led_spacing_x_spin)
        {
            edit_led_spacing_x_spin->setEnabled(true);
            edit_led_spacing_x_spin->blockSignals(true);
            edit_led_spacing_x_spin->setValue(ctrl->led_spacing_mm_x);
            edit_led_spacing_x_spin->blockSignals(false);
        }
        if(edit_led_spacing_y_spin)
        {
            edit_led_spacing_y_spin->setEnabled(true);
            edit_led_spacing_y_spin->blockSignals(true);
            edit_led_spacing_y_spin->setValue(ctrl->led_spacing_mm_y);
            edit_led_spacing_y_spin->blockSignals(false);
        }
        if(edit_led_spacing_z_spin)
        {
            edit_led_spacing_z_spin->setEnabled(true);
            edit_led_spacing_z_spin->blockSignals(true);
            edit_led_spacing_z_spin->setValue(ctrl->led_spacing_mm_z);
            edit_led_spacing_z_spin->blockSignals(false);
        }
        if(apply_spacing_button)
        {
            apply_spacing_button->setEnabled(true);
        }
    }
    else if(index == -1)
    {
        controller_list->setCurrentRow(-1);

        // Disable LED spacing controls
        if(edit_led_spacing_x_spin) edit_led_spacing_x_spin->setEnabled(false);
        if(edit_led_spacing_y_spin) edit_led_spacing_y_spin->setEnabled(false);
        if(edit_led_spacing_z_spin) edit_led_spacing_z_spin->setEnabled(false);
        if(apply_spacing_button) apply_spacing_button->setEnabled(false);
    }

    UpdateSelectionInfo();
    RefreshDisplayPlaneDetails();
}

void OpenRGB3DSpatialTab::on_controller_position_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
        ctrl->transform.position.x = x;
        ctrl->transform.position.y = y;
        ctrl->transform.position.z = z;
        ctrl->world_positions_dirty = true;

        // Block signals to prevent feedback loops
        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);
        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);

        pos_x_spin->setValue(x);
        pos_y_spin->setValue(y);
        pos_z_spin->setValue(z);

        pos_x_slider->setValue((int)(x * 10));
        float constrained_y = std::max(y, (float)0.0f);
        pos_y_slider->setValue((int)(constrained_y * 10));
        pos_z_slider->setValue((int)(z * 10));

        // Unblock signals
        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);
        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);
    }
}

void OpenRGB3DSpatialTab::on_controller_rotation_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
        ctrl->transform.rotation.x = x;
        ctrl->transform.rotation.y = y;
        ctrl->transform.rotation.z = z;
        ctrl->world_positions_dirty = true;

        // Block signals to prevent feedback loops
        rot_x_spin->blockSignals(true);
        rot_y_spin->blockSignals(true);
        rot_z_spin->blockSignals(true);
        rot_x_slider->blockSignals(true);
        rot_y_slider->blockSignals(true);
        rot_z_slider->blockSignals(true);

        rot_x_spin->setValue(x);
        rot_y_spin->setValue(y);
        rot_z_spin->setValue(z);

        rot_x_slider->setValue((int)x);
        rot_y_slider->setValue((int)y);
        rot_z_slider->setValue((int)z);

        // Unblock signals
        rot_x_spin->blockSignals(false);
        rot_y_spin->blockSignals(false);
        rot_z_spin->blockSignals(false);
        rot_x_slider->blockSignals(false);
        rot_y_slider->blockSignals(false);
        rot_z_slider->blockSignals(false);
    }
}


void OpenRGB3DSpatialTab::on_start_effect_clicked()
{
    /*---------------------------------------------------------*\
    | Check if a stack preset is selected                      |
    \*---------------------------------------------------------*/
    if(effect_combo && effect_combo->currentIndex() > 0)
    {
        QVariant data = effect_combo->itemData(effect_combo->currentIndex());
        if(data.isValid() && data.toInt() < 0)
        {
            /*---------------------------------------------------------*\
            | This is a stack preset - load it and start rendering     |
            \*---------------------------------------------------------*/
            int preset_index = -(data.toInt() + 1);
            if(preset_index >= 0 && preset_index < (int)stack_presets.size())
            {
                StackPreset3D* preset = stack_presets[preset_index].get();

                /*---------------------------------------------------------*\
                | Clear current stack                                      |
                \*---------------------------------------------------------*/
                effect_stack.clear();

                /*---------------------------------------------------------*\
                | Load preset effects (deep copy)                          |
                \*---------------------------------------------------------*/
                

                for(unsigned int i = 0; i < preset->effect_instances.size(); i++)
                {
                    nlohmann::json instance_json = preset->effect_instances[i]->ToJson();
                    std::unique_ptr<EffectInstance3D> copied_instance = EffectInstance3D::FromJson(instance_json);
                    if(copied_instance)
                    {
                        // Connect ScreenMirror3D screen preview signal to viewport
                        if (copied_instance->effect_class_name == "ScreenMirror3D" && copied_instance->effect)
                        {
                            ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(copied_instance->effect.get());
                            if (screen_mirror && viewport)
                            {
                                connect(screen_mirror, &ScreenMirror3D::ScreenPreviewChanged,
                                        viewport, &LEDViewport3D::SetShowScreenPreview);
                                screen_mirror->SetReferencePoints(&reference_points);
                            }
                        }

                        effect_stack.push_back(std::move(copied_instance));
                    }
                    else
                    {
                    }
                }

                /*---------------------------------------------------------*\
                | Update Effect Stack tab UI (if visible)                  |
                \*---------------------------------------------------------*/
                UpdateEffectStackList();
                if(!effect_stack.empty())
                {
                    effect_stack_list->setCurrentRow(0);
                }

                /*---------------------------------------------------------*\
                | Put all controllers in direct control mode               |
                \*---------------------------------------------------------*/
                bool has_valid_controller = false;
                for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
                {
                    ControllerTransform* transform = controller_transforms[ctrl_idx].get();
                    if(!transform)
                    {
                        continue;
                    }

                    // Handle virtual controllers - they map to physical controllers
                    if(transform->virtual_controller)
                    {
                        VirtualController3D* virtual_ctrl = transform->virtual_controller;
                        const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

                        // Set all physical controllers mapped to this virtual controller to direct mode
                        std::set<RGBController*> controllers_to_set;
                        for(unsigned int i = 0; i < mappings.size(); i++)
                        {
                            if(mappings[i].controller)
                            {
                                controllers_to_set.insert(mappings[i].controller);
                            }
                        }

                        for(std::set<RGBController*>::iterator it = controllers_to_set.begin(); it != controllers_to_set.end(); ++it)
                        {
                            (*it)->SetCustomMode();
                            has_valid_controller = true;
                        }
                        continue;
                    }

                    // Handle regular controllers
                    RGBController* controller = transform->controller;
                    if(!controller)
                    {
                        continue;
                    }

                    controller->SetCustomMode();
                    has_valid_controller = true;
                }

                

                /*---------------------------------------------------------*\
                | Start effect timer                                       |
                \*---------------------------------------------------------*/
                if(effect_timer && !effect_timer->isActive())
                {
                    effect_time = 0.0f;
                    effect_elapsed.restart();
                    // Compute timer interval from stack effects (use highest requested FPS)
                    unsigned int target_fps = 30;
                    for(size_t i = 0; i < effect_stack.size(); i++)
                    {
                        if(effect_stack[i] && effect_stack[i]->effect && effect_stack[i]->enabled)
                        {
                            unsigned int f = effect_stack[i]->effect->GetTargetFPSSetting();
                            if(f > target_fps) target_fps = f;
                        }
                    }
                    if(target_fps < 1) target_fps = 30;
                    int interval_ms = (int)(1000 / target_fps);
                    if(interval_ms < 1) interval_ms = 1;
                    effect_timer->start(interval_ms);
                    
                }
                else
                {
                    
                }

                /*---------------------------------------------------------*\
                | Update button states                                     |
                \*---------------------------------------------------------*/
                start_effect_button->setEnabled(false);
                stop_effect_button->setEnabled(true);

                
                return;
            }
        }
    }

    /*---------------------------------------------------------*\
    | Regular effect handling                                  |
    \*---------------------------------------------------------*/
    if(!current_effect_ui)
    {
        QMessageBox::warning(this, "No Effect Selected", "Please select an effect before starting.");
        return;
    }

    if(controller_transforms.empty())
    {
        QMessageBox::warning(this, "No Controllers", "Please add controllers to the 3D scene before starting effects.");
        return;
    }

    /*---------------------------------------------------------*\
    | Put all controllers in direct control mode               |
    \*---------------------------------------------------------*/
    bool has_valid_controller = false;
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
        }

        // Handle virtual controllers - they map to physical controllers
        if(transform->virtual_controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            // Set all physical controllers mapped to this virtual controller to direct mode
            std::set<RGBController*> controllers_to_set;
            for(unsigned int i = 0; i < mappings.size(); i++)
            {
                if(mappings[i].controller)
                {
                    controllers_to_set.insert(mappings[i].controller);
                }
            }

            for(std::set<RGBController*>::iterator it = controllers_to_set.begin(); it != controllers_to_set.end(); ++it)
            {
                (*it)->SetCustomMode();
                has_valid_controller = true;
            }
            continue;
        }

        // Handle regular controllers
        RGBController* controller = transform->controller;
        if(!controller)
        {
            continue;
        }

        controller->SetCustomMode();
        has_valid_controller = true;
    }

    if(!has_valid_controller)
    {
        QMessageBox::warning(this, "No Valid Controllers", "No controllers are available for effects.");
        return;
    }

    /*---------------------------------------------------------*\
    | Start the effect                                         |
    \*---------------------------------------------------------*/
    effect_running = true;
    effect_time = 0.0f;
    effect_elapsed.restart();

    /*---------------------------------------------------------*\
    | Set timer interval from effect FPS (default 30 FPS)      |
    \*---------------------------------------------------------*/
    {
        unsigned int target_fps = current_effect_ui ? current_effect_ui->GetTargetFPSSetting() : 30;
        if(target_fps < 1) target_fps = 30;
        int interval_ms = (int)(1000 / target_fps);
        if(interval_ms < 1) interval_ms = 1;
        effect_timer->start(interval_ms);
    }

    /*---------------------------------------------------------*\
    | Update UI                                                |
    \*---------------------------------------------------------*/
    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);

}

void OpenRGB3DSpatialTab::on_stop_effect_clicked()
{
    /*---------------------------------------------------------*\
    | Check if a stack preset is currently running             |
    \*---------------------------------------------------------*/
    if(effect_combo && effect_combo->currentIndex() > 0)
    {
        QVariant data = effect_combo->itemData(effect_combo->currentIndex());
        if(data.isValid() && data.toInt() < 0)
        {
            /*---------------------------------------------------------*\
            | This is a stack preset - stop and clear the stack        |
            \*---------------------------------------------------------*/
            effect_timer->stop();

            /*---------------------------------------------------------*\
            | Clear effect stack                                       |
            \*---------------------------------------------------------*/
            effect_stack.clear();
            UpdateEffectStackList();

            /*---------------------------------------------------------*\
            | Update button states                                     |
            \*---------------------------------------------------------*/
            start_effect_button->setEnabled(true);
            stop_effect_button->setEnabled(false);

            return;
        }
    }

    /*---------------------------------------------------------*\
    | Regular effect stop handling                             |
    \*---------------------------------------------------------*/
    effect_running = false;
    effect_timer->stop();

    /*---------------------------------------------------------*\
    | Update UI                                                |
    \*---------------------------------------------------------*/
    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);
}

void OpenRGB3DSpatialTab::on_effect_updated()
{
    viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_effect_timer_timeout()
{
    // Advance time based on real elapsed time for smooth animation
    qint64 ms = effect_elapsed.isValid() ? effect_elapsed.restart() : 33;
    if(ms <= 0) { ms = 33; }
    float dt = static_cast<float>(ms) / 1000.0f;
    if(dt > 0.1f) dt = 0.1f; // clamp spikes
    effect_time += dt;
    /*---------------------------------------------------------*\
    | Check if we should render effect stack instead of       |
    | single effect                                            |
    \*---------------------------------------------------------*/
    bool has_stack_effects = false;
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        if(effect_stack[i]->enabled && effect_stack[i]->effect)
        {
            has_stack_effects = true;
            break;
        }
    }

    if(has_stack_effects)
    {
        /*---------------------------------------------------------*\
        | Render effect stack (multi-effect mode)                 |
        \*---------------------------------------------------------*/
        
        RenderEffectStack();
        return;
    }
    else
    {
        
    }

    /*---------------------------------------------------------*\
    | Fall back to single effect rendering (Effects tab)       |
    \*---------------------------------------------------------*/
    

    if(!effect_running || !current_effect_ui)
    {
        return;
    }

    

    /*---------------------------------------------------------*\
    | Safety: Check if we have any controllers                |
    \*---------------------------------------------------------*/
    if(controller_transforms.empty())
    {
        return; // No controllers to update
    }

    /*---------------------------------------------------------*\
    | Safety: Verify effect timer and viewport are valid      |
    \*---------------------------------------------------------*/
    if(!effect_timer || !viewport)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect timer or viewport is null, stopping effect");
        on_stop_effect_clicked();
        return;
    }

    // effect_time already advanced at timer start

    /*---------------------------------------------------------*\
    | Calculate room bounds for effects                        |
    | Uses same corner-origin system as Effect Stack          |
    \*---------------------------------------------------------*/
    float grid_min_x = 0.0f, grid_max_x = 0.0f;
    float grid_min_y = 0.0f, grid_max_y = 0.0f;
    float grid_min_z = 0.0f, grid_max_z = 0.0f;

    if(use_manual_room_size)
    {
        /*---------------------------------------------------------*\
        | Use manually configured room dimensions                  |
        | Origin at front-left-floor corner (0,0,0)               |
        | IMPORTANT: Convert millimeters to grid units (/ 10.0f)  |
        | LED world_position uses grid units, not millimeters!    |
        \*---------------------------------------------------------*/
        grid_min_x = 0.0f;
        grid_max_x = manual_room_width / grid_scale_mm;  // Convert mm to grid units
        grid_min_y = 0.0f;
        grid_max_y = manual_room_depth / grid_scale_mm;  // Convert mm to grid units
        grid_min_z = 0.0f;
        grid_max_z = manual_room_height / grid_scale_mm; // Convert mm to grid units

        
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

            for(unsigned int led_idx = 0; led_idx < transform->led_positions.size(); led_idx++)
            {
                float x = transform->led_positions[led_idx].world_position.x;
                float y = transform->led_positions[led_idx].world_position.y;
                float z = transform->led_positions[led_idx].world_position.z;

                if(!has_leds)
                {
                    grid_min_x = grid_max_x = x;
                    grid_min_y = grid_max_y = y;
                    grid_min_z = grid_max_z = z;
                    has_leds = true;
                }
                else
                {
                    if(x < grid_min_x) grid_min_x = x;
                    if(x > grid_max_x) grid_max_x = x;
                    if(y < grid_min_y) grid_min_y = y;
                    if(y > grid_max_y) grid_max_y = y;
                    if(z < grid_min_z) grid_min_z = z;
                    if(z > grid_max_z) grid_max_z = z;
                }
            }
        }

        if(!has_leds)
        {
            // Fallback if no LEDs found (convert default mm to grid units)
            grid_min_x = 0.0f;
            grid_max_x = 1000.0f / grid_scale_mm;
            grid_min_y = 0.0f;
            grid_max_y = 1000.0f / grid_scale_mm;
            grid_min_z = 0.0f;
            grid_max_z = 1000.0f / grid_scale_mm;
        }

        
    }

    // Create grid context for effects
    GridContext3D grid_context(grid_min_x, grid_max_x, grid_min_y, grid_max_y, grid_min_z, grid_max_z);

    

    /*---------------------------------------------------------*\
    | Configure effect origin mode                             |
    | Pass absolute world coords to CalculateColorGrid         |
    \*---------------------------------------------------------*/
    if(current_effect_ui)
    {
        ReferenceMode mode = REF_MODE_ROOM_CENTER;
        Vector3D ref_origin = {0.0f, 0.0f, 0.0f};

        if(effect_origin_combo)
        {
            int index = effect_origin_combo->currentIndex();
            int ref_point_idx = effect_origin_combo->itemData(index).toInt();
            if(ref_point_idx >= 0 && ref_point_idx < (int)reference_points.size())
            {
                VirtualReferencePoint3D* ref_point = reference_points[ref_point_idx].get();
                ref_origin = ref_point->GetPosition();
                mode = REF_MODE_USER_POSITION;
                
            }
            else
            {
                mode = REF_MODE_ROOM_CENTER;
                
            }
        }

        current_effect_ui->SetGlobalReferencePoint(ref_origin);
        current_effect_ui->SetReferenceMode(mode);
    }

    /*---------------------------------------------------------*\
    | Determine which controllers to apply effects to based   |
    | on the selected zone                                     |
    \*---------------------------------------------------------*/
    std::vector<int> allowed_controllers;

    if(!effect_zone_combo || !zone_manager)
    {
        // Safety: If UI not ready, allow all controllers
        for(unsigned int i = 0; i < controller_transforms.size(); i++)
        {
            allowed_controllers.push_back(i);
        }
    }
    else
    {
        int combo_idx = effect_zone_combo->currentIndex();
        int zone_count = zone_manager ? zone_manager->GetZoneCount() : 0;

        /*---------------------------------------------------------*\
        | Safety: If index is invalid, default to all controllers |
        \*---------------------------------------------------------*/
        if(combo_idx < 0 || combo_idx >= effect_zone_combo->count())
        {
            for(unsigned int i = 0; i < controller_transforms.size(); i++)
            {
                allowed_controllers.push_back(i);
            }
        }
        else if(combo_idx == 0)
        {
            /*---------------------------------------------------------*\
            | "All Controllers" selected - allow all                   |
            \*---------------------------------------------------------*/
            for(unsigned int i = 0; i < controller_transforms.size(); i++)
            {
                allowed_controllers.push_back(i);
            }
        }
        else if(zone_count > 0 && combo_idx >= 1 && combo_idx <= zone_count)
        {
            /*---------------------------------------------------------*\
            | Zone selected - get controllers from zone manager       |
            | Zone indices: combo index 1 = zone 0, etc.              |
            \*---------------------------------------------------------*/
            Zone3D* zone = zone_manager->GetZone(combo_idx - 1);
            if(zone)
            {
                allowed_controllers = zone->GetControllers();
            }
            else
            {
                /*---------------------------------------------------------*\
                | Zone not found - allow all as fallback                  |
                \*---------------------------------------------------------*/
                for(unsigned int i = 0; i < controller_transforms.size(); i++)
                {
                    allowed_controllers.push_back(i);
                }
            }
        }
        else
        {
            /*---------------------------------------------------------*\
            | Individual controller selected                           |
            | Combo index = zone_count + 1 + controller_index          |
            \*---------------------------------------------------------*/
            int ctrl_idx = combo_idx - zone_count - 1;
            if(ctrl_idx >= 0 && ctrl_idx < (int)controller_transforms.size())
            {
                allowed_controllers.push_back(ctrl_idx);
            }
            else
            {
                /*---------------------------------------------------------*\
                | Invalid controller index - allow all as fallback        |
                \*---------------------------------------------------------*/
                for(unsigned int i = 0; i < controller_transforms.size(); i++)
                {
                    allowed_controllers.push_back(i);
                }
            }
        }
    }

    // Now map each controller's LEDs to the unified grid and apply effects
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        // Skip controllers not in the selected zone
        if(std::find(allowed_controllers.begin(), allowed_controllers.end(), (int)ctrl_idx) == allowed_controllers.end())
        {
            continue; // Controller not in selected zone
        }

        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
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

                // Use pre-computed world position from cached LED positions
                if(mapping_idx < transform->led_positions.size())
                {
                    float x = transform->led_positions[mapping_idx].world_position.x;
                    float y = transform->led_positions[mapping_idx].world_position.y;
                    float z = transform->led_positions[mapping_idx].world_position.z;

                    // Only apply effects to LEDs within the room-centered grid bounds
                    if(x >= grid_min_x && x <= grid_max_x &&
                       y >= grid_min_y && y <= grid_max_y &&
                       z >= grid_min_z && z <= grid_max_z)
                    {
                        // Safety: Ensure controller is still valid
                        if(!mapping.controller || mapping.controller->zones.empty() || mapping.controller->colors.empty())
                        {
                            continue;
                        }

                        // Calculate effect color using grid-aware method (world coords)
                        RGBColor color = current_effect_ui->CalculateColorGrid(x, y, z, effect_time, grid_context);;
color = current_effect_ui->PostProcessColorGrid(x, y, z, color, grid_context);

                        // Apply color to the mapped physical LED (with bounds checking)
                        if(mapping.zone_idx < mapping.controller->zones.size())
                        {
                            unsigned int led_global_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                            if(led_global_idx < mapping.controller->colors.size())
                            {
                                mapping.controller->colors[led_global_idx] = color;
                            }
                        }
                    }
                }
            }

            // Update the physical controllers that this virtual controller maps to
            std::set<RGBController*> updated_controllers;
            for(unsigned int i = 0; i < mappings.size(); i++)
            {
                if(mappings[i].controller && updated_controllers.find(mappings[i].controller) == updated_controllers.end())
                {
                    mappings[i].controller->UpdateLEDs();
                    updated_controllers.insert(mappings[i].controller);
                }
            }

            continue;
        }

        // Handle regular controllers
        RGBController* controller = transform->controller;
        if(!controller || controller->zones.empty() || controller->colors.empty())
        {
            continue;
        }

        /*---------------------------------------------------------*\
        | Update cached world positions if dirty                  |
        \*---------------------------------------------------------*/
        if(transform->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(transform);
        }

        /*---------------------------------------------------------*\
        | Calculate colors for each LED using cached positions    |
        \*---------------------------------------------------------*/
        for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
        {
            LEDPosition3D& led_position = transform->led_positions[led_pos_idx];

            /*---------------------------------------------------------*\
            | Use pre-computed world position (no calculation!)      |
            \*---------------------------------------------------------*/
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

            // Only apply effects to LEDs within the room-centered grid bounds
            if(x >= grid_min_x && x <= grid_max_x &&
               y >= grid_min_y && y <= grid_max_y &&
               z >= grid_min_z && z <= grid_max_z)
            {
                /*---------------------------------------------------------*\
                | Calculate effect color using grid-aware method          |
                \*---------------------------------------------------------*/
                RGBColor color = current_effect_ui->CalculateColorGrid(x, y, z, effect_time, grid_context);
                color = current_effect_ui->PostProcessColorGrid(x, y, z, color, grid_context);

                // Apply color to the correct LED using the global LED index
                if(led_global_idx < controller->colors.size())
                {
                    controller->colors[led_global_idx] = color;
                }
            }
            // LEDs outside the grid remain unlit (keep their current color)
        }

        /*---------------------------------------------------------*\
        | Update the controller                                    |
        \*---------------------------------------------------------*/
        controller->UpdateLEDs();
    }

    /*---------------------------------------------------------*\
    | Update the 3D viewport                                   |
    \*---------------------------------------------------------*/
    viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_granularity_changed(int)
{
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_led_spacing_preset_changed(int index)
{
    if(!led_spacing_x_spin || !led_spacing_y_spin || !led_spacing_z_spin)
    {
        return;
    }

    // Block signals to prevent triggering changes while updating
    led_spacing_x_spin->blockSignals(true);
    led_spacing_y_spin->blockSignals(true);
    led_spacing_z_spin->blockSignals(true);

    switch(index)
    {
        case 1: // Dense Strip (10mm)
            led_spacing_x_spin->setValue(10.0);
            led_spacing_y_spin->setValue(0.0);
            led_spacing_z_spin->setValue(0.0);
            break;
        case 2: // Keyboard (19mm)
            led_spacing_x_spin->setValue(19.0);
            led_spacing_y_spin->setValue(0.0);
            led_spacing_z_spin->setValue(19.0);
            break;
        case 3: // Sparse Strip (33mm)
            led_spacing_x_spin->setValue(33.0);
            led_spacing_y_spin->setValue(0.0);
            led_spacing_z_spin->setValue(0.0);
            break;
        case 4: // LED Cube (50mm)
            led_spacing_x_spin->setValue(50.0);
            led_spacing_y_spin->setValue(50.0);
            led_spacing_z_spin->setValue(50.0);
            break;
        case 0: // Custom
        default:
            // Do nothing - user controls manually
            break;
    }

    led_spacing_x_spin->blockSignals(false);
    led_spacing_y_spin->blockSignals(false);
    led_spacing_z_spin->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateAvailableItemCombo()
{
    item_combo->clear();

    int list_row = available_controllers_list->currentRow();
    if(list_row < 0)
    {
        return;
    }

    // Check if the selected item has metadata (Reference Point, Display Plane, or Custom Controller)
    QListWidgetItem* selected_item = available_controllers_list->item(list_row);
    if(selected_item && selected_item->data(Qt::UserRole).isValid())
    {
        QPair<int, int> metadata = selected_item->data(Qt::UserRole).value<QPair<int, int>>();
        int type_code = metadata.first;
        int object_index = metadata.second;

        if(type_code == -2) // Reference Point
        {
            item_combo->addItem("Whole Object", QVariant::fromValue(qMakePair(-2, object_index)));
            return;
        }
        else if(type_code == -3) // Display Plane
        {
            item_combo->addItem("Whole Object", QVariant::fromValue(qMakePair(-3, object_index)));
            return;
        }
        else if(type_code == -1) // Custom Controller
        {
            item_combo->addItem("Whole Device", QVariant::fromValue(qMakePair(-1, object_index)));
            return;
        }
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    int actual_ctrl_idx = -1;
    int visible_idx = 0;

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        if(GetUnassignedLEDCount(controllers[i]) > 0)
        {
            if(visible_idx == list_row)
            {
                actual_ctrl_idx = i;
                break;
            }
            visible_idx++;
        }
    }

    if(actual_ctrl_idx >= 0)
    {
        RGBController* controller = controllers[actual_ctrl_idx];
        int granularity = granularity_combo->currentIndex();

        if(granularity == 0)
        {
            if(!IsItemInScene(controller, granularity, 0))
            {
                item_combo->addItem(QString::fromStdString(controller->name), QVariant::fromValue(qMakePair(actual_ctrl_idx, 0)));
            }
        }
        else if(granularity == 1)
        {
            for(unsigned int i = 0; i < controller->zones.size(); i++)
            {
                if(!IsItemInScene(controller, granularity, i))
                {
                    item_combo->addItem(QString::fromStdString(controller->zones[i].name), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        else if(granularity == 2)
        {
            for(unsigned int i = 0; i < controller->leds.size(); i++)
            {
                if(!IsItemInScene(controller, granularity, i))
                {
                    item_combo->addItem(QString::fromStdString(controller->leds[i].name), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        return;
    }
}

void OpenRGB3DSpatialTab::on_add_clicked()
{
    int granularity = granularity_combo->currentIndex();
    int combo_idx = item_combo->currentIndex();

    if(combo_idx < 0)
    {
        QMessageBox::information(this, "No Item Selected",
                                "Please select a controller, zone, or LED to add to the scene.");
        return;
    }

    QPair<int, int> data = item_combo->currentData().value<QPair<int, int>>();
    int ctrl_idx = data.first;
    int item_row = data.second;

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    // Handle Reference Points (-2)
    if(ctrl_idx == -2)
    {
        if(item_row < 0 || item_row >= (int)reference_points.size())
        {
            return;
        }

        VirtualReferencePoint3D* ref_point = reference_points[item_row].get();
        ref_point->SetVisible(true);

        // Add to Controllers in 3D Scene list
        QString name = QString("[Ref Point] ") + QString::fromStdString(ref_point->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(name);
        list_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, item_row)));
        controller_list->addItem(list_item);

        viewport->update();

        QMessageBox::information(this, "Reference Point Added",
                                QString("Reference point '%1' added to 3D view!\n\nYou can now position and configure it.")
                                .arg(QString::fromStdString(ref_point->GetName())));
        return;
    }

    // Handle Display Planes (-3)
    if(ctrl_idx == -3)
    {
        if(item_row < 0 || item_row >= (int)display_planes.size())
        {
            return;
        }

        DisplayPlane3D* plane = display_planes[item_row].get();
        plane->SetVisible(true);

        // Add to Controllers in 3D Scene list
        QString name = QString("[Display] ") + QString::fromStdString(plane->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(name);
        list_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-3, item_row)));
        controller_list->addItem(list_item);

        viewport->SelectDisplayPlane(item_row);
        viewport->update();
        NotifyDisplayPlaneChanged();
        emit GridLayoutChanged();

        QMessageBox::information(this, "Display Plane Added",
                                QString("Display plane '%1' added to 3D view!\n\nYou can now position and configure it.")
                                .arg(QString::fromStdString(plane->GetName())));
        return;
    }

    // Handle Custom Controllers (-1)
    if(ctrl_idx == -1)
    {
        if(item_row >= (int)virtual_controllers.size())
        {
            return;
        }

        VirtualController3D* virtual_ctrl = virtual_controllers[item_row].get();

        std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
        ctrl_transform->controller = nullptr;
        ctrl_transform->virtual_controller = virtual_ctrl;
        ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f}; // Snapped to 0.5 grid
        ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
        ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};

        // Set LED spacing from UI
        ctrl_transform->led_spacing_mm_x = led_spacing_x_spin ? (float)led_spacing_x_spin->value() : 10.0f;
        ctrl_transform->led_spacing_mm_y = led_spacing_y_spin ? (float)led_spacing_y_spin->value() : 0.0f;
        ctrl_transform->led_spacing_mm_z = led_spacing_z_spin ? (float)led_spacing_z_spin->value() : 0.0f;

        // Virtual controllers always use whole device granularity
        ctrl_transform->granularity = -1;  // -1 = virtual controller
        ctrl_transform->item_idx = -1;

        ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
        ctrl_transform->world_positions_dirty = true;

        int hue = (controller_transforms.size() * 137) % 360;
        QColor color = QColor::fromHsv(hue, 200, 255);
        ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

        // Pre-compute world positions before adding to vector
        ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

        controller_transforms.push_back(std::move(ctrl_transform));

        QString name = QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(name);
        controller_list->addItem(list_item);

            viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();
        return;
    }

    if(ctrl_idx >= (int)controllers.size())
    {
        return;
    }

    RGBController* controller = controllers[ctrl_idx];

    std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
    ctrl_transform->controller = controller;
    ctrl_transform->virtual_controller = nullptr;
    ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f}; // Snapped to 0.5 grid
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};

    // Set LED spacing from UI
    ctrl_transform->led_spacing_mm_x = led_spacing_x_spin ? (float)led_spacing_x_spin->value() : 10.0f;
    ctrl_transform->led_spacing_mm_y = led_spacing_y_spin ? (float)led_spacing_y_spin->value() : 0.0f;
    ctrl_transform->led_spacing_mm_z = led_spacing_z_spin ? (float)led_spacing_z_spin->value() : 0.0f;

    // Set granularity
    ctrl_transform->granularity = granularity;
    ctrl_transform->item_idx = item_row;

    QString name;

    if(granularity == 0)
    {
        ctrl_transform->led_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y, custom_grid_z,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        name = QString("[Device] ") + QString::fromStdString(controller->name);
    }
    else if(granularity == 1)
    {
        if(item_row >= (int)controller->zones.size())
        {
            return;  // ctrl_transform auto-deleted
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y, custom_grid_z,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        zone* z = &controller->zones[item_row];

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            if(all_positions[i].zone_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
            }
        }
        

        name = QString("[Zone] ") + QString::fromStdString(controller->name) + " - " + QString::fromStdString(z->name);
    }
    else if(granularity == 2)
    {
        if(item_row >= (int)controller->leds.size())
        {
            return;  // ctrl_transform auto-deleted
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y, custom_grid_z,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            unsigned int global_led_idx = controller->zones[all_positions[i].zone_idx].start_idx + all_positions[i].led_idx;
            if(global_led_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
                break;
            }
        }

        name = QString("[LED] ") + QString::fromStdString(controller->name) + " - " + QString::fromStdString(controller->leds[item_row].name);
    }

    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

    ctrl_transform->world_positions_dirty = true;
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

    controller_transforms.push_back(std::move(ctrl_transform));

    QListWidgetItem* item = new QListWidgetItem(name);
    controller_list->addItem(item);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();

}

void OpenRGB3DSpatialTab::on_remove_controller_clicked()
{
    int selected_row = controller_list->currentRow();
    if(selected_row < 0 || selected_row >= controller_list->count())
    {
        return;
    }

    // Check if this item has metadata (Reference Point or Display Plane)
    QListWidgetItem* item = controller_list->item(selected_row);
    if(item && item->data(Qt::UserRole).isValid())
    {
        QPair<int, int> metadata = item->data(Qt::UserRole).value<QPair<int, int>>();
        int type_code = metadata.first;
        int object_index = metadata.second;

        if(type_code == -2) // Reference Point
        {
            if(object_index >= 0 && object_index < (int)reference_points.size())
            {
                reference_points[object_index]->SetVisible(false);
            }
            controller_list->takeItem(selected_row);
            viewport->update();
            return;
        }
        else if(type_code == -3) // Display Plane
        {
            if(object_index >= 0 && object_index < (int)display_planes.size())
            {
                display_planes[object_index]->SetVisible(false);
            }
            controller_list->takeItem(selected_row);
            viewport->update();
            NotifyDisplayPlaneChanged();
            emit GridLayoutChanged();
            return;
        }
    }

    // Handle regular controllers (in controller_transforms)
    if(selected_row >= (int)controller_transforms.size())
    {
        return;
    }

    controller_transforms.erase(controller_transforms.begin() + selected_row);  // Auto-deleted

    controller_list->takeItem(selected_row);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_remove_controller_from_viewport(int index)
{
    if(index < 0 || index >= (int)controller_transforms.size())
    {
        return;
    }

    controller_transforms.erase(controller_transforms.begin() + index);

    controller_list->takeItem(index);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_clear_all_clicked()
{
    // Hide all Reference Points and Display Planes
    for(unsigned int i = 0; i < reference_points.size(); i++)
    {
        reference_points[i]->SetVisible(false);
    }
    for(unsigned int i = 0; i < display_planes.size(); i++)
    {
        display_planes[i]->SetVisible(false);
    }

    controller_transforms.clear();
    controller_list->clear();

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_apply_spacing_clicked()
{
    int selected_row = controller_list->currentRow();
    if(selected_row < 0 || selected_row >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* ctrl = controller_transforms[selected_row].get();

    // Update LED spacing values
    ctrl->led_spacing_mm_x = edit_led_spacing_x_spin ? (float)edit_led_spacing_x_spin->value() : 10.0f;
    ctrl->led_spacing_mm_y = edit_led_spacing_y_spin ? (float)edit_led_spacing_y_spin->value() : 0.0f;
    ctrl->led_spacing_mm_z = edit_led_spacing_z_spin ? (float)edit_led_spacing_z_spin->value() : 0.0f;

    // Regenerate LED positions with new spacing
    RegenerateLEDPositions(ctrl);

    // Mark world positions dirty so effects and viewport can recompute
    ctrl->world_positions_dirty = true;

    // Update viewport
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
}

void OpenRGB3DSpatialTab::on_save_layout_clicked()
{
    // Update all settings from UI before saving
    if(grid_x_spin) custom_grid_x = grid_x_spin->value();
    if(grid_y_spin) custom_grid_y = grid_y_spin->value();
    if(grid_z_spin) custom_grid_z = grid_z_spin->value();

    // User position is now handled through reference points system

    bool ok;
    QString profile_name = QInputDialog::getText(this, "Save Layout Profile",
                                                 "Profile name:", QLineEdit::Normal,
                                                 layout_profiles_combo->currentText(), &ok);

    if(!ok || profile_name.isEmpty())
    {
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());

    /*---------------------------------------------------------*\
    | Check if profile already exists                          |
    \*---------------------------------------------------------*/
    if(filesystem::exists(layout_path))
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Overwrite Profile",
            QString("Layout profile \"%1\" already exists. Overwrite?").arg(profile_name),
            QMessageBox::Yes | QMessageBox::No);

        if(reply != QMessageBox::Yes)
        {
            return;
        }
    }

    SaveLayout(layout_path);

    PopulateLayoutDropdown();

    int index = layout_profiles_combo->findText(profile_name);
    if(index >= 0)
    {
        layout_profiles_combo->setCurrentIndex(index);
    }

    // Save the selected profile name to settings
    SaveCurrentLayoutName();

    QMessageBox::information(this, "Layout Saved",
                            QString("Profile '%1' saved to plugins directory").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_load_layout_clicked()
{
    QString profile_name = layout_profiles_combo->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to load");
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    QFileInfo check_file(QString::fromStdString(layout_path));

    if(!check_file.exists())
    {
        QMessageBox::warning(this, "Profile Not Found", "Selected profile file not found");
        return;
    }

    LoadLayout(layout_path);
    QMessageBox::information(this, "Layout Loaded",
                            QString("Profile '%1' loaded successfully").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_delete_layout_clicked()
{
    QString profile_name = layout_profiles_combo->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to delete");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Profile",
                                        QString("Are you sure you want to delete profile '%1'?").arg(profile_name),
                                        QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        std::string layout_path = GetLayoutPath(profile_name.toStdString());
        QFile file(QString::fromStdString(layout_path));

        if(file.remove())
        {
            PopulateLayoutDropdown();
            QMessageBox::information(this, "Profile Deleted",
                                    QString("Profile '%1' deleted successfully").arg(profile_name));
        }
        else
        {
            QMessageBox::warning(this, "Delete Failed", "Failed to delete profile file");
        }
    }
}

void OpenRGB3DSpatialTab::on_layout_profile_changed(int)
{
    SaveCurrentLayoutName();
}

void OpenRGB3DSpatialTab::on_create_custom_controller_clicked()
{
    CustomControllerDialog dialog(resource_manager, this);

    if(dialog.exec() == QDialog::Accepted)
    {
        std::unique_ptr<VirtualController3D> virtual_ctrl = std::make_unique<VirtualController3D>(
            dialog.GetControllerName().toStdString(),
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings(),
            dialog.GetSpacingX(),
            dialog.GetSpacingY(),
            dialog.GetSpacingZ()
        );

        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));

        virtual_controllers.push_back(std::move(virtual_ctrl));

        SaveCustomControllers();

        QMessageBox::information(this, "Custom Controller Created",
                                QString("Custom controller '%1' created successfully!\n\nYou can now add it to the 3D view.")
                                .arg(QString::fromStdString(virtual_ctrl->GetName())));
    }
}

void OpenRGB3DSpatialTab::on_export_custom_controller_clicked()
{
    if(virtual_controllers.empty())
    {
        QMessageBox::warning(this, "No Custom Controllers", "No custom controllers available to export");
        return;
    }

    int list_row = custom_controllers_list->currentRow();
    if(list_row < 0)
    {
        QMessageBox::warning(this, "No Selection", "Please select a custom controller from the list to export");
        return;
    }

    if(list_row >= (int)virtual_controllers.size())
    {
        QMessageBox::warning(this, "Invalid Selection", "Selected custom controller does not exist");
        return;
    }

    VirtualController3D* ctrl = virtual_controllers[list_row].get();

    QString filename = QFileDialog::getSaveFileName(this, "Export Custom Controller",
                                                    QString::fromStdString(ctrl->GetName()) + ".3dctrl",
                                                    "3D Controller Files (*.3dctrl)");
    if(filename.isEmpty())
    {
        return;
    }

    nlohmann::json export_data = ctrl->ToJson();

    std::ofstream file(filename.toStdString());
    if(file.is_open())
    {
        file << export_data.dump(4);
        file.close();
        QMessageBox::information(this, "Export Successful",
                                QString("Custom controller '%1' exported successfully to:\n%2")
                                .arg(QString::fromStdString(ctrl->GetName())).arg(filename));
    }
    else
    {
        QMessageBox::critical(this, "Export Failed",
                            QString("Failed to export custom controller to:\n%1").arg(filename));
    }
}

void OpenRGB3DSpatialTab::on_import_custom_controller_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Import Custom Controller",
                                                    "",
                                                    "3D Controller Files (*.3dctrl);;All Files (*)");
    if(filename.isEmpty())
    {
        return;
    }

    std::ifstream file(filename.toStdString());
    if(!file.is_open())
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("Failed to open file:\n%1").arg(filename));
        return;
    }

    try
    {
        nlohmann::json import_data;
        file >> import_data;
        file.close();

        std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(import_data, controllers);

        if(virtual_ctrl)
        {
            std::string ctrl_name = virtual_ctrl->GetName();

            for(unsigned int i = 0; i < virtual_controllers.size(); i++)
            {
                if(virtual_controllers[i]->GetName() == ctrl_name)
                {
                    QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Name",
                        QString("A custom controller named '%1' already exists.\n\nDo you want to replace it?")
                        .arg(QString::fromStdString(ctrl_name)),
                        QMessageBox::Yes | QMessageBox::No);

                    if(reply == QMessageBox::No)
                    {
                        return;  // unique_ptr automatically cleans up
                    }
                    else
                    {
                        for(unsigned int j = 0; j < virtual_controllers.size(); j++)
                        {
                            if(virtual_controllers[j]->GetName() == ctrl_name)
                            {
                                virtual_controllers.erase(virtual_controllers.begin() + j);
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            virtual_controllers.push_back(std::move(virtual_ctrl));
            SaveCustomControllers();
            UpdateAvailableControllersList();


            QMessageBox::information(this, "Import Successful",
                                    QString("Custom controller '%1' imported successfully!\n\n"
                                           "Grid: %2x%3x%4\n"
                                           "LEDs: %5\n\n"
                                           "You can now add it to the 3D view.")
                                    .arg(QString::fromStdString(virtual_ctrl->GetName()))
                                    .arg(virtual_ctrl->GetWidth())
                                    .arg(virtual_ctrl->GetHeight())
                                    .arg(virtual_ctrl->GetDepth())
                                    .arg(virtual_ctrl->GetMappings().size()));
        }
        else
        {
            QMessageBox::warning(this, "Import Warning",
                                "Failed to import custom controller.\n\n"
                                "The required physical controllers may not be connected.");
        }
    }
    catch(const std::exception& e)
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("Failed to parse custom controller file:\n\n%1").arg(e.what()));
    }
}

void OpenRGB3DSpatialTab::on_edit_custom_controller_clicked()
{
    int list_row = custom_controllers_list->currentRow();
    if(list_row < 0)
    {
        QMessageBox::warning(this, "No Selection", "Please select a custom controller from the list to edit");
        return;
    }

    if(list_row >= (int)virtual_controllers.size())
    {
        QMessageBox::warning(this, "Invalid Selection", "Selected custom controller does not exist");
        return;
    }

    VirtualController3D* virtual_ctrl = virtual_controllers[list_row].get();

    CustomControllerDialog dialog(resource_manager, this);
    dialog.setWindowTitle("Edit Custom 3D Controller");
    dialog.LoadExistingController(virtual_ctrl->GetName(),
                                  virtual_ctrl->GetWidth(),
                                  virtual_ctrl->GetHeight(),
                                  virtual_ctrl->GetDepth(),
                                  virtual_ctrl->GetMappings());

    if(dialog.exec() == QDialog::Accepted)
    {
        std::string old_name = virtual_ctrl->GetName();
        std::string new_name = dialog.GetControllerName().toStdString();

        if(old_name != new_name)
        {
            std::string config_dir = resource_manager->GetConfigurationDirectory().string();
            std::string custom_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers";

            std::string safe_old_name = old_name;
            for(unsigned int i = 0; i < safe_old_name.length(); i++)
            {
                if(safe_old_name[i] == '/' || safe_old_name[i] == '\\' || safe_old_name[i] == ':' || safe_old_name[i] == '*' || safe_old_name[i] == '?' || safe_old_name[i] == '"' || safe_old_name[i] == '<' || safe_old_name[i] == '>' || safe_old_name[i] == '|')
                {
                    safe_old_name[i] = '_';
                }
            }

            std::string old_filepath = custom_dir + "/" + safe_old_name + ".json";
            if(filesystem::exists(old_filepath))
            {
                filesystem::remove(old_filepath);
            }
        }

        // Keep pointer to old instance so we can retarget any viewport transforms
        VirtualController3D* old_ptr = virtual_controllers[list_row].get();

        virtual_controllers[list_row] = std::make_unique<VirtualController3D>(
            new_name,
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings(),
            dialog.GetSpacingX(),
            dialog.GetSpacingY(),
            dialog.GetSpacingZ()
        );

        // Update any transforms in the viewport that referenced the old custom controller
        VirtualController3D* new_ptr = virtual_controllers[list_row].get();
        for(size_t i = 0; i < controller_transforms.size(); i++)
        {
            ControllerTransform* t = controller_transforms[i].get();
            if(t && t->virtual_controller == old_ptr)
            {
                t->virtual_controller = new_ptr;
                // Regenerate LED positions from the updated mapping and spacing
                t->led_positions = new_ptr->GenerateLEDPositions(grid_scale_mm);
                t->world_positions_dirty = true;

                // Update controller list item text to reflect the new name
                if(i < (size_t)controller_list->count())
                {
                    controller_list->item((int)i)->setText(QString("[Custom] ") + QString::fromStdString(new_ptr->GetName()));
                }
            }
        }

        SaveCustomControllers();
        UpdateAvailableControllersList();

        // Refresh viewport so changes take effect immediately
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();

        QMessageBox::information(this, "Custom Controller Updated",
                                QString("Custom controller '%1' updated successfully!")
                                .arg(QString::fromStdString(virtual_controllers[list_row]->GetName())));
    }
}

void OpenRGB3DSpatialTab::SaveLayout(const std::string& filename)
{

    nlohmann::json layout_json;

    /*---------------------------------------------------------*\
    | Header Information                                       |
    \*---------------------------------------------------------*/
    layout_json["format"] = "OpenRGB3DSpatialLayout";
    layout_json["version"] = 6;

    /*---------------------------------------------------------*\
    | Grid Settings                                            |
    \*---------------------------------------------------------*/
    layout_json["grid"]["dimensions"]["x"] = custom_grid_x;
    layout_json["grid"]["dimensions"]["y"] = custom_grid_y;
    layout_json["grid"]["dimensions"]["z"] = custom_grid_z;
    layout_json["grid"]["snap_enabled"] = (viewport && viewport->IsGridSnapEnabled());
    layout_json["grid"]["scale_mm"] = grid_scale_mm;

    /*---------------------------------------------------------*\
    | Room Dimensions (Manual room size settings)             |
    \*---------------------------------------------------------*/
    layout_json["room"]["use_manual_size"] = use_manual_room_size;
    layout_json["room"]["width"] = manual_room_width;
    layout_json["room"]["depth"] = manual_room_depth;
    layout_json["room"]["height"] = manual_room_height;

    /*---------------------------------------------------------*\
    | User Position                                            |
    \*---------------------------------------------------------*/
    layout_json["user_position"]["x"] = user_position.x;
    layout_json["user_position"]["y"] = user_position.y;
    layout_json["user_position"]["z"] = user_position.z;
    layout_json["user_position"]["visible"] = user_position.visible;

    /*---------------------------------------------------------*\
    | Camera                                                   |
    \*---------------------------------------------------------*/
    if(viewport)
    {
        float dist, yaw, pitch, tx, ty, tz;
        viewport->GetCamera(dist, yaw, pitch, tx, ty, tz);
        layout_json["camera"]["distance"] = dist;
        layout_json["camera"]["yaw"] = yaw;
        layout_json["camera"]["pitch"] = pitch;
        layout_json["camera"]["target"]["x"] = tx;
        layout_json["camera"]["target"]["y"] = ty;
        layout_json["camera"]["target"]["z"] = tz;
    }

    /*---------------------------------------------------------*\
    | Controllers                                              |
    \*---------------------------------------------------------*/
    layout_json["controllers"] = nlohmann::json::array();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        nlohmann::json controller_json;

        if(ct->controller == nullptr)
        {
            QListWidgetItem* item = controller_list->item(i);
            QString display_name = item ? item->text() : "Unknown Custom Controller";

            controller_json["name"] = display_name.toStdString();
            controller_json["type"] = "virtual";
            controller_json["location"] = "VIRTUAL_CONTROLLER";
        }
        else
        {
            controller_json["name"] = ct->controller->name;
            controller_json["type"] = "physical";
            controller_json["location"] = ct->controller->location;
        }

        /*---------------------------------------------------------*\
        | LED Mappings                                             |
        \*---------------------------------------------------------*/
        controller_json["led_mappings"] = nlohmann::json::array();
        for(unsigned int j = 0; j < ct->led_positions.size(); j++)
        {
            nlohmann::json led_mapping;
            led_mapping["zone_index"] = ct->led_positions[j].zone_idx;
            led_mapping["led_index"] = ct->led_positions[j].led_idx;
            controller_json["led_mappings"].push_back(led_mapping);
        }

        /*---------------------------------------------------------*\
        | Transform                                                |
        \*---------------------------------------------------------*/
        controller_json["transform"]["position"]["x"] = ct->transform.position.x;
        controller_json["transform"]["position"]["y"] = ct->transform.position.y;
        controller_json["transform"]["position"]["z"] = ct->transform.position.z;

        controller_json["transform"]["rotation"]["x"] = ct->transform.rotation.x;
        controller_json["transform"]["rotation"]["y"] = ct->transform.rotation.y;
        controller_json["transform"]["rotation"]["z"] = ct->transform.rotation.z;

        controller_json["transform"]["scale"]["x"] = ct->transform.scale.x;
        controller_json["transform"]["scale"]["y"] = ct->transform.scale.y;
        controller_json["transform"]["scale"]["z"] = ct->transform.scale.z;

        /*---------------------------------------------------------*\
        | LED Spacing                                              |
        \*---------------------------------------------------------*/
        controller_json["led_spacing_mm"]["x"] = ct->led_spacing_mm_x;
        controller_json["led_spacing_mm"]["y"] = ct->led_spacing_mm_y;
        controller_json["led_spacing_mm"]["z"] = ct->led_spacing_mm_z;

        /*---------------------------------------------------------*\
        | Granularity (-1=virtual, 0=device, 1=zone, 2=LED)       |
        \*---------------------------------------------------------*/
        controller_json["granularity"] = ct->granularity;
        controller_json["item_idx"] = ct->item_idx;

        controller_json["display_color"] = ct->display_color;

        layout_json["controllers"].push_back(controller_json);
    }

    /*---------------------------------------------------------*\
    | Reference Points                                         |
    \*---------------------------------------------------------*/
    layout_json["reference_points"] = nlohmann::json::array();
    for(size_t i = 0; i < reference_points.size(); i++)
    {
        if(!reference_points[i]) continue; // Skip null pointers

        layout_json["reference_points"].push_back(reference_points[i]->ToJson());
    }

    /*---------------------------------------------------------*\
    | Display Planes                                           |
    \*---------------------------------------------------------*/
    layout_json["display_planes"] = nlohmann::json::array();
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        if(!display_planes[i]) continue;
        layout_json["display_planes"].push_back(display_planes[i]->ToJson());
    }

    /*---------------------------------------------------------*\
    | Zones                                                    |
    \*---------------------------------------------------------*/
    if(zone_manager)
    {
        layout_json["zones"] = zone_manager->ToJSON();
    }

    /*---------------------------------------------------------*\
    | Write JSON to file                                       |
    \*---------------------------------------------------------*/
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to save layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Save Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open file for writing: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(layout_json.dump(4));
    file.close();

    if(file.error() != QFile::NoError)
    {
        QString error_msg = QString("Failed to write layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Write Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write file: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    
}

void OpenRGB3DSpatialTab::LoadLayoutFromJSON(const nlohmann::json& layout_json)
{

    /*---------------------------------------------------------*\
    | Load Grid Settings                                       |
    \*---------------------------------------------------------*/
    if(layout_json.contains("grid"))
    {
        custom_grid_x = layout_json["grid"]["dimensions"]["x"].get<int>();
        custom_grid_y = layout_json["grid"]["dimensions"]["y"].get<int>();
        custom_grid_z = layout_json["grid"]["dimensions"]["z"].get<int>();

        if(grid_x_spin)
        {
            grid_x_spin->blockSignals(true);
            grid_x_spin->setValue(custom_grid_x);
            grid_x_spin->blockSignals(false);
        }
        if(grid_y_spin)
        {
            grid_y_spin->blockSignals(true);
            grid_y_spin->setValue(custom_grid_y);
            grid_y_spin->blockSignals(false);
        }
        if(grid_z_spin)
        {
            grid_z_spin->blockSignals(true);
            grid_z_spin->setValue(custom_grid_z);
            grid_z_spin->blockSignals(false);
        }

        if(viewport)
        {
            viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
        }

        bool grid_snap_enabled = layout_json["grid"]["snap_enabled"].get<bool>();
        if(grid_snap_checkbox) grid_snap_checkbox->setChecked(grid_snap_enabled);
        if(viewport) viewport->SetGridSnapEnabled(grid_snap_enabled);

        // Load grid scale if available (default to 10mm for older layouts)
        if(layout_json["grid"].contains("scale_mm"))
        {
            grid_scale_mm = layout_json["grid"]["scale_mm"].get<float>();
            if(grid_scale_spin)
            {
                grid_scale_spin->blockSignals(true);
                grid_scale_spin->setValue(grid_scale_mm);
                grid_scale_spin->blockSignals(false);
            }
        }
    }

    /*---------------------------------------------------------*\
    | Load Room Dimensions                                     |
    \*---------------------------------------------------------*/
    if(layout_json.contains("room"))
    {
        if(layout_json["room"].contains("use_manual_size"))
        {
            use_manual_room_size = layout_json["room"]["use_manual_size"].get<bool>();
            if(use_manual_room_size_checkbox)
            {
                use_manual_room_size_checkbox->blockSignals(true);
                use_manual_room_size_checkbox->setChecked(use_manual_room_size);
                use_manual_room_size_checkbox->blockSignals(false);
            }
        }

        if(layout_json["room"].contains("width"))
        {
            manual_room_width = layout_json["room"]["width"].get<float>();
            if(room_width_spin)
            {
                room_width_spin->blockSignals(true);
                room_width_spin->setValue(manual_room_width);
                room_width_spin->setEnabled(use_manual_room_size);
                room_width_spin->blockSignals(false);
            }
        }

        if(layout_json["room"].contains("depth"))
        {
            manual_room_depth = layout_json["room"]["depth"].get<float>();
            if(room_depth_spin)
            {
                room_depth_spin->blockSignals(true);
                room_depth_spin->setValue(manual_room_depth);
                room_depth_spin->setEnabled(use_manual_room_size);
                room_depth_spin->blockSignals(false);
            }
        }

        if(layout_json["room"].contains("height"))
        {
            manual_room_height = layout_json["room"]["height"].get<float>();
            if(room_height_spin)
            {
                room_height_spin->blockSignals(true);
                room_height_spin->setValue(manual_room_height);
                room_height_spin->setEnabled(use_manual_room_size);
                room_height_spin->blockSignals(false);
            }
        }

        // Update viewport with loaded manual room dimensions
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    }

    /*---------------------------------------------------------*\
    | Load User Position                                       |
    \*---------------------------------------------------------*/
    if(layout_json.contains("user_position"))
    {
        user_position.x = layout_json["user_position"]["x"].get<float>();
        user_position.y = layout_json["user_position"]["y"].get<float>();
        user_position.z = layout_json["user_position"]["z"].get<float>();
        user_position.visible = layout_json["user_position"]["visible"].get<bool>();

        // User position UI controls have been removed - values stored for legacy compatibility
        if(viewport) viewport->SetUserPosition(user_position);
    }

    /*---------------------------------------------------------*\
    | Load Camera                                              |
    \*---------------------------------------------------------*/
    if(layout_json.contains("camera") && viewport)
    {
        const nlohmann::json& cam = layout_json["camera"];
        float dist = cam.contains("distance") ? cam["distance"].get<float>() : 20.0f;
        float yaw  = cam.contains("yaw") ? cam["yaw"].get<float>() : 45.0f;
        float pitch= cam.contains("pitch") ? cam["pitch"].get<float>() : 30.0f;
        float tx = 0.0f, ty = 0.0f, tz = 0.0f;
        if(cam.contains("target"))
        {
            const nlohmann::json& tgt = cam["target"];
            if(tgt.contains("x")) tx = tgt["x"].get<float>();
            if(tgt.contains("y")) ty = tgt["y"].get<float>();
            if(tgt.contains("z")) tz = tgt["z"].get<float>();
        }
        viewport->SetCamera(dist, yaw, pitch, tx, ty, tz);
    }

    /*---------------------------------------------------------*\
    | Clear existing controllers                               |
    \*---------------------------------------------------------*/
    on_clear_all_clicked();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    /*---------------------------------------------------------*\
    | Load Controllers                                         |
    \*---------------------------------------------------------*/
    if(layout_json.contains("controllers"))
    {
        const nlohmann::json& controllers_array = layout_json["controllers"];
        for(size_t i = 0; i < controllers_array.size(); i++)
        {
            const nlohmann::json& controller_json = controllers_array[i];
            std::string ctrl_name = controller_json["name"].get<std::string>();
            std::string ctrl_location = controller_json["location"].get<std::string>();
            std::string ctrl_type = controller_json["type"].get<std::string>();

            RGBController* controller = nullptr;
            bool is_virtual = (ctrl_type == "virtual");

            if(!is_virtual)
            {
                for(unsigned int j = 0; j < controllers.size(); j++)
                {
                    if(controllers[j]->name == ctrl_name && controllers[j]->location == ctrl_location)
                    {
                        controller = controllers[j];
                        break;
                    }
                }

                if(!controller)
                {
                    continue;
                }
            }

            std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
            ctrl_transform->controller = controller;
            ctrl_transform->virtual_controller = nullptr;

            // Load LED spacing first (needed for position generation)
            if(controller_json.contains("led_spacing_mm"))
            {
                ctrl_transform->led_spacing_mm_x = controller_json["led_spacing_mm"]["x"].get<float>();
                ctrl_transform->led_spacing_mm_y = controller_json["led_spacing_mm"]["y"].get<float>();
                ctrl_transform->led_spacing_mm_z = controller_json["led_spacing_mm"]["z"].get<float>();
            }
            else
            {
                ctrl_transform->led_spacing_mm_x = 10.0f;
                ctrl_transform->led_spacing_mm_y = 0.0f;
                ctrl_transform->led_spacing_mm_z = 0.0f;
            }

            // Load granularity
            if(controller_json.contains("granularity"))
            {
                ctrl_transform->granularity = controller_json["granularity"].get<int>();
                ctrl_transform->item_idx = controller_json["item_idx"].get<int>();
            }
            else
            {
                // Default for older files: -1 for virtual, 0 for physical
                ctrl_transform->granularity = is_virtual ? -1 : 0;
                ctrl_transform->item_idx = 0;
            }

            if(is_virtual)
            {
                QString virtual_name = QString::fromStdString(ctrl_name);
                if(virtual_name.startsWith("[Custom] "))
                {
                    virtual_name = virtual_name.mid(9);
                }

                VirtualController3D* virtual_ctrl = nullptr;
                for(unsigned int i = 0; i < virtual_controllers.size(); i++)
                {
                    if(QString::fromStdString(virtual_controllers[i]->GetName()) == virtual_name)
                    {
                        virtual_ctrl = virtual_controllers[i].get();
                        break;
                    }
                }

                if(virtual_ctrl)
                {
                    ctrl_transform->controller = nullptr;
                    ctrl_transform->virtual_controller = virtual_ctrl;
                    ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
                }
                else
                {
                    continue;  // ctrl_transform auto-deleted
                }
            }
            else
            {
                // Load LED mappings for physical controllers
                const nlohmann::json& led_mappings_array = controller_json["led_mappings"];
                for(size_t j = 0; j < led_mappings_array.size(); j++)
                {
                    const nlohmann::json& led_mapping = led_mappings_array[j];
                    unsigned int zone_idx = led_mapping["zone_index"].get<unsigned int>();
                    unsigned int led_idx = led_mapping["led_index"].get<unsigned int>();

                    std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
                        controller, custom_grid_x, custom_grid_y, custom_grid_z,
                        ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
                        grid_scale_mm);

                    for(unsigned int k = 0; k < all_positions.size(); k++)
                    {
                        if(all_positions[k].zone_idx == zone_idx && all_positions[k].led_idx == led_idx)
                        {
                            ctrl_transform->led_positions.push_back(all_positions[k]);
                            break;
                        }
                    }
                }

                // Validate/infer granularity from loaded LED positions (FAILSAFE)
                // This corrects any corrupted or mismatched granularity data
                if(ctrl_transform->led_positions.size() > 0)
                {
                    int original_granularity = ctrl_transform->granularity;
                    int original_item_idx = ctrl_transform->item_idx;

                    // Count total LEDs in controller
                    std::vector<LEDPosition3D> all_leds = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
                        controller, custom_grid_x, custom_grid_y, custom_grid_z,
                        ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
                        grid_scale_mm);

                    if(ctrl_transform->led_positions.size() == all_leds.size())
                    {
                        // All LEDs loaded - this is whole device
                        if(ctrl_transform->granularity != 0)
                        {
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                        }
                    }
                    else if(ctrl_transform->led_positions.size() == 1)
                    {
                        // Single LED - granularity should be 2
                        if(ctrl_transform->granularity != 2)
                        {
                            ctrl_transform->granularity = 2;
                            // Calculate global LED index from zone/led indices
                            unsigned int zone_idx = ctrl_transform->led_positions[0].zone_idx;
                            unsigned int led_idx = ctrl_transform->led_positions[0].led_idx;
                            if(zone_idx < controller->zones.size())
                            {
                                ctrl_transform->item_idx = controller->zones[zone_idx].start_idx + led_idx;
                            }
                        }
                    }
                    else
                    {
                        // Multiple LEDs but not all - check if they're all from same zone
                        unsigned int first_zone = ctrl_transform->led_positions[0].zone_idx;
                        bool same_zone = true;
                        for(unsigned int i = 1; i < ctrl_transform->led_positions.size(); i++)
                        {
                            if(ctrl_transform->led_positions[i].zone_idx != first_zone)
                            {
                                same_zone = false;
                                break;
                            }
                        }

                        if(same_zone)
                        {
                            // All from same zone
                            if(ctrl_transform->granularity != 1)
                            {
                                ctrl_transform->granularity = 1;
                                ctrl_transform->item_idx = first_zone;
                            }
                        }
                        else
                        {
                            // LEDs from multiple zones - this is corrupted data!
                            // Best we can do is treat as whole device and regenerate
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] CORRUPTED DATA for '%s': has %d LEDs from multiple zones with granularity=%d. Treating as Whole Device and will regenerate on next change.",
                                        controller->name.c_str(),
                                        (int)ctrl_transform->led_positions.size(),
                                        original_granularity);
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                            // Keep the loaded LED positions for now, but they'll be regenerated on next change
                        }
                    }

                    // Validation success - no logging needed
                    if(ctrl_transform->granularity == original_granularity && ctrl_transform->item_idx == original_item_idx)
                    {
                    }
                }
            }

            // Load transform
            ctrl_transform->transform.position.x = controller_json["transform"]["position"]["x"].get<float>();
            ctrl_transform->transform.position.y = controller_json["transform"]["position"]["y"].get<float>();
            ctrl_transform->transform.position.z = controller_json["transform"]["position"]["z"].get<float>();

            ctrl_transform->transform.rotation.x = controller_json["transform"]["rotation"]["x"].get<float>();
            ctrl_transform->transform.rotation.y = controller_json["transform"]["rotation"]["y"].get<float>();
            ctrl_transform->transform.rotation.z = controller_json["transform"]["rotation"]["z"].get<float>();

            ctrl_transform->transform.scale.x = controller_json["transform"]["scale"]["x"].get<float>();
            ctrl_transform->transform.scale.y = controller_json["transform"]["scale"]["y"].get<float>();
            ctrl_transform->transform.scale.z = controller_json["transform"]["scale"]["z"].get<float>();

            ctrl_transform->display_color = controller_json["display_color"].get<unsigned int>();

            // Save values before moving ctrl_transform
            unsigned int display_color = ctrl_transform->display_color;
            int granularity = ctrl_transform->granularity;
            int item_idx = ctrl_transform->item_idx;
            size_t led_positions_size = ctrl_transform->led_positions.size();
            unsigned int first_zone_idx = (led_positions_size > 0) ? ctrl_transform->led_positions[0].zone_idx : 0;
            unsigned int first_led_idx = (led_positions_size > 0) ? ctrl_transform->led_positions[0].led_idx : 0;

            // Pre-compute world positions
            ctrl_transform->world_positions_dirty = true;
            ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

            controller_transforms.push_back(std::move(ctrl_transform));

            QColor color;
            color.setRgb(display_color & 0xFF,
                         (display_color >> 8) & 0xFF,
                         (display_color >> 16) & 0xFF);

            QString name;
            if(is_virtual)
            {
                name = QString::fromStdString(ctrl_name);
            }
            else
            {
                // Use granularity info to create proper name with prefix
                if(granularity == 0)
                {
                    name = QString("[Device] ") + QString::fromStdString(controller->name);
                }
                else if(granularity == 1)
                {
                    name = QString("[Zone] ") + QString::fromStdString(controller->name);
                    if(item_idx >= 0 && item_idx < (int)controller->zones.size())
                    {
                        name += " - " + QString::fromStdString(controller->zones[item_idx].name);
                    }
                }
                else if(granularity == 2)
                {
                    name = QString("[LED] ") + QString::fromStdString(controller->name);
                    if(item_idx >= 0 && item_idx < (int)controller->leds.size())
                    {
                        name += " - " + QString::fromStdString(controller->leds[item_idx].name);
                    }
                }
                else
                {
                    // Fallback for old files without granularity
                    name = QString::fromStdString(controller->name);
                    if(led_positions_size < controller->leds.size())
                    {
                        if(led_positions_size == 1)
                        {
                            unsigned int led_global_idx = controller->zones[first_zone_idx].start_idx + first_led_idx;
                            name = QString("[LED] ") + name + " - " + QString::fromStdString(controller->leds[led_global_idx].name);
                        }
                        else
                        {
                            name = QString("[Zone] ") + name + " - " + QString::fromStdString(controller->zones[first_zone_idx].name);
                        }
                    }
                    else
                    {
                        name = QString("[Device] ") + name;
                    }
                }
            }

            QListWidgetItem* item = new QListWidgetItem(name);
            controller_list->addItem(item);
        }
    }

    /*---------------------------------------------------------*\
    | Load Reference Points                                    |
    \*---------------------------------------------------------*/
    // Clear existing reference points
    reference_points.clear();

    if(layout_json.contains("reference_points"))
    {
        const nlohmann::json& ref_points_array = layout_json["reference_points"];
        for(size_t i = 0; i < ref_points_array.size(); i++)
        {
            std::unique_ptr<VirtualReferencePoint3D> ref_point = VirtualReferencePoint3D::FromJson(ref_points_array[i]);
            if(ref_point)
            {
                reference_points.push_back(std::move(ref_point));
            }
        }
    }

    UpdateReferencePointsList();

    /*---------------------------------------------------------*\
    | Load Display Planes                                      |
    \*---------------------------------------------------------*/
    display_planes.clear();
    current_display_plane_index = -1;
    if(layout_json.contains("display_planes"))
    {
        const nlohmann::json& planes_array = layout_json["display_planes"];
        for(size_t i = 0; i < planes_array.size(); i++)
        {
            std::unique_ptr<DisplayPlane3D> plane = DisplayPlane3D::FromJson(planes_array[i]);
            if(plane)
            {
                display_planes.push_back(std::move(plane));
            }
        }
    }
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();

    // Sync display planes to global manager
    std::vector<DisplayPlane3D*> plane_ptrs;
    for(auto& plane : display_planes)
    {
        if(plane)
        {
            plane_ptrs.push_back(plane.get());
        }
    }
    DisplayPlaneManager::instance()->SetDisplayPlanes(plane_ptrs);

    emit GridLayoutChanged();

    /*---------------------------------------------------------*\
    | Load Zones                                               |
    \*---------------------------------------------------------*/
    if(zone_manager)
    {
        if(layout_json.contains("zones"))
        {
            try
            {
                zone_manager->FromJSON(layout_json["zones"]);
                UpdateZonesList();
                
            }
            catch(const std::exception& e)
            {
                LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to load zones from layout: %s", e.what());
                zone_manager->ClearAllZones();
                UpdateZonesList();
            }
        }
        else
        {
            // Old layout file without zones - just initialize empty
            
            zone_manager->ClearAllZones();
            UpdateZonesList();
        }
    }

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetReferencePoints(&reference_points);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::LoadLayout(const std::string& filename)
{

    QFile file(QString::fromStdString(filename));

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to open layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Load Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open file for reading: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if(file.error() != QFile::NoError)
    {
        QString error_msg = QString("Failed to read layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Read Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to read file: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    try
    {
        nlohmann::json layout_json = nlohmann::json::parse(content.toStdString());
        LoadLayoutFromJSON(layout_json);
        
        return;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to parse JSON: %s", e.what());
        QMessageBox::critical(this, "Invalid Layout File",
                            QString("Failed to parse layout file:\n%1\n\nThe file may be corrupted or in an invalid format.\n\nError: %2")
                            .arg(QString::fromStdString(filename))
                            .arg(e.what()));
        return;
    }
}

std::string OpenRGB3DSpatialTab::GetLayoutPath(const std::string& layout_name)
{
    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path plugins_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "layouts";

    QDir dir;
    dir.mkpath(QString::fromStdString(plugins_dir.string()));

    std::string filename = layout_name + ".json";
    filesystem::path layout_file = plugins_dir / filename;

    return layout_file.string();
}

void OpenRGB3DSpatialTab::PopulateLayoutDropdown()
{
    QString current_text = layout_profiles_combo->currentText();

    layout_profiles_combo->blockSignals(true);
    layout_profiles_combo->clear();

    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path layouts_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "layouts";


    QDir dir(QString::fromStdString(layouts_dir.string()));
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);


    for(int i = 0; i < files.size(); i++)
    {
        QString base_name = files[i].baseName();
        layout_profiles_combo->addItem(base_name);
    }

    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    QString saved_profile = "";
    if(settings.contains("SelectedProfile"))
    {
        saved_profile = QString::fromStdString(settings["SelectedProfile"].get<std::string>());
    }
    else
    {
    }

    if(!saved_profile.isEmpty())
    {
        int index = layout_profiles_combo->findText(saved_profile);
        if(index >= 0)
        {
            layout_profiles_combo->setCurrentIndex(index);
        }
        else
        {
        }
    }
    else if(!current_text.isEmpty())
    {
        int index = layout_profiles_combo->findText(current_text);
        if(index >= 0)
        {
            layout_profiles_combo->setCurrentIndex(index);
        }
    }

    layout_profiles_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::SaveCurrentLayoutName()
{
    if(!layout_profiles_combo || !auto_load_checkbox)
    {
        return;
    }

    std::string profile_name = layout_profiles_combo->currentText().toStdString();
    bool auto_load_enabled = auto_load_checkbox->isChecked();

    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["SelectedProfile"] = profile_name;
    settings["AutoLoadEnabled"] = auto_load_enabled;
    resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
    resource_manager->GetSettingsManager()->SaveSettings();
}

void OpenRGB3DSpatialTab::TryAutoLoadLayout()
{
    if(!first_load)
    {
        return;
    }

    first_load = false;

    if(!auto_load_checkbox || !layout_profiles_combo)
    {
        return;
    }

    /*---------------------------------------------------------*\
    | Load saved settings                                      |
    \*---------------------------------------------------------*/
    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");

    bool auto_load_enabled = false;
    std::string saved_profile;

    if(settings.contains("AutoLoadEnabled"))
    {
        auto_load_enabled = settings["AutoLoadEnabled"].get<bool>();
    }

    if(settings.contains("SelectedProfile"))
    {
        saved_profile = settings["SelectedProfile"].get<std::string>();
    }


    /*---------------------------------------------------------*\
    | Restore checkbox state                                   |
    \*---------------------------------------------------------*/
    auto_load_checkbox->blockSignals(true);
    auto_load_checkbox->setChecked(auto_load_enabled);
    auto_load_checkbox->blockSignals(false);

    /*---------------------------------------------------------*\
    | Restore profile selection                                |
    \*---------------------------------------------------------*/
    if(!saved_profile.empty())
    {
        int index = layout_profiles_combo->findText(QString::fromStdString(saved_profile));
        if(index >= 0)
        {
            layout_profiles_combo->blockSignals(true);
            layout_profiles_combo->setCurrentIndex(index);
            layout_profiles_combo->blockSignals(false);
        }
    }

    /*---------------------------------------------------------*\
    | Auto-load if enabled                                     |
    \*---------------------------------------------------------*/
    if(auto_load_enabled && !saved_profile.empty())
    {
        std::string layout_path = GetLayoutPath(saved_profile);
        QFileInfo check_file(QString::fromStdString(layout_path));

        if(check_file.exists())
        {
            LoadLayout(layout_path);
        }
    }

    /*---------------------------------------------------------*\
    | Try to auto-load effect profile after layout loads      |
    \*---------------------------------------------------------*/
    TryAutoLoadEffectProfile();
}

void OpenRGB3DSpatialTab::SaveCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers";

    std::error_code create_dir_ec;
    filesystem::create_directories(custom_dir, create_dir_ec);
    if(create_dir_ec)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create custom controller directory: %s (%s)",
                  custom_dir.c_str(), create_dir_ec.message().c_str());
        return;
    }

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        std::string safe_name = virtual_controllers[i]->GetName();
        for(unsigned int j = 0; j < safe_name.length(); j++)
        {
            if(safe_name[j] == '/' || safe_name[j] == '\\' || safe_name[j] == ':' || safe_name[j] == '*' || safe_name[j] == '?' || safe_name[j] == '"' || safe_name[j] == '<' || safe_name[j] == '>' || safe_name[j] == '|')
            {
                safe_name[j] = '_';
            }
        }

        std::string filepath = custom_dir + "/" + safe_name + ".json";
        std::ofstream file(filepath);
        if(file.is_open())
        {
            nlohmann::json ctrl_json = virtual_controllers[i]->ToJson();
            file << ctrl_json.dump(4);
            file.close();

            if(file.fail())
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write custom controller: %s", filepath.c_str());
                // Don't show error dialog here - too noisy during auto-save
            }
            else
            {
                
            }
        }
        else
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open custom controller file: %s", filepath.c_str());
            // Don't show error dialog here - too noisy during auto-save
        }
    }
}

void OpenRGB3DSpatialTab::LoadCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers";

    filesystem::path dir_path(custom_dir);
    if(!filesystem::exists(dir_path))
    {
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    int loaded_count = 0;

    try
    {
        std::filesystem::directory_iterator dir_iter(dir_path);
        std::filesystem::directory_iterator end_iter;

        for(std::filesystem::directory_iterator entry = dir_iter; entry != end_iter; ++entry)
        {
            if(entry->path().extension() == ".json")
            {
                std::ifstream file(entry->path().string());
                if(file.is_open())
                {
                    try
                    {
                        nlohmann::json ctrl_json;
                        file >> ctrl_json;
                        file.close();

                        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(ctrl_json, controllers);
                        if(virtual_ctrl)
                        {
                            std::string ctrl_name = virtual_ctrl->GetName();
                            available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(ctrl_name));
                            virtual_controllers.push_back(std::move(virtual_ctrl));
                            loaded_count++;
                            
                        }
                        else
                        {
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to create custom controller from: %s",
                                      entry->path().filename().string().c_str());
                        }
                    }
                    catch(const std::exception& e)
                    {
                        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load custom controller %s: %s",
                                entry->path().filename().string().c_str(), e.what());
                    }
                }
                else
                {
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to open custom controller file: %s",
                              entry->path().string().c_str());
                }
            }
        }

    }
    catch(const std::exception&)
    {
    }
}

bool OpenRGB3DSpatialTab::IsItemInScene(RGBController* controller, int granularity, int item_idx)
{
    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(ct->controller == nullptr) continue;
        if(ct->controller != controller) continue;

        // Use granularity field if available
        if(ct->granularity == granularity && ct->item_idx == item_idx)
        {
            return true;
        }

        // Fallback: check by LED positions (for older data or edge cases)
        if(granularity == 0)
        {
            // Check if this is whole device by comparing LED count
            if(ct->granularity == 0)
            {
                return true;
            }
            // Legacy check for controllers without granularity field
            if(ct->granularity < 0 || ct->granularity > 2)
            {
                std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayout(controller, custom_grid_x, custom_grid_y, custom_grid_z);
                if(ct->led_positions.size() == all_positions.size())
                {
                    return true;
                }
            }
        }
        else if(granularity == 1)
        {
            // Check if any LED from this zone is in the controller
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                if(ct->led_positions[j].zone_idx == (unsigned int)item_idx)
                {
                    return true;
                }
            }
        }
        else if(granularity == 2)
        {
            // Check if this specific LED is in the controller
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                unsigned int global_led_idx = controller->zones[ct->led_positions[j].zone_idx].start_idx + ct->led_positions[j].led_idx;
                if(global_led_idx == (unsigned int)item_idx)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

int OpenRGB3DSpatialTab::GetUnassignedZoneCount(RGBController* controller)
{
    int unassigned_count = 0;
    for(unsigned int i = 0; i < controller->zones.size(); i++)
    {
        if(!IsItemInScene(controller, 1, i))
        {
            unassigned_count++;
        }
    }
    return unassigned_count;
}

int OpenRGB3DSpatialTab::GetUnassignedLEDCount(RGBController* controller)
{
    int total_leds = (int)controller->leds.size();
    int assigned_leds = 0;

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        if(controller_transforms[i]->controller == controller)
        {
            assigned_leds += (int)controller_transforms[i]->led_positions.size();
        }
    }

    return total_leds - assigned_leds;
}

void OpenRGB3DSpatialTab::RegenerateLEDPositions(ControllerTransform* transform)
{
    if(!transform) return;

    if(transform->virtual_controller)
    {
        // Virtual controller
        transform->led_positions = transform->virtual_controller->GenerateLEDPositions(grid_scale_mm);
    }
    else if(transform->controller)
    {
        // Physical controller - regenerate with spacing and respect granularity
        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            transform->controller,
            custom_grid_x, custom_grid_y, custom_grid_z,
            transform->led_spacing_mm_x, transform->led_spacing_mm_y, transform->led_spacing_mm_z,
            grid_scale_mm);

        transform->led_positions.clear();

        if(transform->granularity == 0)
        {
            // Whole device - use all positions
            transform->led_positions = all_positions;
        }
        else if(transform->granularity == 1)
        {
            // Zone - filter to specific zone
            for(unsigned int i = 0; i < all_positions.size(); i++)
            {
                if(all_positions[i].zone_idx == (unsigned int)transform->item_idx)
                {
                    transform->led_positions.push_back(all_positions[i]);
                }
            }
        }
        else if(transform->granularity == 2)
        {
            // LED - filter to specific LED
            for(unsigned int i = 0; i < all_positions.size(); i++)
            {
                unsigned int global_led_idx = transform->controller->zones[all_positions[i].zone_idx].start_idx + all_positions[i].led_idx;
                if(global_led_idx == (unsigned int)transform->item_idx)
                {
                    transform->led_positions.push_back(all_positions[i]);
                    break;
                }
            }
        }
    }
}


/*---------------------------------------------------------*\
| Display Plane Management                                 |
\*---------------------------------------------------------*/

DisplayPlane3D* OpenRGB3DSpatialTab::GetSelectedDisplayPlane()
{
    if(current_display_plane_index >= 0 &&
       current_display_plane_index < (int)display_planes.size())
    {
        return display_planes[current_display_plane_index].get();
    }
    return nullptr;
}

void OpenRGB3DSpatialTab::SyncDisplayPlaneControls(DisplayPlane3D* plane)
{
    if(!plane)
    {
        return;
    }

    const Transform3D& transform = plane->GetTransform();

    if(pos_x_spin) { QSignalBlocker block(pos_x_spin); pos_x_spin->setValue(transform.position.x); }
    if(pos_x_slider) { QSignalBlocker block(pos_x_slider); pos_x_slider->setValue((int)std::lround(transform.position.x * 10.0f)); }

    if(pos_y_spin) { QSignalBlocker block(pos_y_spin); pos_y_spin->setValue(transform.position.y); }
    if(pos_y_slider) { QSignalBlocker block(pos_y_slider); pos_y_slider->setValue((int)std::lround(transform.position.y * 10.0f)); }

    if(pos_z_spin) { QSignalBlocker block(pos_z_spin); pos_z_spin->setValue(transform.position.z); }
    if(pos_z_slider) { QSignalBlocker block(pos_z_slider); pos_z_slider->setValue((int)std::lround(transform.position.z * 10.0f)); }

    if(rot_x_spin) { QSignalBlocker block(rot_x_spin); rot_x_spin->setValue(transform.rotation.x); }
    if(rot_x_slider) { QSignalBlocker block(rot_x_slider); rot_x_slider->setValue((int)std::lround(transform.rotation.x)); }

    if(rot_y_spin) { QSignalBlocker block(rot_y_spin); rot_y_spin->setValue(transform.rotation.y); }
    if(rot_y_slider) { QSignalBlocker block(rot_y_slider); rot_y_slider->setValue((int)std::lround(transform.rotation.y)); }

    if(rot_z_spin) { QSignalBlocker block(rot_z_spin); rot_z_spin->setValue(transform.rotation.z); }
    if(rot_z_slider) { QSignalBlocker block(rot_z_slider); rot_z_slider->setValue((int)std::lround(transform.rotation.z)); }

    if(display_plane_name_edit)
    {
        QSignalBlocker block(display_plane_name_edit);
        display_plane_name_edit->setText(QString::fromStdString(plane->GetName()));
    }
    if(display_plane_width_spin)
    {
        QSignalBlocker block(display_plane_width_spin);
        display_plane_width_spin->setValue(plane->GetWidthMM());
    }
    if(display_plane_height_spin)
    {
        QSignalBlocker block(display_plane_height_spin);
        display_plane_height_spin->setValue(plane->GetHeightMM());
    }
    if(display_plane_bezel_spin)
    {
        QSignalBlocker block(display_plane_bezel_spin);
        display_plane_bezel_spin->setValue(plane->GetBezelMM());
    }
    if(display_plane_capture_combo)
    {
        QSignalBlocker block(display_plane_capture_combo);
        std::string current_source = plane->GetCaptureSourceId();

        // Try to find and select the current source
        int index = -1;
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString().toStdString() == current_source)
            {
                index = i;
                break;
            }
        }

        if(index >= 0)
        {
            display_plane_capture_combo->setCurrentIndex(index);
        }
        else if(!current_source.empty())
        {
            // Source not in list, but plane has one configured - add it as custom entry
            display_plane_capture_combo->addItem(QString::fromStdString(current_source) + " (custom)",
                                                  QString::fromStdString(current_source));
            display_plane_capture_combo->setCurrentIndex(display_plane_capture_combo->count() - 1);
        }
        else
        {
            // No source configured, select "(None)"
            display_plane_capture_combo->setCurrentIndex(0);
        }
    }
    if(display_plane_visible_check)
    {
        QSignalBlocker block(display_plane_visible_check);
        display_plane_visible_check->setCheckState(plane->IsVisible() ? Qt::Checked : Qt::Unchecked);
    }
}

void OpenRGB3DSpatialTab::UpdateDisplayPlanesList()
{
    if(!display_planes_list)
    {
        return;
    }

    int desired_index = current_display_plane_index;

    display_planes_list->blockSignals(true);
    display_planes_list->clear();
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        const DisplayPlane3D* plane = display_planes[i].get();
        if(!plane) continue;
        QString label = QString::fromStdString(plane->GetName()) +
                        QString(" (%1 x %2 mm)")
                            .arg(plane->GetWidthMM(), 0, 'f', 0)
                            .arg(plane->GetHeightMM(), 0, 'f', 0);
        QListWidgetItem* item = new QListWidgetItem(label, display_planes_list);
        if(!plane->IsVisible())
        {
            item->setForeground(QColor("#888888"));
        }
    }
    display_planes_list->blockSignals(false);

    if(display_planes.empty())
    {
        current_display_plane_index = -1;
        if(remove_display_plane_button) remove_display_plane_button->setEnabled(false);
        if(viewport) viewport->SelectDisplayPlane(-1);
        RefreshDisplayPlaneDetails();
        return;
    }

    if(desired_index < 0 || desired_index >= (int)display_planes.size())
    {
        desired_index = 0;
    }

    current_display_plane_index = desired_index;
    display_planes_list->setCurrentRow(desired_index);
    if(viewport) viewport->SelectDisplayPlane(desired_index);
    RefreshDisplayPlaneDetails();
}


void OpenRGB3DSpatialTab::RefreshDisplayPlaneDetails()
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    bool has_plane = (plane != nullptr);

    if(remove_display_plane_button) remove_display_plane_button->setEnabled(has_plane);

    QList<QWidget*> widgets = {
        display_plane_name_edit,
        display_plane_width_spin,
        display_plane_height_spin,
        display_plane_bezel_spin,
        display_plane_capture_combo,
        display_plane_refresh_capture_btn,
        display_plane_visible_check
    };

    for(QWidget* w : widgets)
    {
        if(w) w->setEnabled(has_plane);
    }

    if(!has_plane)
    {
        if(display_plane_name_edit) display_plane_name_edit->setText("");
        if(display_plane_width_spin) display_plane_width_spin->setValue(1000.0);
        if(display_plane_height_spin) display_plane_height_spin->setValue(600.0);
        if(display_plane_bezel_spin) display_plane_bezel_spin->setValue(10.0);
        if(display_plane_capture_combo) display_plane_capture_combo->setCurrentIndex(0);
        if(display_plane_visible_check) display_plane_visible_check->setCheckState(Qt::Unchecked);
        return;
    }

    if(display_plane_name_edit)
    {
        QSignalBlocker block(display_plane_name_edit);
        display_plane_name_edit->setText(QString::fromStdString(plane->GetName()));
    }
    if(display_plane_width_spin)
    {
        QSignalBlocker block(display_plane_width_spin);
        display_plane_width_spin->setValue(plane->GetWidthMM());
    }
    if(display_plane_height_spin)
    {
        QSignalBlocker block(display_plane_height_spin);
        display_plane_height_spin->setValue(plane->GetHeightMM());
    }
    if(display_plane_bezel_spin)
    {
        QSignalBlocker block(display_plane_bezel_spin);
        display_plane_bezel_spin->setValue(plane->GetBezelMM());
    }
    if(display_plane_capture_combo)
    {
        QSignalBlocker block(display_plane_capture_combo);
        std::string current_source = plane->GetCaptureSourceId();

        // Try to find and select the current source
        int index = -1;
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString().toStdString() == current_source)
            {
                index = i;
                break;
            }
        }

        if(index >= 0)
        {
            display_plane_capture_combo->setCurrentIndex(index);
        }
        else if(!current_source.empty())
        {
            // Source not in list, but plane has one configured - add it as custom entry
            display_plane_capture_combo->addItem(QString::fromStdString(current_source) + " (custom)",
                                                  QString::fromStdString(current_source));
            display_plane_capture_combo->setCurrentIndex(display_plane_capture_combo->count() - 1);
        }
        else
        {
            // No source configured, select "(None)"
            display_plane_capture_combo->setCurrentIndex(0);
        }
    }
    if(display_plane_visible_check)
    {
        QSignalBlocker block(display_plane_visible_check);
        display_plane_visible_check->setCheckState(plane->IsVisible() ? Qt::Checked : Qt::Unchecked);
    }

    SyncDisplayPlaneControls(plane);
}

void OpenRGB3DSpatialTab::NotifyDisplayPlaneChanged()
{
    if(viewport)
    {
        viewport->NotifyDisplayPlaneChanged();
    }

    // Sync display planes to global manager for effects to access
    std::vector<DisplayPlane3D*> plane_ptrs;
    for(auto& plane : display_planes)
    {
        if(plane)
        {
            plane_ptrs.push_back(plane.get());
        }
    }
    DisplayPlaneManager::instance()->SetDisplayPlanes(plane_ptrs);

    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_selected(int index)
{
    current_display_plane_index = index;

    if(controller_list)
    {
        QSignalBlocker block(controller_list);
        controller_list->clearSelection();
    }
    if(reference_points_list)
    {
        QSignalBlocker block(reference_points_list);
        reference_points_list->clearSelection();
    }

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    if(viewport) viewport->SelectDisplayPlane(index);
}

void OpenRGB3DSpatialTab::on_add_display_plane_clicked()
{
    std::string base_name = "Display Plane";
    int suffix = (int)display_planes.size() + 1;
    std::string full_name = base_name + " " + std::to_string(suffix);
    std::unique_ptr<DisplayPlane3D> plane = std::make_unique<DisplayPlane3D>(full_name);

    float room_depth_units = room_depth_spin ? ((float)room_depth_spin->value() / grid_scale_mm) : 100.0f;
    float room_height_units = room_height_spin ? ((float)room_height_spin->value() / grid_scale_mm) : 100.0f;

    plane->GetTransform().position.x = 0.0f;
    plane->GetTransform().position.y = -room_depth_units * 0.25f;
    plane->GetTransform().position.z = room_height_units * 0.5f;
    plane->SetVisible(false);  // Not visible until added to viewport

    display_planes.push_back(std::move(plane));

    // Add to available controllers list with metadata
    int display_index = (int)display_planes.size() - 1;
    QListWidgetItem* item = new QListWidgetItem(QString("[Display] ") + QString::fromStdString(full_name));
    item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-3, display_index))); // -3 = display plane
    available_controllers_list->addItem(item);
    current_display_plane_index = (int)display_planes.size() - 1;
    UpdateDisplayPlanesList();
    DisplayPlane3D* new_plane = GetSelectedDisplayPlane();
    SyncDisplayPlaneControls(new_plane);
    RefreshDisplayPlaneDetails();

    QMessageBox::information(this, "Display Plane Created",
                            QString("Display plane '%1' created successfully!\n\nYou can now add it to the 3D view from the Available Controllers list.")
                            .arg(QString::fromStdString(full_name)));
}

void OpenRGB3DSpatialTab::on_remove_display_plane_clicked()
{
    if(current_display_plane_index < 0 ||
       current_display_plane_index >= (int)display_planes.size())
    {
        return;
    }

    display_planes.erase(display_planes.begin() + current_display_plane_index);
    if(current_display_plane_index >= (int)display_planes.size())
    {
        current_display_plane_index = (int)display_planes.size() - 1;
    }
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_name_edited(const QString& text)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane)
    {
        return;
    }
    plane->SetName(text.toStdString());
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_width_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetWidthMM((float)value);
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_height_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetHeightMM((float)value);
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_bezel_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetBezelMM((float)value);
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_capture_changed(int index)
{
    if(!display_plane_capture_combo) return;

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;

    QString capture_id = display_plane_capture_combo->itemData(index).toString();
    plane->SetCaptureSourceId(capture_id.toStdString());
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_refresh_capture_clicked()
{
    RefreshDisplayPlaneCaptureSourceList();
}

void OpenRGB3DSpatialTab::RefreshDisplayPlaneCaptureSourceList()
{
    if(!display_plane_capture_combo)
    {
        return;
    }

    QString current_selection;
    if(display_plane_capture_combo->currentIndex() >= 0)
    {
        current_selection = display_plane_capture_combo->currentData().toString();
    }

    auto& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    capture_mgr.RefreshSources();
    auto sources = capture_mgr.GetAvailableSources();

    display_plane_capture_combo->clear();
    display_plane_capture_combo->addItem("(None)", "");

    for(const auto& source : sources)
    {
        QString label = QString::fromStdString(source.name);
        if(source.is_primary)
        {
            label += " [Primary]";
        }
        label += QString(" (%1x%2)").arg(source.width).arg(source.height);

        display_plane_capture_combo->addItem(label, QString::fromStdString(source.id));
    }

    if(!current_selection.isEmpty())
    {
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString() == current_selection)
            {
                display_plane_capture_combo->setCurrentIndex(i);
                return;
            }
        }
    }

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(plane)
    {
        std::string plane_source = plane->GetCaptureSourceId();
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString().toStdString() == plane_source)
            {
                display_plane_capture_combo->setCurrentIndex(i);
                return;
            }
        }
    }
}

void OpenRGB3DSpatialTab::on_display_plane_position_signal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        current_display_plane_index = -1;
        if(display_planes_list)
        {
            QSignalBlocker block(display_planes_list);
            display_planes_list->clearSelection();
        }
        RefreshDisplayPlaneDetails();
        return;
    }

    if(index >= (int)display_planes.size())
    {
        return;
    }

    current_display_plane_index = index;
    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->setCurrentRow(index);
    }
    if(controller_list)
    {
        QSignalBlocker block(controller_list);
        controller_list->clearSelection();
    }
    if(reference_points_list)
    {
        QSignalBlocker block(reference_points_list);
        reference_points_list->clearSelection();
    }

    DisplayPlane3D* plane = display_planes[index].get();
    if(!plane)
    {
        return;
    }

    Transform3D& transform = plane->GetTransform();
    transform.position.x = x;
    transform.position.y = y;
    transform.position.z = z;

    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_rotation_signal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        return;
    }

    if(index >= (int)display_planes.size())
    {
        return;
    }

    current_display_plane_index = index;
    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->setCurrentRow(index);
    }
    if(controller_list)
    {
        QSignalBlocker block(controller_list);
        controller_list->clearSelection();
    }
    if(reference_points_list)
    {
        QSignalBlocker block(reference_points_list);
        reference_points_list->clearSelection();
    }

    DisplayPlane3D* plane = display_planes[index].get();
    if(!plane)
    {
        return;
    }

    Transform3D& transform = plane->GetTransform();
    transform.rotation.x = x;
    transform.rotation.y = y;
    transform.rotation.z = z;

    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_visible_toggled(Qt::CheckState state)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetVisible(state == Qt::CheckState::Checked);
    UpdateDisplayPlanesList();
    SyncDisplayPlaneControls(plane);
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
}

