// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ControllerDisplayUtils.h"
#include "GridSpaceUtils.h"
#include "PluginUiUtils.h"
#include "ReferencePointDialog.h"
#include "ZoneControllerPickerDialog.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVariant>

namespace
{
QString BuildZoneDialogControllerName(const ControllerTransform* ctrl, size_t index)
{
    if(!ctrl)
    {
        return QString("Controller %1").arg(index);
    }

    if(ctrl->virtual_controller)
    {
        return QString("[Custom] ") + QString::fromStdString(ctrl->virtual_controller->GetName());
    }

    if(ctrl->controller)
    {
        QString name = ControllerDisplay::FormatRgbControllerTitle(ctrl->controller);
        if(ctrl->granularity == 1 && ctrl->item_idx >= 0 && ctrl->item_idx < (int)ctrl->controller->zones.size())
        {
            name += " - " + QString::fromStdString(ctrl->controller->GetZoneName((unsigned int)ctrl->item_idx));
        }
        return name;
    }

    return QString("Controller %1").arg(index);
}

std::vector<ZoneControllerPickerDialog::Entry> BuildZoneControllerEntries(
    const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
    const Zone3D* zone_for_edit)
{
    std::vector<ZoneControllerPickerDialog::Entry> entries;
    entries.reserve(controller_transforms.size());
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ZoneControllerPickerDialog::Entry entry;
        entry.label = BuildZoneDialogControllerName(controller_transforms[i].get(), i);
        if(zone_for_edit)
        {
            entry.checked = zone_for_edit->ContainsController((int)i);
        }
        entries.push_back(entry);
    }
    return entries;
}

bool RunZoneControllerPicker(QWidget* parent,
                             const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                             const QString& window_title,
                             const QString& prompt,
                             Zone3D* zone)
{
    if(!parent || !zone)
    {
        return false;
    }

    const std::vector<ZoneControllerPickerDialog::Entry> entries =
        BuildZoneControllerEntries(controller_transforms, zone);
    std::vector<bool> checked;
    if(!ZoneControllerPickerDialog::run(parent, window_title, prompt, entries, checked))
    {
        return false;
    }

    zone->ClearControllers();
    for(size_t i = 0; i < checked.size(); i++)
    {
        if(checked[i])
        {
            zone->AddController((int)i);
        }
    }
    return true;
}
}

void OpenRGB3DSpatialTab::createZoneClicked()
{
    if(!zone_manager) return;
    UpdateDeviceList();
    bool ok;
    QString zone_name = QInputDialog::getText(this, "Create Zone",
                                              "Zone name:", QLineEdit::Normal,
                                              "", &ok);

    if(!ok || zone_name.isEmpty())
    {
        return;
    }

    if(zone_manager->ZoneExists(zone_name.toStdString()))
    {
        QMessageBox::warning(this, "Zone Exists", "A zone with this name already exists.");
        return;
    }

    Zone3D* zone = zone_manager->CreateZone(zone_name.toStdString());

    const QString prompt = QString("Select controllers to add to zone '%1':").arg(zone_name);
    const std::vector<ZoneControllerPickerDialog::Entry> entries =
        BuildZoneControllerEntries(controller_transforms, nullptr);
    std::vector<bool> checked;
    if(ZoneControllerPickerDialog::run(this,
                                       tr("Select Controllers for Zone"),
                                       prompt,
                                       entries,
                                       checked))
    {
        for(size_t i = 0; i < checked.size(); i++)
        {
            if(checked[i])
            {
                zone->AddController((int)i);
            }
        }

        UpdateZonesList();
        SaveZones();
        SetLayoutDirty();

        QMessageBox::information(this, "Zone Created",
                                QString("Zone '%1' created with %2 controller(s).")
                                .arg(zone_name).arg(zone->GetControllerCount()));
    }
    else
    {
        zone_manager->DeleteZone(zone_name.toStdString());
    }
}

