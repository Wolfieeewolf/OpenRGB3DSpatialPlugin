// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ControllerDisplayUtils.h"
#include "SpatialTabLedHelpers.h"
#include "SpatialControllerCardList.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "DisplayPlaneManager.h"
#include "PluginLog.h"
#include "TransformJson.h"
#include <algorithm>
#include <stdexcept>

namespace
{
constexpr int kLayoutVersion = 7;
constexpr int kMinSupportedLayoutVersion = 6;

std::string ValidateLayoutDocument(const nlohmann::json& j)
{
    if(!j.is_object())
    {
        return "layout root must be a JSON object";
    }
    if(j.value("format", std::string()) != "OpenRGB3DSpatialLayout")
    {
        return "layout format must be OpenRGB3DSpatialLayout";
    }
    if(!j.contains("version") || !j["version"].is_number_integer())
    {
        return "layout missing version";
    }
    const int version = j["version"].get<int>();
    if(version < kMinSupportedLayoutVersion || version > kLayoutVersion)
    {
        return "layout version must be 6 or 7";
    }

    /* Camera may still appear in older saves; it is ignored and no longer required. */
    static const char* kSections[] = {
        "grid", "room", "controllers", "reference_points", "display_planes", "zones"};
    for(const char* section : kSections)
    {
        if(!j.contains(section))
        {
            return std::string("layout missing section: ") + section;
        }
    }
    return {};
}
} // namespace

nlohmann::json OpenRGB3DSpatialTab::BuildLayoutJson() const
{
    nlohmann::json layout_json;

    layout_json["format"] = "OpenRGB3DSpatialLayout";
    layout_json["version"] = kLayoutVersion;

    layout_json["grid"]["dimensions"]["x"] = custom_grid_x;
    layout_json["grid"]["dimensions"]["y"] = custom_grid_y;
    layout_json["grid"]["dimensions"]["z"] = custom_grid_z;
    layout_json["grid"]["snap_enabled"] = (viewport && viewport->IsGridSnapEnabled());
    layout_json["grid"]["scale_mm"] = grid_scale_mm;

    layout_json["room"]["use_manual_size"] = use_manual_room_size;
    layout_json["room"]["width"] = manual_room_width;
    layout_json["room"]["depth"] = manual_room_depth;
    layout_json["room"]["height"] = manual_room_height;

    layout_json["controllers"] = nlohmann::json::array();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(!ct)
        {
            continue;
        }

        nlohmann::json controller_json;

        if(ct->controller == nullptr)
        {
            if(ct->virtual_controller)
            {
                controller_json["name"] =
                    std::string("[Custom] ") + ct->virtual_controller->GetName();
            }
            else
            {
                controller_json["name"] = "Unknown Custom Controller";
            }
            controller_json["type"] = "virtual";
            controller_json["location"] = "VIRTUAL_CONTROLLER";
        }
        else
        {
            controller_json["name"] = ct->controller->GetName();
            controller_json["type"] = "physical";
            controller_json["location"] = ct->controller->GetLocation();
        }

        controller_json["led_mappings"] = nlohmann::json::array();
        for(unsigned int j = 0; j < ct->led_positions.size(); j++)
        {
            nlohmann::json led_mapping;
            led_mapping["zone_index"] = ct->led_positions[j].zone_idx;
            led_mapping["led_index"] = ct->led_positions[j].led_idx;
            controller_json["led_mappings"].push_back(led_mapping);
        }

        TransformJson::WriteTransform(controller_json, ct->transform);

        controller_json["led_spacing_mm"]["x"] = ct->led_spacing_mm_x;
        controller_json["led_spacing_mm"]["y"] = ct->led_spacing_mm_y;
        controller_json["led_spacing_mm"]["z"] = ct->led_spacing_mm_z;

        controller_json["granularity"] = ct->granularity;
        controller_json["item_idx"] = ct->item_idx;
        controller_json["linked_reference_point_index"] = ct->linked_reference_point_index;

        controller_json["display_color"] = ct->display_color;

        layout_json["controllers"].push_back(controller_json);
    }

    layout_json["reference_points"] = nlohmann::json::array();
    for(size_t i = 0; i < reference_points.size(); i++)
    {
        if(!reference_points[i]) continue;

        layout_json["reference_points"].push_back(reference_points[i]->ToJson());
    }

    layout_json["display_planes"] = nlohmann::json::array();
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        if(!display_planes[i]) continue;
        layout_json["display_planes"].push_back(display_planes[i]->ToJson());
    }

    if(zone_manager)
    {
        layout_json["zones"] = zone_manager->ToJSON();
    }
    else
    {
        layout_json["zones"] = nlohmann::json::object();
    }

    return layout_json;
}

