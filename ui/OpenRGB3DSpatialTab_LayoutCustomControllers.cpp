// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ControllerDisplayUtils.h"
#include "SpatialTabLedHelpers.h"
#include "PluginSettingsPaths.h"
#include "SpatialControllerCardList.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"
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
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace filesystem = std::filesystem;

void OpenRGB3DSpatialTab::on_create_custom_controller_clicked()
{
    CustomControllerDialog dialog(resource_manager, this);
    dialog.SetLayoutGridScaleMm(grid_scale_mm);

    if(dialog.exec() != QDialog::Accepted)
        return;

    QString controller_name = dialog.GetControllerName();
    std::string name_str = controller_name.toStdString();

    int existing_index = -1;
    for(size_t i = 0; i < virtual_controllers.size(); i++)
    {
        if(virtual_controllers[i] && virtual_controllers[i]->GetName() == name_str)
        {
            existing_index = static_cast<int>(i);
            break;
        }
    }
    if(existing_index >= 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Name",
            QString("A custom controller named '%1' already exists.\n\nReplace it?")
                .arg(controller_name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(reply != QMessageBox::Yes)
        {
            SetObjectCreatorStatus(QString("Create cancelled. Use a different name and try again."), true);
            return;
        }
        VirtualController3D* to_remove = virtual_controllers[existing_index].get();
        std::vector<int> transform_indices_to_remove;
        for(size_t ti = 0; ti < controller_transforms.size(); ti++)
        {
            if(controller_transforms[ti] && controller_transforms[ti]->virtual_controller == to_remove)
                transform_indices_to_remove.push_back(static_cast<int>(ti));
        }
        std::vector<int> row_for_transform(controller_transforms.size(), -1);
        int transform_count = 0;
        for(int row = 0; row < scene_controllers_.count() && transform_count < (int)controller_transforms.size(); row++)
        {
            if(!scene_controllers_.hasUserRole(row))
            {
                row_for_transform[transform_count++] = row;
            }
        }
        std::vector<int> rows_to_remove;
        for(int ti : transform_indices_to_remove)
        {
            if(ti >= 0 && ti < (int)row_for_transform.size() && row_for_transform[ti] >= 0)
                rows_to_remove.push_back(row_for_transform[ti]);
        }
        std::sort(rows_to_remove.begin(), rows_to_remove.end(), std::greater<int>());
        for(int row : rows_to_remove)
            scene_controllers_.removeAt(row);
        std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
        for(int ti : transform_indices_to_remove)
        {
            RemoveControllerLinkedReferencePoint(ti);
            controller_transforms.erase(controller_transforms.begin() + ti);
        }
        if(viewport)
        {
            viewport->SetControllerTransforms(&controller_transforms);
            viewport->update();
        }
        virtual_controllers.erase(virtual_controllers.begin() + existing_index);
        if(existing_index < (int)virtual_controller_json_files.size())
        {
            virtual_controller_json_files.erase(virtual_controller_json_files.begin() + existing_index);
        }
        if(customControllersList() && existing_index < customControllersList()->count())
            delete customControllersList()->takeItem(existing_index);
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();
        RefreshHiddenControllerStates();
    }

    std::unique_ptr<VirtualController3D> virtual_ctrl = std::make_unique<VirtualController3D>(
        name_str,
        dialog.GetGridWidth(),
        dialog.GetGridHeight(),
        dialog.GetGridDepth(),
        dialog.GetLEDMappings(),
        dialog.GetSpacingX(),
        dialog.GetSpacingY(),
        dialog.GetSpacingZ()
    );

    int new_index = (int)virtual_controllers.size();
    virtual_controllers.push_back(std::move(virtual_ctrl));
    virtual_controller_json_files.push_back(std::string());

    SaveCustomControllers();
    UpdateCustomControllersList();
    UpdateAvailableControllersList();
    SelectAvailableControllerEntry(-1, new_index);
}


void OpenRGB3DSpatialTab::on_export_custom_controller_clicked()
{
    if(virtual_controllers.empty())
    {
        QMessageBox::warning(this, "No Custom Controllers", "No custom controllers available to export");
        return;
    }

    int list_row = customControllersList()->currentRow();
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

    const QString default_dir = QString::fromStdString(
        PluginSettingsPaths::ControllersDir(resource_manager).string());
    const std::string default_filename =
        VirtualController3D::PresetFilenameSlug(ctrl->GetName()) + ".json";
    QString filename = QFileDialog::getSaveFileName(this, tr("Export portable controller preset"),
                                                    default_dir + "/" + QString::fromStdString(default_filename),
                                                    PluginSettingsPaths::kControllerLayoutJsonFilter);
    if(filename.isEmpty())
    {
        return;
    }

    nlohmann::json export_data = ctrl->ToPortablePresetJson();

    std::ofstream file(filename.toStdString());
    if(file.is_open())
    {
        try
        {
            file << export_data.dump(4);
            if(file.fail() || file.bad())
            {
                file.close();
                QMessageBox::critical(this, "Export Failed",
                                    QString("Failed to write custom controller to:\n%1").arg(filename));
                return;
            }
            file.close();
            QMessageBox::information(
                this,
                tr("Export Successful"),
                tr("Portable preset '%1' exported to:\n%2\n\n"
                   "Uses controller_location \"1:1\" and OpenRGB device names — safe to share or submit to "
                   "OpenRGB3DSpatialPresets.")
                    .arg(QString::fromStdString(ctrl->GetName()), filename));
        }
        catch(const std::exception& e)
        {
            file.close();
            QMessageBox::critical(this, "Export Failed",
                                QString("Exception while exporting custom controller:\n%1\n\nError: %2")
                                .arg(filename, e.what()));
        }
    }
    else
    {
        QMessageBox::critical(this, "Export Failed",
                            QString("Failed to open file for writing:\n%1").arg(filename));
    }
}

void OpenRGB3DSpatialTab::on_import_custom_controller_clicked()
{
    if(!resource_manager)
    {
        return;
    }

    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    const filesystem::path controllers_dir = PluginSettingsPaths::ControllersDir(resource_manager);
    const QString default_dir = QString::fromStdString(controllers_dir.string());

    const QStringList sources = QFileDialog::getOpenFileNames(
        this,
        "Import Controller Layouts",
        default_dir,
        PluginSettingsPaths::kControllerLayoutJsonFilter);
    if(sources.isEmpty())
    {
        return;
    }

    int copied = 0;
    int skipped = 0;
    int failed = 0;
    QStringList failed_names;

    for(const QString& src_q : sources)
    {
        const QFileInfo src_info(src_q);
        if(!src_info.exists() || !src_info.isFile())
        {
            failed++;
            failed_names.append(src_info.fileName());
            continue;
        }

        if(src_info.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0)
        {
            skipped++;
            continue;
        }

        const std::string dest_name = src_info.fileName().toStdString();
        const filesystem::path dest_path = controllers_dir / dest_name;

        if(filesystem::exists(dest_path))
        {
            const QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                "Replace File?",
                QString("'%1' already exists in the controllers folder.\n\nReplace it?")
                    .arg(QString::fromStdString(dest_name)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if(reply != QMessageBox::Yes)
            {
                skipped++;
                continue;
            }
        }

        if(QFile::exists(QString::fromStdString(dest_path.string())))
        {
            QFile::remove(QString::fromStdString(dest_path.string()));
        }

        if(!QFile::copy(src_q, QString::fromStdString(dest_path.string())))
        {
            failed++;
            failed_names.append(src_info.fileName());
            continue;
        }

        if(!LoadControllerFromJsonFile(dest_path, true))
        {
            failed++;
            failed_names.append(src_info.fileName());
            std::error_code remove_ec;
            filesystem::remove(dest_path, remove_ec);
            continue;
        }

        copied++;
    }

    UpdateCustomControllersList();
    UpdateAvailableControllersList();

    QString summary = QString("Imported %1 controller file(s) into:\n%2")
                          .arg(copied)
                          .arg(default_dir);
    if(skipped > 0)
    {
        summary += QString("\n\nSkipped: %1").arg(skipped);
    }
    if(failed > 0)
    {
        summary += QString("\n\nFailed: %1 (%2)")
                       .arg(failed)
                       .arg(failed_names.join(QStringLiteral(", ")));
    }

    if(copied > 0)
    {
        QMessageBox::information(this, "Import Complete", summary);
    }
    else
    {
        QMessageBox::warning(this, "Import", summary);
    }
}

void OpenRGB3DSpatialTab::on_edit_custom_controller_clicked()
{
    int list_row = customControllersList()->currentRow();
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
                                  virtual_ctrl->GetMappings(),
                                  virtual_ctrl->GetSpacingX(),
                                  virtual_ctrl->GetSpacingY(),
                                  virtual_ctrl->GetSpacingZ());

    dialog.SetLayoutGridScaleMm(grid_scale_mm);

    if(dialog.exec() == QDialog::Accepted)
    {
        std::string old_name = virtual_ctrl->GetName();
        std::string new_name = dialog.GetControllerName().toStdString();

        int other_index = -1;
        for(size_t i = 0; i < virtual_controllers.size(); i++)
        {
            if((int)i != list_row && virtual_controllers[i] && virtual_controllers[i]->GetName() == new_name)
            {
                other_index = static_cast<int>(i);
                break;
            }
        }
        if(other_index >= 0)
        {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Name",
                QString("A custom controller named '%1' already exists.\n\nReplace it?")
                    .arg(QString::fromStdString(new_name)),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if(reply != QMessageBox::Yes)
            {
                SetObjectCreatorStatus(QString("Edit cancelled. Use a different name and try again."), true);
                return;
            }
            VirtualController3D* to_remove = virtual_controllers[other_index].get();
            std::vector<int> transform_indices_to_remove;
            for(size_t ti = 0; ti < controller_transforms.size(); ti++)
            {
                if(controller_transforms[ti] && controller_transforms[ti]->virtual_controller == to_remove)
                    transform_indices_to_remove.push_back(static_cast<int>(ti));
            }
            std::vector<int> row_for_transform(controller_transforms.size(), -1);
            int transform_count = 0;
            for(int row = 0; row < scene_controllers_.count() && transform_count < (int)controller_transforms.size(); row++)
            {
                if(!scene_controllers_.hasUserRole(row))
                {
                    row_for_transform[transform_count++] = row;
                }
            }
            std::vector<int> rows_to_remove;
            for(int ti : transform_indices_to_remove)
            {
                if(ti >= 0 && ti < (int)row_for_transform.size() && row_for_transform[ti] >= 0)
                    rows_to_remove.push_back(row_for_transform[ti]);
            }
            std::sort(rows_to_remove.begin(), rows_to_remove.end(), std::greater<int>());
            for(int row : rows_to_remove)
                scene_controllers_.removeAt(row);
            std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
            for(int ti : transform_indices_to_remove)
            {
                RemoveControllerLinkedReferencePoint(ti);
                controller_transforms.erase(controller_transforms.begin() + ti);
            }
            if(viewport)
            {
                viewport->SetControllerTransforms(&controller_transforms);
                viewport->update();
            }
            virtual_controllers.erase(virtual_controllers.begin() + other_index);
            if(other_index < (int)virtual_controller_json_files.size())
            {
                virtual_controller_json_files.erase(virtual_controller_json_files.begin() + other_index);
            }
            if(customControllersList() && other_index < customControllersList()->count())
                delete customControllersList()->takeItem(other_index);
            if(other_index < list_row)
                list_row--;
            UpdateAvailableControllersList();
            UpdateAvailableItemCombo();
            RefreshHiddenControllerStates();
        }

        if(old_name != new_name && list_row < (int)virtual_controller_json_files.size())
        {
            const filesystem::path previous_filepath =
                PluginSettingsPaths::ControllersDir(resource_manager) / virtual_controller_json_files[list_row];
            std::error_code remove_ec;
            filesystem::remove(previous_filepath, remove_ec);
        }

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

        VirtualController3D* new_ptr = virtual_controllers[list_row].get();
        for(size_t i = 0; i < controller_transforms.size(); i++)
        {
            ControllerTransform* t = controller_transforms[i].get();
            if(t && t->virtual_controller == old_ptr)
            {
                t->virtual_controller = new_ptr;
                t->led_spacing_mm_x = new_ptr->GetSpacingX();
                t->led_spacing_mm_y = new_ptr->GetSpacingY();
                t->led_spacing_mm_z = new_ptr->GetSpacingZ();
                t->led_positions = new_ptr->GenerateLEDPositions(grid_scale_mm);
                ControllerLayout3D::MarkWorldPositionsDirty(t);

                int list_row = TransformIndexToControllerListRow(static_cast<int>(i));
                if(list_row >= 0)
                {
                    scene_controllers_.setTextAt(list_row,
                                                 QString("[Custom] ") + QString::fromStdString(new_ptr->GetName()));
                }
            }
        }

        SaveCustomControllers();
        UpdateAvailableControllersList();

        if(viewport)
        {
            viewport->SetControllerTransforms(&controller_transforms);
            viewport->update();
        }
    }
}

void OpenRGB3DSpatialTab::on_delete_custom_controller_clicked()
{
    if(!customControllersList()) return;
    int list_row = customControllersList()->currentRow();
    if(list_row < 0)
    {
        QMessageBox::warning(this, "No Selection", "Please select a custom controller from the list to delete.");
        return;
    }
    if(list_row >= (int)virtual_controllers.size())
    {
        QMessageBox::warning(this, "Invalid Selection", "Selected custom controller does not exist.");
        return;
    }

    VirtualController3D* to_delete = virtual_controllers[list_row].get();
    std::string name = to_delete->GetName();

    std::vector<int> transform_indices_to_remove;
    for(size_t ti = 0; ti < controller_transforms.size(); ti++)
    {
        if(controller_transforms[ti] && controller_transforms[ti]->virtual_controller == to_delete)
        {
            transform_indices_to_remove.push_back(static_cast<int>(ti));
        }
    }

    if(!transform_indices_to_remove.empty())
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Controller in 3D Scene",
            QString("Custom controller '%1' is in the 3D scene.\n\nRemove it from the scene and delete from library?")
                .arg(QString::fromStdString(name)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(reply != QMessageBox::Yes)
            return;

        std::vector<int> row_for_transform(controller_transforms.size(), -1);
        int transform_count = 0;
        for(int row = 0; row < scene_controllers_.count() && transform_count < (int)controller_transforms.size(); row++)
        {
            if(!scene_controllers_.hasUserRole(row))
            {
                row_for_transform[transform_count++] = row;
            }
        }
        std::vector<int> rows_to_remove;
        for(int ti : transform_indices_to_remove)
        {
            if(ti >= 0 && ti < (int)row_for_transform.size() && row_for_transform[ti] >= 0)
                rows_to_remove.push_back(row_for_transform[ti]);
        }
        std::sort(rows_to_remove.begin(), rows_to_remove.end(), std::greater<int>());
        for(int row : rows_to_remove)
        {
            scene_controllers_.removeAt(row);
        }
        std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
        for(int ti : transform_indices_to_remove)
        {
            RemoveControllerLinkedReferencePoint(ti);
            controller_transforms.erase(controller_transforms.begin() + ti);
        }
        if(viewport)
        {
            viewport->SetControllerTransforms(&controller_transforms);
            viewport->update();
        }
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();
        RefreshHiddenControllerStates();
    }
    else
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Custom Controller",
            QString("Delete custom controller '%1' from library?").arg(QString::fromStdString(name)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(reply != QMessageBox::Yes)
            return;
    }

    if(list_row < (int)virtual_controller_json_files.size())
    {
        const filesystem::path json_path =
            PluginSettingsPaths::ControllersDir(resource_manager) / virtual_controller_json_files[list_row];
        std::error_code remove_ec;
        filesystem::remove(json_path, remove_ec);
    }

    virtual_controllers.erase(virtual_controllers.begin() + list_row);
    if(list_row < (int)virtual_controller_json_files.size())
    {
        virtual_controller_json_files.erase(virtual_controller_json_files.begin() + list_row);
    }
    if(customControllersList())
    {
        delete customControllersList()->takeItem(list_row);
    }
    UpdateCustomControllersList();
    UpdateAvailableControllersList();
    SetObjectCreatorStatus(QString("Custom controller '%1' deleted.").arg(QString::fromStdString(name)));
    QMessageBox::information(this, "Deleted", QString("Custom controller '%1' removed from library.").arg(QString::fromStdString(name)));
}

std::string OpenRGB3DSpatialTab::SafeControllerJsonFilename(const std::string& controller_name)
{
    std::string safe_name = controller_name;
    for(unsigned int j = 0; j < safe_name.length(); j++)
    {
        if(safe_name[j] == '/' || safe_name[j] == '\\' || safe_name[j] == ':' || safe_name[j] == '*'
           || safe_name[j] == '?' || safe_name[j] == '"' || safe_name[j] == '<' || safe_name[j] == '>'
           || safe_name[j] == '|')
        {
            safe_name[j] = '_';
        }
    }
    return safe_name + ".json";
}

void OpenRGB3DSpatialTab::RemoveVirtualControllerFromLibrary(int index)
{
    if(index < 0 || index >= (int)virtual_controllers.size())
    {
        return;
    }

    VirtualController3D* to_remove = virtual_controllers[index].get();
    std::vector<int> transform_indices_to_remove;
    for(size_t ti = 0; ti < controller_transforms.size(); ti++)
    {
        if(controller_transforms[ti] && controller_transforms[ti]->virtual_controller == to_remove)
        {
            transform_indices_to_remove.push_back(static_cast<int>(ti));
        }
    }

    if(!transform_indices_to_remove.empty())
    {
        std::vector<int> row_for_transform(controller_transforms.size(), -1);
        int transform_count = 0;
        for(int row = 0; row < scene_controllers_.count() && transform_count < (int)controller_transforms.size(); row++)
        {
            if(!scene_controllers_.hasUserRole(row))
            {
                row_for_transform[transform_count++] = row;
            }
        }
        std::vector<int> rows_to_remove;
        for(int ti : transform_indices_to_remove)
        {
            if(ti >= 0 && ti < (int)row_for_transform.size() && row_for_transform[ti] >= 0)
            {
                rows_to_remove.push_back(row_for_transform[ti]);
            }
        }
        std::sort(rows_to_remove.begin(), rows_to_remove.end(), std::greater<int>());
        for(int row : rows_to_remove)
        {
            scene_controllers_.removeAt(row);
        }
        std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
        for(int ti : transform_indices_to_remove)
        {
            RemoveControllerLinkedReferencePoint(ti);
            controller_transforms.erase(controller_transforms.begin() + ti);
        }
        if(viewport)
        {
            viewport->SetControllerTransforms(&controller_transforms);
            viewport->update();
        }
    }

    if(index < (int)virtual_controller_json_files.size())
    {
        std::error_code remove_ec;
        filesystem::remove(PluginSettingsPaths::ControllersDir(resource_manager) / virtual_controller_json_files[index],
                           remove_ec);
    }

    virtual_controllers.erase(virtual_controllers.begin() + index);
    if(index < (int)virtual_controller_json_files.size())
    {
        virtual_controller_json_files.erase(virtual_controller_json_files.begin() + index);
    }
}

bool OpenRGB3DSpatialTab::LoadControllerFromJsonFile(const filesystem::path& json_path,
                                                     bool replace_if_same_filename)
{
    if(!resource_manager)
    {
        return false;
    }

    if(!filesystem::exists(json_path) || json_path.extension() != ".json")
    {
        return false;
    }

    const std::string filename = json_path.filename().string();

    int existing_index = -1;
    for(size_t i = 0; i < virtual_controller_json_files.size(); ++i)
    {
        if(virtual_controller_json_files[i] == filename)
        {
            existing_index = static_cast<int>(i);
            break;
        }
    }

    if(existing_index >= 0)
    {
        if(!replace_if_same_filename)
        {
            return true;
        }
        RemoveVirtualControllerFromLibrary(existing_index);
    }

    std::ifstream file(json_path.string());
    if(!file.is_open())
    {
        return false;
    }

    try
    {
        nlohmann::json ctrl_json;
        file >> ctrl_json;
        file.close();

        std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(ctrl_json, controllers);
        if(!virtual_ctrl)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to create controller from: %s", filename.c_str());
            return false;
        }

        virtual_controllers.push_back(std::move(virtual_ctrl));
        virtual_controller_json_files.push_back(filename);
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load controller %s: %s", filename.c_str(), e.what());
        return false;
    }
}

