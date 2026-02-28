// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "OpenRGB3DSpatialTab_Presets.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"
#include "RGBController.h"
#include "DisplayPlaneManager.h"
#include "ScreenCaptureManager.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "SettingsManager.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
#include <nlohmann/json.hpp>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QTextStream>
#include <QApplication>
#include <QPalette>
#include <QFont>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <set>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <system_error>
#include <limits>
#include <functional>

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
    QPalette pal = object_creator_status_label->palette();
    QPalette app_palette = QApplication::palette(object_creator_status_label);
    pal.setColor(QPalette::WindowText, app_palette.color(is_error ? QPalette::BrightText : QPalette::Link));
    object_creator_status_label->setPalette(pal);
    QFont status_font = object_creator_status_label->font();
    status_font.setBold(is_error);
    object_creator_status_label->setFont(status_font);
    object_creator_status_label->setText(message);
}


void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager)
    {
        return;
    }

    UpdateAvailableControllersList();

    if(viewport) viewport->SetControllerTransforms(&controller_transforms);
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::UpdateAvailableControllersList()
{
    if(!resource_manager) return;
    if(!available_controllers_list)
    {
        return;
    }

    QPair<int, int> previous_metadata( std::numeric_limits<int>::min(), 0 );
    QString previous_text;
    int previous_row = available_controllers_list->currentRow();
    if(previous_row >= 0 && previous_row < available_controllers_list->count())
    {
        QListWidgetItem* prev_item = available_controllers_list->item(previous_row);
        if(prev_item)
        {
            QVariant data = prev_item->data(Qt::UserRole);
            if(data.isValid())
            {
                previous_metadata = data.value<QPair<int, int>>();
            }
            previous_text = prev_item->text();
        }
    }

    available_controllers_list->blockSignals(true);
    available_controllers_list->clear();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    std::unordered_set<const VirtualController3D*> virtuals_in_scene;
    for(size_t transform_index = 0; transform_index < controller_transforms.size(); transform_index++)
    {
        const std::unique_ptr<ControllerTransform>& transform_ptr = controller_transforms[transform_index];
        if(transform_ptr && transform_ptr->virtual_controller)
        {
            virtuals_in_scene.insert(transform_ptr->virtual_controller);
        }
    }

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        int unassigned_zones = GetUnassignedZoneCount(controllers[i]);
        int unassigned_leds = GetUnassignedLEDCount(controllers[i]);

        if(unassigned_leds > 0)
        {
            QString display_text = QString::fromStdString(controllers[i]->GetName()) +
                                   QString(" [%1 zones, %2 LEDs available]").arg(unassigned_zones).arg(unassigned_leds);
            QListWidgetItem* item = new QListWidgetItem(display_text);
            item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(static_cast<int>(i), -1)));
            available_controllers_list->addItem(item);
        }
    }

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        VirtualController3D* virtual_ctrl = virtual_controllers[i].get();
        if(!virtual_ctrl)
        {
            continue;
        }

        if(virtuals_in_scene.find(virtual_ctrl) != virtuals_in_scene.end())
        {
            continue;
        }

        QListWidgetItem* item = new QListWidgetItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));
        item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-1, static_cast<int>(i))));
        available_controllers_list->addItem(item);
    }

    for(unsigned int i = 0; i < reference_points.size(); i++)
    {
        if(reference_points[i] && !reference_points[i]->IsVisible())
        {
            QListWidgetItem* item = new QListWidgetItem(QString("[Ref Point] ") + QString::fromStdString(reference_points[i]->GetName()));
            item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, static_cast<int>(i))));
            available_controllers_list->addItem(item);
        }
    }

    for(unsigned int i = 0; i < display_planes.size(); i++)
    {
        if(display_planes[i] && !display_planes[i]->IsVisible())
        {
            QListWidgetItem* item = new QListWidgetItem(QString("[Display] ") + QString::fromStdString(display_planes[i]->GetName()));
            item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-3, display_planes[i]->GetId())));
            available_controllers_list->addItem(item);
        }
    }

    available_controllers_list->blockSignals(false);

    UpdateCustomControllersList();

    bool selection_restored = false;
    if(previous_metadata.first != std::numeric_limits<int>::min())
    {
        int row = FindAvailableControllerRow(previous_metadata.first, previous_metadata.second);
        if(row >= 0)
        {
            available_controllers_list->setCurrentRow(row);
            selection_restored = true;
        }
    }

    if(!selection_restored && !previous_text.isEmpty())
    {
        for(int row = 0; row < available_controllers_list->count(); row++)
        {
            QListWidgetItem* item = available_controllers_list->item(row);
            if(item && item->text() == previous_text)
            {
                available_controllers_list->setCurrentRow(row);
                selection_restored = true;
                break;
            }
        }
    }

    if(!selection_restored && available_controllers_list->count() > 0)
    {
        available_controllers_list->setCurrentRow(0);
    }

    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::UpdateCustomControllersList()
{
    custom_controllers_list->clear();

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        custom_controllers_list->addItem(QString::fromStdString(virtual_controllers[i]->GetName()));
    }

    if(custom_controllers_empty_label)
    {
        custom_controllers_empty_label->setVisible(custom_controllers_list->count() == 0);
    }
}

int OpenRGB3DSpatialTab::FindAvailableControllerRow(int type_code, int object_index) const
{
    if(!available_controllers_list)
    {
        return -1;
    }

    for(int row = 0; row < available_controllers_list->count(); row++)
    {
        QListWidgetItem* item = available_controllers_list->item(row);
        if(!item)
        {
            continue;
        }
        QVariant data = item->data(Qt::UserRole);
        if(!data.isValid())
        {
            continue;
        }
        QPair<int, int> metadata = data.value<QPair<int, int>>();
        if(metadata.first == type_code && metadata.second == object_index)
        {
            return row;
        }
    }

    return -1;
}

void OpenRGB3DSpatialTab::SelectAvailableControllerEntry(int type_code, int object_index)
{
    if(!available_controllers_list)
    {
        return;
    }

    int row = FindAvailableControllerRow(type_code, object_index);
    if(row < 0)
    {
        return;
    }

    if(available_controllers_list->currentRow() == row)
    {
        UpdateAvailableItemCombo();
        return;
    }

    QSignalBlocker blocker(available_controllers_list);
    available_controllers_list->setCurrentRow(row);
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::UpdateDeviceList()
{
    LoadDevices();
}

void OpenRGB3DSpatialTab::on_viewport_controller_selected(int transform_index)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        on_controller_selected(-1);
        return;
    }
    int list_row = TransformIndexToControllerListRow(transform_index);
    if(list_row < 0)
        return;
    QSignalBlocker block(controller_list);
    if(controller_list)
        controller_list->setCurrentRow(list_row);
    on_controller_selected(list_row);
}

void OpenRGB3DSpatialTab::on_controller_selected(int index)
{
    if(controller_list && index >= 0 && index < controller_list->count())
    {
        QListWidgetItem* selected_item = controller_list->item(index);
        if(selected_item && selected_item->data(Qt::UserRole).isValid())
        {
            QPair<int, int> metadata = selected_item->data(Qt::UserRole).value<QPair<int, int>>();
            if(metadata.first == -2)
            {
                if(reference_points_list)
                {
                    QSignalBlocker block(reference_points_list);
                    if(metadata.second >= 0 && metadata.second < reference_points_list->count())
                    {
                        reference_points_list->setCurrentRow(metadata.second);
                    }
                }
                on_ref_point_selected(metadata.second);

                if(display_planes_list)
                {
                    QSignalBlocker block(display_planes_list);
                    display_planes_list->clearSelection();
                }
                current_display_plane_index = -1;
                if(viewport) viewport->SelectDisplayPlane(-1);
                return;
            }
            else if(metadata.first == -3)
            {
                int plane_index = FindDisplayPlaneIndexById(metadata.second);
                if(plane_index >= 0 && plane_index < (int)display_planes.size())
                {
                    current_display_plane_index = plane_index;
                    if(display_planes_list)
                    {
                        QSignalBlocker block(display_planes_list);
                        display_planes_list->setCurrentRow(plane_index);
                    }
                    if(reference_points_list)
                    {
                        QSignalBlocker block(reference_points_list);
                        reference_points_list->clearSelection();
                    }
                    DisplayPlane3D* plane = display_planes[plane_index].get();
                    SyncDisplayPlaneControls(plane);
                    RefreshDisplayPlaneDetails();
                    if(viewport) viewport->SelectDisplayPlane(plane_index);
                    if(pos_x_spin) pos_x_spin->setEnabled(true);
                    if(pos_y_spin) pos_y_spin->setEnabled(true);
                    if(pos_z_spin) pos_z_spin->setEnabled(true);
                    if(pos_x_slider) pos_x_slider->setEnabled(true);
                    if(pos_y_slider) pos_y_slider->setEnabled(true);
                    if(pos_z_slider) pos_z_slider->setEnabled(true);
                    if(rot_x_spin) rot_x_spin->setEnabled(true);
                    if(rot_y_spin) rot_y_spin->setEnabled(true);
                    if(rot_z_spin) rot_z_spin->setEnabled(true);
                    if(rot_x_slider) rot_x_slider->setEnabled(true);
                    if(rot_y_slider) rot_y_slider->setEnabled(true);
                    if(rot_z_slider) rot_z_slider->setEnabled(true);
                }
                return;
            }
        }
    }

    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->clearSelection();
    }
    current_display_plane_index = -1;
    if(viewport) viewport->SelectDisplayPlane(-1);

    int transform_index = index;
    if(controller_list && index >= 0 && index < controller_list->count())
    {
        QListWidgetItem* item = controller_list->item(index);
        if(item && !item->data(Qt::UserRole).isValid())
        {
            transform_index = ControllerListRowToTransformIndex(index);
        }
    }

    if(transform_index >= 0 && transform_index < (int)controller_transforms.size())
    {
        int list_row = TransformIndexToControllerListRow(transform_index);
        if(controller_list && list_row >= 0)
        {
            controller_list->setCurrentRow(list_row);
        }

        ControllerTransform* ctrl = controller_transforms[transform_index].get();

        bool has_transform_ui = pos_x_spin && pos_y_spin && pos_z_spin && rot_x_spin && rot_y_spin && rot_z_spin
            && pos_x_slider && pos_y_slider && pos_z_slider && rot_x_slider && rot_y_slider && rot_z_slider;

        if(has_transform_ui)
        {
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

            double scale_mm = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
            if(scale_mm < 0.001) scale_mm = 10.0;
            double pos_x_mm = (double)ctrl->transform.position.x * scale_mm;
            double pos_y_mm = (double)ctrl->transform.position.y * scale_mm;
            double pos_z_mm = (double)ctrl->transform.position.z * scale_mm;
            pos_x_spin->setValue(pos_x_mm);
            pos_y_spin->setValue(pos_y_mm);
            pos_z_spin->setValue(pos_z_mm);
            rot_x_spin->setValue(ctrl->transform.rotation.x);
            rot_y_spin->setValue(ctrl->transform.rotation.y);
            rot_z_spin->setValue(ctrl->transform.rotation.z);

            pos_x_slider->setValue((int)std::lround(pos_x_mm));
            double constrained_y_mm = std::max(pos_y_mm, 0.0);
            pos_y_slider->setValue((int)std::lround(constrained_y_mm));
            pos_z_slider->setValue((int)std::lround(pos_z_mm));
            rot_x_slider->setValue((int)(ctrl->transform.rotation.x));
            rot_y_slider->setValue((int)(ctrl->transform.rotation.y));
            rot_z_slider->setValue((int)(ctrl->transform.rotation.z));

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

            pos_x_spin->setEnabled(true);
            pos_y_spin->setEnabled(true);
            pos_z_spin->setEnabled(true);
            pos_x_slider->setEnabled(true);
            pos_y_slider->setEnabled(true);
            pos_z_slider->setEnabled(true);
            rot_x_spin->setEnabled(true);
            rot_y_spin->setEnabled(true);
            rot_z_spin->setEnabled(true);
            rot_x_slider->setEnabled(true);
            rot_y_slider->setEnabled(true);
            rot_z_slider->setEnabled(true);
        }
        else
        {
            if(pos_x_spin) pos_x_spin->setEnabled(true);
            if(pos_y_spin) pos_y_spin->setEnabled(true);
            if(pos_z_spin) pos_z_spin->setEnabled(true);
            if(pos_x_slider) pos_x_slider->setEnabled(true);
            if(pos_y_slider) pos_y_slider->setEnabled(true);
            if(pos_z_slider) pos_z_slider->setEnabled(true);
            if(rot_x_spin) rot_x_spin->setEnabled(true);
            if(rot_y_spin) rot_y_spin->setEnabled(true);
            if(rot_z_spin) rot_z_spin->setEnabled(true);
            if(rot_x_slider) rot_x_slider->setEnabled(true);
            if(rot_y_slider) rot_y_slider->setEnabled(true);
            if(rot_z_slider) rot_z_slider->setEnabled(true);
        }

        if(reference_points_list)
        {
            reference_points_list->blockSignals(true);
            reference_points_list->clearSelection();
            reference_points_list->blockSignals(false);
        }

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

        if(pos_x_spin) pos_x_spin->setEnabled(true);
        if(pos_y_spin) pos_y_spin->setEnabled(true);
        if(pos_z_spin) pos_z_spin->setEnabled(true);
        if(pos_x_slider) pos_x_slider->setEnabled(true);
        if(pos_y_slider) pos_y_slider->setEnabled(true);
        if(pos_z_slider) pos_z_slider->setEnabled(true);
        if(rot_x_spin) rot_x_spin->setEnabled(true);
        if(rot_y_spin) rot_y_spin->setEnabled(true);
        if(rot_z_spin) rot_z_spin->setEnabled(true);
        if(rot_x_slider) rot_x_slider->setEnabled(true);
        if(rot_y_slider) rot_y_slider->setEnabled(true);
        if(rot_z_slider) rot_z_slider->setEnabled(true);
    }
    else if(transform_index == -1 || index == -1)
    {
        if(controller_list) controller_list->setCurrentRow(-1);

        if(pos_x_spin) pos_x_spin->setEnabled(false);
        if(pos_y_spin) pos_y_spin->setEnabled(false);
        if(pos_z_spin) pos_z_spin->setEnabled(false);
        if(rot_x_spin) rot_x_spin->setEnabled(false);
        if(rot_y_spin) rot_y_spin->setEnabled(false);
        if(rot_z_spin) rot_z_spin->setEnabled(false);
        if(pos_x_slider) pos_x_slider->setEnabled(false);
        if(pos_y_slider) pos_y_slider->setEnabled(false);
        if(pos_z_slider) pos_z_slider->setEnabled(false);
        if(rot_x_slider) rot_x_slider->setEnabled(false);
        if(rot_y_slider) rot_y_slider->setEnabled(false);
        if(rot_z_slider) rot_z_slider->setEnabled(false);

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
        ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
        SetLayoutDirty();

        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);
        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);

        double scale_mm = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
        if(scale_mm < 0.001) scale_mm = 10.0;
        double x_mm = (double)x * scale_mm;
        double y_mm = (double)y * scale_mm;
        double z_mm = (double)z * scale_mm;
        pos_x_spin->setValue(x_mm);
        pos_y_spin->setValue(y_mm);
        pos_z_spin->setValue(z_mm);

        pos_x_slider->setValue((int)std::lround(x_mm));
        double constrained_y_mm = std::max(y_mm, 0.0);
        pos_y_slider->setValue((int)std::lround(constrained_y_mm));
        pos_z_slider->setValue((int)std::lround(z_mm));

        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);
        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);

        if(effect_running)
        {
            RenderEffectStack();
        }
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
        ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
        SetLayoutDirty();

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

        rot_x_spin->blockSignals(false);
        rot_y_spin->blockSignals(false);
        rot_z_spin->blockSignals(false);
        rot_x_slider->blockSignals(false);
        rot_y_slider->blockSignals(false);
        rot_z_slider->blockSignals(false);

        if(effect_running)
        {
            RenderEffectStack();
        }
    }
}


