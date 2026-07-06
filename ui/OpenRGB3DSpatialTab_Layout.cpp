// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ControllerDisplayUtils.h"
#include "SpatialTabLedHelpers.h"
#include "PluginSettingsPaths.h"
#include "SpatialControllerCardList.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "DisplayPlaneManager.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "SettingsManager.h"
#include "PluginUiUtils.h"
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextStream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace filesystem = std::filesystem;

namespace
{
constexpr int kLayoutVersion = 6;

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
    if(!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() != kLayoutVersion)
    {
        return "layout version must be 6";
    }

    static const char* kSections[] = {
        "grid", "room", "camera", "controllers", "reference_points", "display_planes", "zones"};
    for(const char* section : kSections)
    {
        if(!j.contains(section))
        {
            return std::string("layout missing section: ") + section;
        }
    }
    return {};
}

QString LayoutLoadErrorUserMessage(const std::string& filename, const char* detail)
{
    const QString path = QString::fromStdString(filename);
    const QString err  = QString::fromUtf8(detail);
    QString       msg  = QStringLiteral("Could not load layout profile:\n%1\n\n%2").arg(path, err);

    if(err.contains(QStringLiteral("version"), Qt::CaseInsensitive)
       || err.contains(QStringLiteral("format"), Qt::CaseInsensitive)
       || err.contains(QStringLiteral("missing section"), Qt::CaseInsensitive))
    {
        msg += QStringLiteral(
            "\n\nThis file is not a current layout (format OpenRGB3DSpatialLayout, version 6). "
            "Open the plugin, set up your scene, and use Save layout to write a new profile.");
    }
    return msg;
}
} // namespace

void OpenRGB3DSpatialTab::saveLayoutClicked()
{
    if(gridXSpin()) custom_grid_x = gridXSpin()->value();
    if(gridYSpin()) custom_grid_y = gridYSpin()->value();
    if(gridZSpin()) custom_grid_z = gridZSpin()->value();

    if(!layoutProfilesCombo()) return;
    bool ok;
    QString profile_name = QInputDialog::getText(this, "Save Layout Profile",
                                                 "Profile name:", QLineEdit::Normal,
                                                 layoutProfilesCombo()->currentText(), &ok);

    if(!ok || profile_name.isEmpty())
    {
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    if(layout_path.empty()) return;

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
    ClearLayoutDirty();

    PopulateLayoutDropdown();

    int index = layoutProfilesCombo()->findText(profile_name);
    if(index >= 0)
    {
        layoutProfilesCombo()->setCurrentIndex(index);
    }

    SaveCurrentLayoutName();

    QMessageBox::information(this, "Layout Saved",
                            QString("Profile '%1' saved to plugins directory").arg(profile_name));
}

void OpenRGB3DSpatialTab::loadLayoutClicked()
{
    if(!PromptSaveIfDirty())
    {
        return;
    }
    
    if(!layoutProfilesCombo()) return;
    QString profile_name = layoutProfilesCombo()->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to load");
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    if(layout_path.empty()) return;
    QFileInfo check_file(QString::fromStdString(layout_path));

    if(!check_file.exists())
    {
        QMessageBox::warning(this, "Profile Not Found", "Selected profile file not found");
        return;
    }

    LoadLayout(layout_path);
    ClearLayoutDirty();
    QMessageBox::information(this, "Layout Loaded",
                            QString("Profile '%1' loaded successfully").arg(profile_name));
}

void OpenRGB3DSpatialTab::deleteLayoutClicked()
{
    if(!layoutProfilesCombo()) return;
    QString profile_name = layoutProfilesCombo()->currentText();

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
        if(layout_path.empty()) return;
        QFile file(QString::fromStdString(layout_path));

        if(file.remove())
        {
            PopulateLayoutDropdown();
            if(layoutProfilesCombo())
            {
                if(layoutProfilesCombo()->count() > 0)
                {
                    layoutProfilesCombo()->setCurrentIndex(0);
                }
                else
                {
                    layoutProfilesCombo()->setCurrentIndex(-1);
                }
            }
            SaveCurrentLayoutName();
            QMessageBox::information(this, "Profile Deleted",
                                    QString("Profile '%1' deleted successfully").arg(profile_name));
        }
        else
        {
            QMessageBox::warning(this, "Delete Failed", "Failed to delete profile file");
        }
    }
}

void OpenRGB3DSpatialTab::layoutProfileChanged(int)
{
    SaveCurrentLayoutName();
}