void OpenRGB3DSpatialTab::LoadLayoutFromJSON(const nlohmann::json& layout_json)
{
    const std::string layout_error = ValidateLayoutDocument(layout_json);
    if(!layout_error.empty())
    {
        throw std::runtime_error(layout_error);
    }

    const nlohmann::json& grid_json = layout_json["grid"];
    custom_grid_x                 = grid_json["dimensions"]["x"].get<int>();
    custom_grid_y                 = grid_json["dimensions"]["y"].get<int>();
    custom_grid_z                 = grid_json["dimensions"]["z"].get<int>();

    if(gridXSpin())
    {
        gridXSpin()->blockSignals(true);
        gridXSpin()->setValue(custom_grid_x);
        gridXSpin()->blockSignals(false);
    }
    if(gridYSpin())
    {
        gridYSpin()->blockSignals(true);
        gridYSpin()->setValue(custom_grid_y);
        gridYSpin()->blockSignals(false);
    }
    if(gridZSpin())
    {
        gridZSpin()->blockSignals(true);
        gridZSpin()->setValue(custom_grid_z);
        gridZSpin()->blockSignals(false);
    }

    if(viewport)
    {
        viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
    }

    const bool grid_snap_enabled = grid_json["snap_enabled"].get<bool>();
    if(gridSnapCheckbox())
    {
        gridSnapCheckbox()->setChecked(grid_snap_enabled);
    }
    if(viewport)
    {
        viewport->SetGridSnapEnabled(grid_snap_enabled);
    }

    grid_scale_mm = grid_json["scale_mm"].get<float>();
    if(grid_scale_mm < 0.001f)
    {
        throw std::runtime_error("layout grid.scale_mm must be greater than zero");
    }
    if(gridScaleSpin())
    {
        gridScaleSpin()->blockSignals(true);
        gridScaleSpin()->setValue(grid_scale_mm);
        gridScaleSpin()->blockSignals(false);
    }

    const nlohmann::json& room_json = layout_json["room"];
    use_manual_room_size            = room_json["use_manual_size"].get<bool>();
    manual_room_width               = room_json["width"].get<float>();
    manual_room_depth               = room_json["depth"].get<float>();
    manual_room_height              = room_json["height"].get<float>();

    if(useManualRoomSizeCheckbox())
    {
        useManualRoomSizeCheckbox()->blockSignals(true);
        useManualRoomSizeCheckbox()->setChecked(use_manual_room_size);
        useManualRoomSizeCheckbox()->blockSignals(false);
    }
    if(roomWidthSpin())
    {
        roomWidthSpin()->blockSignals(true);
        roomWidthSpin()->setValue(manual_room_width);
        roomWidthSpin()->setEnabled(use_manual_room_size);
        roomWidthSpin()->blockSignals(false);
    }
    if(roomDepthSpin())
    {
        roomDepthSpin()->blockSignals(true);
        roomDepthSpin()->setValue(manual_room_depth);
        roomDepthSpin()->setEnabled(use_manual_room_size);
        roomDepthSpin()->blockSignals(false);
    }
    if(roomHeightSpin())
    {
        roomHeightSpin()->blockSignals(true);
        roomHeightSpin()->setValue(manual_room_height);
        roomHeightSpin()->setEnabled(use_manual_room_size);
        roomHeightSpin()->blockSignals(false);
    }

    if(viewport)
    {
        viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
    }
    emit GridLayoutChanged();

    clearAllClicked();

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();
    std::unordered_map<RGBControllerInterface*, Vector3D> physical_spacing_by_controller;

    {
        const nlohmann::json& controllers_array = layout_json["controllers"];
        for(size_t i = 0; i < controllers_array.size(); i++)
        {
            const nlohmann::json& controller_json = controllers_array[i];
            std::string ctrl_name = controller_json["name"].get<std::string>();
            std::string ctrl_location = controller_json["location"].get<std::string>();
            std::string ctrl_type = controller_json["type"].get<std::string>();

            RGBControllerInterface* controller = nullptr;
            bool is_virtual = (ctrl_type == "virtual");

            if(!is_virtual)
            {
                for(unsigned int j = 0; j < controllers.size(); j++)
                {
                    if(controllers[j]->GetName() == ctrl_name && controllers[j]->GetLocation() == ctrl_location)
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
            ctrl_transform->hidden_by_virtual = false;

            const nlohmann::json& spacing_json = controller_json["led_spacing_mm"];
            ctrl_transform->led_spacing_mm_x   = spacing_json["x"].get<float>();
            ctrl_transform->led_spacing_mm_y   = spacing_json["y"].get<float>();
            ctrl_transform->led_spacing_mm_z   = spacing_json["z"].get<float>();

            if(!is_virtual && controller)
            {
                std::unordered_map<RGBControllerInterface*, Vector3D>::iterator it = physical_spacing_by_controller.find(controller);
                if(it != physical_spacing_by_controller.end())
                {
                    ctrl_transform->led_spacing_mm_x = it->second.x;
                    ctrl_transform->led_spacing_mm_y = it->second.y;
                    ctrl_transform->led_spacing_mm_z = it->second.z;
                }
                else
                {
                    physical_spacing_by_controller[controller] = {
                        ctrl_transform->led_spacing_mm_x,
                        ctrl_transform->led_spacing_mm_y,
                        ctrl_transform->led_spacing_mm_z
                    };
                }
            }

            ctrl_transform->granularity = controller_json["granularity"].get<int>();
            ctrl_transform->item_idx    = controller_json["item_idx"].get<int>();

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
                    ctrl_transform->controller         = nullptr;
                    ctrl_transform->virtual_controller = virtual_ctrl;
                    ctrl_transform->led_positions      = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
                }
                else
                {
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Layout skipped missing custom controller '%s'",
                                virtual_name.toUtf8().constData());
                    continue;
                }
            }
            else
            {
                const nlohmann::json& led_mappings_array = controller_json["led_mappings"];
                for(size_t j = 0; j < led_mappings_array.size(); j++)
                {
                    const nlohmann::json& led_mapping = led_mappings_array[j];
                    unsigned int zone_idx = led_mapping["zone_index"].get<unsigned int>();
                    unsigned int led_idx = led_mapping["led_index"].get<unsigned int>();

                    std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
                        controller, custom_grid_x, custom_grid_y,
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

                if(ctrl_transform->led_positions.size() > 0)
                {
                    int original_granularity = ctrl_transform->granularity;

                    std::vector<LEDPosition3D> all_leds = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
                        controller, custom_grid_x, custom_grid_y,
                        ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
                        grid_scale_mm);

                    if(ctrl_transform->led_positions.size() == all_leds.size())
                    {
                        if(ctrl_transform->granularity != 0)
                        {
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                        }
                    }
                    else if(ctrl_transform->led_positions.size() == 1)
                    {
                        if(ctrl_transform->granularity != 2)
                        {
                            ctrl_transform->granularity = 2;
                            unsigned int zone_idx = ctrl_transform->led_positions[0].zone_idx;
                            unsigned int led_idx = ctrl_transform->led_positions[0].led_idx;
                            unsigned int global_led_idx = 0;
                            if(TryGetObjectCreatorGlobalLedIndex(controller, zone_idx, led_idx, &global_led_idx))
                            {
                                ctrl_transform->item_idx = global_led_idx;
                            }
                            else
                            {
                                ctrl_transform->item_idx = 0;
                                ctrl_transform->granularity = 0;
                            }
                        }
                    }
                    else
                    {
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
                            if(ctrl_transform->granularity != 1)
                            {
                                ctrl_transform->granularity = 1;
                                ctrl_transform->item_idx = first_zone;
                            }
                        }
                        else
                        {
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] CORRUPTED DATA for '%s': has %d LEDs from multiple zones with granularity=%d. Treating as Whole Device and will regenerate on next change.",
                                        controller->GetName().c_str(),
                                        (int)ctrl_transform->led_positions.size(),
                                        original_granularity);
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                        }
                    }

                }
            }

            TransformJson::ReadTransform(controller_json, ctrl_transform->transform);

            ctrl_transform->display_color = controller_json["display_color"].get<unsigned int>();
            ctrl_transform->linked_reference_point_index =
                controller_json.value("linked_reference_point_index", -1);

            unsigned int display_color = ctrl_transform->display_color;
            int granularity = ctrl_transform->granularity;
            int item_idx = ctrl_transform->item_idx;
            size_t led_positions_size = ctrl_transform->led_positions.size();
            unsigned int first_zone_idx = (led_positions_size > 0) ? ctrl_transform->led_positions[0].zone_idx : 0;
            unsigned int first_led_idx = (led_positions_size > 0) ? ctrl_transform->led_positions[0].led_idx : 0;

            ControllerLayout3D::MarkWorldPositionsDirty(ctrl_transform.get());
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
                if(granularity == 0)
                {
                    name = QString("[Device] ") + ControllerDisplay::FormatRgbControllerTitle(controller);
                }
                else if(granularity == 1)
                {
                    name = QString("[Zone] ") + ControllerDisplay::FormatRgbControllerTitle(controller);
                    if(item_idx >= 0 && item_idx < (int)controller->GetZoneCount())
                    {
                        name += " - " + QString::fromStdString(controller->GetZoneName((unsigned int)item_idx));
                    }
                }
                else if(granularity == 2)
                {
                    name = QString("[LED] ") + ControllerDisplay::FormatRgbControllerTitle(controller);
                    if(item_idx >= 0 && item_idx < (int)controller->GetLEDCount())
                    {
                        name += " - " + QString::fromStdString(controller->GetLEDName((unsigned int)item_idx));
                    }
                }
                else
                {
                    name = ControllerDisplay::FormatRgbControllerTitle(controller);
                    if(led_positions_size < controller->GetLEDCount())
                    {
                        if(led_positions_size == 1)
                        {
                            unsigned int led_global_idx = 0;
                            if(TryGetObjectCreatorGlobalLedIndex(controller, first_zone_idx, first_led_idx, &led_global_idx))
                            {
                                name = QString("[LED] ") + name + " - " + QString::fromStdString(controller->GetLEDName(led_global_idx));
                            }
                            else
                            {
                                name = QString("[Device] ") + name;
                            }
                        }
                        else
                        {
                            if(first_zone_idx < controller->GetZoneCount())
                            {
                                name = QString("[Zone] ") + name + " - " + QString::fromStdString(controller->GetZoneName(first_zone_idx));
                            }
                            else
                            {
                                name = QString("[Device] ") + name;
                            }
                        }
                    }
                    else
                    {
                        name = QString("[Device] ") + name;
                    }
                }
            }

            scene_controllers_.append(name);
        }
    }

    reference_points.clear();

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

    int ref_count_loaded = (int)reference_points.size();
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(!ct)
        {
            continue;
        }
        QString base_name = QString("Controller %1").arg(static_cast<int>(i) + 1);
        const int scene_row = TransformIndexToControllerListRow(static_cast<int>(i));
        if(scene_row >= 0)
        {
            base_name = scene_controllers_.textAt(scene_row);
        }

        if(ct->linked_reference_point_index < 0 || ct->linked_reference_point_index >= ref_count_loaded)
        {
            EnsureControllerLinkedReferencePoint(static_cast<int>(i), base_name);
        }
        else
        {
            SyncControllerLinkedReferencePoint(static_cast<int>(i));
            const int ref_index = ct->linked_reference_point_index;
            if(ref_index >= 0 && ref_index < (int)reference_points.size())
            {
                if(VirtualReferencePoint3D* ref_point = reference_points[ref_index].get())
                {
                    ref_point->SetVisible(true);
                }
            }
        }
    }

    display_planes.clear();
    current_display_plane_index = -1;
    {
        const nlohmann::json& planes_array = layout_json["display_planes"];
        const int             ref_count    = (int)reference_points.size();
        for(size_t i = 0; i < planes_array.size(); i++)
        {
            std::unique_ptr<DisplayPlane3D> plane = DisplayPlane3D::FromJson(planes_array[i]);
            if(plane)
            {
                int ref_idx = plane->GetReferencePointIndex();
                if(ref_idx < 0 || ref_idx >= ref_count)
                {
                    plane->SetReferencePointIndex(-1);
                }
                display_planes.push_back(std::move(plane));
            }
        }
        for(size_t i = 0; i < display_planes.size(); i++)
        {
            if(!display_planes[i])
            {
                continue;
            }
            int ref_idx = display_planes[i]->GetReferencePointIndex();
            if(ref_idx < 0 || ref_idx >= ref_count)
            {
                display_planes[i]->SetReferencePointIndex(-1);
            }
        }
    }
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();

    std::vector<bool> ref_added(reference_points.size(), false);

    for(size_t i = 0; i < display_planes.size(); i++)
    {
        DisplayPlane3D* plane = display_planes[i].get();
        if(!plane || !plane->IsVisible()) continue;

        scene_controllers_.append(QString("[Display] ") + QString::fromStdString(plane->GetName()),
                                  qMakePair(-3, plane->GetId()));

        int ref_idx = plane->GetReferencePointIndex();
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
        {
            ref_added[ref_idx] = true;
        }
    }

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        if(ref_added[i] || IsDeviceLinkedReferencePoint(static_cast<int>(i))) continue;
        VirtualReferencePoint3D* ref_pt = reference_points[i].get();
        if(!ref_pt || !ref_pt->IsVisible()) continue;

        scene_controllers_.append(QString("[Ref Point] ") + QString::fromStdString(ref_pt->GetName()),
                                  qMakePair(-2, static_cast<int>(i)));
    }

    NotifyDisplayPlaneChanged();

    if(zone_manager)
    {
        zone_manager->FromJSON(layout_json["zones"]);
        UpdateZonesList();
    }

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->SetReferencePoints(&reference_points);
        viewport->update();
    }
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RebindCustomControllerDeviceMappings();
    SyncSpatialLightingSceneForUi();
    if(viewport)
    {
        viewport->ResetCameraToDefault();
    }
    ClearLayoutDirty();
}