void OpenRGB3DSpatialTab::on_start_effect_clicked()
{
    if(controller_transforms.empty())
    {
        QMessageBox::warning(this, "No Controllers", "Please add controllers to the 3D scene before starting effects.");
        return;
    }

    bool stack_has_entries = false;
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();
        if(instance && !instance->effect_class_name.empty())
        {
            stack_has_entries = true;
            break;
        }
    }

    bool stack_ready = PrepareStackForPlayback();

    if(stack_has_entries)
    {
        if(!stack_ready)
        {
            QMessageBox::warning(this, "No Enabled Effects", "Enable at least one effect in the stack before starting.");
            return;
        }

        bool has_valid_controller = false;
        SetControllersToCustomMode(has_valid_controller);
        if(!has_valid_controller)
        {
            QMessageBox::warning(this, "No Valid Controllers", "No controllers are available for effects.");
            return;
        }

        effect_running = true;
        effect_time = 0.0f;
        effect_elapsed.restart();

        if(effect_timer)
        {
            unsigned int target_fps = 30;
            for(size_t i = 0; i < effect_stack.size(); i++)
            {
                if(effect_stack[i])
                {
                    unsigned int fps = effect_stack[i]->GetEffectiveTargetFPS();
                    if(fps > target_fps)
                    {
                        target_fps = fps;
                    }
                }
            }
            if(target_fps < 1) target_fps = 30;
            int interval_ms = (int)(1000 / target_fps);
            if(interval_ms < 1) interval_ms = 1;
            effect_timer->start(interval_ms);
        }

        if(start_effect_button) start_effect_button->setEnabled(false);
        if(stop_effect_button) stop_effect_button->setEnabled(true);
        return;
    }

    if(!current_effect_ui)
    {
        QMessageBox::warning(this, "No Effects", "Add an effect to the stack before starting.");
        return;
    }

    bool has_valid_controller = false;
    SetControllersToCustomMode(has_valid_controller);
    if(!has_valid_controller)
    {
        QMessageBox::warning(this, "No Valid Controllers", "No controllers are available for effects.");
        return;
    }

    effect_running = true;
    effect_time = 0.0f;
    effect_elapsed.restart();

    if(effect_timer)
    {
        unsigned int target_fps = current_effect_ui->GetTargetFPSSetting();
        if(target_fps < 1) target_fps = 30;
        int interval_ms = (int)(1000 / target_fps);
        if(interval_ms < 1) interval_ms = 1;
        effect_timer->start(interval_ms);
    }

    if(start_effect_button) start_effect_button->setEnabled(false);
    if(stop_effect_button) stop_effect_button->setEnabled(true);
}

void OpenRGB3DSpatialTab::on_stop_effect_clicked()
{
    effect_running = false;
    if(effect_timer && effect_timer->isActive())
    {
        effect_timer->stop();
    }
    if(start_effect_button) start_effect_button->setEnabled(true);
    if(stop_effect_button) stop_effect_button->setEnabled(false);
}

void OpenRGB3DSpatialTab::on_effect_updated()
{
    if(viewport) viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_effect_timer_timeout()
{
    if(!effect_running)
    {
        return;
    }

    qint64 ms = effect_elapsed.isValid() ? effect_elapsed.restart() : 33;
    if(ms <= 0) { ms = 33; }
    float dt = static_cast<float>(ms) / 1000.0f;
    if(dt > 0.1f) dt = 0.1f;
    effect_time += dt;

    RenderEffectStack();
}

void OpenRGB3DSpatialTab::on_granularity_changed(int)
{
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::UpdateAvailableItemCombo()
{
    item_combo->clear();

    int list_row = available_controllers_list->currentRow();
    if(list_row < 0)
    {
        return;
    }

    int actual_ctrl_idx = -1;

    QListWidgetItem* selected_item = available_controllers_list->item(list_row);
    if(selected_item && selected_item->data(Qt::UserRole).isValid())
    {
        QPair<int, int> metadata = selected_item->data(Qt::UserRole).value<QPair<int, int>>();
        int type_code = metadata.first;
        int object_index = metadata.second;

        if(type_code == -2)
        {
            item_combo->addItem("Whole Object", QVariant::fromValue(qMakePair(-2, object_index)));
            return;
        }
        else if(type_code == -3)
        {
            int plane_index = FindDisplayPlaneIndexById(object_index);
            if(plane_index >= 0)
            {
                item_combo->addItem("Whole Object", QVariant::fromValue(qMakePair(-3, display_planes[plane_index]->GetId())));
            }
            return;
        }
        else if(type_code == -1)
        {
            item_combo->addItem("Whole Device", QVariant::fromValue(qMakePair(-1, object_index)));
            return;
        }
        else if(type_code >= 0)
        {
            actual_ctrl_idx = type_code;
        }
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    if(actual_ctrl_idx < 0)
    {
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
    }

    if(actual_ctrl_idx >= 0)
    {
        RGBController* controller = controllers[actual_ctrl_idx];
        int granularity = granularity_combo->currentIndex();

        if(granularity == 0)
        {
            if(!IsItemInScene(controller, granularity, 0))
            {
                item_combo->addItem(QString::fromStdString(controller->GetName()), QVariant::fromValue(qMakePair(actual_ctrl_idx, 0)));
            }
        }
        else if(granularity == 1)
        {
            std::size_t zone_count = controller->GetZoneCount();
            for(std::size_t i = 0; i < zone_count; i++)
            {
                if(!IsItemInScene(controller, granularity, (unsigned int)i))
                {
                    item_combo->addItem(QString::fromStdString(controller->GetZoneName((unsigned int)i)), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        else if(granularity == 2)
        {
            std::size_t led_count = controller->GetLEDCount();
            for(std::size_t i = 0; i < led_count; i++)
            {
                if(!IsItemInScene(controller, granularity, (unsigned int)i))
                {
                    item_combo->addItem(QString::fromStdString(controller->GetLEDName((unsigned int)i)), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        return;
    }
}

bool OpenRGB3DSpatialTab::AddCustomControllerToScene(int virtual_controller_index)
{
    if(virtual_controller_index < 0 || virtual_controller_index >= (int)virtual_controllers.size())
        return false;
    VirtualController3D* virtual_ctrl = virtual_controllers[virtual_controller_index].get();
    if(!virtual_ctrl) return false;

    std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
    ctrl_transform->controller = nullptr;
    ctrl_transform->virtual_controller = virtual_ctrl;
    ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f};
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};
    ctrl_transform->hidden_by_virtual = false;
    ctrl_transform->led_spacing_mm_x = led_spacing_x_spin ? (float)led_spacing_x_spin->value() : 10.0f;
    ctrl_transform->led_spacing_mm_y = led_spacing_y_spin ? (float)led_spacing_y_spin->value() : 0.0f;
    ctrl_transform->led_spacing_mm_z = led_spacing_z_spin ? (float)led_spacing_z_spin->value() : 0.0f;
    ctrl_transform->granularity = -1;
    ctrl_transform->item_idx = -1;
    ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
    ControllerLayout3D::MarkWorldPositionsDirty(ctrl_transform.get());
    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());
    controller_transforms.push_back(std::move(ctrl_transform));
    QString name = QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName());
    QListWidgetItem* list_item = new QListWidgetItem(name);
    controller_list->addItem(list_item);
    controller_list->setCurrentRow(controller_list->count() - 1);
    if(viewport)
    {
        viewport->SelectController((int)controller_transforms.size() - 1);
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
    return true;
}

void OpenRGB3DSpatialTab::AddCustomControllerPreview(CustomControllerDialog* dialog)
{
    if(!dialog) return;

    ClearCustomControllerPreview();

    std::string name_str = dialog->GetControllerName().trimmed().toStdString();
    if(name_str.empty()) name_str = "[Preview]";

    std::vector<GridLEDMapping> mappings = dialog->GetLEDMappings();
    int w = dialog->GetGridWidth();
    int h = dialog->GetGridHeight();
    int d = dialog->GetGridDepth();
    float sx = dialog->GetSpacingX();
    float sy = dialog->GetSpacingY();
    float sz = dialog->GetSpacingZ();

    preview_virtual_controller = std::make_unique<VirtualController3D>(
        name_str, w, h, d, mappings, sx, sy, sz);

    std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
    ctrl_transform->controller = nullptr;
    ctrl_transform->virtual_controller = preview_virtual_controller.get();
    ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f};
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};
    ctrl_transform->hidden_by_virtual = false;
    ctrl_transform->led_spacing_mm_x = sx;
    ctrl_transform->led_spacing_mm_y = sy;
    ctrl_transform->led_spacing_mm_z = sz;
    ctrl_transform->granularity = -1;
    ctrl_transform->item_idx = -1;
    ctrl_transform->led_positions = preview_virtual_controller->GenerateLEDPositions(grid_scale_mm);
    ControllerLayout3D::MarkWorldPositionsDirty(ctrl_transform.get());
    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());
    controller_transforms.push_back(std::move(ctrl_transform));

    QString name = QString("[Custom] ") + QString::fromStdString(name_str);
    QListWidgetItem* list_item = new QListWidgetItem(name);
    controller_list->addItem(list_item);
    controller_list->setCurrentRow(controller_list->count() - 1);
    if(viewport)
    {
        viewport->SelectController((int)controller_transforms.size() - 1);
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::ClearCustomControllerPreview()
{
    if(!preview_virtual_controller) return;

    VirtualController3D* preview_ptr = preview_virtual_controller.get();
    int remove_index = -1;
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        if(controller_transforms[i]->virtual_controller == preview_ptr)
        {
            remove_index = static_cast<int>(i);
            break;
        }
    }
    if(remove_index < 0)
    {
        preview_virtual_controller.reset();
        return;
    }

    int list_row = TransformIndexToControllerListRow(remove_index);
    if(controller_list && list_row >= 0)
    {
        delete controller_list->takeItem(list_row);
    }
    controller_transforms.erase(controller_transforms.begin() + remove_index);
    preview_virtual_controller.reset();

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
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

    if(ctrl_idx == -1)
    {
        if(AddCustomControllerToScene(item_row))
        {
            QMessageBox::information(this, "Custom Controller Added",
                QString("Custom controller '%1' added to 3D view!\n\nYou can now position and configure it.")
                .arg(QString::fromStdString(virtual_controllers[item_row]->GetName())));
        }
        return;
    }

    if(ctrl_idx == -2)
    {
        if(item_row < 0 || item_row >= (int)reference_points.size())
        {
            return;
        }

        VirtualReferencePoint3D* ref_point = reference_points[item_row].get();
        ref_point->SetVisible(true);

        QString name = QString("[Ref Point] ") + QString::fromStdString(ref_point->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(name);
        list_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, item_row)));
        controller_list->addItem(list_item);

        if(viewport) viewport->update();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();

        QMessageBox::information(this, "Reference Point Added",
                                QString("Reference point '%1' added to 3D view!\n\nYou can now position and configure it.")
                                .arg(QString::fromStdString(ref_point->GetName())));
        return;
    }

    if(ctrl_idx == -3)
    {
        int plane_index = FindDisplayPlaneIndexById(item_row);
        if(plane_index < 0 || plane_index >= (int)display_planes.size())
        {
            return;
        }

        DisplayPlane3D* plane = display_planes[plane_index].get();
        plane->SetVisible(true);

        QString plane_name = QString::fromStdString(plane->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(QString("[Display] ") + plane_name);
        list_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-3, plane->GetId())));
        controller_list->addItem(list_item);

        int linked_ref_idx = plane->GetReferencePointIndex();
        if(linked_ref_idx >= 0 && linked_ref_idx < (int)reference_points.size())
        {
            VirtualReferencePoint3D* ref_pt = reference_points[linked_ref_idx].get();
            if(ref_pt && !ref_pt->IsVisible())
            {
                const Transform3D& pt = plane->GetTransform();
                ref_pt->SetPosition({pt.position.x, pt.position.y, pt.position.z});
                ref_pt->SetRotation({pt.rotation.x, pt.rotation.y, pt.rotation.z});
                ref_pt->SetVisible(true);

                QListWidgetItem* ref_item = new QListWidgetItem(
                    QString("[Ref] ") + QString::fromStdString(ref_pt->GetName()));
                ref_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, linked_ref_idx)));
                controller_list->addItem(ref_item);
            }
        }

        if(viewport)
        {
            viewport->SelectDisplayPlane(plane_index);
            viewport->update();
        }
        NotifyDisplayPlaneChanged();
        emit GridLayoutChanged();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();

        QMessageBox::information(this, "Display Plane Added",
                                QString("Display plane '%1' added to 3D view!\n\nIts reference point has been placed at the plane centre and is listed below it.")
                                .arg(plane_name));
        RefreshHiddenControllerStates();
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
    ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f};
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};
    ctrl_transform->hidden_by_virtual = false;

    ctrl_transform->led_spacing_mm_x = led_spacing_x_spin ? (float)led_spacing_x_spin->value() : 10.0f;
    ctrl_transform->led_spacing_mm_y = led_spacing_y_spin ? (float)led_spacing_y_spin->value() : 0.0f;
    ctrl_transform->led_spacing_mm_z = led_spacing_z_spin ? (float)led_spacing_z_spin->value() : 0.0f;

    ctrl_transform->granularity = granularity;
    ctrl_transform->item_idx = item_row;

    QString name;

    if(granularity == 0)
    {
        ctrl_transform->led_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        name = QString("[Device] ") + QString::fromStdString(controller->GetName());
    }
    else if(granularity == 1)
    {
        std::size_t zone_count = controller->GetZoneCount();
        if(item_row >= (int)zone_count)
        {
            return;
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        std::string zone_name = controller->GetZoneName((unsigned int)item_row);

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            if(all_positions[i].zone_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
            }
        }
        

        name = QString("[Zone] ") + QString::fromStdString(controller->GetName()) + " - " + QString::fromStdString(zone_name);
    }
    else if(granularity == 2)
    {
        std::size_t led_count = controller->GetLEDCount();
        if(item_row >= (int)led_count)
        {
            return;
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            unsigned int zone_start = controller->GetZoneStartIndex(all_positions[i].zone_idx);
            unsigned int global_led_idx = zone_start + all_positions[i].led_idx;
            if(global_led_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
                break;
            }
        }

        name = QString("[LED] ") + QString::fromStdString(controller->GetName()) + " - " + QString::fromStdString(controller->GetLEDName((unsigned int)item_row));
    }

    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

    ControllerLayout3D::MarkWorldPositionsDirty(ctrl_transform.get());
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

    controller_transforms.push_back(std::move(ctrl_transform));

    QListWidgetItem* item = new QListWidgetItem(name);
    controller_list->addItem(item);

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    
    SetLayoutDirty();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();

}