void OpenRGB3DSpatialTab::SaveCustomControllers()
{
    if(!resource_manager)
    {
        return;
    }

    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    const filesystem::path controllers_dir = PluginSettingsPaths::ControllersDir(resource_manager);

    if(virtual_controller_json_files.size() < virtual_controllers.size())
    {
        virtual_controller_json_files.resize(virtual_controllers.size());
    }

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        const std::string json_filename = SafeControllerJsonFilename(virtual_controllers[i]->GetName());
        const filesystem::path filepath = controllers_dir / json_filename;

        if(i < virtual_controller_json_files.size()
           && !virtual_controller_json_files[i].empty()
           && virtual_controller_json_files[i] != json_filename)
        {
            const filesystem::path old_path = controllers_dir / virtual_controller_json_files[i];
            std::error_code remove_ec;
            filesystem::remove(old_path, remove_ec);
        }

        virtual_controller_json_files[i] = json_filename;

        std::ofstream file(filepath.string());
        if(file.is_open())
        {
            try
            {
                nlohmann::json ctrl_json = virtual_controllers[i]->ToJson();
                file << ctrl_json.dump(4);
                file.close();

                if(file.fail() || file.bad())
                {
                    LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write controller: %s", filepath.string().c_str());
                }
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Exception while saving controller: %s - %s",
                          filepath.string().c_str(),
                          e.what());
                file.close();
            }
        }
        else
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open controller file for writing: %s",
                      filepath.string().c_str());
        }
    }
}

