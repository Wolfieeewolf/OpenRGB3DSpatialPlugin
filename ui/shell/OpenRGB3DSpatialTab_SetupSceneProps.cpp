// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ScenePropDialog.h"
#include "SceneProp3D.h"
#include "ScenePropManager.h"
#include "GridSpaceUtils.h"
#include "ObjectCreatorTabPanel.h"
#include "LEDViewport3D.h"
#include "SpatialControllerCardList.h"
#include "SpatialEffectTypes.h"
#include "SceneObjectEditHostPanel.h"
#include "SceneTransformPanel.h"
#include "VirtualController3D.h"
#include "ui_OpenRGB3DSpatialTab.h"

#include <QListWidget>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <cmath>

namespace
{
QString ScenePropListLabel(const SceneProp3D& prop)
{
    const QString dims = QStringLiteral("%1 × %2 × %3 mm")
        .arg(prop.GetWidthMM(), 0, 'f', 0)
        .arg(prop.GetHeightMM(), 0, 'f', 0)
        .arg(prop.GetDepthMM(), 0, 'f', 0);
    if(prop.GetShape() == ScenePropShape::Mesh)
    {
        return QStringLiteral("%1 [mesh] (%2)")
            .arg(QString::fromStdString(prop.GetName()))
            .arg(dims);
    }
    return QStringLiteral("%1 (%2)")
        .arg(QString::fromStdString(prop.GetName()))
        .arg(dims);
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
    SyncScenePropManager();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::SyncScenePropManager()
{
    std::vector<SceneProp3D*> prop_ptrs;
    prop_ptrs.reserve(scene_props_.size());
    for(const std::unique_ptr<SceneProp3D>& prop : scene_props_)
    {
        if(prop)
        {
            prop_ptrs.push_back(prop.get());
        }
    }
    ScenePropManager::instance()->SetSceneProps(prop_ptrs);
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
    dialog.setResourceManager(resource_manager);
    dialog.setRoomSizeMm(
        roomWidthSpin() ? (float)roomWidthSpin()->value() : DEFAULT_ROOM_SIZE_MM,
        roomHeightSpin() ? (float)roomHeightSpin()->value() : DEFAULT_ROOM_SIZE_MM,
        roomDepthSpin() ? (float)roomDepthSpin()->value() : DEFAULT_ROOM_SIZE_MM);
    dialog.setCreateMode();
    dialog.setCreateDefaults(suggested, 200.0f, 450.0f, 450.0f, 0x555555u);
    const int dialog_result = dialog.exec();
    if(viewport)
    {
        viewport->makeCurrent();
        viewport->doneCurrent();
        viewport->update();
    }
    if(dialog_result != QDialog::Accepted)
    {
        return;
    }
    if(dialog.shape() == ScenePropShape::Mesh && dialog.meshAssetRelative().isEmpty())
    {
        QMessageBox::warning(this, tr("Create Scene Prop"),
                             tr("Import a mesh file before creating a mesh prop."));
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
    dialog.setResourceManager(resource_manager);
    dialog.setRoomSizeMm(
        roomWidthSpin() ? (float)roomWidthSpin()->value() : DEFAULT_ROOM_SIZE_MM,
        roomHeightSpin() ? (float)roomHeightSpin()->value() : DEFAULT_ROOM_SIZE_MM,
        roomDepthSpin() ? (float)roomDepthSpin()->value() : DEFAULT_ROOM_SIZE_MM);
    dialog.setEditMode();
    dialog.loadFrom(*prop);
    const int dialog_result = dialog.exec();
    if(viewport)
    {
        viewport->makeCurrent();
        viewport->doneCurrent();
        viewport->update();
    }
    if(dialog_result != QDialog::Accepted)
    {
        return;
    }
    if(dialog.shape() == ScenePropShape::Mesh && dialog.meshAssetRelative().isEmpty())
    {
        QMessageBox::warning(this, tr("Edit Scene Prop"),
                             tr("Import a mesh file for mesh props."));
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

namespace {

int settingsRightStackIndex(const Ui::OpenRGB3DSpatialTab* ui)
{
    if(!ui || !ui->rightStacked || !ui->settingsTabs)
    {
        return -1;
    }
    return ui->rightStacked->indexOf(ui->settingsTabs);
}

} // namespace

int OpenRGB3DSpatialTab::sceneObjectEditSceneRow() const
{
    if(ui && ui->sceneObjectEditHostPanel)
    {
        const int host_row = ui->sceneObjectEditHostPanel->sceneListRow();
        if(host_row >= 0)
        {
            return host_row;
        }
    }

    return sceneControllerCurrentRow();
}

void OpenRGB3DSpatialTab::InitSceneObjectEditTab()
{
    if(!ui || !ui->settingsTabs || !ui->sceneObjectEditTabScroll)
    {
        return;
    }

    if(scene_object_edit_tab_index_ < 0)
    {
        scene_object_edit_tab_index_ = ui->settingsTabs->indexOf(ui->sceneObjectEditTabScroll);
    }
}

bool OpenRGB3DSpatialTab::IsSceneObjectEditTabActive() const
{
    if(!ui || scene_object_edit_tab_index_ < 0)
    {
        return false;
    }

    const int settings_stack_idx = settingsRightStackIndex(ui);
    return settings_stack_idx >= 0 && ui->rightStacked->currentIndex() == settings_stack_idx
        && ui->settingsTabs->currentIndex() == scene_object_edit_tab_index_;
}

void OpenRGB3DSpatialTab::ShowSceneObjectEditPanel(int scene_list_row, bool sync_scene_selection)
{
    InitSceneObjectEditTab();

    if(!ui || !ui->settingsTabs || scene_object_edit_tab_index_ < 0)
    {
        return;
    }

    const int settings_stack_idx = settingsRightStackIndex(ui);
    if(settings_stack_idx < 0)
    {
        return;
    }

    if(sync_scene_selection && scene_list_row >= 0)
    {
        sceneControllerCardsSelectionChanged(scene_list_row);
    }
    else if(scene_list_row < 0)
    {
        scene_list_row = sceneControllerCurrentRow();
    }

    const bool already_showing_edit = IsSceneObjectEditTabActive();
    if(!already_showing_edit)
    {
        right_stack_before_edit_ = ui->rightStacked->currentIndex();
        if(ui->settingsTabs->currentIndex() != scene_object_edit_tab_index_)
        {
            settings_tab_before_edit_ = ui->settingsTabs->currentIndex();
        }
    }

    if(ui->rightStacked->currentIndex() != settings_stack_idx)
    {
        ui->rightStacked->setCurrentIndex(settings_stack_idx);
    }

    if(ui->settingsTabs->currentIndex() != scene_object_edit_tab_index_)
    {
        ui->settingsTabs->setCurrentIndex(scene_object_edit_tab_index_);
    }

    if(ui->sceneObjectEditHostPanel)
    {
        ui->sceneObjectEditHostPanel->syncFromSceneRow(scene_list_row);
    }
}

bool OpenRGB3DSpatialTab::HasSceneObjectEditTarget() const
{
    if(sceneControllerCurrentRow() >= 0)
    {
        return true;
    }

    if(!viewport)
    {
        return false;
    }

    if(viewport->GetSelectedControllerIndex() >= 0 || !viewport->GetSelectedControllers().empty())
    {
        return true;
    }

    if(viewport->GetSelectedReferencePointIndex() >= 0)
    {
        return true;
    }

    if(viewport->GetSelectedDisplayPlaneIndex() >= 0)
    {
        return true;
    }

    return viewport->GetSelectedScenePropIndex() >= 0;
}

void OpenRGB3DSpatialTab::MaybeHideSceneObjectEditOnDeselect()
{
    if(!IsSceneObjectEditTabActive() || HasSceneObjectEditTarget())
    {
        return;
    }

    HideSceneObjectEditPanel();
}

void OpenRGB3DSpatialTab::HideSceneObjectEditPanel()
{
    if(!ui || !ui->settingsTabs || scene_object_edit_tab_index_ < 0)
    {
        return;
    }

    if(!IsSceneObjectEditTabActive())
    {
        return;
    }

    int settings_tab_back = settings_tab_before_edit_;
    if(settings_tab_back < 0 || settings_tab_back >= ui->settingsTabs->count()
       || settings_tab_back == scene_object_edit_tab_index_)
    {
        settings_tab_back = 0;
    }
    ui->settingsTabs->setCurrentIndex(settings_tab_back);
    settings_tab_before_edit_ = -1;

    if(ui->rightStacked && right_stack_before_edit_ >= 0
       && right_stack_before_edit_ < ui->rightStacked->count())
    {
        ui->rightStacked->setCurrentIndex(right_stack_before_edit_);
    }
    right_stack_before_edit_ = -1;
}

void OpenRGB3DSpatialTab::SyncSceneObjectSpacingPanel()
{
    if(!ui || !ui->sceneObjectEditHostPanel || !IsSceneObjectEditTabActive())
    {
        return;
    }

    const int row = sceneObjectEditSceneRow();
    if(row >= 0)
    {
        ui->sceneObjectEditHostPanel->syncFromSceneRow(row);
    }
}

bool OpenRGB3DSpatialTab::ControllerTransformHasVirtualController(int transform_index) const
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return false;
    }

    const ControllerTransform* ctrl = controller_transforms[transform_index].get();
    return ctrl && ctrl->virtual_controller != nullptr;
}

int OpenRGB3DSpatialTab::FindVirtualControllerLibraryIndex(const VirtualController3D* vc) const
{
    if(!vc)
    {
        return -1;
    }

    for(size_t i = 0; i < virtual_controllers.size(); i++)
    {
        if(virtual_controllers[i].get() == vc)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void OpenRGB3DSpatialTab::FocusObjectCreatorTab()
{
    if(!ui || !ui->settingsTabs || !ui->objectCreatorTabScroll)
    {
        return;
    }

    const int settings_stack_idx = settingsRightStackIndex(ui);
    if(settings_stack_idx >= 0 && ui->rightStacked->currentIndex() != settings_stack_idx)
    {
        ui->rightStacked->setCurrentIndex(settings_stack_idx);
    }

    const int object_creator_tab = ui->settingsTabs->indexOf(ui->objectCreatorTabScroll);
    if(object_creator_tab >= 0)
    {
        ui->settingsTabs->setCurrentIndex(object_creator_tab);
    }
}

void OpenRGB3DSpatialTab::EditCustomControllerForCurrentSceneSelection()
{
    const int scene_row = sceneObjectEditSceneRow();
    if(scene_row < 0 || sceneControllerRowHasUserRole(scene_row))
    {
        return;
    }

    const int transform_index = ControllerListRowToTransformIndex(scene_row);
    if(transform_index < 0 || !ControllerTransformHasVirtualController(transform_index))
    {
        return;
    }

    const ControllerTransform* ctrl = controller_transforms[transform_index].get();
    if(!ctrl || !ctrl->virtual_controller)
    {
        return;
    }

    const int library_row = FindVirtualControllerLibraryIndex(ctrl->virtual_controller);
    if(library_row < 0 || !customControllersList())
    {
        return;
    }

    FocusObjectCreatorTab();
    if(ui->objectCreatorTabPanel)
    {
        ui->objectCreatorTabPanel->showCustomControllerSection();
    }

    customControllersList()->setCurrentRow(library_row);
    editCustomControllerClicked();
}

void OpenRGB3DSpatialTab::EditReferencePointForCurrentSceneSelection()
{
    const int scene_row = sceneObjectEditSceneRow();
    if(scene_row < 0 || !sceneControllerRowHasUserRole(scene_row))
    {
        return;
    }

    const SpatialControllerEntryKey key = sceneControllerRowKey(scene_row);
    if(key.first != -2 || key.second < 0)
    {
        return;
    }

    EditReferencePointAtIndex(key.second);
}

void OpenRGB3DSpatialTab::EditDisplayPlaneForCurrentSceneSelection()
{
    int plane_index = -1;

    const int scene_row = sceneObjectEditSceneRow();
    if(scene_row >= 0 && sceneControllerRowHasUserRole(scene_row))
    {
        const SpatialControllerEntryKey key = sceneControllerRowKey(scene_row);
        if(key.first == -3)
        {
            plane_index = FindDisplayPlaneIndexById(key.second);
        }
    }

    if(plane_index < 0 && viewport)
    {
        plane_index = viewport->GetSelectedDisplayPlaneIndex();
    }

    if(plane_index < 0)
    {
        return;
    }

    if(!EditDisplayPlaneAtIndex(plane_index))
    {
        return;
    }

    if(displayPlanesList())
    {
        QSignalBlocker block(displayPlanesList());
        displayPlanesList()->setCurrentRow(plane_index);
    }
    current_display_plane_index = plane_index;
}

void OpenRGB3DSpatialTab::sceneCardEdit(int scene_list_row)
{
    ShowSceneObjectEditPanel(scene_list_row);
}

SceneTransformPanel* OpenRGB3DSpatialTab::sceneTransformPanelWidget() const
{
    return ui && ui->sceneObjectEditHostPanel ? ui->sceneObjectEditHostPanel->transformPanel() : nullptr;
}
