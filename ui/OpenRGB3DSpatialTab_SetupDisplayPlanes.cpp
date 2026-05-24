// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "DisplayPlaneDialog.h"
#include "DisplayPlaneManager.h"
#include "ControllerLayout3D.h"
#include "SpatialTabLedHelpers.h"
#include "SpatialControllerCardList.h"
#include "ScreenCaptureManager.h"
#include "GridSpaceUtils.h"
#include "PluginUiUtils.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <QMessageBox>
#include <QSignalBlocker>
#include <cmath>
#include <vector>

DisplayPlane3D* OpenRGB3DSpatialTab::GetSelectedDisplayPlane()
{
    if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
    {
        return display_planes[current_display_plane_index].get();
    }

    if(displayPlanesList())
    {
        const int selected_row = displayPlanesList()->currentRow();
        if(selected_row >= 0 && selected_row < (int)display_planes.size())
        {
            current_display_plane_index = selected_row;
            return display_planes[selected_row].get();
        }
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
    double scale_mm = (gridScaleSpin() != nullptr) ? gridScaleSpin()->value() : (double)grid_scale_mm;
    if(scale_mm < 0.001) scale_mm = 10.0;
    double pos_x_mm = (double)transform.position.x * scale_mm;
    double pos_y_mm = (double)transform.position.y * scale_mm;
    double pos_z_mm = (double)transform.position.z * scale_mm;

    SetScenePositionControlsMm(pos_x_mm, pos_y_mm, pos_z_mm);

    if(rotXSpin()) { QSignalBlocker block(rotXSpin()); rotXSpin()->setValue(transform.rotation.x); }
    if(rotXSlider()) { QSignalBlocker block(rotXSlider()); rotXSlider()->setValue((int)std::lround(transform.rotation.x)); }

    if(rotYSpin()) { QSignalBlocker block(rotYSpin()); rotYSpin()->setValue(transform.rotation.y); }
    if(rotYSlider()) { QSignalBlocker block(rotYSlider()); rotYSlider()->setValue((int)std::lround(transform.rotation.y)); }

    if(rotZSpin()) { QSignalBlocker block(rotZSpin()); rotZSpin()->setValue(transform.rotation.z); }
    if(rotZSlider()) { QSignalBlocker block(rotZSlider()); rotZSlider()->setValue((int)std::lround(transform.rotation.z)); }
}

void OpenRGB3DSpatialTab::UpdateCurrentDisplayPlaneListItemLabel()
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane || !displayPlanesList() || current_display_plane_index < 0 ||
       current_display_plane_index >= displayPlanesList()->count())
    {
        return;
    }
    QListWidgetItem* item = displayPlanesList()->item(current_display_plane_index);
    if(!item) return;
    QString label = QString::fromStdString(plane->GetName()) +
        QString(" (%1 x %2 mm)")
            .arg(plane->GetWidthMM(), 0, 'f', 0)
            .arg(plane->GetHeightMM(), 0, 'f', 0);
    item->setText(label);
}

void OpenRGB3DSpatialTab::UpdateDisplayPlanesList()
{
    if(!displayPlanesList())
    {
        return;
    }

    int desired_index = current_display_plane_index;

    bool restore_signals = displayPlanesList()->blockSignals(true);
    displayPlanesList()->clear();
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        const DisplayPlane3D* plane = display_planes[i].get();
        if(!plane) continue;
        QString label = QString::fromStdString(plane->GetName()) +
                        QString(" (%1 x %2 mm)")
                            .arg(plane->GetWidthMM(), 0, 'f', 0)
                            .arg(plane->GetHeightMM(), 0, 'f', 0);
        QListWidgetItem* item = new QListWidgetItem(label, displayPlanesList());
        item->setData(Qt::UserRole, plane->GetId());
        if(!plane->IsVisible())
        {
            item->setForeground(QColor(0x888888));
        }
    }
    
    if(desired_index >= 0 && desired_index < (int)display_planes.size())
    {
        displayPlanesList()->setCurrentRow(desired_index);
    }
    else
    {
        displayPlanesList()->setCurrentRow(-1);
    }
    
    displayPlanesList()->blockSignals(restore_signals);

    if(displayPlanesEmptyLabel())
    {
        displayPlanesEmptyLabel()->setVisible(displayPlanesList()->count() == 0);
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
    const int row = displayPlanesList() ? displayPlanesList()->currentRow() : -1;
    on_display_planes_list_selection_changed(row);

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(plane)
    {
        SyncDisplayPlaneControls(plane);
    }
}