void OpenRGB3DSpatialTab::on_remove_controller_clicked()
{
    if(!controller_list) return;
    int selected_row = controller_list->currentRow();
    if(selected_row < 0 || selected_row >= controller_list->count())
    {
        return;
    }

    QListWidgetItem* item = controller_list->item(selected_row);
    if(item && item->data(Qt::UserRole).isValid())
    {
        QPair<int, int> metadata = item->data(Qt::UserRole).value<QPair<int, int>>();
        int type_code = metadata.first;
        int object_index = metadata.second;

        if(type_code == -2)
        {
            if(object_index >= 0 && object_index < (int)reference_points.size())
        {
            reference_points[object_index]->SetVisible(false);
        }
        delete controller_list->takeItem(selected_row);
        if(viewport) viewport->update();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();
        RefreshHiddenControllerStates();
        return;
    }
        else if(type_code == -3)
        {
            int plane_index = FindDisplayPlaneIndexById(object_index);
            if(plane_index >= 0 && plane_index < (int)display_planes.size())
            {
                DisplayPlane3D* plane = display_planes[plane_index].get();
                plane->SetVisible(false);

                int linked_ref_idx = plane->GetReferencePointIndex();
                if(linked_ref_idx >= 0 && linked_ref_idx < (int)reference_points.size())
                {
                    reference_points[linked_ref_idx]->SetVisible(false);
                    for(int r = 0; r < controller_list->count(); r++)
                    {
                        QListWidgetItem* it = controller_list->item(r);
                        if(!it) continue;
                        QVariant v = it->data(Qt::UserRole);
                        if(!v.isValid()) continue;
                        QPair<int,int> meta = v.value<QPair<int,int>>();
                        if(meta.first == -2 && meta.second == linked_ref_idx)
                        {
                            delete controller_list->takeItem(r);
                            break;
                        }
                    }
                }

                if(current_display_plane_index == plane_index)
                {
                    current_display_plane_index = -1;
                    if(display_planes_list)
                    {
                        QSignalBlocker block(display_planes_list);
                        display_planes_list->setCurrentRow(-1);
                    }
                    RefreshDisplayPlaneDetails();
                }
            }
            delete controller_list->takeItem(selected_row);
            if(viewport) viewport->update();
            NotifyDisplayPlaneChanged();
            emit GridLayoutChanged();
            UpdateDisplayPlanesList();
            UpdateAvailableControllersList();
            UpdateAvailableItemCombo();
            RefreshHiddenControllerStates();
            return;
        }
    }

    int transform_index = ControllerListRowToTransformIndex(selected_row);
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return;
    }

    controller_transforms.erase(controller_transforms.begin() + transform_index);

    delete controller_list->takeItem(selected_row);

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::on_remove_controller_from_viewport(int index)
{
    if(index < 0 || index >= (int)controller_transforms.size())
    {
        return;
    }

    int list_row = TransformIndexToControllerListRow(index);
    controller_transforms.erase(controller_transforms.begin() + index);

    if(controller_list && list_row >= 0)
    {
        delete controller_list->takeItem(list_row);
    }

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    
    SetLayoutDirty();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::on_clear_all_clicked()
{
    for(unsigned int i = 0; i < reference_points.size(); i++)
    {
        reference_points[i]->SetVisible(false);
    }
    for(unsigned int i = 0; i < display_planes.size(); i++)
    {
        display_planes[i]->SetVisible(false);
    }

    controller_transforms.clear();
    if(controller_list) controller_list->clear();

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    
    SetLayoutDirty();

    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::on_apply_spacing_clicked()
{
    int selected_row = controller_list ? controller_list->currentRow() : -1;
    int transform_index = (selected_row >= 0) ? ControllerListRowToTransformIndex(selected_row) : -1;
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* ctrl = controller_transforms[transform_index].get();

    ctrl->led_spacing_mm_x = edit_led_spacing_x_spin ? (float)edit_led_spacing_x_spin->value() : 10.0f;
    ctrl->led_spacing_mm_y = edit_led_spacing_y_spin ? (float)edit_led_spacing_y_spin->value() : 0.0f;
    ctrl->led_spacing_mm_z = edit_led_spacing_z_spin ? (float)edit_led_spacing_z_spin->value() : 0.0f;

    RegenerateLEDPositions(ctrl);

    ControllerLayout3D::MarkWorldPositionsDirty(ctrl);

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::on_save_layout_clicked()
{
    if(grid_x_spin) custom_grid_x = grid_x_spin->value();
    if(grid_y_spin) custom_grid_y = grid_y_spin->value();
    if(grid_z_spin) custom_grid_z = grid_z_spin->value();

    if(!layout_profiles_combo) return;
    bool ok;
    QString profile_name = QInputDialog::getText(this, "Save Layout Profile",
                                                 "Profile name:", QLineEdit::Normal,
                                                 layout_profiles_combo->currentText(), &ok);

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

    int index = layout_profiles_combo->findText(profile_name);
    if(index >= 0)
    {
        layout_profiles_combo->setCurrentIndex(index);
    }

    SaveCurrentLayoutName();

    QMessageBox::information(this, "Layout Saved",
                            QString("Profile '%1' saved to plugins directory").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_load_layout_clicked()
{
    if(!PromptSaveIfDirty())
    {
        return;
    }
    
    if(!layout_profiles_combo) return;
    QString profile_name = layout_profiles_combo->currentText();

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

void OpenRGB3DSpatialTab::on_delete_layout_clicked()
{
    if(!layout_profiles_combo) return;
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
        if(layout_path.empty()) return;
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
    connect(&dialog, &CustomControllerDialog::previewRequested, this, [this, &dialog]() { AddCustomControllerPreview(&dialog); });
    connect(&dialog, &QDialog::finished, this, [this]() { ClearCustomControllerPreview(); });

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
        if(controller_list)
        {
            for(int row = 0; row < controller_list->count() && transform_count < (int)controller_transforms.size(); row++)
            {
                QListWidgetItem* list_item = controller_list->item(row);
                if(list_item && !list_item->data(Qt::UserRole).isValid())
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
            delete controller_list->takeItem(row);
        std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
        for(int ti : transform_indices_to_remove)
            controller_transforms.erase(controller_transforms.begin() + ti);
        if(viewport)
        {
            viewport->SetControllerTransforms(&controller_transforms);
            viewport->update();
        }
        virtual_controllers.erase(virtual_controllers.begin() + existing_index);
        if(custom_controllers_list && existing_index < custom_controllers_list->count())
            delete custom_controllers_list->takeItem(existing_index);
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

    SaveCustomControllers();
    UpdateCustomControllersList();
    UpdateAvailableControllersList();
    SelectAvailableControllerEntry(-1, new_index);

    QMessageBox::StandardButton add_now = QMessageBox::question(this, "Custom Controller Created",
        QString("Custom controller '%1' created successfully!\n\nAdd it to the 3D view now?")
            .arg(controller_name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    bool added = (add_now == QMessageBox::Yes && AddCustomControllerToScene(new_index));
    if(added)
        SetObjectCreatorStatus(QString("Custom controller '%1' added to 3D view. Position it in the scene.")
            .arg(controller_name));
    else
        SetObjectCreatorStatus(QString("Custom controller '%1' saved. Add it from Available Controllers when ready.")
            .arg(controller_name));
}

void OpenRGB3DSpatialTab::on_add_from_preset_clicked()
{
    if(!resource_manager)
    {
        return;
    }

    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string preset_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/controller_presets";

    std::error_code ec;
    filesystem::create_directories(preset_dir, ec);

    filesystem::path dir_path(preset_dir);
    if(!filesystem::exists(dir_path))
    {
        QMessageBox::information(this, "Preset Library",
            QString("Preset folder does not exist yet.\n\nAdd JSON files (one per preset) to:\n%1")
                .arg(QString::fromStdString(preset_dir)));
        return;
    }

    for(size_t i = 0; i < kDefaultControllerPresetCount; ++i)
    {
        const DefaultControllerPreset& def = kDefaultControllerPresets[i];
        filesystem::path out_path = dir_path / def.filename;
        if(filesystem::exists(out_path))
        {
            continue;
        }
        std::ofstream out(out_path.string());
        if(out)
        {
            out << def.content;
        }
    }

    static const std::vector<std::pair<std::string, QString>> kPresetCategories = {
        {"graphics_cards", QObject::tr("Graphics Cards")},
        {"motherboards", QObject::tr("Motherboards")},
        {"motherboard", QObject::tr("Motherboards")},
        {"ram_sticks", QObject::tr("RAM Sticks")},
        {"ram", QObject::tr("RAM Sticks")},
        {"mouses", QObject::tr("Mouses")},
        {"keyboards", QObject::tr("Keyboards")},
        {"mousemats", QObject::tr("Mousemats")},
        {"coolers", QObject::tr("Coolers")},
        {"led_strips", QObject::tr("LED Strips")},
        {"headsets", QObject::tr("Headsets & Headset Stands")},
        {"gamepads", QObject::tr("Gamepads")},
        {"accessories", QObject::tr("Accessories")},
        {"microphones", QObject::tr("Microphones")},
        {"speakers", QObject::tr("Speakers")},
        {"storages", QObject::tr("Storages")},
        {"cases", QObject::tr("Cases")},
        {"fans", QObject::tr("Fans")},
        {"cpu_cooler", QObject::tr("CPU cooler")},
        {"waterblock", QObject::tr("Waterblock")},
        {"psu", QObject::tr("PSU")},
        {"other", QObject::tr("Other")},
    };

    auto categoryLabel = [](const std::string& key) -> QString
    {
        for(const auto& p : kPresetCategories)
        {
            if(p.first == key) return p.second;
        }
        return QObject::tr("Other");
    };

    struct PresetEntry
    {
        std::string filepath;
        std::string display_name;
        std::string controller_name;
        std::string category;
        std::string brand;
        std::string model;
    };
    std::vector<PresetEntry> presets;

    try
    {
        for(const auto& entry : filesystem::directory_iterator(dir_path))
        {
            if(entry.path().extension() != ".json")
            {
                continue;
            }
            std::ifstream file(entry.path().string());
            if(!file.is_open())
            {
                continue;
            }
            try
            {
                nlohmann::json j;
                file >> j;
                file.close();
                PresetEntry e;
                e.filepath = entry.path().string();
                e.display_name = j.value("name", entry.path().stem().string());
                e.controller_name.clear();
                e.category = j.value("category", std::string("other"));
                if(e.category.empty()) e.category = "other";
                e.brand = j.value("brand", std::string());
                e.model = j.value("model", std::string());
                if(j.contains("mappings") && j["mappings"].is_array() && !j["mappings"].empty())
                {
                    std::string ctrl = j["mappings"][0].value("controller_name", std::string());
                    if(!ctrl.empty() && ctrl != "Unknown (not found on this system)")
                    {
                        e.controller_name = ctrl;
                    }
                }
                presets.push_back(e);
            }
            catch(const std::exception&)
            {
                file.close();
                PresetEntry e;
                e.filepath = entry.path().string();
                e.display_name = entry.path().stem().string();
                e.controller_name.clear();
                e.category = "other";
                e.brand.clear();
                e.model.clear();
                presets.push_back(e);
            }
        }
    }
    catch(const std::exception& e)
    {
        QMessageBox::critical(this, "Preset Library",
            QString("Failed to read preset folder:\n%1").arg(e.what()));
        return;
    }

    if(presets.empty())
    {
        QMessageBox::information(this, "Preset Library",
            QString("No preset JSON files found.\n\nAdd exported custom controller JSON files to:\n%1\n\nThen use \"Add from preset\" to add them; names use device (e.g. Corsair SP120 RGB PRO or Corsair SP120 RGB PRO 1, 2, 3).")
                .arg(QString::fromStdString(preset_dir)));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Add from preset");
    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* hint = new QLabel("Select a preset. List shows Brand — Model — name. Matching uses all OpenRGB device fields (name, description, location, vendor, serial, version, type) and preset brand/model so presets work for anyone with the same device.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray; font-size: small;");
    layout->addWidget(hint);

    QHBoxLayout* filter_row = new QHBoxLayout();
    QLabel* filter_label = new QLabel(tr("Filter:"));
    filter_label->setStyleSheet("color: gray; font-size: small;");
    filter_row->addWidget(filter_label);
    QComboBox* category_combo = new QComboBox();
    category_combo->addItem(tr("All"), QString());
    for(const auto& p : kPresetCategories)
    {
        category_combo->addItem(p.second, QString::fromStdString(p.first));
    }
    filter_row->addWidget(category_combo);

    QLabel* sort_label = new QLabel(tr("Sort:"));
    sort_label->setStyleSheet("color: gray; font-size: small;");
    filter_row->addWidget(sort_label);
    QComboBox* sort_combo = new QComboBox();
    sort_combo->addItem(tr("Name"), QString("name"));
    sort_combo->addItem(tr("Category"), QString("category"));
    sort_combo->addItem(tr("Brand"), QString("brand"));
    sort_combo->addItem(tr("Model"), QString("model"));
    filter_row->addWidget(sort_combo);
    filter_row->addStretch();
    layout->addLayout(filter_row);

    QListWidget* list = new QListWidget();
    list->setMinimumHeight(200);

    auto buildPresetList = [list, &presets, category_combo, sort_combo, &categoryLabel]()
    {
        QString filter = category_combo->currentData().toString();
        QString sort_key = sort_combo->currentData().toString();
        std::vector<size_t> indices;
        for(size_t i = 0; i < presets.size(); i++)
        {
            if(!filter.isEmpty() && QString::fromStdString(presets[i].category) != filter)
            {
                continue;
            }
            indices.push_back(i);
        }
        std::sort(indices.begin(), indices.end(), [&presets, &sort_key](size_t a, size_t b)
        {
            const PresetEntry& pa = presets[a];
            const PresetEntry& pb = presets[b];
            if(sort_key == QLatin1String("category"))
            {
                return pa.category < pb.category;
            }
            if(sort_key == QLatin1String("brand"))
            {
                std::string ba = pa.brand.empty() ? pa.model : pa.brand;
                std::string bb = pb.brand.empty() ? pb.model : pb.brand;
                return ba < bb;
            }
            if(sort_key == QLatin1String("model"))
            {
                int cm = QString::fromStdString(pa.model).compare(QString::fromStdString(pb.model), Qt::CaseInsensitive);
                if(cm != 0) return cm < 0;
                std::string ba = pa.brand.empty() ? pa.model : pa.brand;
                std::string bb = pb.brand.empty() ? pb.model : pb.brand;
                return ba < bb;
            }
            return pa.display_name < pb.display_name;
        });
        list->clear();
        for(size_t idx : indices)
        {
            const PresetEntry& e = presets[idx];
            QString cat = categoryLabel(e.category);
            QString text;
            if(!e.brand.empty() && !e.model.empty())
            {
                text = QString("[%1] %2 — %3 — %4").arg(cat, QString::fromStdString(e.brand), QString::fromStdString(e.model), QString::fromStdString(e.display_name));
            }
            else if(!e.brand.empty() || !e.model.empty())
            {
                std::string device = e.brand.empty() ? e.model : e.brand + " " + e.model;
                text = QString("[%1] %2 — %3").arg(cat, QString::fromStdString(device), QString::fromStdString(e.display_name));
            }
            else
            {
                text = QString("[%1] ").arg(cat) + QString::fromStdString(e.display_name);
                if(!e.controller_name.empty())
                {
                    text += " — " + QString::fromStdString(e.controller_name);
                }
            }
            QListWidgetItem* item = new QListWidgetItem(text);
            item->setData(Qt::UserRole, QString::fromStdString(e.filepath));
            item->setData(Qt::UserRole + 1, QString::fromStdString(e.category));
            list->addItem(item);
        }
        if(list->count() > 0)
        {
            list->setCurrentRow(0);
        }
    };
    buildPresetList();
    layout->addWidget(list);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    auto updateOkAndList = [buildPresetList, list, buttons]()
    {
        buildPresetList();
        QPushButton* ok_btn = buttons->button(QDialogButtonBox::Ok);
        if(ok_btn) ok_btn->setEnabled(list->count() > 0);
    };
    updateOkAndList();
    connect(category_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [updateOkAndList](int) { updateOkAndList(); });
    connect(sort_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [updateOkAndList](int) { updateOkAndList(); });

    if(dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QListWidgetItem* selected = list->currentItem();
    if(!selected)
    {
        return;
    }

    std::string chosen_path = selected->data(Qt::UserRole).toString().toStdString();
    std::ifstream file(chosen_path);
    if(!file.is_open())
    {
        QMessageBox::critical(this, "Add from preset",
            QString("Could not open file:\n%1").arg(QString::fromStdString(chosen_path)));
        return;
    }

    nlohmann::json j;
    try
    {
        file >> j;
        file.close();
    }
    catch(const std::exception& e)
    {
        file.close();
        QMessageBox::critical(this, "Add from preset",
            QString("Invalid JSON in preset file:\n%1").arg(e.what()));
        return;
    }

    if(!j.contains("name") || !j.contains("mappings") || !j["mappings"].is_array())
    {
        QMessageBox::critical(this, "Add from preset", "Preset JSON must contain \"name\" and \"mappings\" array.");
        return;
    }

    std::string preset_display_name = j["name"].get<std::string>();
    std::string category = j.value("category", std::string("other"));
    if(category.empty()) category = "other";

    std::string brand = j.value("brand", std::string());
    std::string model = j.value("model", std::string());
    std::string brand_model;
    if(!brand.empty() && !model.empty())
    {
        brand_model = brand + " " + model;
    }
    else if(!model.empty())
    {
        brand_model = model;
    }

    std::string controller_name;
    if(!j["mappings"].empty())
    {
        controller_name = j["mappings"][0].value("controller_name", std::string());
        if(controller_name == "Unknown (not found on this system)")
        {
            controller_name.clear();
        }
    }

    std::string cat_prefix;
    if(category != "other")
    {
        cat_prefix = categoryLabel(category).toStdString() + " - ";
    }

    auto truncate = [](const std::string& s, size_t max_len) -> std::string
    {
        if(s.length() <= max_len) return s;
        return s.substr(0, max_len - 3) + "...";
    };

    auto device_label = [&brand_model, &truncate](RGBController* c, size_t max_len) -> std::string
    {
        if(!brand_model.empty())
        {
            return truncate(brand_model, max_len);
        }
        return c ? truncate(c->GetName(), max_len) : std::string();
    };

    std::string preset_location = j["mappings"].empty() ? std::string() : j["mappings"][0].value("controller_location", std::string());
    std::string preset_brand = brand;

    auto controllerSearchText = [](RGBController* c) -> QString
    {
        std::string t;
        std::string name = c->GetName();
        std::string description = c->GetDescription();
        std::string location = c->GetLocation();
        std::string vendor = c->GetVendor();
        std::string serial = c->GetSerial();
        std::string version = c->GetVersion();
        
        if(!name.empty()) t += name + " ";
        if(!description.empty()) t += description + " ";
        if(!location.empty()) t += location + " ";
        if(!vendor.empty()) t += vendor + " ";
        if(!serial.empty()) t += serial + " ";
        if(!version.empty()) t += version + " ";
        t += device_type_to_str(c->GetDeviceType());
        return QString::fromStdString(t).toLower();
    };

    std::vector<RGBController*>& all_controllers = resource_manager->GetRGBControllers();
    std::vector<RGBController*> matching;
    QString preset_ctrl_name = QString::fromStdString(controller_name);
    QString preset_model = QString::fromStdString(model).trimmed();
    QString preset_brand_model = QString::fromStdString(brand_model).trimmed();
    QString preset_brand_q = QString::fromStdString(preset_brand).trimmed();
    QString preset_location_q = QString::fromStdString(preset_location).trimmed();
    for(RGBController* c : all_controllers)
    {
        if(!c) continue;
        QString actual_name = QString::fromStdString(c->GetName());
        if(actual_name.compare(preset_ctrl_name, Qt::CaseInsensitive) == 0)
        {
            matching.push_back(c);
            continue;
        }
        if(controller_name.size() >= 15 && actual_name.size() >= 15 &&
           (actual_name.contains(preset_ctrl_name, Qt::CaseInsensitive) ||
            preset_ctrl_name.contains(actual_name, Qt::CaseInsensitive)))
        {
            matching.push_back(c);
            continue;
        }
        if(!preset_location_q.isEmpty() && QString::fromStdString(c->GetLocation()).trimmed() == preset_location_q)
        {
            matching.push_back(c);
            continue;
        }
        QString searchText = controllerSearchText(c);
        bool tag_match = false;
        if(preset_ctrl_name.size() >= 4 && searchText.contains(preset_ctrl_name.toLower()))
            tag_match = true;
        if(!tag_match && preset_brand_model.size() >= 4 && searchText.contains(preset_brand_model.toLower()))
            tag_match = true;
        if(!tag_match && preset_model.size() >= 4 && searchText.contains(preset_model.toLower()))
            tag_match = true;
        if(!tag_match && preset_brand_q.size() >= 2 && searchText.contains(preset_brand_q.toLower()))
            tag_match = true;
        if(tag_match)
            matching.push_back(c);
    }

    if(matching.empty())
    {
        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(j, all_controllers);
        if(!virtual_ctrl)
        {
            QMessageBox::critical(this, "Add from preset", "Could not create virtual controller from preset (no valid mappings).");
            return;
        }
        std::string device_part = brand_model.empty()
            ? preset_display_name
            : truncate(brand_model, 60);
        std::string name_not_found = brand_model.empty()
            ? (cat_prefix + device_part + " (device not found)")
            : (device_part + " (device not found)");
        virtual_ctrl->SetName(name_not_found);
        virtual_controllers.push_back(std::move(virtual_ctrl));
        SaveCustomControllers();
        UpdateCustomControllersList();
        UpdateAvailableControllersList();
        QMessageBox::information(this, "Add from preset",
            QString("Preset '%1' added.\n\nNo matching device was found on this system; mappings show as \"Unknown device\". You can reassign cells or leave as placeholder.")
                .arg(QString::fromStdString(preset_display_name)));
        return;
    }

    if(matching.size() == 1)
    {
        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(j, all_controllers);
        if(!virtual_ctrl)
        {
            QMessageBox::critical(this, "Add from preset", "Could not create virtual controller from preset.");
            return;
        }
        std::string new_name = brand_model.empty()
            ? (cat_prefix + device_label(matching[0], 80) + " - " + preset_display_name)
            : device_label(matching[0], 80);
        virtual_ctrl->SetName(truncate(new_name, 120));
        virtual_controllers.push_back(std::move(virtual_ctrl));
        SaveCustomControllers();
        UpdateCustomControllersList();
        UpdateAvailableControllersList();
        QMessageBox::information(this, "Add from preset",
            QString("Preset '%1' added as '%2'.")
                .arg(QString::fromStdString(preset_display_name), QString::fromStdString(truncate(new_name, 120))));
        return;
    }

    for(size_t i = 0; i < matching.size(); i++)
    {
        std::string display_name = device_label(matching[i], 50) + " " + std::to_string(i + 1);
        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJsonForController(j, matching[i], display_name);
        if(virtual_ctrl)
        {
            virtual_controllers.push_back(std::move(virtual_ctrl));
        }
    }
    SaveCustomControllers();
    UpdateCustomControllersList();
    UpdateAvailableControllersList();
    QMessageBox::information(this, "Add from preset",
        QString("Added %1 custom controller(s) from preset '%2' (one per matching device: %3).")
            .arg(matching.size())
            .arg(QString::fromStdString(preset_display_name))
            .arg(QString::fromStdString(controller_name)));
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
            QMessageBox::information(this, "Export Successful",
                                    QString("Custom controller '%1' exported successfully to:\n%2")
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
    
    if(file.bad() || file.fail())
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("File is not readable:\n%1").arg(filename));
        file.close();
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
                        return;
                    }
                    else
                    {
                        for(unsigned int j = 0; j < virtual_controllers.size(); j++)
                        {
                            if(virtual_controllers[j]->GetName() == ctrl_name)
                            {
                                VirtualController3D* to_remove = virtual_controllers[j].get();
                                std::vector<int> transform_indices_to_remove;
                                for(size_t ti = 0; ti < controller_transforms.size(); ti++)
                                {
                                    if(controller_transforms[ti] && controller_transforms[ti]->virtual_controller == to_remove)
                                    {
                                        transform_indices_to_remove.push_back(static_cast<int>(ti));
                                    }
                                }
                                std::vector<int> row_for_transform(controller_transforms.size(), -1);
                                int transform_count = 0;
                                if(controller_list)
                                {
                                    for(int row = 0; row < controller_list->count() && transform_count < (int)controller_transforms.size(); row++)
                                    {
                                        QListWidgetItem* list_item = controller_list->item(row);
                                        if(list_item && !list_item->data(Qt::UserRole).isValid())
                                        {
                                            row_for_transform[transform_count++] = row;
                                        }
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
                                    delete controller_list->takeItem(row);
                                }
                                std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
                                for(int ti : transform_indices_to_remove)
                                {
                                    controller_transforms.erase(controller_transforms.begin() + ti);
                                }
                                virtual_controllers.erase(virtual_controllers.begin() + j);
                                if(viewport)
                                {
                                    viewport->SetControllerTransforms(&controller_transforms);
                                    viewport->update();
                                }
                                UpdateAvailableControllersList();
                                UpdateAvailableItemCombo();
                                RefreshHiddenControllerStates();
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            VirtualController3D* added = virtual_ctrl.get();
            virtual_controllers.push_back(std::move(virtual_ctrl));
            SaveCustomControllers();
            UpdateAvailableControllersList();

            int unmatched = 0;
            if(added)
            {
                const std::vector<GridLEDMapping>& maps = added->GetMappings();
                for(size_t i = 0; i < maps.size(); i++)
                {
                    if(!maps[i].controller)
                    {
                        unmatched++;
                    }
                }
            }
            if(unmatched > 0)
            {
                QMessageBox::information(this, "Import Complete",
                    QString("Custom controller imported.\n\n%1 mapping(s) could not be matched to current devices and will appear as \"Unknown device\" in the grid. You can reassign those cells or leave them as placeholders.")
                    .arg(unmatched));
            }


            QMessageBox::information(this, "Import Successful",
                                    QString("Custom controller '%1' imported successfully!\n\n"
                                           "Grid: %2x%3x%4\n"
                                           "LEDs: %5\n\n"
                                           "You can now add it to the 3D view.")
                                    .arg(added ? QString::fromStdString(added->GetName()) : QString())
                                    .arg(added ? added->GetWidth() : 0)
                                    .arg(added ? added->GetHeight() : 0)
                                    .arg(added ? added->GetDepth() : 0)
                                    .arg(added ? (int)added->GetMappings().size() : 0));
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
                                  virtual_ctrl->GetMappings(),
                                  virtual_ctrl->GetSpacingX(),
                                  virtual_ctrl->GetSpacingY(),
                                  virtual_ctrl->GetSpacingZ());

    connect(&dialog, &CustomControllerDialog::previewRequested, this, [this, &dialog]() { AddCustomControllerPreview(&dialog); });
    connect(&dialog, &QDialog::finished, this, [this]() { ClearCustomControllerPreview(); });

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
            if(controller_list)
            {
                for(int row = 0; row < controller_list->count() && transform_count < (int)controller_transforms.size(); row++)
                {
                    QListWidgetItem* list_item = controller_list->item(row);
                    if(list_item && !list_item->data(Qt::UserRole).isValid())
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
                delete controller_list->takeItem(row);
            std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
            for(int ti : transform_indices_to_remove)
                controller_transforms.erase(controller_transforms.begin() + ti);
            if(viewport)
            {
                viewport->SetControllerTransforms(&controller_transforms);
                viewport->update();
            }
            virtual_controllers.erase(virtual_controllers.begin() + other_index);
            if(custom_controllers_list && other_index < custom_controllers_list->count())
                delete custom_controllers_list->takeItem(other_index);
            if(other_index < list_row)
                list_row--;
            UpdateAvailableControllersList();
            UpdateAvailableItemCombo();
            RefreshHiddenControllerStates();
            virtual_ctrl = virtual_controllers[list_row].get();
        }

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

            std::string previous_filepath = custom_dir + "/" + safe_old_name + ".json";
            if(filesystem::exists(previous_filepath))
            {
                filesystem::remove(previous_filepath);
            }
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
                t->led_positions = new_ptr->GenerateLEDPositions(grid_scale_mm);
                ControllerLayout3D::MarkWorldPositionsDirty(t);

                int list_row = TransformIndexToControllerListRow(static_cast<int>(i));
                if(controller_list && list_row >= 0)
                {
                    controller_list->item(list_row)->setText(QString("[Custom] ") + QString::fromStdString(new_ptr->GetName()));
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

        QMessageBox::information(this, "Custom Controller Updated",
                                QString("Custom controller '%1' updated successfully!")
                                .arg(QString::fromStdString(virtual_controllers[list_row]->GetName())));
    }
}

void OpenRGB3DSpatialTab::on_delete_custom_controller_clicked()
{
    if(!custom_controllers_list) return;
    int list_row = custom_controllers_list->currentRow();
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
        if(controller_list)
        {
            for(int row = 0; row < controller_list->count() && transform_count < (int)controller_transforms.size(); row++)
            {
                QListWidgetItem* list_item = controller_list->item(row);
                if(list_item && !list_item->data(Qt::UserRole).isValid())
                {
                    row_for_transform[transform_count++] = row;
                }
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
            delete controller_list->takeItem(row);
        }
        std::sort(transform_indices_to_remove.begin(), transform_indices_to_remove.end(), std::greater<int>());
        for(int ti : transform_indices_to_remove)
        {
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

    virtual_controllers.erase(virtual_controllers.begin() + list_row);
    if(custom_controllers_list)
    {
        delete custom_controllers_list->takeItem(list_row);
    }
    SaveCustomControllers();
    UpdateCustomControllersList();
    UpdateAvailableControllersList();
    SetObjectCreatorStatus(QString("Custom controller '%1' deleted.").arg(QString::fromStdString(name)));
    QMessageBox::information(this, "Deleted", QString("Custom controller '%1' removed from library.").arg(QString::fromStdString(name)));
}

nlohmann::json OpenRGB3DSpatialTab::ExportLayoutToJSON() const
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
            int list_row = TransformIndexToControllerListRow(static_cast<int>(i));
            QListWidgetItem* item = (controller_list && list_row >= 0) ? controller_list->item(list_row) : nullptr;
            QString display_name = item ? item->text() : "Unknown Custom Controller";

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

    return layout_json;
}

void OpenRGB3DSpatialTab::SaveLayout(const std::string& filename)
{
    nlohmann::json layout_json = ExportLayoutToJSON();

    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to save layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename), file.errorString());
        QMessageBox::critical(nullptr, "Save Failed", error_msg);
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
        QMessageBox::critical(nullptr, "Write Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write file: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }
}

void OpenRGB3DSpatialTab::LoadLayoutFromJSON(const nlohmann::json& layout_json)
{

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

        if(layout_json["grid"].contains("scale_mm"))
        {
            grid_scale_mm = layout_json["grid"]["scale_mm"].get<float>();
            if(grid_scale_mm < 0.001f)
                grid_scale_mm = 10.0f;
            if(grid_scale_spin)
            {
                grid_scale_spin->blockSignals(true);
                grid_scale_spin->setValue(grid_scale_mm);
                grid_scale_spin->blockSignals(false);
            }
        }
    }

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

        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    }

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

    on_clear_all_clicked();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

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

            if(controller_json.contains("granularity"))
            {
                ctrl_transform->granularity = controller_json["granularity"].get<int>();
                ctrl_transform->item_idx = controller_json["item_idx"].get<int>();
            }
            else
            {
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
                            std::size_t zone_count = controller->GetZoneCount();
                            if(zone_idx < zone_count)
                            {
                                ctrl_transform->item_idx = controller->GetZoneStartIndex(zone_idx) + led_idx;
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
                std::size_t zone_count = controller->GetZoneCount();
                std::size_t led_count = controller->GetLEDCount();
                if(granularity == 0)
                {
                    name = QString("[Device] ") + QString::fromStdString(controller->GetName());
                }
                else if(granularity == 1)
                {
                    name = QString("[Zone] ") + QString::fromStdString(controller->GetName());
                    if(item_idx >= 0 && item_idx < (int)zone_count)
                    {
                        name += " - " + QString::fromStdString(controller->GetZoneName((unsigned int)item_idx));
                    }
                }
                else if(granularity == 2)
                {
                    name = QString("[LED] ") + QString::fromStdString(controller->GetName());
                    if(item_idx >= 0 && item_idx < (int)led_count)
                    {
                        name += " - " + QString::fromStdString(controller->GetLEDName((unsigned int)item_idx));
                    }
                }
                else
                {
                    name = QString::fromStdString(controller->GetName());
                    if(led_positions_size < led_count)
                    {
                        if(led_positions_size == 1)
                        {
                            unsigned int zone_start = controller->GetZoneStartIndex(first_zone_idx);
                            unsigned int led_global_idx = zone_start + first_led_idx;
                            name = QString("[LED] ") + name + " - " + QString::fromStdString(controller->GetLEDName(led_global_idx));
                        }
                        else
                        {
                            name = QString("[Zone] ") + name + " - " + QString::fromStdString(controller->GetZoneName(first_zone_idx));
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

    display_planes.clear();
    current_display_plane_index = -1;
    if(layout_json.contains("display_planes"))
    {
        const nlohmann::json& planes_array = layout_json["display_planes"];
        int ref_count = (int)reference_points.size();
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
            if(!display_planes[i]) continue;
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

        QListWidgetItem* plane_item = new QListWidgetItem(
            QString("[Display] ") + QString::fromStdString(plane->GetName()));
        plane_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-3, plane->GetId())));
        controller_list->addItem(plane_item);

        int ref_idx = plane->GetReferencePointIndex();
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
        {
            VirtualReferencePoint3D* ref_pt = reference_points[ref_idx].get();
            if(ref_pt && ref_pt->IsVisible())
            {
                QListWidgetItem* ref_item = new QListWidgetItem(
                    QString("[Ref] ") + QString::fromStdString(ref_pt->GetName()));
                ref_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, ref_idx)));
                controller_list->addItem(ref_item);
                ref_added[ref_idx] = true;
            }
        }
    }

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        if(ref_added[i]) continue;
        VirtualReferencePoint3D* ref_pt = reference_points[i].get();
        if(!ref_pt || !ref_pt->IsVisible()) continue;

        QListWidgetItem* ref_item = new QListWidgetItem(
            QString("[Ref Point] ") + QString::fromStdString(ref_pt->GetName()));
        ref_item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, static_cast<int>(i))));
        controller_list->addItem(ref_item);
    }

    std::vector<DisplayPlane3D*> plane_ptrs;
    for(size_t plane_index = 0; plane_index < display_planes.size(); plane_index++)
    {
        std::unique_ptr<DisplayPlane3D>& plane = display_planes[plane_index];
        if(plane)
        {
            plane_ptrs.push_back(plane.get());
        }
    }
    DisplayPlaneManager::instance()->SetDisplayPlanes(plane_ptrs);
    NotifyDisplayPlaneChanged();

    emit GridLayoutChanged();

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
            
            zone_manager->ClearAllZones();
            UpdateZonesList();
        }
    }

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->SetReferencePoints(&reference_points);
        viewport->update();
    }
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
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
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to parse JSON: %s", e.what());
        QMessageBox::critical(this, "Invalid Layout File",
                            QString("Failed to parse layout file:\n%1\n\nThe file may be corrupted or in an invalid format.\n\nError: %2")
                            .arg(QString::fromStdString(filename), e.what()));
        return;
    }
}

std::string OpenRGB3DSpatialTab::GetLayoutPath(const std::string& layout_name)
{
    if(!resource_manager) return std::string();
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
    if(!resource_manager || !layout_profiles_combo) return;
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

    if(!auto_load_checkbox || !layout_profiles_combo)
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


    auto_load_checkbox->blockSignals(true);
    auto_load_checkbox->setChecked(auto_load_enabled);
    auto_load_checkbox->blockSignals(false);

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

void OpenRGB3DSpatialTab::SaveCustomControllers()
{
    if(!resource_manager) return;
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
            try
            {
                nlohmann::json ctrl_json = virtual_controllers[i]->ToJson();
                file << ctrl_json.dump(4);
                file.close();

                if(file.fail() || file.bad())
                {
                    LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write custom controller: %s", filepath.c_str());
                }
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Exception while saving custom controller: %s - %s", filepath.c_str(), e.what());
                file.close();
            }
        }
        else
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open custom controller file for writing: %s", filepath.c_str());
        }
    }
}

void OpenRGB3DSpatialTab::LoadCustomControllers()
{
    if(!resource_manager) return;
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

        if(ct->granularity == granularity && ct->item_idx == item_idx)
        {
            return true;
        }

        if(granularity == 0)
        {
            if(ct->granularity == 0)
            {
                return true;
            }
        }
        else if(granularity == 1)
        {
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
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                unsigned int zone_start = controller->GetZoneStartIndex(ct->led_positions[j].zone_idx);
                unsigned int global_led_idx = zone_start + ct->led_positions[j].led_idx;
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
    std::size_t zone_count = controller->GetZoneCount();
    int unassigned_count = 0;
    for(std::size_t i = 0; i < zone_count; i++)
    {
        if(!IsItemInScene(controller, 1, (int)i))
        {
            unassigned_count++;
        }
    }
    return unassigned_count;
}

int OpenRGB3DSpatialTab::GetUnassignedLEDCount(RGBController* controller)
{
    int total_leds = (int)controller->GetLEDCount();
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
                unsigned int zone_start = transform->controller->GetZoneStartIndex(all_positions[i].zone_idx);
                unsigned int global_led_idx = zone_start + all_positions[i].led_idx;
                if(global_led_idx == (unsigned int)transform->item_idx)
                {
                    transform->led_positions.push_back(all_positions[i]);
                    break;
                }
            }
        }
    }
}


