// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "VirtualReferencePoint3D.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
#include <QColorDialog>
#include <QSignalBlocker>

static QString RGBColorToCssHex(unsigned int color_value)
{
    unsigned int red = color_value & 0xFF;
    unsigned int green = (color_value >> 8) & 0xFF;
    unsigned int blue = (color_value >> 16) & 0xFF;
    return QString("#%1%2%3")
        .arg(red, 2, 16, QChar('0'))
        .arg(green, 2, 16, QChar('0'))
        .arg(blue, 2, 16, QChar('0'))
        .toUpper();
}

// Reference Points Management
void OpenRGB3DSpatialTab::on_add_ref_point_clicked()
{
    if(!ref_point_name_edit || !ref_point_type_combo || !reference_points_list) return;
    std::string name = ref_point_name_edit->text().toStdString();
    if(name.empty())
    {
        name = "Reference Point " + std::to_string(reference_points.size() + 1);
    }

    ReferencePointType type = (ReferencePointType)ref_point_type_combo->currentIndex();

    std::unique_ptr<VirtualReferencePoint3D> ref_point = std::make_unique<VirtualReferencePoint3D>(name, type, 0.0f, 0.0f, 0.0f);
    ref_point->SetDisplayColor(selected_ref_point_color);
    ref_point->SetVisible(false);  // Not visible until added to viewport

    reference_points.push_back(std::move(ref_point));

    int ref_index = (int)reference_points.size() - 1;
    UpdateAvailableControllersList();
    SelectAvailableControllerEntry(-2, ref_index);
    UpdateReferencePointsList();
    SaveReferencePoints(); // Mark layout as dirty

    // Clear inputs for next point
    ref_point_name_edit->clear();
    ref_point_type_combo->setCurrentIndex(0);

    if(reference_points_list)
        reference_points_list->setCurrentRow((int)reference_points.size() - 1);

    SetObjectCreatorStatus(QString("Reference point '%1' created. Add it from the Available Controllers list when ready.")
                           .arg(QString::fromStdString(name)),
                           false);
}

void OpenRGB3DSpatialTab::on_remove_ref_point_clicked()
{
    if(!reference_points_list) return;
    int index = reference_points_list->currentRow();
    if(index < 0 || index >= (int)reference_points.size())
    {
        return;
    }

    // Update display planes: clear or renumber reference_point_index so they stay valid after erase
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        if(!display_planes[i]) continue;
        int ref_idx = display_planes[i]->GetReferencePointIndex();
        if(ref_idx == index)
        {
            display_planes[i]->SetReferencePointIndex(-1);
        }
        else if(ref_idx > index)
        {
            display_planes[i]->SetReferencePointIndex(ref_idx - 1);
        }
    }

    RemoveReferencePointControllerEntries(index);

    reference_points.erase(reference_points.begin() + index);
    UpdateReferencePointsList();
    SaveReferencePoints(); // Mark layout as dirty
    if(viewport) viewport->update();
    UpdateAvailableControllersList();
}

void OpenRGB3DSpatialTab::on_ref_point_selected(int index)
{
    if(display_planes_list)
    {
        QSignalBlocker block(*display_planes_list);
        display_planes_list->clearSelection();
    }
    current_display_plane_index = -1;
    RefreshDisplayPlaneDetails();
    if(viewport) viewport->SelectDisplayPlane(-1);

    bool has_selection = (index >= 0 && index < (int)reference_points.size());
    if(remove_ref_point_button) remove_ref_point_button->setEnabled(has_selection);

    if(has_selection)
    {
        // Update the reference points list selection
        if(reference_points_list)
        {
            reference_points_list->blockSignals(true);
            reference_points_list->setCurrentRow(index);
            reference_points_list->blockSignals(false);
        }

        // Clear controller selection when reference point is selected
        if(controller_list)
        {
            controller_list->blockSignals(true);
            controller_list->clearSelection();
            controller_list->blockSignals(false);
        }

        // Update position/rotation controls with reference point values (position in grid units -> display in mm)
        VirtualReferencePoint3D* ref_point = reference_points[index].get();
        Vector3D pos = ref_point->GetPosition();
        double scale_mm = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
        if(scale_mm < 0.001) scale_mm = 10.0;
        double pos_x_mm = (double)pos.x * scale_mm;
        double pos_y_mm = (double)pos.y * scale_mm;
        double pos_z_mm = (double)pos.z * scale_mm;

        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);
        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);

        pos_x_slider->setValue((int)std::lround(pos_x_mm));
        pos_y_slider->setValue((int)std::lround(pos_y_mm));
        pos_z_slider->setValue((int)std::lround(pos_z_mm));
        pos_x_spin->setValue(pos_x_mm);
        pos_y_spin->setValue(pos_y_mm);
        pos_z_spin->setValue(pos_z_mm);

        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);
        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);

        // Enable position and rotation controls so user can move the reference point
        if(pos_x_spin) pos_x_spin->setEnabled(true);
        if(pos_y_spin) pos_y_spin->setEnabled(true);
        if(pos_z_spin) pos_z_spin->setEnabled(true);
        if(pos_x_slider) pos_x_slider->setEnabled(true);
        if(pos_y_slider) pos_y_slider->setEnabled(true);
        if(pos_z_slider) pos_z_slider->setEnabled(true);
        rot_x_slider->setEnabled(true);
        rot_y_slider->setEnabled(true);
        rot_z_slider->setEnabled(true);
        rot_x_spin->setEnabled(true);
        rot_y_spin->setEnabled(true);
        rot_z_spin->setEnabled(true);

        // Update rotation sliders with reference point rotation
        Rotation3D rot = ref_point->GetRotation();

        rot_x_slider->blockSignals(true);
        rot_y_slider->blockSignals(true);
        rot_z_slider->blockSignals(true);
        rot_x_spin->blockSignals(true);
        rot_y_spin->blockSignals(true);
        rot_z_spin->blockSignals(true);

        rot_x_slider->setValue((int)rot.x);
        rot_y_slider->setValue((int)rot.y);
        rot_z_slider->setValue((int)rot.z);
        rot_x_spin->setValue(rot.x);
        rot_y_spin->setValue(rot.y);
        rot_z_spin->setValue(rot.z);

        rot_x_slider->blockSignals(false);
        rot_y_slider->blockSignals(false);
        rot_z_slider->blockSignals(false);
        rot_x_spin->blockSignals(false);
        rot_y_spin->blockSignals(false);
        rot_z_spin->blockSignals(false);

        // Tell viewport about the selection
        if(viewport)
        {
            viewport->SelectReferencePoint(index);
        }
    }
}

