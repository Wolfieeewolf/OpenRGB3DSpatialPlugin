// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ScenePropDialog.h"
#include "SceneProp3D.h"
#include "GridSpaceUtils.h"
#include "ObjectCreatorTabPanel.h"
#include "LEDViewport3D.h"
#include "SpatialControllerCardList.h"
#include "SpatialEffectTypes.h"

#include <QListWidget>
#include <QMessageBox>
#include <QSignalBlocker>
#include <cmath>

namespace
{
QString ScenePropListLabel(const SceneProp3D& prop)
{
    return QStringLiteral("%1 (%2 × %3 × %4 mm)")
        .arg(QString::fromStdString(prop.GetName()))
        .arg(prop.GetWidthMM(), 0, 'f', 0)
        .arg(prop.GetHeightMM(), 0, 'f', 0)
        .arg(prop.GetDepthMM(), 0, 'f', 0);
}
} // namespace

SceneProp3D* OpenRGB3DSpatialTab::GetSelectedSceneProp()
{
    if(current_scene_prop_index_ >= 0 && current_scene_prop_index_ < (int)scene_props_.size())
    {
        return scene_props_[(size_t)current_scene_prop_index_].get();
    }
    if(scenePropsList())
    {
        const int row = scenePropsList()->currentRow();
        if(row >= 0 && row < (int)scene_props_.size())
        {
            current_scene_prop_index_ = row;
            return scene_props_[(size_t)row].get();
        }
    }
    return nullptr;
}

void OpenRGB3DSpatialTab::SyncScenePropControls(SceneProp3D* prop)
{
    if(!prop)
    {
        return;
    }

    const Transform3D& transform = prop->GetTransform();
    const float scale_mm = static_cast<float>(EffectiveGridScaleMm());
    const double pos_x_mm = static_cast<double>(GridUnitsToMM(transform.position.x, scale_mm));
    const double pos_y_mm = static_cast<double>(GridUnitsToMM(transform.position.y, scale_mm));
    const double pos_z_mm = static_cast<double>(GridUnitsToMM(transform.position.z, scale_mm));

    SetScenePositionControlsMm(pos_x_mm, pos_y_mm, pos_z_mm);

    if(rotXSpin()) { QSignalBlocker block(rotXSpin()); rotXSpin()->setValue(transform.rotation.x); }
    if(rotXSlider()) { QSignalBlocker block(rotXSlider()); rotXSlider()->setValue((int)std::lround(transform.rotation.x)); }

    if(rotYSpin()) { QSignalBlocker block(rotYSpin()); rotYSpin()->setValue(transform.rotation.y); }
    if(rotYSlider()) { QSignalBlocker block(rotYSlider()); rotYSlider()->setValue((int)std::lround(transform.rotation.y)); }

    if(rotZSpin()) { QSignalBlocker block(rotZSpin()); rotZSpin()->setValue(transform.rotation.z); }
    if(rotZSlider()) { QSignalBlocker block(rotZSlider()); rotZSlider()->setValue((int)std::lround(transform.rotation.z)); }
}