bool OpenRGB3DSpatialTab::ParseMonitorPresetEntry(const nlohmann::json& entry, const QString& file_id, MonitorPreset& out_preset)
{
    out_preset.brand = QString::fromStdString(entry.value("brand", std::string()));
    out_preset.model = QString::fromStdString(entry.value("model", std::string()));
    out_preset.width_mm = entry.value("width_mm", 0.0);
    out_preset.height_mm = entry.value("height_mm", 0.0);
    QString preset_id = QString::fromStdString(entry.value("id", std::string()));
    if(preset_id.isEmpty())
    {
        preset_id = file_id;
    }
    if(preset_id.isEmpty())
    {
        QString combined = (out_preset.brand + " " + out_preset.model).trimmed();
        QString sanitized;
        sanitized.reserve(combined.length());
        bool last_was_underscore = false;
        for(int i = 0; i < combined.length(); i++)
        {
            const QChar ch = combined[i];
            if(ch.isLetterOrNumber())
            {
                sanitized.append(ch.toLower());
                last_was_underscore = false;
            }
            else if(!last_was_underscore)
            {
                sanitized.append('_');
                last_was_underscore = true;
            }
        }
        while(sanitized.endsWith('_'))
        {
            sanitized.chop(1);
        }
        preset_id = sanitized;
    }
    if(preset_id.isEmpty() || (out_preset.brand.isEmpty() && out_preset.model.isEmpty()) ||
       out_preset.width_mm <= 0.0 || out_preset.height_mm <= 0.0)
    {
        return false;
    }
    out_preset.id = preset_id;
    return true;
}