void OpenRGB3DSpatialTab::LoadCustomControllers()
{
    if(!resource_manager)
    {
        return;
    }

    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    const filesystem::path dir_path = PluginSettingsPaths::ControllersDir(resource_manager);
    if(filesystem::exists(dir_path))
    {
        try
        {
            for(const filesystem::directory_entry& entry : filesystem::directory_iterator(dir_path))
            {
                if(entry.is_regular_file() && entry.path().extension() == ".json")
                {
                    LoadControllerFromJsonFile(entry.path(), false);
                }
            }
        }
        catch(const std::exception&)
        {
        }
    }

    UpdateAvailableControllersList();
    UpdateCustomControllersList();
}

bool OpenRGB3DSpatialTab::IsItemInScene(RGBController* controller, int granularity, int item_idx) const
{
    if(!controller)
    {
        return false;
    }

    std::vector<bool> used_leds(controller->leds.size(), false);
    std::function<void(unsigned int, unsigned int)> mark_led_used = [&](unsigned int zone_idx, unsigned int led_idx)
    {
        unsigned int global_led_idx = 0;
        if(TryGetObjectCreatorGlobalLedIndex(controller, zone_idx, led_idx, &global_led_idx) &&
           global_led_idx < used_leds.size())
        {
            used_leds[global_led_idx] = true;
        }
    };

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(!ct)
        {
            continue;
        }

        if(ct->controller == controller)
        {
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                mark_led_used(ct->led_positions[j].zone_idx, ct->led_positions[j].led_idx);
            }
        }
        else if(ct->virtual_controller)
        {
            const std::vector<GridLEDMapping>& mappings = ct->virtual_controller->GetMappings();
            for(unsigned int j = 0; j < mappings.size(); j++)
            {
                if(mappings[j].controller != controller)
                {
                    continue;
                }
                mark_led_used(mappings[j].zone_idx, mappings[j].led_idx);
            }
        }
    }

    if(granularity == 0)
    {
        for(unsigned int i = 0; i < used_leds.size(); i++)
        {
            if(!used_leds[i])
            {
                return false;
            }
        }
        return !used_leds.empty();
    }
    else if(granularity == 1)
    {
        if(item_idx < 0 || item_idx >= (int)controller->zones.size())
        {
            return true;
        }
        const zone& selected_zone = controller->zones[(unsigned int)item_idx];
        for(unsigned int led_idx = 0; led_idx < selected_zone.leds_count; led_idx++)
        {
            unsigned int global_led_idx = selected_zone.start_idx + led_idx;
            if(global_led_idx < used_leds.size() && !used_leds[global_led_idx])
            {
                return false;
            }
        }
        return true;
    }
    else if(granularity == 2)
    {
        if(item_idx < 0)
        {
            return true;
        }
        unsigned int led_idx = (unsigned int)item_idx;
        return (led_idx >= used_leds.size()) ? true : used_leds[led_idx];
    }

    return false;
}