void OpenRGB3DSpatialTab::SaveLayout(const std::string& filename)
{

    nlohmann::json layout_json;

    layout_json["format"] = "OpenRGB3DSpatialLayout";
    layout_json["version"] = 6;

    layout_json["grid"]["dimensions"]["x"] = custom_grid_x;
    layout_json["grid"]["dimensions"]["y"] = custom_grid_y;
    layout_json["grid"]["dimensions"]["z"] = custom_grid_z;
    layout_json["grid"]["snap_enabled"] = (viewport && viewport->IsGridSnapEnabled());
    layout_json["grid"]["scale_mm"] = grid_scale_mm;

    layout_json["room"]["use_manual_size"] = use_manual_room_size;
    layout_json["room"]["width"] = manual_room_width;
    layout_json["room"]["depth"] = manual_room_depth;
    layout_json["room"]["height"] = manual_room_height;

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

    layout_json["controllers"] = nlohmann::json::array();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        nlohmann::json controller_json;

        if(ct->controller == nullptr)
        {
            const int list_row = TransformIndexToControllerListRow(static_cast<int>(i));
            QString     display_name =
                list_row >= 0 ? scene_controllers_.textAt(list_row) : QStringLiteral("Unknown Custom Controller");

            controller_json["name"] = display_name.toStdString();
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

        controller_json["transform"]["position"]["x"] = ct->transform.position.x;
        controller_json["transform"]["position"]["y"] = ct->transform.position.y;
        controller_json["transform"]["position"]["z"] = ct->transform.position.z;

        controller_json["transform"]["rotation"]["x"] = ct->transform.rotation.x;
        controller_json["transform"]["rotation"]["y"] = ct->transform.rotation.y;
        controller_json["transform"]["rotation"]["z"] = ct->transform.rotation.z;

        controller_json["transform"]["scale"]["x"] = ct->transform.scale.x;
        controller_json["transform"]["scale"]["y"] = ct->transform.scale.y;
        controller_json["transform"]["scale"]["z"] = ct->transform.scale.z;

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

    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to save layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename), file.errorString());
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
            .arg(QString::fromStdString(filename), file.errorString());
        QMessageBox::critical(this, "Write Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write file: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    
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

    if(viewport)
    {
        const nlohmann::json& cam = layout_json["camera"];
        const float           dist  = cam["distance"].get<float>();
        const float           yaw   = cam["yaw"].get<float>();
        const float           pitch = cam["pitch"].get<float>();
        const nlohmann::json& tgt   = cam["target"];
        const float           tx    = tgt["x"].get<float>();
        const float           ty    = tgt["y"].get<float>();
        const float           tz    = tgt["z"].get<float>();
        viewport->SetCamera(dist, yaw, pitch, tx, ty, tz);
    }

    clearAllClicked();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    std::unordered_map<RGBController*, Vector3D> physical_spacing_by_controller;

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
                std::unordered_map<RGBController*, Vector3D>::iterator it = physical_spacing_by_controller.find(controller);
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
                    ctrl_transform->led_positions      = virtual_ctrl->GenerateLEDPositions(
                        grid_scale_mm,
                        ctrl_transform->led_spacing_mm_x,
                        ctrl_transform->led_spacing_mm_y,
                        ctrl_transform->led_spacing_mm_z);
                }
                else
                {
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
                    if(item_idx >= 0 && item_idx < (int)controller->zones.size())
                    {
                        name += " - " + QString::fromStdString(controller->GetZoneName((unsigned int)item_idx));
                    }
                }
                else if(granularity == 2)
                {
                    name = QString("[LED] ") + ControllerDisplay::FormatRgbControllerTitle(controller);
                    if(item_idx >= 0 && item_idx < (int)controller->leds.size())
                    {
                        name += " - " + QString::fromStdString(controller->GetLEDName((unsigned int)item_idx));
                    }
                }
                else
                {
                    name = ControllerDisplay::FormatRgbControllerTitle(controller);
                    if(led_positions_size < controller->leds.size())
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
                            if(first_zone_idx < controller->zones.size())
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
}