void OpenRGB3DSpatialTab::LoadMonitorPresets()
{
    monitor_presets.clear();
    std::string monitors_dir;
    if(resource_manager)
    {
        std::string config_dir = resource_manager->GetConfigurationDirectory().string();
        if(!config_dir.empty())
        {
            monitors_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/monitors";
        }
    }

    if(monitors_dir.empty())
    {
        PopulateMonitorPresetCombo();
        return;
    }

    filesystem::path monitors_path(monitors_dir);
    std::error_code ec;
    filesystem::create_directories(monitors_path, ec);

    try
    {
        nlohmann::json default_array = nlohmann::json::parse(kDefaultMonitorPresetJson);
        if(default_array.is_array())
        {
            for(size_t i = 0; i < default_array.size(); i++)
            {
                const nlohmann::json& entry = default_array[i];
                std::string id = entry.value("id", std::string());
                if(id.empty())
                {
                    continue;
                }
                filesystem::path out_path = monitors_path / (id + ".json");
                if(filesystem::exists(out_path))
                {
                    continue;
                }
                std::ofstream out(out_path.string());
                if(out)
                {
                    out << entry.dump(1);
                }
            }
        }
        std::string legacy_path = monitors_path.parent_path().string() + "/monitors.json";
        filesystem::path legacy_file(legacy_path);
        if(filesystem::exists(legacy_file))
        {
            std::ifstream in(legacy_path);
            if(in)
            {
                nlohmann::json legacy_array = nlohmann::json::parse(in);
                if(legacy_array.is_array())
                {
                    for(size_t i = 0; i < legacy_array.size(); i++)
                    {
                        const nlohmann::json& entry = legacy_array[i];
                        std::string id = entry.value("id", std::string());
                        if(id.empty())
                        {
                            continue;
                        }
                        filesystem::path out_path = monitors_path / (id + ".json");
                        if(filesystem::exists(out_path))
                        {
                            continue;
                        }
                        std::ofstream out(out_path.string());
                        if(out)
                        {
                            out << entry.dump(1);
                        }
                    }
                }
            }
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to seed monitor presets: %s", e.what());
    }

    std::set<QString> seen_ids;
    for(filesystem::directory_iterator it(monitors_path, ec), end; it != end && !ec; it.increment(ec))
    {
        if(it->path().extension() != ".json")
        {
            continue;
        }
        QString file_id = QString::fromStdString(it->path().stem().string());
        std::ifstream in(it->path().string());
        if(!in)
        {
            continue;
        }
        try
        {
            nlohmann::json j;
            in >> j;
            if(!j.is_object())
            {
                continue;
            }
            MonitorPreset preset;
            if(!ParseMonitorPresetEntry(j, file_id, preset))
            {
                continue;
            }
            if(seen_ids.find(preset.id) != seen_ids.end())
            {
                continue;
            }
            seen_ids.insert(preset.id);
            monitor_presets.push_back(preset);
        }
        catch(const std::exception& e)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to parse monitor preset %s: %s",
                       it->path().string().c_str(), e.what());
        }
    }

    PopulateMonitorPresetCombo();
}