void OpenRGB3DSpatialTab::NotifyScenePropChanged()
{
    if(viewport)
    {
        viewport->NotifyScenePropChanged();
    }
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::UpdateScenePropsList()
{
    QListWidget* list = scenePropsList();
    if(!list)
    {
        return;
    }

    int desired_index = current_scene_prop_index_;

    const QSignalBlocker block(list);
    list->clear();
    for(const auto& prop : scene_props_)
    {
        if(!prop)
        {
            continue;
        }
        list->addItem(ScenePropListLabel(*prop));
    }
    if(scenePropsEmptyLabel())
    {
        scenePropsEmptyLabel()->setVisible(scene_props_.empty());
    }

    if(desired_index >= 0 && desired_index < list->count())
    {
        list->setCurrentRow(desired_index);
        current_scene_prop_index_ = desired_index;
        scenePropSelected(desired_index);
    }
    else
    {
        list->setCurrentRow(-1);
        current_scene_prop_index_ = -1;
        scenePropSelected(-1);
    }
}

void OpenRGB3DSpatialTab::scenePropSelected(int index)
{
    if(index < 0 || index >= (int)scene_props_.size())
    {
        current_scene_prop_index_ = -1;
        if(sceneControllerCards())
        {
            sceneControllerCards()->setSelectedSceneRow(-1, false);
        }
        if(viewport)
        {
            viewport->SelectSceneProp(-1);
        }
        MaybeHideSceneObjectEditOnDeselect();
        return;
    }

    current_scene_prop_index_ = index;

    const int scene_row = FindSceneRowForSceneProp(index);
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

    SceneProp3D* prop = GetSelectedSceneProp();
    if(prop)
    {
        SyncScenePropControls(prop);
        if(viewport)
        {
            if(prop->IsVisible())
            {
                viewport->SelectSceneProp(index);
            }
            else
            {
                viewport->SelectSceneProp(-1);
            }
        }
    }
    else if(viewport)
    {
        viewport->SelectSceneProp(-1);
    }
}

void OpenRGB3DSpatialTab::scenePropsListSelectionChanged(int row)
{
    if(editScenePropButton())
    {
        editScenePropButton()->setEnabled(row >= 0 && row < (int)scene_props_.size());
    }
    if(removeScenePropButton())
    {
        removeScenePropButton()->setEnabled(row >= 0 && row < (int)scene_props_.size());
    }
    scenePropSelected(row);
}

void OpenRGB3DSpatialTab::viewportScenePropSelected(int prop_index)
{
    if(prop_index >= 0)
    {
        const int scene_row = FindSceneRowForSceneProp(prop_index);
        if(scene_row >= 0)
        {
            sceneControllerCardsSelectionChanged(scene_row);
            ShowSceneObjectEditPanel(scene_row, false);
            return;
        }
    }

    scene_controllers_.clearSelection();
    if(sceneControllerCards())
    {
        sceneControllerCards()->setSelectedSceneRow(-1, false);
    }
    scenePropSelected(prop_index);
    if(prop_index < 0)
    {
        MaybeHideSceneObjectEditOnDeselect();
    }
}

void OpenRGB3DSpatialTab::addScenePropClicked()
{
    const QString suggested = QStringLiteral("Prop %1").arg((int)scene_props_.size() + 1);
    ScenePropDialog dialog(this);
    dialog.setCreateMode();
    /* Default mid-tower-ish box; user can reshape for desk / pegboard / etc. */
    dialog.setCreateDefaults(suggested, 200.0f, 450.0f, 450.0f, 0x555555u);
    if(dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::string full_name = dialog.name().toStdString();
    if(full_name.empty())
    {
        full_name = suggested.toStdString();
    }

    auto prop = std::make_unique<SceneProp3D>(full_name);
    dialog.applyTo(prop.get());
    prop->SetVisible(false);

    const float scale = static_cast<float>(EffectiveGridScaleMm());
    const float room_w = roomWidthSpin()
        ? MMToGridUnits(static_cast<float>(roomWidthSpin()->value()), scale)
        : MMToGridUnits(DEFAULT_ROOM_SIZE_MM, scale);
    const float room_d = roomDepthSpin()
        ? MMToGridUnits(static_cast<float>(roomDepthSpin()->value()), scale)
        : MMToGridUnits(DEFAULT_ROOM_SIZE_MM, scale);
    const float h_units = MMToGridUnits(prop->GetHeightMM(), scale);
    prop->GetTransform().position.x = room_w * 0.5f;
    prop->GetTransform().position.y = h_units * 0.5f;
    prop->GetTransform().position.z = room_d * 0.5f;

    std::string ref_point_name = full_name + " Reference";
    Vector3D prop_pos = prop->GetTransform().position;
    std::unique_ptr<VirtualReferencePoint3D> ref_point = std::make_unique<VirtualReferencePoint3D>(
        ref_point_name,
        REF_POINT_CUSTOM,
        prop_pos.x,
        prop_pos.y,
        prop_pos.z
    );
    ref_point->SetDisplayColor(0xFFAA00);
    ref_point->SetVisible(false);

    const int ref_point_index = (int)reference_points.size();
    reference_points.push_back(std::move(ref_point));
    prop->SetReferencePointIndex(ref_point_index);

    if(ref_point_index >= 0 && ref_point_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_pt = reference_points[ref_point_index].get();
        if(ref_pt)
        {
            ref_pt->GetTransform().rotation = prop->GetTransform().rotation;
        }
    }

    scene_props_.push_back(std::move(prop));
    SceneProp3D* created = scene_props_.back().get();
    const int new_prop_id = created ? created->GetId() : -1;

    current_scene_prop_index_ = (int)scene_props_.size() - 1;
    SetLayoutDirty();
    UpdateScenePropsList();
    NotifyScenePropChanged();
    UpdateAvailableControllersList();
    UpdateReferencePointsList();

    if(new_prop_id >= 0)
    {
        SelectAvailableControllerEntry(-4, new_prop_id);
    }

    current_scene_prop_index_ = (int)scene_props_.size() - 1;
    if(scenePropsList() && current_scene_prop_index_ >= 0)
    {
        QSignalBlocker block(scenePropsList());
        scenePropsList()->setCurrentRow(current_scene_prop_index_);
    }
    if(SceneProp3D* selected = GetSelectedSceneProp())
    {
        SyncScenePropControls(selected);
    }
}

void OpenRGB3DSpatialTab::editScenePropClicked()
{
    SceneProp3D* prop = GetSelectedSceneProp();
    if(!prop)
    {
        QMessageBox::information(this, tr("Edit Scene Prop"), tr("Select a scene prop first."));
        return;
    }

    ScenePropDialog dialog(this);
    dialog.setEditMode();
    dialog.loadFrom(*prop);
    if(dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    dialog.applyTo(prop);

    const int linked_ref = prop->GetReferencePointIndex();
    if(linked_ref >= 0 && linked_ref < (int)reference_points.size() && reference_points[linked_ref])
    {
        reference_points[linked_ref]->SetName(prop->GetName() + " Reference");
    }

    UpdateScenePropsList();
    UpdateAvailableControllersList();
    NotifyScenePropChanged();
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::removeScenePropClicked()
{
    int prop_index = current_scene_prop_index_;
    if(scenePropsList())
    {
        const int list_row = scenePropsList()->currentRow();
        if(list_row >= 0)
        {
            prop_index = list_row;
        }
    }
    if(prop_index < 0 || prop_index >= (int)scene_props_.size())
    {
        return;
    }
    if(QMessageBox::question(this,
                             tr("Remove Scene Prop"),
                             tr("Remove the selected scene prop?"),
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)
       != QMessageBox::Yes)
    {
        return;
    }

    current_scene_prop_index_ = prop_index;
    const int removed_prop_id = scene_props_[(size_t)prop_index]->GetId();

    SceneProp3D* prop_to_remove = scene_props_[(size_t)prop_index].get();
    if(prop_to_remove)
    {
        const int ref_point_index = prop_to_remove->GetReferencePointIndex();
        if(ref_point_index >= 0 && ref_point_index < (int)reference_points.size())
        {
            for(size_t i = 0; i < scene_props_.size(); i++)
            {
                if(!scene_props_[i])
                {
                    continue;
                }
                int prop_ref_idx = scene_props_[i]->GetReferencePointIndex();
                if(prop_ref_idx == ref_point_index)
                {
                    scene_props_[i]->SetReferencePointIndex(-1);
                }
                else if(prop_ref_idx > ref_point_index)
                {
                    scene_props_[i]->SetReferencePointIndex(prop_ref_idx - 1);
                }
            }
            for(size_t i = 0; i < display_planes.size(); i++)
            {
                if(!display_planes[i])
                {
                    continue;
                }
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
            for(size_t i = 0; i < controller_transforms.size(); i++)
            {
                ControllerTransform* ct = controller_transforms[i].get();
                if(!ct)
                {
                    continue;
                }
                if(ct->linked_reference_point_index == ref_point_index)
                {
                    ct->linked_reference_point_index = -1;
                }
                else if(ct->linked_reference_point_index > ref_point_index)
                {
                    ct->linked_reference_point_index -= 1;
                }
            }
            RemoveReferencePointControllerEntries(ref_point_index);
            reference_points.erase(reference_points.begin() + ref_point_index);
            UpdateReferencePointsList();
        }
    }

    scene_props_.erase(scene_props_.begin() + prop_index);
    SetLayoutDirty();

    if(current_scene_prop_index_ >= (int)scene_props_.size())
    {
        current_scene_prop_index_ = (int)scene_props_.size() - 1;
    }
    if(current_scene_prop_index_ < 0 && !scene_props_.empty())
    {
        current_scene_prop_index_ = 0;
    }

    RemoveScenePropControllerEntries(removed_prop_id);
    UpdateScenePropsList();
    NotifyScenePropChanged();
    emit GridLayoutChanged();
    UpdateAvailableControllersList();
}

void OpenRGB3DSpatialTab::SetScenePropVisibleInScene(SceneProp3D* prop, bool visible)
{
    if(!prop)
    {
        return;
    }

    prop->SetVisible(visible);
    const int prop_id = prop->GetId();
    if(visible)
    {
        bool has_entry = false;
        for(int row = 0; row < scene_controllers_.count(); row++)
        {
            if(!scene_controllers_.hasUserRole(row))
            {
                continue;
            }
            const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(row);
            if(metadata.first == -4 && metadata.second == prop_id)
            {
                has_entry = true;
                break;
            }
        }
        if(!has_entry)
        {
            scene_controllers_.append(QString("[Prop] ") + QString::fromStdString(prop->GetName()),
                                      qMakePair(-4, prop_id));
        }
    }
    else
    {
        RemoveScenePropControllerEntries(prop_id);
    }

    const int linked_ref_idx = prop->GetReferencePointIndex();
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
                for(size_t i = 0; i < scene_props_.size(); i++)
                {
                    SceneProp3D* other = scene_props_[i].get();
                    if(!other || other == prop)
                    {
                        continue;
                    }
                    if(other->IsVisible() && other->GetReferencePointIndex() == linked_ref_idx)
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
    NotifyScenePropChanged();
    emit GridLayoutChanged();
    UpdateAvailableControllersList();
    RefreshHiddenControllerStates();
}

void OpenRGB3DSpatialTab::scenePropPositionSignal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        return;
    }
    if(index >= (int)scene_props_.size())
    {
        return;
    }

    current_scene_prop_index_ = index;
    if(scenePropsList())
    {
        QSignalBlocker block(scenePropsList());
        scenePropsList()->setCurrentRow(index);
    }
    scene_controllers_.clearSelection();
    if(referencePointsList())
    {
        QSignalBlocker block(referencePointsList());
        referencePointsList()->clearSelection();
    }

    SceneProp3D* prop = scene_props_[(size_t)index].get();
    if(!prop)
    {
        return;
    }

    Transform3D& transform = prop->GetTransform();
    transform.position.x = x;
    transform.position.y = y;
    transform.position.z = z;
    SetLayoutDirty();

    const int ref_index = prop->GetReferencePointIndex();
    if(ref_index >= 0 && ref_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
        if(ref_point)
        {
            ref_point->SetPosition({x, y, z});
        }
    }

    SyncScenePropControls(prop);
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::scenePropRotationSignal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        return;
    }
    if(index >= (int)scene_props_.size())
    {
        return;
    }

    current_scene_prop_index_ = index;
    if(scenePropsList())
    {
        QSignalBlocker block(scenePropsList());
        scenePropsList()->setCurrentRow(index);
    }
    scene_controllers_.clearSelection();
    if(referencePointsList())
    {
        QSignalBlocker block(referencePointsList());
        referencePointsList()->clearSelection();
    }

    SceneProp3D* prop = scene_props_[(size_t)index].get();
    if(!prop)
    {
        return;
    }

    Transform3D& transform = prop->GetTransform();
    transform.rotation.x = x;
    transform.rotation.y = y;
    transform.rotation.z = z;
    SetLayoutDirty();

    const int ref_index = prop->GetReferencePointIndex();
    if(ref_index >= 0 && ref_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
        if(ref_point)
        {
            Rotation3D ref_rot = {x, y, z};
            ref_point->GetTransform().rotation = ref_rot;
        }
    }

    SyncScenePropControls(prop);
    emit GridLayoutChanged();
}