void OpenRGB3DSpatialTab::editZoneClicked()
{
    QListWidget* list = zonesList();
    if(!list || !zone_manager) return;
    UpdateDeviceList();
    int selected_idx = list->currentRow();
    if(selected_idx < 0 || selected_idx >= zone_manager->GetZoneCount())
    {
        return;
    }

    Zone3D* zone = zone_manager->GetZone(selected_idx);
    if(!zone)
    {
        return;
    }

    QString zone_name = QString::fromStdString(zone->GetName());
    const QString prompt = QString("Select controllers for zone '%1':").arg(zone_name);
    if(RunZoneControllerPicker(this,
                               controller_transforms,
                               QString("Edit Zone: %1").arg(zone_name),
                               prompt,
                               zone))
    {
        UpdateZonesList();
        SaveZones();
        SetLayoutDirty();

        QMessageBox::information(this, "Zone Updated",
                                QString("Zone '%1' now has %2 controller(s).")
                                .arg(zone_name).arg(zone->GetControllerCount()));
    }
}

void OpenRGB3DSpatialTab::deleteZoneClicked()
{
    QListWidget* list = zonesList();
    if(!list || !zone_manager) return;
    int selected_idx = list->currentRow();
    if(selected_idx < 0 || selected_idx >= zone_manager->GetZoneCount())
    {
        return;
    }

    Zone3D* zone = zone_manager->GetZone(selected_idx);
    if(!zone)
    {
        return;
    }

    QString zone_name = QString::fromStdString(zone->GetName());

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete Zone",
        QString("Are you sure you want to delete zone '%1'?").arg(zone_name),
        QMessageBox::Yes | QMessageBox::No
    );

    if(reply == QMessageBox::Yes)
    {
        zone_manager->DeleteZone(selected_idx);
        UpdateZonesList();
        SaveZones();
        SetLayoutDirty();
    }
}

void OpenRGB3DSpatialTab::zoneSelected(int index)
{
    if(!zone_manager) return;
    bool has_selection = (index >= 0 && index < zone_manager->GetZoneCount());
    if(QPushButton* edit_btn = editZoneButton()) edit_btn->setEnabled(has_selection);
    if(QPushButton* delete_btn = deleteZoneButton()) delete_btn->setEnabled(has_selection);
}

void OpenRGB3DSpatialTab::UpdateZonesList()
{
    QListWidget* list = zonesList();
    if(!list || !zone_manager)
    {
        return;
    }

    int selected_row = list->currentRow();
    list->clear();

    for(int i = 0; i < zone_manager->GetZoneCount(); i++)
    {
        Zone3D* zone = zone_manager->GetZone(i);
        if(zone)
        {
            QString item_text = QString("%1 (%2 controllers)")
                                .arg(QString::fromStdString(zone->GetName()))
                                .arg(zone->GetControllerCount());
            list->addItem(item_text);
        }
    }

    UpdateEffectZoneCombo();
    UpdateStackEffectZoneCombo();

    if(selected_row >= 0 && selected_row < list->count())
    {
        list->setCurrentRow(selected_row);
    }
    else if(list->count() > 0)
    {
        list->setCurrentRow(0);
    }
    else
    {
        zoneSelected(-1);
    }
}

void OpenRGB3DSpatialTab::PopulateZoneTargetCombo(QComboBox* combo, int saved_value)
{
    if(!combo)
    {
        return;
    }

    bool restore_signals = combo->blockSignals(true);
    combo->clear();

    combo->addItem("All Controllers", QVariant(-1));

    if(zone_manager)
    {
        for(int i = 0; i < zone_manager->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_manager->GetZone(i);
            if(zone)
            {
                QString zone_name = QString("[Zone] ") + QString::fromStdString(zone->GetName());
                combo->addItem(zone_name, QVariant(i));
            }
        }
    }

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* transform = controller_transforms[i].get();
        QString base_name;
        QString prefix = "[Controller] ";

        if(transform)
        {
            if(transform->controller)
            {
                base_name = ControllerDisplay::FormatRgbControllerTitle(transform->controller);
            }
            if(transform->virtual_controller && base_name.isEmpty())
            {
                prefix = "[Virtual] ";
                base_name = QString::fromStdString(transform->virtual_controller->GetName());
            }
        }

        if(base_name.isEmpty())
        {
            base_name = QString("Controller %1").arg((int)i);
        }

        combo->addItem(prefix + base_name, QVariant(-(int)(i + 1000)));
    }

    int restore_index = combo->findData(saved_value);
    if(restore_index < 0)
    {
        restore_index = combo->findData(-1);
    }

    if(restore_index < 0)
    {
        restore_index = 0;
    }

    combo->setCurrentIndex(restore_index);
    combo->blockSignals(restore_signals);
}