void OpenRGB3DSpatialTab::on_monitor_filter_or_sort_changed(int)
{
    PopulateMonitorPresetCombo();
}

void OpenRGB3DSpatialTab::PopulateMonitorPresetCombo()
{
    if(!display_plane_monitor_combo)
    {
        return;
    }

    QString current_id;
    if(display_plane_monitor_combo->currentIndex() >= 0)
    {
        current_id = display_plane_monitor_combo->currentData().toString();
    }

    QString filter_brand;
    if(display_plane_monitor_brand_filter && display_plane_monitor_brand_filter->currentIndex() >= 0)
    {
        filter_brand = display_plane_monitor_brand_filter->currentData().toString();
    }

    if(display_plane_monitor_brand_filter)
    {
        std::set<QString> brands;
        for(const MonitorPreset& p : monitor_presets)
        {
            if(!p.brand.isEmpty()) brands.insert(p.brand);
        }
        display_plane_monitor_brand_filter->blockSignals(true);
        display_plane_monitor_brand_filter->clear();
        display_plane_monitor_brand_filter->addItem(tr("All brands"), QString());
        for(const QString& b : brands)
        {
            display_plane_monitor_brand_filter->addItem(b, b);
        }
        int restore = 0;
        for(int i = 0; i < display_plane_monitor_brand_filter->count(); i++)
        {
            if(display_plane_monitor_brand_filter->itemData(i).toString() == filter_brand)
            {
                restore = i;
                break;
            }
        }
        display_plane_monitor_brand_filter->setCurrentIndex(restore);
        display_plane_monitor_brand_filter->blockSignals(false);
        filter_brand = display_plane_monitor_brand_filter->currentData().toString();
    }

    std::vector<size_t> indices;
    for(size_t i = 0; i < monitor_presets.size(); i++)
    {
        if(!filter_brand.isEmpty() && monitor_presets[i].brand != filter_brand)
        {
            continue;
        }
        indices.push_back(i);
    }

    QString sort_key = QString("brand");
    if(display_plane_monitor_sort_combo && display_plane_monitor_sort_combo->currentIndex() >= 0)
    {
        sort_key = display_plane_monitor_sort_combo->currentData().toString();
    }
    std::sort(indices.begin(), indices.end(), [this, &sort_key](size_t a, size_t b)
    {
        const MonitorPreset& pa = monitor_presets[a];
        const MonitorPreset& pb = monitor_presets[b];
        if(sort_key == QLatin1String("model"))
        {
            return QString::compare(pa.model, pb.model, Qt::CaseInsensitive) < 0;
        }
        if(sort_key == QLatin1String("width"))
        {
            return pa.width_mm < pb.width_mm;
        }
        return QString::compare(pa.brand, pb.brand, Qt::CaseInsensitive) < 0;
    });

    display_plane_monitor_combo->blockSignals(true);
    display_plane_monitor_combo->clear();
    display_plane_monitor_combo->setCurrentIndex(-1);
    display_plane_monitor_combo->setEditText(QString());

    for(size_t idx : indices)
    {
        const MonitorPreset& preset = monitor_presets[idx];
        display_plane_monitor_combo->addItem(preset.DisplayLabel(), preset.id);
    }

    display_plane_monitor_combo->blockSignals(false);

    if(!monitor_preset_completer)
    {
        monitor_preset_completer = new QCompleter(display_plane_monitor_combo->model(), display_plane_monitor_combo);
        monitor_preset_completer->setCaseSensitivity(Qt::CaseInsensitive);
        monitor_preset_completer->setFilterMode(Qt::MatchContains);
        display_plane_monitor_combo->setCompleter(monitor_preset_completer);
    }
    else
    {
        monitor_preset_completer->setModel(display_plane_monitor_combo->model());
    }

    if(!current_id.isEmpty())
    {
        for(int i = 0; i < display_plane_monitor_combo->count(); i++)
        {
            if(display_plane_monitor_combo->itemData(i).toString() == current_id)
            {
                display_plane_monitor_combo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void OpenRGB3DSpatialTab::ClearMonitorPresetSelectionIfManualEdit()
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane || !display_plane_monitor_combo)
    {
        return;
    }

    QString current_id = QString::fromStdString(plane->GetMonitorPresetId());
    if(current_id.isEmpty())
    {
        return;
    }

    std::vector<MonitorPreset>::iterator it = std::find_if(
        monitor_presets.begin(),
        monitor_presets.end(),
        [&current_id](const MonitorPreset& preset) {
            return preset.id == current_id;
        });
    if(it == monitor_presets.end())
    {
        plane->SetMonitorPresetId(std::string());
        SetObjectCreatorStatus(QString());
        return;
    }

    double width_diff = std::abs(plane->GetWidthMM() - it->width_mm);
    double height_diff = std::abs(plane->GetHeightMM() - it->height_mm);
    if(width_diff > 0.5 || height_diff > 0.5)
    {
        plane->SetMonitorPresetId(std::string());
        QSignalBlocker block(display_plane_monitor_combo);
        display_plane_monitor_combo->setCurrentIndex(-1);
        display_plane_monitor_combo->setEditText(QString());
        SetObjectCreatorStatus("Custom display dimensions set. Monitor preset cleared.");
    }
}

DisplayPlane3D* OpenRGB3DSpatialTab::GetSelectedDisplayPlane()
{
    if(current_display_plane_index >= 0 &&
       current_display_plane_index < (int)display_planes.size())
    {
        return display_planes[current_display_plane_index].get();
    }
    return nullptr;
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
        int list_row = TransformIndexToControllerListRow(static_cast<int>(i));
        if(controller_list && list_row >= 0)
        {
            QListWidgetItem* item = controller_list->item(list_row);
            if(item)
            {
                item->setHidden(false);
            }
        }
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
                int list_row = TransformIndexToControllerListRow(static_cast<int>(j));
                if(controller_list && list_row >= 0)
                {
                    QListWidgetItem* item = controller_list->item(list_row);
                    if(item)
                    {
                        item->setHidden(true);
                    }
                }
            }
        }
    }
}

