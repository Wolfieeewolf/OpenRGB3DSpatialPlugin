// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "SceneObjectEditHostPanel.h"
#include "SceneTransformPanel.h"
#include "VirtualController3D.h"
#include "ui_OpenRGB3DSpatialTab.h"

#include <QSignalBlocker>
#include <QStackedWidget>

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

    return viewport->GetSelectedDisplayPlaneIndex() >= 0;
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