void OpenRGB3DSpatialTab::RegenerateLEDPositions(ControllerTransform* transform)
{
    if(!transform) return;

    if(transform->virtual_controller)
    {
        transform->led_positions = transform->virtual_controller->GenerateLEDPositions(grid_scale_mm);
    }
    else if(transform->controller)
    {
        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            transform->controller,
            custom_grid_x, custom_grid_y,
            transform->led_spacing_mm_x, transform->led_spacing_mm_y, transform->led_spacing_mm_z,
            grid_scale_mm);

        transform->led_positions.clear();

        if(transform->granularity == 0)
        {
            transform->led_positions = all_positions;
        }
        else if(transform->granularity == 1)
        {
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
            for(unsigned int i = 0; i < all_positions.size(); i++)
            {
                unsigned int global_led_idx = 0;
                if(!TryGetObjectCreatorGlobalLedIndex(transform->controller, all_positions[i].zone_idx, all_positions[i].led_idx, &global_led_idx))
                {
                    continue;
                }
                if(global_led_idx == (unsigned int)transform->item_idx)
                {
                    transform->led_positions.push_back(all_positions[i]);
                    break;
                }
            }
        }
    }
}

void OpenRGB3DSpatialTab::RefreshHiddenControllerStates()
{
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* transform = controller_transforms[i].get();
        if(!transform)
        {
            continue;
        }

        transform->hidden_by_virtual = false;
    }

    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* transform = controller_transforms[i].get();
        if(!transform || !transform->virtual_controller)
        {
            continue;
        }

        const std::vector<GridLEDMapping>& mappings = transform->virtual_controller->GetMappings();
        for(const GridLEDMapping& mapping : mappings)
        {
            if(!mapping.controller)
            {
                continue;
            }

            for(size_t j = 0; j < controller_transforms.size(); j++)
            {
                ControllerTransform* candidate = controller_transforms[j].get();
                if(!candidate || candidate->controller != mapping.controller)
                {
                    continue;
                }

                candidate->hidden_by_virtual = true;
            }
        }
    }

    RebuildSceneControllerCards();
}