int OpenRGB3DSpatialTab::FindDisplayPlaneIndexById(int plane_id) const
{
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        if(display_planes[i] && display_planes[i]->GetId() == plane_id)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void OpenRGB3DSpatialTab::RemoveDisplayPlaneControllerEntries(int plane_id)
{
    if(!controller_list)
    {
        return;
    }

    for(int row = controller_list->count() - 1; row >= 0; row--)
    {
        QListWidgetItem* item = controller_list->item(row);
        if(!item)
        {
            continue;
        }
        QVariant data = item->data(Qt::UserRole);
        if(!data.isValid())
        {
            continue;
        }
        QPair<int, int> metadata = data.value<QPair<int, int>>();
        if(metadata.first == -3 && metadata.second == plane_id)
        {
            delete controller_list->takeItem(row);
        }
    }
}

void OpenRGB3DSpatialTab::RemoveReferencePointControllerEntries(int removed_index)
{
    if(!controller_list) return;
    for(int row = controller_list->count() - 1; row >= 0; row--)
    {
        QListWidgetItem* item = controller_list->item(row);
        if(!item) continue;
        QVariant data = item->data(Qt::UserRole);
        if(!data.isValid()) continue;
        QPair<int, int> metadata = data.value<QPair<int, int>>();
        if(metadata.first != -2) continue;
        int ref_idx = metadata.second;
        if(ref_idx == removed_index)
        {
            delete controller_list->takeItem(row);
        }
        else if(ref_idx > removed_index)
        {
            item->setData(Qt::UserRole, QVariant::fromValue(qMakePair(-2, ref_idx - 1)));
        }
    }
}

int OpenRGB3DSpatialTab::ControllerListRowToTransformIndex(int row) const
{
    if(!controller_list || row < 0 || row >= controller_list->count()) return -1;
    QListWidgetItem* item = controller_list->item(row);
    if(!item || item->data(Qt::UserRole).isValid()) return -1;
    int transform_count = 0;
    for(int r = 0; r <= row; r++)
    {
        QListWidgetItem* it = controller_list->item(r);
        if(it && !it->data(Qt::UserRole).isValid())
        {
            if(r == row) return transform_count;
            transform_count++;
        }
    }
    return -1;
}

int OpenRGB3DSpatialTab::TransformIndexToControllerListRow(int transform_index) const
{
    if(!controller_list || transform_index < 0 || transform_index >= (int)controller_transforms.size()) return -1;
    int transform_count = 0;
    for(int row = 0; row < controller_list->count(); row++)
    {
        QListWidgetItem* item = controller_list->item(row);
        if(!item) continue;
        if(!item->data(Qt::UserRole).isValid())
        {
            if(transform_count == transform_index) return row;
            transform_count++;
        }
    }
    return -1;
}

void OpenRGB3DSpatialTab::SyncDisplayPlaneControls(DisplayPlane3D* plane)
{
    if(!plane)
    {
        return;
    }

    const Transform3D& transform = plane->GetTransform();
    double scale_mm = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
    if(scale_mm < 0.001) scale_mm = 10.0;
    double pos_x_mm = (double)transform.position.x * scale_mm;
    double pos_y_mm = (double)transform.position.y * scale_mm;
    double pos_z_mm = (double)transform.position.z * scale_mm;

    if(pos_x_spin) { QSignalBlocker block(pos_x_spin); pos_x_spin->setValue(pos_x_mm); }
    if(pos_x_slider) { QSignalBlocker block(pos_x_slider); pos_x_slider->setValue((int)std::lround(pos_x_mm)); }

    if(pos_y_spin) { QSignalBlocker block(pos_y_spin); pos_y_spin->setValue(pos_y_mm); }
    if(pos_y_slider) { QSignalBlocker block(pos_y_slider); pos_y_slider->setValue((int)std::lround(pos_y_mm)); }

    if(pos_z_spin) { QSignalBlocker block(pos_z_spin); pos_z_spin->setValue(pos_z_mm); }
    if(pos_z_slider) { QSignalBlocker block(pos_z_slider); pos_z_slider->setValue((int)std::lround(pos_z_mm)); }

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
    if(display_plane_capture_combo)
    {
        QSignalBlocker block(display_plane_capture_combo);
        std::string current_source = plane->GetCaptureSourceId();

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
            display_plane_capture_combo->addItem(QString::fromStdString(current_source) + " (custom)",
                                                  QString::fromStdString(current_source));
            display_plane_capture_combo->setCurrentIndex(display_plane_capture_combo->count() - 1);
        }
        else
        {
            display_plane_capture_combo->setCurrentIndex(0);
        }
    }
    if(display_plane_visible_check)
    {
        QSignalBlocker block(display_plane_visible_check);
        display_plane_visible_check->setCheckState(plane->IsVisible() ? Qt::Checked : Qt::Unchecked);
    }
}