void OpenRGB3DSpatialTab::on_display_planes_list_selection_changed(int row)
{
    const bool has_plane = (row >= 0 && row < (int)display_planes.size());
    if(editDisplayPlaneButton())
    {
        editDisplayPlaneButton()->setEnabled(has_plane);
    }
    if(removeDisplayPlaneButton())
    {
        removeDisplayPlaneButton()->setEnabled(has_plane);
    }
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
        current_display_plane_index = -1;
        RefreshDisplayPlaneDetails();
        if(sceneControllerCards())
        {
            sceneControllerCards()->setSelectedSceneRow(-1, false);
        }
        if(viewport)
        {
            viewport->SelectDisplayPlane(-1);
        }
        MaybeHideSceneObjectEditOnDeselect();
        return;
    }

    current_display_plane_index = index;

    const int scene_row = FindSceneRowForDisplayPlane(index);
    if(scene_row >= 0)
    {
        scene_controllers_.setCurrentRow(scene_row);
    }
    else
    {
        scene_controllers_.clearSelection();
    }
    if(sceneControllerCards())
    {
        sceneControllerCards()->setSelectedSceneRow(scene_row);
    }

    if(referencePointsList())
    {
        QSignalBlocker block(referencePointsList());
        referencePointsList()->clearSelection();
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
    const int suffix = (int)display_planes.size() + 1;
    const QString suggested_name = QString("Display Plane %1").arg(suffix);

    DisplayPlaneDialog dialog(this);
    dialog.setCreateMode();
    dialog.setCreateDefaults(suggested_name, 1000.0f, 600.0f);
    if(dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::string full_name = dialog.name().toStdString();
    if(full_name.empty())
    {
        full_name = suggested_name.toStdString();
    }

    std::unique_ptr<DisplayPlane3D> plane = std::make_unique<DisplayPlane3D>(full_name);
    plane->SetWidthMM(dialog.widthMm());
    plane->SetHeightMM(dialog.heightMm());
    plane->SetCaptureSourceId(dialog.captureSourceId());
    plane->SetVisible(dialog.isVisibleInViewport());

    float room_height_units = roomHeightSpin() ? MMToGridUnits((float)roomHeightSpin()->value(), grid_scale_mm) : 100.0f;
    float room_depth_units = roomDepthSpin() ? MMToGridUnits((float)roomDepthSpin()->value(), grid_scale_mm) : 100.0f;

    plane->GetTransform().position.x = 0.0f;
    plane->GetTransform().position.y = -room_height_units * 0.25f;
    plane->GetTransform().position.z = room_depth_units * 0.5f;

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

    DisplayPlane3D* created_plane = display_planes.back().get();
    if(created_plane && created_plane->IsVisible())
    {
        SetDisplayPlaneVisibleInScene(created_plane, true);
    }
    
    SetLayoutDirty();

    int new_plane_id = created_plane ? created_plane->GetId() : -1;

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
    if(displayPlanesList() && current_display_plane_index >= 0)
    {
        QSignalBlocker block(displayPlanesList());
        displayPlanesList()->setCurrentRow(current_display_plane_index);
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

void OpenRGB3DSpatialTab::on_edit_display_plane_clicked()
{
    if(current_display_plane_index < 0 || current_display_plane_index >= (int)display_planes.size())
    {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select a display plane from the list to edit."));
        return;
    }

    EditDisplayPlaneAtIndex(current_display_plane_index);
}

bool OpenRGB3DSpatialTab::EditDisplayPlaneAtIndex(int plane_index)
{
    if(plane_index < 0 || plane_index >= (int)display_planes.size())
    {
        return false;
    }

    DisplayPlane3D* plane = display_planes[plane_index].get();
    if(!plane)
    {
        return false;
    }

    DisplayPlaneDialog dialog(this);
    dialog.setEditMode();
    dialog.loadFrom(*plane);

    if(dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    std::string name = dialog.name().toStdString();
    if(name.empty())
    {
        name = plane->GetName();
    }

    const bool was_visible = plane->IsVisible();
    const bool want_visible = dialog.isVisibleInViewport();

    plane->SetName(name);
    plane->SetWidthMM(dialog.widthMm());
    plane->SetHeightMM(dialog.heightMm());
    plane->SetCaptureSourceId(dialog.captureSourceId());
    plane->SetVisible(want_visible);

    if(was_visible != want_visible)
    {
        SetDisplayPlaneVisibleInScene(plane, want_visible);
    }

    SetLayoutDirty();
    current_display_plane_index = plane_index;
    UpdateDisplayPlanesList();
    UpdateCurrentDisplayPlaneListItemLabel();
    NotifyDisplayPlaneChanged();
    UpdateAvailableControllersList();

    if(displayPlanesList())
    {
        QSignalBlocker block(displayPlanesList());
        displayPlanesList()->setCurrentRow(plane_index);
    }
    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();

    return true;
}

void OpenRGB3DSpatialTab::on_remove_display_plane_clicked()
{
    if(current_display_plane_index < 0 ||
       current_display_plane_index >= (int)display_planes.size())
    {
        return;
    }
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

void OpenRGB3DSpatialTab::FillDisplayPlaneCaptureCombo(QComboBox* combo, const std::string& prefer_source_id)
{
    if(!combo)
    {
        return;
    }

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    capture_mgr.RefreshSources();
    const std::vector<CaptureSourceInfo> sources = capture_mgr.GetAvailableSources();

    QSignalBlocker block(combo);
    combo->clear();
    combo->addItem("(None)", "");

    for(const CaptureSourceInfo& source : sources)
    {
        QString label = QString::fromStdString(source.name);
        if(source.is_primary)
        {
            label += " [Primary]";
        }
        label += QString(" (%1x%2)").arg(source.width).arg(source.height);
        combo->addItem(label, QString::fromStdString(source.id));
    }

    const QString prefer_q = QString::fromStdString(prefer_source_id);
    if(!prefer_q.isEmpty())
    {
        for(int i = 0; i < combo->count(); i++)
        {
            if(combo->itemData(i).toString() == prefer_q)
            {
                combo->setCurrentIndex(i);
                return;
            }
        }

        combo->addItem(prefer_q + " (custom)", prefer_q);
        combo->setCurrentIndex(combo->count() - 1);
        return;
    }

    combo->setCurrentIndex(0);
}

void OpenRGB3DSpatialTab::SetDisplayPlaneVisibleInScene(DisplayPlane3D* plane, bool visible)
{
    if(!plane)
    {
        return;
    }

    plane->SetVisible(visible);
    const int plane_id = plane->GetId();
    if(visible)
    {
        bool has_plane_entry = false;
        for(int row = 0; row < scene_controllers_.count(); row++)
        {
            if(!scene_controllers_.hasUserRole(row))
            {
                continue;
            }
            const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(row);
            if(metadata.first == -3 && metadata.second == plane_id)
            {
                has_plane_entry = true;
                break;
            }
        }
        if(!has_plane_entry)
        {
            scene_controllers_.append(QString("[Display] ") + QString::fromStdString(plane->GetName()),
                                      qMakePair(-3, plane_id));
        }
    }
    else
    {
        RemoveDisplayPlaneControllerEntries(plane_id);
    }

    const int linked_ref_idx = plane->GetReferencePointIndex();
    if(linked_ref_idx >= 0 && linked_ref_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_pt = reference_points[linked_ref_idx].get();
        if(ref_pt)
        {
            bool keep_ref_visible = false;
            if(visible)
            {
                keep_ref_visible = true;
            }
            else
            {
                for(size_t plane_index = 0; plane_index < display_planes.size(); plane_index++)
                {
                    DisplayPlane3D* other_plane = display_planes[plane_index].get();
                    if(!other_plane || other_plane == plane)
                    {
                        continue;
                    }
                    if(other_plane->IsVisible() && other_plane->GetReferencePointIndex() == linked_ref_idx)
                    {
                        keep_ref_visible = true;
                        break;
                    }
                }
            }

            ref_pt->SetVisible(keep_ref_visible);
        }
    }

    SetLayoutDirty();
    UpdateCurrentDisplayPlaneListItemLabel();
    if(displayPlanesList() && current_display_plane_index >= 0 &&
       current_display_plane_index < displayPlanesList()->count())
    {
        QListWidgetItem* item = displayPlanesList()->item(current_display_plane_index);
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
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::on_display_plane_position_signal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        current_display_plane_index = -1;
        if(displayPlanesList())
        {
            QSignalBlocker block(displayPlanesList());
            displayPlanesList()->clearSelection();
        }
        return;
    }

    if(index >= (int)display_planes.size())
    {
        return;
    }

    current_display_plane_index = index;
    if(displayPlanesList())
    {
        QSignalBlocker block(displayPlanesList());
        displayPlanesList()->setCurrentRow(index);
    }
    scene_controllers_.clearSelection();
    if(referencePointsList())
    {
        QSignalBlocker block(referencePointsList());
        referencePointsList()->clearSelection();
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
        if(displayPlanesList())
        {
            QSignalBlocker block(displayPlanesList());
            displayPlanesList()->clearSelection();
        }
        return;
    }

    if(index >= (int)display_planes.size())
    {
        return;
    }

    current_display_plane_index = index;
    if(displayPlanesList())
    {
        QSignalBlocker block(displayPlanesList());
        displayPlanesList()->setCurrentRow(index);
    }
    scene_controllers_.clearSelection();
    if(referencePointsList())
    {
        QSignalBlocker block(referencePointsList());
        referencePointsList()->clearSelection();
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