void OpenRGB3DSpatialTab::LoadLayout(const std::string& filename)
{

    QFile file(QString::fromStdString(filename));

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to open layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename), file.errorString());
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
            .arg(QString::fromStdString(filename), file.errorString());
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
    catch(const nlohmann::json::parse_error& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to parse layout JSON: %s - %s", filename.c_str(), e.what());
        QMessageBox::critical(this, tr("Invalid Layout File"),
                              tr("Could not parse layout JSON:\n%1\n\n%2\n\nSave a new profile from the plugin if this file is corrupt.")
                                  .arg(QString::fromStdString(filename), QString::fromUtf8(e.what())));
        return;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load layout: %s - %s", filename.c_str(), e.what());
        QMessageBox::critical(this, tr("Layout Not Loaded"), LayoutLoadErrorUserMessage(filename, e.what()));
        return;
    }
}

std::string OpenRGB3DSpatialTab::GetLayoutPath(const std::string& layout_name)
{
    if(!resource_manager) return std::string();
    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    filesystem::path plugins_dir = PluginSettingsPaths::LayoutsDir(resource_manager);

    QDir dir;
    dir.mkpath(QString::fromStdString(plugins_dir.string()));

    std::string filename = layout_name + ".json";
    filesystem::path layout_file = plugins_dir / filename;

    return layout_file.string();
}

void OpenRGB3DSpatialTab::PopulateLayoutDropdown()
{
    if(!resource_manager || !layoutProfilesCombo()) return;
    QString current_text = layoutProfilesCombo()->currentText();

    layoutProfilesCombo()->blockSignals(true);
    layoutProfilesCombo()->clear();

    filesystem::path layouts_dir = PluginSettingsPaths::LayoutsDir(resource_manager);

    QDir dir(QString::fromStdString(layouts_dir.string()));
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for(int i = 0; i < files.size(); i++)
    {
        QString base_name = files[i].baseName();
        layoutProfilesCombo()->addItem(base_name);
    }

    nlohmann::json settings = GetPluginSettings();
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
        int index = layoutProfilesCombo()->findText(saved_profile);
        if(index >= 0)
        {
            layoutProfilesCombo()->setCurrentIndex(index);
        }
        else
        {
        }
    }
    else if(!current_text.isEmpty())
    {
        int index = layoutProfilesCombo()->findText(current_text);
        if(index >= 0)
        {
            layoutProfilesCombo()->setCurrentIndex(index);
        }
    }

    layoutProfilesCombo()->blockSignals(false);
}

void OpenRGB3DSpatialTab::SaveCurrentLayoutName()
{
    if(!layoutProfilesCombo() || !autoLoadLayoutCheckbox())
    {
        return;
    }

    std::string profile_name = layoutProfilesCombo()->currentText().toStdString();
    bool auto_load_enabled = autoLoadLayoutCheckbox()->isChecked();

    nlohmann::json settings = GetPluginSettings();
    settings["SelectedProfile"] = profile_name;
    settings["AutoLoadEnabled"] = auto_load_enabled;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::TryAutoLoadLayout()
{
    if(!first_load)
    {
        return;
    }

    first_load = false;

    if(!autoLoadLayoutCheckbox() || !layoutProfilesCombo())
    {
        return;
    }

    nlohmann::json settings = GetPluginSettings();

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

    bool restore_auto_load_signals = autoLoadLayoutCheckbox()->blockSignals(true);
    autoLoadLayoutCheckbox()->setChecked(auto_load_enabled);
    autoLoadLayoutCheckbox()->blockSignals(restore_auto_load_signals);

    if(!saved_profile.empty())
    {
        int index = layoutProfilesCombo()->findText(QString::fromStdString(saved_profile));
        if(index >= 0)
        {
            bool restore_layout_combo_signals = layoutProfilesCombo()->blockSignals(true);
            layoutProfilesCombo()->setCurrentIndex(index);
            layoutProfilesCombo()->blockSignals(restore_layout_combo_signals);
        }
    }

    if(auto_load_enabled && !saved_profile.empty())
    {
        std::string layout_path = GetLayoutPath(saved_profile);
        if(layout_path.empty()) return;
        QFileInfo check_file(QString::fromStdString(layout_path));

        if(check_file.exists())
        {
            LoadLayout(layout_path);
        }
    }

    TryAutoLoadEffectProfile();
}

void OpenRGB3DSpatialTab::RegenerateLEDPositions(ControllerTransform* transform)
{
    if(!transform) return;

    if(transform->virtual_controller)
    {
        transform->led_positions = transform->virtual_controller->GenerateLEDPositions(
            grid_scale_mm,
            transform->led_spacing_mm_x,
            transform->led_spacing_mm_y,
            transform->led_spacing_mm_z);
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