void OpenRGB3DSpatialTab::UpdateCurrentDisplayPlaneListItemLabel()
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane || !display_planes_list || current_display_plane_index < 0 ||
       current_display_plane_index >= display_planes_list->count())
    {
        return;
    }
    QListWidgetItem* item = display_planes_list->item(current_display_plane_index);
    if(!item) return;
    QString label = QString::fromStdString(plane->GetName()) +
        QString(" (%1 x %2 mm)")
            .arg(plane->GetWidthMM(), 0, 'f', 0)
            .arg(plane->GetHeightMM(), 0, 'f', 0);
    item->setText(label);
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
        item->setData(Qt::UserRole, plane->GetId());
        if(!plane->IsVisible())
        {
            item->setForeground(QColor(0x888888));
        }
    }
    
    if(desired_index >= 0 && desired_index < (int)display_planes.size())
    {
        display_planes_list->setCurrentRow(desired_index);
    }
    else
    {
        display_planes_list->setCurrentRow(-1);
    }
    
    display_planes_list->blockSignals(false);

    if(display_planes_empty_label)
    {
        display_planes_empty_label->setVisible(display_planes_list->count() == 0);
    }


    if(display_planes.empty())
    {
        current_display_plane_index = -1;
        if(viewport) viewport->SelectDisplayPlane(-1);
        RefreshDisplayPlaneDetails();
        return;
    }

    if(desired_index >= 0 && desired_index < (int)display_planes.size())
    {
        current_display_plane_index = desired_index;
        on_display_plane_selected(desired_index);
    }
    else
    {
        current_display_plane_index = -1;
        on_display_plane_selected(-1);
    }
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
        display_plane_monitor_combo,
        display_plane_capture_combo,
        display_plane_refresh_capture_btn,
        display_plane_visible_check
    };

    for(int widget_index = 0; widget_index < widgets.size(); widget_index++)
    {
        QWidget* w = widgets[widget_index];
        if(w) w->setEnabled(has_plane);
    }

    if(!has_plane)
    {
        if(display_plane_name_edit) display_plane_name_edit->setText("");
        if(display_plane_width_spin) display_plane_width_spin->setValue(1000.0);
        if(display_plane_height_spin) display_plane_height_spin->setValue(600.0);
        if(display_plane_monitor_combo)
        {
            QSignalBlocker block(display_plane_monitor_combo);
            display_plane_monitor_combo->setCurrentIndex(-1);
            display_plane_monitor_combo->setEditText(QString());
        }
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
    if(display_plane_monitor_combo)
    {
        QSignalBlocker block(display_plane_monitor_combo);
        QString preset_id = QString::fromStdString(plane->GetMonitorPresetId());
        if(!preset_id.isEmpty())
        {
            bool found = false;
            for(int i = 0; i < display_plane_monitor_combo->count(); i++)
            {
                if(display_plane_monitor_combo->itemData(i).toString() == preset_id)
                {
                    display_plane_monitor_combo->setCurrentIndex(i);
                    found = true;
                    break;
                }
            }
            if(!found)
            {
                display_plane_monitor_combo->setCurrentIndex(-1);
                display_plane_monitor_combo->setEditText(QString());
            }
        }
        else
        {
            display_plane_monitor_combo->setCurrentIndex(-1);
            display_plane_monitor_combo->setEditText(QString());
        }
    }
    if(display_plane_capture_combo)
    {
        QSignalBlocker block(display_plane_capture_combo);
        std::string current_source = plane->GetCaptureSourceId();

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
            display_plane_capture_combo->addItem(QString::fromStdString(current_source) + " (custom)",
                                                  QString::fromStdString(current_source));
            display_plane_capture_combo->setCurrentIndex(display_plane_capture_combo->count() - 1);
        }
        else
        {
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

    std::vector<DisplayPlane3D*> plane_ptrs;
    for(size_t plane_index = 0; plane_index < display_planes.size(); plane_index++)
    {
        std::unique_ptr<DisplayPlane3D>& plane = display_planes[plane_index];
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
    if(index < 0 || index >= (int)display_planes.size())
    {
        if(!display_planes.empty() && display_planes_list)
        {
            int restore = (current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
                ? current_display_plane_index : 0;
            QSignalBlocker block(display_planes_list);
            display_planes_list->setCurrentRow(restore);
            current_display_plane_index = restore;
            return;
        }
        current_display_plane_index = -1;
        RefreshDisplayPlaneDetails();
        if(viewport)
        {
            viewport->SelectDisplayPlane(-1);
        }
        return;
    }

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
    if(plane)
    {
        SyncDisplayPlaneControls(plane);
        RefreshDisplayPlaneDetails();
        if(viewport)
        {
            if(plane->IsVisible())
            {
                viewport->SelectDisplayPlane(index);
            }
            else
            {
                viewport->SelectDisplayPlane(-1);
            }
        }
    }
    else
    {
        RefreshDisplayPlaneDetails();
        if(viewport)
        {
            viewport->SelectDisplayPlane(-1);
        }
    }
}

void OpenRGB3DSpatialTab::on_add_display_plane_clicked()
{
    std::string base_name = "Display Plane";
    int suffix = (int)display_planes.size() + 1;
    std::string full_name = base_name + " " + std::to_string(suffix);
    std::unique_ptr<DisplayPlane3D> plane = std::make_unique<DisplayPlane3D>(full_name);

    float room_height_units = room_height_spin ? MMToGridUnits((float)room_height_spin->value(), grid_scale_mm) : 100.0f;
    float room_depth_units = room_depth_spin ? MMToGridUnits((float)room_depth_spin->value(), grid_scale_mm) : 100.0f;

    plane->GetTransform().position.x = 0.0f;
    plane->GetTransform().position.y = -room_height_units * 0.25f;
    plane->GetTransform().position.z = room_depth_units * 0.5f;
    plane->SetVisible(false);

    std::string ref_point_name = full_name + " Reference";
    Vector3D plane_pos = plane->GetTransform().position;
    std::unique_ptr<VirtualReferencePoint3D> ref_point = std::make_unique<VirtualReferencePoint3D>(
        ref_point_name, 
        REF_POINT_MONITOR,
        plane_pos.x,
        plane_pos.y,
        plane_pos.z
    );
    ref_point->SetDisplayColor(0x00FF00);
    ref_point->SetVisible(false);
    
    int ref_point_index = (int)reference_points.size();
    reference_points.push_back(std::move(ref_point));
    plane->SetReferencePointIndex(ref_point_index);
    
    if(ref_point_index >= 0 && ref_point_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_pt = reference_points[ref_point_index].get();
        if(ref_pt)
        {
            ref_pt->GetTransform().rotation = plane->GetTransform().rotation;
        }
    }

    display_planes.push_back(std::move(plane));
    
    SetLayoutDirty();

    int new_plane_id = display_planes.back() ? display_planes.back()->GetId() : -1;

    current_display_plane_index = (int)display_planes.size() - 1;
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();

    QMessageBox::information(this, "Display Plane Created",
                            QString("Display plane '%1' created successfully!\n\nYou can now add it to the 3D view from the Available Controllers list.")
                            .arg(QString::fromStdString(full_name)));

    UpdateAvailableControllersList();
    UpdateReferencePointsList();

    if(new_plane_id >= 0)
    {
        SelectAvailableControllerEntry(-3, new_plane_id);
    }

    current_display_plane_index = (int)display_planes.size() - 1;
    if(display_planes_list && current_display_plane_index >= 0)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->setCurrentRow(current_display_plane_index);
    }
    if(DisplayPlane3D* plane = GetSelectedDisplayPlane())
    {
        SyncDisplayPlaneControls(plane);
    }
    RefreshDisplayPlaneDetails();
    if(viewport)
    {
        viewport->SelectDisplayPlane(current_display_plane_index);
    }
}

void OpenRGB3DSpatialTab::on_display_plane_monitor_preset_selected(int index)
{
    if(!display_plane_monitor_combo || index < 0 || index >= display_plane_monitor_combo->count())
    {
        return;
    }

    QString preset_id = display_plane_monitor_combo->itemData(index).toString();
    if(preset_id.isEmpty())
    {
        return;
    }

    std::vector<MonitorPreset>::iterator it = std::find_if(
        monitor_presets.begin(),
        monitor_presets.end(),
        [&preset_id](const MonitorPreset& preset) {
            return preset.id == preset_id;
        });
    if(it == monitor_presets.end())
    {
        return;
    }

    if(display_plane_width_spin)
    {
        QSignalBlocker block(display_plane_width_spin);
        display_plane_width_spin->setValue(it->width_mm);
    }
    if(display_plane_height_spin)
    {
        QSignalBlocker block(display_plane_height_spin);
        display_plane_height_spin->setValue(it->height_mm);
    }

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane)
    {
        return;
    }

    plane->SetWidthMM(static_cast<float>(it->width_mm));
    plane->SetHeightMM(static_cast<float>(it->height_mm));
    plane->SetMonitorPresetId(preset_id.toStdString());

    QString preset_name = QString("%1 %2").arg(it->brand, it->model).trimmed();
    plane->SetName(preset_name.toStdString());
    if(display_plane_name_edit)
    {
        QSignalBlocker name_block(display_plane_name_edit);
        display_plane_name_edit->setText(preset_name);
    }

    UpdateCurrentDisplayPlaneListItemLabel();
    NotifyDisplayPlaneChanged();
    UpdateAvailableControllersList();

    SetObjectCreatorStatus(QString("Preset applied: %1 %2 (%3 x %4 mm)")
                               .arg(it->brand, it->model)
                               .arg(it->width_mm, 0, 'f', 0)
                               .arg(it->height_mm, 0, 'f', 0));
}

void OpenRGB3DSpatialTab::on_monitor_preset_text_edited(const QString& text)
{
    if(text.isEmpty())
    {
        DisplayPlane3D* plane = GetSelectedDisplayPlane();
        if(plane)
        {
            plane->SetMonitorPresetId(std::string());
        }
    }
}

void OpenRGB3DSpatialTab::on_remove_display_plane_clicked()
{
    if(current_display_plane_index < 0 ||
       current_display_plane_index >= (int)display_planes.size())
    {
        return;
    }

    QSignalBlocker block_controller(controller_list);

    int removed_plane_id = display_planes[current_display_plane_index]->GetId();
    
    DisplayPlane3D* plane_to_remove = display_planes[current_display_plane_index].get();
    if(plane_to_remove)
    {
        int ref_point_index = plane_to_remove->GetReferencePointIndex();
        if(ref_point_index >= 0 && ref_point_index < (int)reference_points.size())
        {
            for(size_t i = 0; i < display_planes.size(); i++)
            {
                if(!display_planes[i]) continue;
                int plane_ref_idx = display_planes[i]->GetReferencePointIndex();
                if(plane_ref_idx == ref_point_index)
                {
                    display_planes[i]->SetReferencePointIndex(-1);
                }
                else if(plane_ref_idx > ref_point_index)
                {
                    display_planes[i]->SetReferencePointIndex(plane_ref_idx - 1);
                }
            }
            RemoveReferencePointControllerEntries(ref_point_index);
            reference_points.erase(reference_points.begin() + ref_point_index);
            UpdateReferencePointsList();
        }
    }

    display_planes.erase(display_planes.begin() + current_display_plane_index);
    
    SetLayoutDirty();
    
    if(current_display_plane_index >= (int)display_planes.size())
    {
        current_display_plane_index = (int)display_planes.size() - 1;
    }
    if(current_display_plane_index < 0 && !display_planes.empty())
    {
        current_display_plane_index = 0;
    }
    
    RemoveDisplayPlaneControllerEntries(removed_plane_id);
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
    UpdateAvailableControllersList();
}

void OpenRGB3DSpatialTab::on_display_plane_name_edited(const QString& text)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetName(text.toStdString());
    SetLayoutDirty();
    UpdateCurrentDisplayPlaneListItemLabel();
    NotifyDisplayPlaneChanged();
    UpdateAvailableControllersList();
}

void OpenRGB3DSpatialTab::on_display_plane_width_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetWidthMM((float)value);
    SetLayoutDirty();
    UpdateCurrentDisplayPlaneListItemLabel();
    NotifyDisplayPlaneChanged();
    ClearMonitorPresetSelectionIfManualEdit();
}

void OpenRGB3DSpatialTab::on_display_plane_height_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetHeightMM((float)value);
    SetLayoutDirty();
    UpdateCurrentDisplayPlaneListItemLabel();
    NotifyDisplayPlaneChanged();
    ClearMonitorPresetSelectionIfManualEdit();
}

void OpenRGB3DSpatialTab::on_display_plane_capture_changed(int index)
{
    if(!display_plane_capture_combo || index < 0) return;

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

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    capture_mgr.RefreshSources();
    std::vector<CaptureSourceInfo> sources = capture_mgr.GetAvailableSources();

    QSignalBlocker block(display_plane_capture_combo);
    display_plane_capture_combo->clear();
    display_plane_capture_combo->addItem("(None)", "");

    for(size_t source_index = 0; source_index < sources.size(); source_index++)
    {
        const CaptureSourceInfo& source = sources[source_index];
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
    SetLayoutDirty();

    int ref_index = plane->GetReferencePointIndex();
    if(ref_index >= 0 && ref_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
        if(ref_point)
        {
            ref_point->SetPosition({x, y, z});
        }
    }

    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_rotation_signal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        current_display_plane_index = -1;
        if(display_planes_list)
        {
            QSignalBlocker block(display_planes_list);
            display_planes_list->clearSelection();
        }
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
    SetLayoutDirty();

    int ref_index = plane->GetReferencePointIndex();
    if(ref_index >= 0 && ref_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
        if(ref_point)
        {
            Rotation3D ref_rot = {x, y, z};
            ref_point->GetTransform().rotation = ref_rot;
        }
    }

    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_visible_toggled(Qt::CheckState state)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetVisible(state == Qt::CheckState::Checked);
    UpdateCurrentDisplayPlaneListItemLabel();
    if(display_planes_list && current_display_plane_index >= 0 &&
       current_display_plane_index < display_planes_list->count())
    {
        QListWidgetItem* item = display_planes_list->item(current_display_plane_index);
        if(item)
        {
            if(plane->IsVisible())
            {
                item->setData(Qt::ForegroundRole, QVariant());
            }
            else
            {
                item->setForeground(QColor(0x888888));
            }
        }
    }
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
    UpdateAvailableControllersList();
}

void OpenRGB3DSpatialTab::on_led_spacing_preset_changed(int index)
{
    if(!led_spacing_x_spin || !led_spacing_y_spin || !led_spacing_z_spin)
    {
        return;
    }

    struct Preset
    {
        double x;
        double y;
        double z;
    };

    static const Preset presets[] = {
        {0.0, 0.0, 0.0},
        {10.0, 0.0, 0.0},
        {19.0, 0.0, 0.0},
        {33.0, 0.0, 0.0},
        {50.0, 50.0, 50.0}
    };

    if(index <= 0 || index >= static_cast<int>(sizeof(presets) / sizeof(Preset)))
    {
        return;
    }

    const Preset& preset = presets[index];

    std::function<void(QDoubleSpinBox*, double)> set_spin_value = [](QDoubleSpinBox* spin, double value)
    {
        if(!spin)
        {
            return;
        }
        bool blocked = spin->blockSignals(true);
        spin->setValue(value);
        spin->blockSignals(blocked);
    };

    set_spin_value(led_spacing_x_spin, preset.x);
    set_spin_value(led_spacing_y_spin, preset.y);
    set_spin_value(led_spacing_z_spin, preset.z);

    set_spin_value(edit_led_spacing_x_spin, preset.x);
    set_spin_value(edit_led_spacing_y_spin, preset.y);
    set_spin_value(edit_led_spacing_z_spin, preset.z);

    if(apply_spacing_button && controller_list)
    {
        apply_spacing_button->setEnabled(controller_list->currentRow() >= 0);
    }
}