int OpenRGB3DSpatialTab::ResolveZoneTargetSelection(const QComboBox* combo) const
{
    if(!combo)
    {
        return -1;
    }

    QVariant data = combo->currentData();
    return data.isValid() ? data.toInt() : -1;
}

void OpenRGB3DSpatialTab::UpdateEffectZoneCombo()
{
    PopulateZoneTargetCombo(effectZoneCombo(), ResolveZoneTargetSelection(effectZoneCombo()));
}

void OpenRGB3DSpatialTab::SaveZones()
{
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::addRefPointClicked()
{
    if(!referencePointsList())
    {
        return;
    }

    ReferencePointDialog dialog(this);
    dialog.setCreateMode();
    dialog.setDefaultColor(0x00808080);

    if(dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::string name = dialog.name().toStdString();
    if(name.empty())
    {
        name = "Reference Point " + std::to_string(reference_points.size() + 1);
    }

    const ReferencePointType type = dialog.type();

    std::unique_ptr<VirtualReferencePoint3D> ref_point = std::make_unique<VirtualReferencePoint3D>(name, type, 0.0f, 0.0f, 0.0f);
    ref_point->SetDisplayColor(static_cast<RGBColor>(dialog.displayColorRgb()));
    ref_point->SetVisible(false);

    reference_points.push_back(std::move(ref_point));

    const int ref_index = (int)reference_points.size() - 1;
    UpdateAvailableControllersList();
    SelectAvailableControllerEntry(-2, ref_index);
    UpdateReferencePointsList();
    SaveReferencePoints();

    const int list_row = FindReferencePointListRow(ref_index);
    if(list_row >= 0)
    {
        referencePointsList()->setCurrentRow(list_row);
    }

}

void OpenRGB3DSpatialTab::editReferencePointClicked()
{
    if(!referencePointsList())
    {
        return;
    }

    const int ref_index = ReferencePointIndexFromListRow(referencePointsList()->currentRow());
    if(ref_index < 0)
    {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select a reference point from the list to edit."));
        return;
    }

    EditReferencePointAtIndex(ref_index);
}

void OpenRGB3DSpatialTab::referencePointsListSelectionChanged(int row)
{
    Q_UNUSED(row);
    const bool has_selection = (ReferencePointIndexFromListRow(referencePointsList() ? referencePointsList()->currentRow() : -1) >= 0);
    if(editReferencePointButton())
    {
        editReferencePointButton()->setEnabled(has_selection);
    }
}

bool OpenRGB3DSpatialTab::EditReferencePointAtIndex(int ref_index)
{
    if(ref_index < 0 || ref_index >= (int)reference_points.size())
    {
        return false;
    }

    VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
    if(!ref_point)
    {
        return false;
    }

    ReferencePointDialog dialog(this);
    dialog.setEditMode();
    dialog.loadFrom(*ref_point);

    if(dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    std::string name = dialog.name().toStdString();
    if(name.empty())
    {
        name = ref_point->GetName();
    }

    ref_point->SetName(name);
    ref_point->SetType(dialog.type());
    ref_point->SetDisplayColor(static_cast<RGBColor>(dialog.displayColorRgb()));

    SetLayoutDirty();
    UpdateReferencePointsList();
    SaveReferencePoints();
    UpdateAvailableControllersList();
    if(viewport)
    {
        viewport->update();
    }

    const int list_row = FindReferencePointListRow(ref_index);
    if(referencePointsList() && list_row >= 0)
    {
        referencePointsList()->setCurrentRow(list_row);
    }

    return true;
}

void OpenRGB3DSpatialTab::removeRefPointClicked()
{
    if(!referencePointsList()) return;
    int index = ReferencePointIndexFromListRow(referencePointsList()->currentRow());
    if(index < 0 || index >= (int)reference_points.size())
    {
        return;
    }

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
    SaveReferencePoints();
    if(viewport) viewport->update();
    UpdateAvailableControllersList();
}

void OpenRGB3DSpatialTab::refPointSelected(int index, bool from_scene_controller_list)
{
    if(!from_scene_controller_list && index >= 0)
    {
        const int scene_row = FindSceneRowForReferencePoint(index);
        if(scene_row >= 0)
        {
            sceneControllerCardsSelectionChanged(scene_row);
            ShowSceneObjectEditPanel(scene_row, false);
            return;
        }
    }

    if(index >= 0 && IsDeviceLinkedReferencePoint(index))
    {
        for(size_t ti = 0; ti < controller_transforms.size(); ti++)
        {
            ControllerTransform* transform = controller_transforms[ti].get();
            if(transform && transform->linked_reference_point_index == index)
            {
                controllerSelected(static_cast<int>(ti));
                return;
            }
        }
        index = -1;
    }

    if(displayPlanesList())
    {
        QSignalBlocker block(*displayPlanesList());
        displayPlanesList()->clearSelection();
    }
    current_display_plane_index = -1;
    RefreshDisplayPlaneDetails();
    if(viewport) viewport->SelectDisplayPlane(-1);

    bool has_selection = (index >= 0 && index < (int)reference_points.size());
    if(editReferencePointButton()) editReferencePointButton()->setEnabled(has_selection);
    if(removeRefPointButton()) removeRefPointButton()->setEnabled(has_selection);

    if(!has_selection)
    {
        if(posXSpin()) posXSpin()->setEnabled(false);
        if(posYSpin()) posYSpin()->setEnabled(false);
        if(posZSpin()) posZSpin()->setEnabled(false);
        if(posXSlider()) posXSlider()->setEnabled(false);
        if(posYSlider()) posYSlider()->setEnabled(false);
        if(posZSlider()) posZSlider()->setEnabled(false);
        if(rotXSlider()) rotXSlider()->setEnabled(false);
        if(rotYSlider()) rotYSlider()->setEnabled(false);
        if(rotZSlider()) rotZSlider()->setEnabled(false);
        if(rotXSpin()) rotXSpin()->setEnabled(false);
        if(rotYSpin()) rotYSpin()->setEnabled(false);
        if(rotZSpin()) rotZSpin()->setEnabled(false);
        if(viewport)
        {
            viewport->SelectReferencePoint(-1);
        }
        MaybeHideSceneObjectEditOnDeselect();
        return;
    }

    if(has_selection)
    {
        if(referencePointsList())
        {
            const int list_row = FindReferencePointListRow(index);
            referencePointsList()->blockSignals(true);
            if(list_row >= 0)
            {
                referencePointsList()->setCurrentRow(list_row);
            }
            referencePointsList()->blockSignals(false);
        }

        if(!from_scene_controller_list)
        {
            scene_controllers_.clearSelection();
        }

        VirtualReferencePoint3D* ref_point = reference_points[index].get();
        if(!ref_point)
        {
            if(viewport)
            {
                viewport->SelectReferencePoint(-1);
            }
            return;
        }
        Vector3D pos = ref_point->GetPosition();
        const float scale_mm = static_cast<float>(EffectiveGridScaleMm());
        const double pos_x_mm = static_cast<double>(GridUnitsToMM(pos.x, scale_mm));
        const double pos_y_mm = static_cast<double>(GridUnitsToMM(pos.y, scale_mm));
        const double pos_z_mm = static_cast<double>(GridUnitsToMM(pos.z, scale_mm));

        SetScenePositionControlsMm(pos_x_mm, pos_y_mm, pos_z_mm);

        if(posXSpin()) posXSpin()->setEnabled(true);
        if(posYSpin()) posYSpin()->setEnabled(true);
        if(posZSpin()) posZSpin()->setEnabled(true);
        if(posXSlider()) posXSlider()->setEnabled(true);
        if(posYSlider()) posYSlider()->setEnabled(true);
        if(posZSlider()) posZSlider()->setEnabled(true);
        rotXSlider()->setEnabled(true);
        rotYSlider()->setEnabled(true);
        rotZSlider()->setEnabled(true);
        rotXSpin()->setEnabled(true);
        rotYSpin()->setEnabled(true);
        rotZSpin()->setEnabled(true);

        Rotation3D rot = ref_point->GetRotation();

        rotXSlider()->blockSignals(true);
        rotYSlider()->blockSignals(true);
        rotZSlider()->blockSignals(true);
        rotXSpin()->blockSignals(true);
        rotYSpin()->blockSignals(true);
        rotZSpin()->blockSignals(true);

        rotXSlider()->setValue((int)rot.x);
        rotYSlider()->setValue((int)rot.y);
        rotZSlider()->setValue((int)rot.z);
        rotXSpin()->setValue(rot.x);
        rotYSpin()->setValue(rot.y);
        rotZSpin()->setValue(rot.z);

        rotXSlider()->blockSignals(false);
        rotYSlider()->blockSignals(false);
        rotZSlider()->blockSignals(false);
        rotXSpin()->blockSignals(false);
        rotYSpin()->blockSignals(false);
        rotZSpin()->blockSignals(false);

        if(viewport)
        {
            viewport->SelectReferencePoint(index);
        }
    }
}

void OpenRGB3DSpatialTab::refPointPositionChanged(int index, float x, float y, float z)
{
    if(index < 0 || index >= (int)reference_points.size()) return;
    if(!posXSlider() || !posXSpin()) return;
    VirtualReferencePoint3D* ref_point = reference_points[index].get();
    if(!ref_point) return;

    if(IsDeviceLinkedReferencePoint(index))
    {
        for(size_t ti = 0; ti < controller_transforms.size(); ti++)
        {
            ControllerTransform* transform = controller_transforms[ti].get();
            if(transform && transform->linked_reference_point_index == index)
            {
                SyncControllerLinkedReferencePoint(static_cast<int>(ti));
                break;
            }
        }
        for(size_t pi = 0; pi < display_planes.size(); pi++)
        {
            DisplayPlane3D* plane = display_planes[pi].get();
            if(plane && plane->GetReferencePointIndex() == index)
            {
                const Transform3D& pt = plane->GetTransform();
                ref_point->SetPosition({pt.position.x, pt.position.y, pt.position.z});
                break;
            }
        }
        if(viewport) viewport->update();
        return;
    }

    Vector3D pos = {x, y, z};
    ref_point->SetPosition(pos);

    const float scale_mm = static_cast<float>(EffectiveGridScaleMm());
    const double x_mm = static_cast<double>(GridUnitsToMM(x, scale_mm));
    const double y_mm = static_cast<double>(GridUnitsToMM(y, scale_mm));
    const double z_mm = static_cast<double>(GridUnitsToMM(z, scale_mm));

    SetScenePositionControlsMm(x_mm, y_mm, z_mm);

    SetLayoutDirty();

    if(viewport)
    {
        viewport->UpdateGizmoPosition();
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::UpdateReferencePointsList()
{
    if(!referencePointsList()) return;
    const int selected_ref_index = ReferencePointIndexFromListRow(referencePointsList()->currentRow());
    referencePointsList()->clear();
    for(size_t i = 0; i < reference_points.size(); i++)
    {
        const std::unique_ptr<VirtualReferencePoint3D>& ref_point = reference_points[i];
        if(!ref_point || IsDeviceLinkedReferencePoint(static_cast<int>(i))) continue;

        QString item_text = QString::fromStdString(ref_point->GetName());
        QListWidgetItem* item = new QListWidgetItem(item_text);
        item->setData(Qt::UserRole, static_cast<int>(i));
        referencePointsList()->addItem(item);
    }

    if(refPointsEmptyLabel())
    {
        refPointsEmptyLabel()->setVisible(referencePointsList()->count() == 0);
    }

    const int restored_row = FindReferencePointListRow(selected_ref_index);
    if(restored_row >= 0)
    {
        referencePointsList()->setCurrentRow(restored_row);
    }
    else if(referencePointsList()->count() == 0)
    {
        refPointSelected(-1);
    }

    UpdateEffectOriginCombo();

    RefreshAmbilightReferencePointDropdowns();
}

void OpenRGB3DSpatialTab::SaveReferencePoints()
{
    SetLayoutDirty();
}