int OpenRGB3DSpatialTab::GetUnassignedZoneCount(RGBController* controller) const
{
    if(!controller)
    {
        return 0;
    }

    std::vector<bool> used_leds(controller->leds.size(), false);
    std::function<void(unsigned int, unsigned int)> mark_led_used = [&](unsigned int zone_idx, unsigned int led_idx)
    {
        unsigned int global_led_idx = 0;
        if(TryGetObjectCreatorGlobalLedIndex(controller, zone_idx, led_idx, &global_led_idx) &&
           global_led_idx < used_leds.size())
        {
            used_leds[global_led_idx] = true;
        }
    };

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(!ct)
        {
            continue;
        }

        if(ct->controller == controller)
        {
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                mark_led_used(ct->led_positions[j].zone_idx, ct->led_positions[j].led_idx);
            }
        }
        else if(ct->virtual_controller)
        {
            const std::vector<GridLEDMapping>& mappings = ct->virtual_controller->GetMappings();
            for(unsigned int j = 0; j < mappings.size(); j++)
            {
                if(mappings[j].controller != controller)
                {
                    continue;
                }
                mark_led_used(mappings[j].zone_idx, mappings[j].led_idx);
            }
        }
    }

    int unassigned_count = 0;
    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        const zone& z = controller->zones[zone_idx];
        bool zone_has_free_led = false;
        for(unsigned int led_idx = 0; led_idx < z.leds_count; led_idx++)
        {
            unsigned int global_led_idx = z.start_idx + led_idx;
            if(global_led_idx < used_leds.size() && !used_leds[global_led_idx])
            {
                zone_has_free_led = true;
                break;
            }
        }
        if(zone_has_free_led)
        {
            unassigned_count++;
        }
    }
    return unassigned_count;
}

