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

        // Update position/rotation controls with reference point values
        VirtualReferencePoint3D* ref_point = reference_points[index].get();
        Vector3D pos = ref_point->GetPosition();

        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);
        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);

        pos_x_slider->setValue((int)(pos.x * 10));
        pos_y_slider->setValue((int)(pos.y * 10));
        pos_z_slider->setValue((int)(pos.z * 10));
        pos_x_spin->setValue(pos.x);
        pos_y_spin->setValue(pos.y);
        pos_z_spin->setValue(pos.z);

        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);
        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);

        // Enable rotation controls - reference points have rotation
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

    pos_x_slider->blockSignals(true);
    pos_y_slider->blockSignals(true);
    pos_z_slider->blockSignals(true);
    pos_x_spin->blockSignals(true);
    pos_y_spin->blockSignals(true);
    pos_z_spin->blockSignals(true);

    pos_x_slider->setValue((int)(x * 10));
    pos_y_slider->setValue((int)(y * 10));
    pos_z_slider->setValue((int)(z * 10));
    pos_x_spin->setValue(x);
    pos_y_spin->setValue(y);
    pos_z_spin->setValue(z);

    pos_x_slider->blockSignals(false);
    pos_y_slider->blockSignals(false);
    pos_z_slider->blockSignals(false);
    pos_x_spin->blockSignals(false);
    pos_y_spin->blockSignals(false);
    pos_z_spin->blockSignals(false);

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

    // Update effect origin combo whenever reference points change
    UpdateEffectOriginCombo();
    UpdateAudioEffectOriginCombo();

    // Update ScreenMirror3D reference point dropdowns in effect stack
    for (unsigned int i = 0; i < effect_stack.size(); i++)
    {
        std::unique_ptr<EffectInstance3D>& inst = effect_stack[i];
        if (inst && inst->effect_class_name == "ScreenMirror3D" && inst->effect)
        {
            ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(inst->effect.get());
            if (screen_mirror)
            {
                screen_mirror->RefreshReferencePointDropdowns();
            }
        }
    }

    // Update ScreenMirror3D UI effect reference point dropdowns
    if (current_effect_ui)
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(current_effect_ui);
        if (screen_mirror)
        {
            screen_mirror->RefreshReferencePointDropdowns();
        }
    }
}

void OpenRGB3DSpatialTab::SaveReferencePoints()
{
    // Reference points are now saved as part of the layout JSON
    // This function is kept for future standalone save functionality if needed
}

void OpenRGB3DSpatialTab::LoadReferencePoints()
{
    // Reference points are now loaded as part of the layout JSON
    // This function is kept for future standalone load functionality if needed
}