void OpenRGB3DSpatialTab::on_ref_point_position_changed(int index, float x, float y, float z)
{
    if(index < 0 || index >= (int)reference_points.size()) return;
    if(!pos_x_slider || !pos_x_spin) return;
    VirtualReferencePoint3D* ref_point = reference_points[index].get();
    Vector3D pos = {x, y, z};
    ref_point->SetPosition(pos);

    double scale_mm = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
    if(scale_mm < 0.001) scale_mm = 10.0;
    double x_mm = (double)x * scale_mm;
    double y_mm = (double)y * scale_mm;
    double z_mm = (double)z * scale_mm;

    pos_x_slider->blockSignals(true);
    pos_y_slider->blockSignals(true);
    pos_z_slider->blockSignals(true);
    pos_x_spin->blockSignals(true);
    pos_y_spin->blockSignals(true);
    pos_z_spin->blockSignals(true);

    pos_x_slider->setValue((int)std::lround(x_mm));
    pos_y_slider->setValue((int)std::lround(y_mm));
    pos_z_slider->setValue((int)std::lround(z_mm));
    pos_x_spin->setValue(x_mm);
    pos_y_spin->setValue(y_mm);
    pos_z_spin->setValue(z_mm);

    pos_x_slider->blockSignals(false);
    pos_y_slider->blockSignals(false);
    pos_z_slider->blockSignals(false);
    pos_x_spin->blockSignals(false);
    pos_y_spin->blockSignals(false);
    pos_z_spin->blockSignals(false);

    // Mark layout as dirty when reference point moves
    SetLayoutDirty();

    if(viewport) viewport->update();
}

void OpenRGB3DSpatialTab::on_ref_point_color_clicked()
{
    QColor current_color = QColor(
        selected_ref_point_color & 0xFF,
        (selected_ref_point_color >> 8) & 0xFF,
        (selected_ref_point_color >> 16) & 0xFF
    );

    QColor color = QColorDialog::getColor(current_color, this, "Select Reference Point Color");
    if(color.isValid())
    {
        selected_ref_point_color = (color.blue() << 16) | (color.green() << 8) | color.red();
        ref_point_color_button->setStyleSheet(QString("background-color: %1").arg(RGBColorToCssHex(selected_ref_point_color)));
    }
}

void OpenRGB3DSpatialTab::UpdateReferencePointsList()
{
    if(!reference_points_list) return;
    reference_points_list->clear();
    for(size_t i = 0; i < reference_points.size(); i++)
    {
        const std::unique_ptr<VirtualReferencePoint3D>& ref_point = reference_points[i];
        if(!ref_point) continue; // Skip null pointers

        QString item_text = QString::fromStdString(ref_point->GetName());
        reference_points_list->addItem(item_text);
    }

    if(ref_points_empty_label)
    {
        ref_points_empty_label->setVisible(reference_points_list->count() == 0);
    }

    // Update effect origin combo whenever reference points change
    UpdateEffectOriginCombo();

    // Update ScreenMirror3D reference point dropdowns in effect stack
    for(unsigned int i = 0; i < effect_stack.size(); i++)
    {
        std::unique_ptr<EffectInstance3D>& inst = effect_stack[i];
        if(inst && inst->effect_class_name == "ScreenMirror3D" && inst->effect)
        {
            ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(inst->effect.get());
            if(screen_mirror)
            {
                screen_mirror->RefreshReferencePointDropdowns();
            }
        }
    }

    // Update ScreenMirror3D UI effect reference point dropdowns
    if(current_effect_ui)
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(current_effect_ui);
        if(screen_mirror)
        {
            screen_mirror->RefreshReferencePointDropdowns();
        }
    }
}

void OpenRGB3DSpatialTab::SaveReferencePoints()
{
    // Mark layout as dirty - reference points will be saved when user saves layout profile
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::LoadReferencePoints()
{
    // Reference points are now loaded as part of the layout JSON
    // This function is kept for future standalone load functionality if needed
}