int OpenRGB3DSpatialTab::GetUnassignedLEDCount(RGBController* controller) const
{
    if(!controller)
    {
        return 0;
    }

    std::vector<bool> used_leds(controller->leds.size(), false);
    std::function<void(unsigned int, unsigned int)> mark_led_used = [&](unsigned int zone_idx, unsigned int led_idx)
    {
        unsigned int global_led_idx = 0;
        if(TryGetObjectCreatorGlobalLedIndex(controller, zone_idx, led_idx, &global_led_idx) &&
           global_led_idx < used_leds.size())
        {
            used_leds[global_led_idx] = true;
        }
    };

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(!ct)
        {
            continue;
        }

        if(ct->controller == controller)
        {
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                mark_led_used(ct->led_positions[j].zone_idx, ct->led_positions[j].led_idx);
            }
        }
        else if(ct->virtual_controller)
        {
            const std::vector<GridLEDMapping>& mappings = ct->virtual_controller->GetMappings();
            for(unsigned int j = 0; j < mappings.size(); j++)
            {
                if(mappings[j].controller != controller)
                {
                    continue;
                }
                mark_led_used(mappings[j].zone_idx, mappings[j].led_idx);
            }
        }
    }

    int used_count = 0;
    for(unsigned int i = 0; i < used_leds.size(); i++)
    {
        if(used_leds[i])
        {
            used_count++;
        }
    }

    int total_leds = (int)controller->leds.size();
    return std::max(0, total_leds - used_count);
}

