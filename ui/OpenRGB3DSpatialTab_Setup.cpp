// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "SceneObjectEditHostPanel.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include "ControllerDisplayUtils.h"
#include "SpatialTabLedHelpers.h"
#include "PluginSettingsPaths.h"
#include "SpatialControllerCardList.h"
#include "SpatialControllerEntryKey.h"
#include "DisplayPlaneManager.h"
#include "ScreenCaptureManager.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "PluginLog.h"
#include "CustomControllerDialog.h"
#include "PluginUiUtils.h"
#include "RGBController/RGBController.h"
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QListWidget>
#include <QVector3D>
#include <limits>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QPalette>
#include <QSignalBlocker>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace filesystem = std::filesystem;

void OpenRGB3DSpatialTab::SetObjectCreatorStatus(const QString& message, bool is_error)
{
    if(!objectCreatorStatusLabel())
    {
        return;
    }

    objectCreatorStatusLabel()->setText(message);
    QPalette pal = objectCreatorStatusLabel()->palette();
    QPalette app_palette = QApplication::palette(objectCreatorStatusLabel());
    pal.setColor(QPalette::WindowText, app_palette.color(is_error ? QPalette::BrightText : QPalette::Link));
    objectCreatorStatusLabel()->setPalette(pal);
}

void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager)
    {
        return;
    }

    UpdateAvailableControllersList();
    RebindCustomControllerDeviceMappings();
}

void OpenRGB3DSpatialTab::UpdateAvailableControllersList()
{
    if(!resource_manager)
    {
        return;
    }

    SpatialControllerEntryKey previous_metadata(std::numeric_limits<int>::min(), 0);
    QString                     previous_text;
    const int                   previous_row = available_controllers_.currentRow();
    if(previous_row >= 0 && previous_row < available_controllers_.count())
    {
        previous_metadata = available_controllers_.userRoleAt(previous_row);
        previous_text     = available_controllers_.textAt(previous_row);
    }

    available_controllers_.clear();

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();

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
            QString display_text = ControllerDisplay::FormatRgbControllerTitle(controllers[i]) +
                                   QString(" [%1 zones, %2 LEDs available]").arg(unassigned_zones).arg(unassigned_leds);
            available_controllers_.append(display_text, qMakePair(static_cast<int>(i), -1));
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

        available_controllers_.append(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()),
                                    qMakePair(-1, static_cast<int>(i)));
    }

    for(unsigned int i = 0; i < reference_points.size(); i++)
    {
        if(reference_points[i] && !reference_points[i]->IsVisible() && !IsDeviceLinkedReferencePoint(static_cast<int>(i)))
        {
            available_controllers_.append(QString("[Ref Point] ") + QString::fromStdString(reference_points[i]->GetName()),
                                        qMakePair(-2, static_cast<int>(i)));
        }
    }

    for(unsigned int i = 0; i < display_planes.size(); i++)
    {
        if(display_planes[i] && !display_planes[i]->IsVisible())
        {
            available_controllers_.append(QString("[Display] ") + QString::fromStdString(display_planes[i]->GetName()),
                                        qMakePair(-3, display_planes[i]->GetId()));
        }
    }

    UpdateCustomControllersList();

    bool selection_restored = false;
    if(previous_metadata.first != std::numeric_limits<int>::min())
    {
        int row = FindAvailableControllerRow(previous_metadata.first, previous_metadata.second);
        if(row >= 0)
        {
            available_controllers_.setCurrentRow(row);
            selection_restored = true;
        }
    }

    if(!selection_restored && !previous_text.isEmpty())
    {
        for(int row = 0; row < available_controllers_.count(); row++)
        {
            if(available_controllers_.textAt(row) == previous_text)
            {
                available_controllers_.setCurrentRow(row);
                selection_restored = true;
                break;
            }
        }
    }

    if(!selection_restored && available_controllers_.count() > 0)
    {
        available_controllers_.setCurrentRow(0);
    }

    RebuildAvailableControllerCards();
    RebuildSceneControllerCards();
}

void OpenRGB3DSpatialTab::RebuildAvailableControllerCards()
{
    if(availableControllerCards())
    {
        availableControllerCards()->rebuildAvailable(this);
    }
}

void OpenRGB3DSpatialTab::RebuildSceneControllerCards()
{
    if(sceneControllerCards())
    {
        sceneControllerCards()->rebuildInScene(this);
    }
}

QList<SpatialControllerEntryKey> OpenRGB3DSpatialTab::GetAvailableControllerKeys() const
{
    QList<SpatialControllerEntryKey> keys;
    keys.reserve(available_controllers_.count());
    for(int row = 0; row < available_controllers_.count(); row++)
    {
        keys.append(available_controllers_.userRoleAt(row));
    }
    return keys;
}

QList<QString> OpenRGB3DSpatialTab::GetAvailableControllerTitles() const
{
    QList<QString> titles;
    titles.reserve(available_controllers_.count());
    for(int row = 0; row < available_controllers_.count(); row++)
    {
        titles.append(available_controllers_.textAt(row));
    }
    return titles;
}

QList<bool> OpenRGB3DSpatialTab::GetAvailableControllerGranularityFlags() const
{
    QList<bool> flags;
    for(const SpatialControllerEntryKey& key : GetAvailableControllerKeys())
    {
        flags.append(key.first >= 0);
    }
    return flags;
}

void OpenRGB3DSpatialTab::LoadDefaultLedSpacingFromSettings()
{
    default_led_spacing_x_ = 10.0f;
    default_led_spacing_y_ = 0.0f;
    default_led_spacing_z_ = 0.0f;

    try
    {
        const nlohmann::json settings = GetPluginSettings();
        if(settings.contains("LEDSpacing"))
        {
            const nlohmann::json& s = settings["LEDSpacing"];
            default_led_spacing_x_ = (float)std::max(0.0, std::min(1000.0, (double)s.value("X", 10.0)));
            default_led_spacing_y_ = (float)std::max(0.0, std::min(1000.0, (double)s.value("Y", 0.0)));
            default_led_spacing_z_ = (float)std::max(0.0, std::min(1000.0, (double)s.value("Z", 0.0)));
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to load LED spacing settings: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::SaveDefaultLedSpacingToSettings()
{
    try
    {
        nlohmann::json settings = GetPluginSettings();
        settings["LEDSpacing"]["X"] = default_led_spacing_x_;
        settings["LEDSpacing"]["Y"] = default_led_spacing_y_;
        settings["LEDSpacing"]["Z"] = default_led_spacing_z_;
        SetPluginSettings(settings);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to save LED spacing settings: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::GetSuggestedSpacingForAvailableRgb(int controller_index,
                                                           float& x_mm,
                                                           float& y_mm,
                                                           float& z_mm) const
{
    const float fallback_x = SafeGridScaleMm(grid_scale_mm);
    x_mm                   = default_led_spacing_x_;
    y_mm                   = default_led_spacing_y_;
    z_mm                   = default_led_spacing_z_;
    if(x_mm < 0.001f)
    {
        x_mm = fallback_x;
    }

    if(controller_index < 0 || !resource_manager)
    {
        return;
    }

    if(available_rgb_spacing_draft_.contains(controller_index))
    {
        const QVector3D draft = available_rgb_spacing_draft_.value(controller_index);
        x_mm                    = draft.x();
        y_mm                    = draft.y();
        z_mm                    = draft.z();
        return;
    }

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();
    if(controller_index >= (int)controllers.size() || !controllers[controller_index])
    {
        return;
    }

    float canonical_x = x_mm;
    float canonical_y = y_mm;
    float canonical_z = z_mm;
    if(TryGetCanonicalPhysicalSpacing(controller_transforms, controllers[controller_index], canonical_x, canonical_y,
                                      canonical_z))
    {
        x_mm = canonical_x;
        y_mm = canonical_y;
        z_mm = canonical_z;
    }
}

void OpenRGB3DSpatialTab::RememberAvailableRgbSpacingDraft(int controller_index, float x_mm, float y_mm, float z_mm)
{
    if(controller_index < 0)
    {
        return;
    }

    available_rgb_spacing_draft_.insert(controller_index, QVector3D(x_mm, y_mm, z_mm));
}

bool OpenRGB3DSpatialTab::GetTransformLedSpacing(int transform_index, float& x_mm, float& y_mm, float& z_mm) const
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return false;
    }

    const ControllerTransform* transform = controller_transforms[transform_index].get();
    if(!transform)
    {
        return false;
    }

    x_mm = transform->led_spacing_mm_x;
    y_mm = transform->led_spacing_mm_y;
    z_mm = transform->led_spacing_mm_z;
    return true;
}

void OpenRGB3DSpatialTab::ApplyLedSpacingToTransform(int transform_index, float x_mm, float y_mm, float z_mm)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* ctrl = controller_transforms[transform_index].get();
    if(!ctrl)
    {
        return;
    }

    ctrl->led_spacing_mm_x = x_mm;
    ctrl->led_spacing_mm_y = y_mm;
    ctrl->led_spacing_mm_z = z_mm;

    if(ctrl->virtual_controller)
    {
        ctrl->virtual_controller->SetSpacing(x_mm, y_mm, z_mm);
        const int controller_index = IndexOfVirtualController(ctrl->virtual_controller);
        if(controller_index >= 0)
        {
            SaveCustomController(static_cast<unsigned int>(controller_index));
        }
    }

    RegenerateLEDPositions(ctrl);
    ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
    SyncControllerLinkedReferencePoint(transform_index);

    SetLayoutDirty();

    if(viewport)
    {
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }

    SyncSceneObjectSpacingPanel();
}

void OpenRGB3DSpatialTab::UpdateCustomControllersList()
{
    if(!customControllersList())
    {
        return;
    }

    int selected_row = customControllersList()->currentRow();
    customControllersList()->clear();

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        customControllersList()->addItem(QString::fromStdString(virtual_controllers[i]->GetName()));
    }

    if(customControllersEmptyLabel())
    {
        customControllersEmptyLabel()->setVisible(customControllersList()->count() == 0);
    }

    if(selected_row >= 0 && selected_row < customControllersList()->count())
    {
        customControllersList()->setCurrentRow(selected_row);
    }

    customControllerSelectionChanged(customControllersList()->currentRow());
}

void OpenRGB3DSpatialTab::customControllerSelectionChanged(int row)
{
    const bool has_selection = (row >= 0);
    if(editCustomControllerButton())   editCustomControllerButton()->setEnabled(has_selection);
    if(exportCustomControllerButton()) exportCustomControllerButton()->setEnabled(has_selection);
    if(deleteCustomControllerButton()) deleteCustomControllerButton()->setEnabled(has_selection);
}

int OpenRGB3DSpatialTab::FindAvailableControllerRow(int type_code, int object_index) const
{
    return available_controllers_.findByKey(type_code, object_index);
}

void OpenRGB3DSpatialTab::SelectAvailableControllerEntry(int type_code, int object_index)
{
    const SpatialControllerEntryKey key(type_code, object_index);

    if(availableControllerCards())
    {
        availableControllerCards()->setSelectedAvailableKey(key);
        UpdateAvailableItemCombo();
        return;
    }

    int row = FindAvailableControllerRow(type_code, object_index);
    if(row < 0)
    {
        return;
    }

    if(available_controllers_.currentRow() == row)
    {
        UpdateAvailableItemCombo();
        return;
    }

    available_controllers_.setCurrentRow(row);
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::UpdateDeviceList()
{
    LoadDevices();
}

void OpenRGB3DSpatialTab::viewportControllerSelected(int transform_index)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        controllerSelected(-1);
        return;
    }

    const int list_row = TransformIndexToControllerListRow(transform_index);
    if(list_row < 0)
    {
        return;
    }

    sceneControllerCardsSelectionChanged(list_row);
    ShowSceneObjectEditPanel(list_row, false);
}

void OpenRGB3DSpatialTab::viewportDisplayPlaneSelected(int plane_index)
{
    if(plane_index >= 0)
    {
        const int scene_row = FindSceneRowForDisplayPlane(plane_index);
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
    displayPlaneSelected(plane_index);
    if(plane_index < 0)
    {
        MaybeHideSceneObjectEditOnDeselect();
    }
}

void OpenRGB3DSpatialTab::controllerSelected(int index)
{
    if(index >= 0 && index < scene_controllers_.count() && scene_controllers_.hasUserRole(index))
    {
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(index);
        if(metadata.first == -2)
        {
            if(sceneControllerCards())
            {
                sceneControllerCards()->setSelectedSceneRow(index);
            }

            if(referencePointsList())
                {
                    QSignalBlocker block(referencePointsList());
                    const int list_row = FindReferencePointListRow(metadata.second);
                    if(list_row >= 0)
                    {
                        referencePointsList()->setCurrentRow(list_row);
                    }
                }
                refPointSelected(metadata.second, true);

                if(displayPlanesList())
                {
                    QSignalBlocker block(displayPlanesList());
                    displayPlanesList()->clearSelection();
                }
                current_display_plane_index = -1;
                if(viewport) viewport->SelectDisplayPlane(-1);
            return;
        }
        if(metadata.first == -3)
        {
                int plane_index = FindDisplayPlaneIndexById(metadata.second);
                if(plane_index >= 0 && plane_index < (int)display_planes.size())
                {
                    if(sceneControllerCards())
                    {
                        sceneControllerCards()->setSelectedSceneRow(index);
                    }

                    current_display_plane_index = plane_index;
                    if(displayPlanesList())
                    {
                        QSignalBlocker block(displayPlanesList());
                        displayPlanesList()->setCurrentRow(plane_index);
                    }
                    if(referencePointsList())
                    {
                        QSignalBlocker block(referencePointsList());
                        referencePointsList()->clearSelection();
                    }
                    DisplayPlane3D* plane = display_planes[plane_index].get();
                    SyncDisplayPlaneControls(plane);
                    RefreshDisplayPlaneDetails();
                    if(viewport) viewport->SelectDisplayPlane(plane_index);
                    if(posXSpin()) posXSpin()->setEnabled(true);
                    if(posYSpin()) posYSpin()->setEnabled(true);
                    if(posZSpin()) posZSpin()->setEnabled(true);
                    if(posXSlider()) posXSlider()->setEnabled(true);
                    if(posYSlider()) posYSlider()->setEnabled(true);
                    if(posZSlider()) posZSlider()->setEnabled(true);
                    if(rotXSpin()) rotXSpin()->setEnabled(true);
                    if(rotYSpin()) rotYSpin()->setEnabled(true);
                    if(rotZSpin()) rotZSpin()->setEnabled(true);
                    if(rotXSlider()) rotXSlider()->setEnabled(true);
                    if(rotYSlider()) rotYSlider()->setEnabled(true);
                    if(rotZSlider()) rotZSlider()->setEnabled(true);
            }
            return;
        }
    }

    if(displayPlanesList())
    {
        QSignalBlocker block(displayPlanesList());
        displayPlanesList()->clearSelection();
    }
    current_display_plane_index = -1;
    if(viewport) viewport->SelectDisplayPlane(-1);

    int transform_index = index;
    if(index >= 0 && index < scene_controllers_.count() && !scene_controllers_.hasUserRole(index))
    {
        transform_index = ControllerListRowToTransformIndex(index);
    }

    if(transform_index >= 0 && transform_index < (int)controller_transforms.size())
    {
        if(viewport)
        {
            viewport->SelectController(transform_index);
        }

        int list_row = TransformIndexToControllerListRow(transform_index);
        if(list_row >= 0)
        {
            scene_controllers_.setCurrentRow(list_row);
        }
        if(sceneControllerCards() && list_row >= 0)
        {
            sceneControllerCards()->setSelectedSceneRow(list_row);
        }

        ControllerTransform* ctrl = controller_transforms[transform_index].get();

        bool has_transform_ui = posXSpin() && posYSpin() && posZSpin() && rotXSpin() && rotYSpin() && rotZSpin()
            && posXSlider() && posYSlider() && posZSlider() && rotXSlider() && rotYSlider() && rotZSlider();

        if(has_transform_ui)
        {
            posXSpin()->blockSignals(true);
            posYSpin()->blockSignals(true);
            posZSpin()->blockSignals(true);
            rotXSpin()->blockSignals(true);
            rotYSpin()->blockSignals(true);
            rotZSpin()->blockSignals(true);
            posXSlider()->blockSignals(true);
            posYSlider()->blockSignals(true);
            posZSlider()->blockSignals(true);
            rotXSlider()->blockSignals(true);
            rotYSlider()->blockSignals(true);
            rotZSlider()->blockSignals(true);

            const float scale_mm = static_cast<float>(EffectiveGridScaleMm());
            const double pos_x_mm = static_cast<double>(GridUnitsToMM(ctrl->transform.position.x, scale_mm));
            const double pos_y_mm = static_cast<double>(GridUnitsToMM(ctrl->transform.position.y, scale_mm));
            const double pos_z_mm = static_cast<double>(GridUnitsToMM(ctrl->transform.position.z, scale_mm));
            SetScenePositionControlsMm(pos_x_mm, pos_y_mm, pos_z_mm);
            rotXSpin()->setValue(ctrl->transform.rotation.x);
            rotYSpin()->setValue(ctrl->transform.rotation.y);
            rotZSpin()->setValue(ctrl->transform.rotation.z);

            rotXSlider()->setValue((int)(ctrl->transform.rotation.x));
            rotYSlider()->setValue((int)(ctrl->transform.rotation.y));
            rotZSlider()->setValue((int)(ctrl->transform.rotation.z));

            posXSpin()->blockSignals(false);
            posYSpin()->blockSignals(false);
            posZSpin()->blockSignals(false);
            rotXSpin()->blockSignals(false);
            rotYSpin()->blockSignals(false);
            rotZSpin()->blockSignals(false);
            posXSlider()->blockSignals(false);
            posYSlider()->blockSignals(false);
            posZSlider()->blockSignals(false);
            rotXSlider()->blockSignals(false);
            rotYSlider()->blockSignals(false);
            rotZSlider()->blockSignals(false);

            posXSpin()->setEnabled(true);
            posYSpin()->setEnabled(true);
            posZSpin()->setEnabled(true);
            posXSlider()->setEnabled(true);
            posYSlider()->setEnabled(true);
            posZSlider()->setEnabled(true);
            rotXSpin()->setEnabled(true);
            rotYSpin()->setEnabled(true);
            rotZSpin()->setEnabled(true);
            rotXSlider()->setEnabled(true);
            rotYSlider()->setEnabled(true);
            rotZSlider()->setEnabled(true);
        }
        else
        {
            if(posXSpin()) posXSpin()->setEnabled(true);
            if(posYSpin()) posYSpin()->setEnabled(true);
            if(posZSpin()) posZSpin()->setEnabled(true);
            if(posXSlider()) posXSlider()->setEnabled(true);
            if(posYSlider()) posYSlider()->setEnabled(true);
            if(posZSlider()) posZSlider()->setEnabled(true);
            if(rotXSpin()) rotXSpin()->setEnabled(true);
            if(rotYSpin()) rotYSpin()->setEnabled(true);
            if(rotZSpin()) rotZSpin()->setEnabled(true);
            if(rotXSlider()) rotXSlider()->setEnabled(true);
            if(rotYSlider()) rotYSlider()->setEnabled(true);
            if(rotZSlider()) rotZSlider()->setEnabled(true);
        }

        if(referencePointsList())
        {
            referencePointsList()->blockSignals(true);
            referencePointsList()->clearSelection();
            referencePointsList()->blockSignals(false);
        }

        if(posXSpin()) posXSpin()->setEnabled(true);
        if(posYSpin()) posYSpin()->setEnabled(true);
        if(posZSpin()) posZSpin()->setEnabled(true);
        if(posXSlider()) posXSlider()->setEnabled(true);
        if(posYSlider()) posYSlider()->setEnabled(true);
        if(posZSlider()) posZSlider()->setEnabled(true);
        if(rotXSpin()) rotXSpin()->setEnabled(true);
        if(rotYSpin()) rotYSpin()->setEnabled(true);
        if(rotZSpin()) rotZSpin()->setEnabled(true);
        if(rotXSlider()) rotXSlider()->setEnabled(true);
        if(rotYSlider()) rotYSlider()->setEnabled(true);
        if(rotZSlider()) rotZSlider()->setEnabled(true);
    }
    else if(transform_index == -1 || index == -1)
    {
        scene_controllers_.clearSelection();
        if(sceneControllerCards())
        {
            sceneControllerCards()->setSelectedSceneRow(-1, false);
        }

        if(posXSpin()) posXSpin()->setEnabled(false);
        if(posYSpin()) posYSpin()->setEnabled(false);
        if(posZSpin()) posZSpin()->setEnabled(false);
        if(rotXSpin()) rotXSpin()->setEnabled(false);
        if(rotYSpin()) rotYSpin()->setEnabled(false);
        if(rotZSpin()) rotZSpin()->setEnabled(false);
        if(posXSlider()) posXSlider()->setEnabled(false);
        if(posYSlider()) posYSlider()->setEnabled(false);
        if(posZSlider()) posZSlider()->setEnabled(false);
        if(rotXSlider()) rotXSlider()->setEnabled(false);
        if(rotYSlider()) rotYSlider()->setEnabled(false);
        if(rotZSlider()) rotZSlider()->setEnabled(false);

        MaybeHideSceneObjectEditOnDeselect();
    }

    UpdateSelectionInfo();
    RefreshDisplayPlaneDetails();
}

void OpenRGB3DSpatialTab::controllerPositionChanged(int index, float x, float y, float z)
{
    if(viewport && viewport->IsGizmoDragging())
    {
        return;
    }

    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
        ctrl->transform.position.x = x;
        ctrl->transform.position.y = y;
        ctrl->transform.position.z = z;
        ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
        ControllerLayout3D::UpdateWorldPositions(ctrl);
        SyncControllerLinkedReferencePoint(index);
        SetLayoutDirty();

        posXSpin()->blockSignals(true);
        posYSpin()->blockSignals(true);
        posZSpin()->blockSignals(true);
        posXSlider()->blockSignals(true);
        posYSlider()->blockSignals(true);
        posZSlider()->blockSignals(true);

        const float scale_mm = static_cast<float>(EffectiveGridScaleMm());
        const double x_mm = static_cast<double>(GridUnitsToMM(x, scale_mm));
        const double y_mm = static_cast<double>(GridUnitsToMM(y, scale_mm));
        const double z_mm = static_cast<double>(GridUnitsToMM(z, scale_mm));
        SetScenePositionControlsMm(x_mm, y_mm, z_mm);

        posXSpin()->blockSignals(false);
        posYSpin()->blockSignals(false);
        posZSpin()->blockSignals(false);
        posXSlider()->blockSignals(false);
        posYSlider()->blockSignals(false);
        posZSlider()->blockSignals(false);

        if(effect_running)
        {
            RenderEffectStack();
        }
    }
}

void OpenRGB3DSpatialTab::controllerRotationChanged(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
        ctrl->transform.rotation.x = x;
        ctrl->transform.rotation.y = y;
        ctrl->transform.rotation.z = z;
        ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
        ControllerLayout3D::UpdateWorldPositions(ctrl);
        SyncControllerLinkedReferencePoint(index);
        SetLayoutDirty();

        rotXSpin()->blockSignals(true);
        rotYSpin()->blockSignals(true);
        rotZSpin()->blockSignals(true);
        rotXSlider()->blockSignals(true);
        rotYSlider()->blockSignals(true);
        rotZSlider()->blockSignals(true);

        rotXSpin()->setValue(x);
        rotYSpin()->setValue(y);
        rotZSpin()->setValue(z);

        rotXSlider()->setValue((int)x);
        rotYSlider()->setValue((int)y);
        rotZSlider()->setValue((int)z);

        rotXSpin()->blockSignals(false);
        rotYSpin()->blockSignals(false);
        rotZSpin()->blockSignals(false);
        rotXSlider()->blockSignals(false);
        rotYSlider()->blockSignals(false);
        rotZSlider()->blockSignals(false);

        if(effect_running)
        {
            RenderEffectStack();
        }
    }
}

void OpenRGB3DSpatialTab::UpdateAvailableItemCombo()
{
    if(availableControllerCards())
    {
        availableControllerCards()->refreshAvailableFromHost();
    }
}

void OpenRGB3DSpatialTab::PopulateAvailableItemCombo(const SpatialControllerEntryKey& key, int granularity, QComboBox* combo)
{
    if(!combo || !resource_manager)
    {
        return;
    }

    combo->clear();

    const int type_code     = key.first;
    const int object_index  = key.second;

    if(type_code == -2)
    {
        combo->addItem(tr("Whole object"), QVariant::fromValue(qMakePair(-2, object_index)));
        return;
    }
    if(type_code == -3)
    {
        const int plane_index = FindDisplayPlaneIndexById(object_index);
        if(plane_index >= 0)
        {
            combo->addItem(tr("Whole object"),
                           QVariant::fromValue(qMakePair(-3, display_planes[plane_index]->GetId())));
        }
        return;
    }
    if(type_code == -1)
    {
        combo->addItem(tr("Whole device"), QVariant::fromValue(qMakePair(-1, object_index)));
        return;
    }
    if(type_code < 0)
    {
        return;
    }

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();
    if(type_code >= (int)controllers.size() || !controllers[type_code])
    {
        return;
    }

    RGBControllerInterface* controller = controllers[type_code];
    if(granularity == 0)
    {
        if(!IsItemInScene(controller, granularity, 0))
        {
            combo->addItem(ControllerDisplay::FormatRgbControllerTitle(controller),
                           QVariant::fromValue(qMakePair(type_code, 0)));
        }
    }
    else if(granularity == 1)
    {
        for(unsigned int i = 0; i < controller->GetZoneCount(); i++)
        {
            if(!IsItemInScene(controller, granularity, (int)i))
            {
                combo->addItem(QString::fromStdString(controller->GetZoneName(i)),
                               QVariant::fromValue(qMakePair(type_code, (int)i)));
            }
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int i = 0; i < controller->GetLEDCount(); i++)
        {
            if(!IsItemInScene(controller, granularity, (int)i))
            {
                combo->addItem(QString::fromStdString(controller->GetLEDName(i)),
                               QVariant::fromValue(qMakePair(type_code, (int)i)));
            }
        }
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
    ctrl_transform->led_spacing_mm_x = virtual_ctrl->GetSpacingX();
    ctrl_transform->led_spacing_mm_y = virtual_ctrl->GetSpacingY();
    ctrl_transform->led_spacing_mm_z = virtual_ctrl->GetSpacingZ();
    ctrl_transform->granularity = -1;
    ctrl_transform->item_idx = -1;
    ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
    ControllerLayout3D::MarkWorldPositionsDirty(ctrl_transform.get());
    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());
    controller_transforms.push_back(std::move(ctrl_transform));
    int new_transform_index = (int)controller_transforms.size() - 1;
    QString name = QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName());
    scene_controllers_.append(name);
    scene_controllers_.setCurrentRow(scene_controllers_.count() - 1);

    EnsureControllerLinkedReferencePoint(new_transform_index, name);
    if(viewport)
    {
        viewport->SelectController((int)controller_transforms.size() - 1);
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
    }
    SetLayoutDirty();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
    RefreshHiddenControllerStates();
    return true;
}

int OpenRGB3DSpatialTab::CreateControllerLinkedReferencePoint(int transform_index, const QString& base_name)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return -1;
    }

    ControllerTransform* transform = controller_transforms[transform_index].get();
    if(!transform)
    {
        return -1;
    }

    const std::string ref_name = base_name.toStdString() + " Reference";

    std::unique_ptr<VirtualReferencePoint3D> ref_point = std::make_unique<VirtualReferencePoint3D>(
        ref_name,
        REF_POINT_CUSTOM,
        0.0f,
        0.0f,
        0.0f
    );
    ref_point->SetDisplayColor(0x00A0FF);
    ref_point->SetVisible(false);

    int ref_index = (int)reference_points.size();
    reference_points.push_back(std::move(ref_point));
    transform->linked_reference_point_index = ref_index;
    SyncControllerLinkedReferencePoint(transform_index);
    UpdateReferencePointsList();
    return ref_index;
}

bool OpenRGB3DSpatialTab::IsDeviceLinkedReferencePoint(int ref_index) const
{
    if(ref_index < 0)
    {
        return false;
    }

    for(const std::unique_ptr<ControllerTransform>& transform : controller_transforms)
    {
        if(transform && transform->linked_reference_point_index == ref_index)
        {
            return true;
        }
    }

    for(const std::unique_ptr<DisplayPlane3D>& plane : display_planes)
    {
        if(plane && plane->GetReferencePointIndex() == ref_index)
        {
            return true;
        }
    }

    return false;
}

int OpenRGB3DSpatialTab::ReferencePointIndexFromListRow(int list_row) const
{
    if(!referencePointsList() || list_row < 0 || list_row >= referencePointsList()->count())
    {
        return -1;
    }

    QListWidgetItem* item = referencePointsList()->item(list_row);
    if(!item)
    {
        return -1;
    }

    bool ok = false;
    const int ref_index = item->data(Qt::UserRole).toInt(&ok);
    return ok ? ref_index : -1;
}

int OpenRGB3DSpatialTab::FindReferencePointListRow(int ref_index) const
{
    if(!referencePointsList() || ref_index < 0)
    {
        return -1;
    }

    for(int row = 0; row < referencePointsList()->count(); row++)
    {
        if(ReferencePointIndexFromListRow(row) == ref_index)
        {
            return row;
        }
    }

    return -1;
}

void OpenRGB3DSpatialTab::EnsureControllerLinkedReferencePoint(int transform_index, const QString& base_name)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* transform = controller_transforms[transform_index].get();
    if(!transform)
    {
        return;
    }

    if(transform->linked_reference_point_index < 0 ||
       transform->linked_reference_point_index >= (int)reference_points.size())
    {
        CreateControllerLinkedReferencePoint(transform_index, base_name);
    }
    else
    {
        SyncControllerLinkedReferencePoint(transform_index);
    }

    const int ref_index = transform->linked_reference_point_index;
    if(ref_index >= 0 && ref_index < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
        if(ref_point)
        {
            ref_point->SetVisible(true);
        }
    }
}

void OpenRGB3DSpatialTab::SyncControllerLinkedReferencePoint(int transform_index)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* transform = controller_transforms[transform_index].get();
    if(!transform)
    {
        return;
    }

    const int ref_index = transform->linked_reference_point_index;
    if(ref_index < 0 || ref_index >= (int)reference_points.size())
    {
        return;
    }

    VirtualReferencePoint3D* ref_point = reference_points[ref_index].get();
    if(!ref_point)
    {
        return;
    }

    ControllerLayout3D::UpdateWorldPositions(transform);
    ref_point->SetPosition(ControllerLayout3D::GetControllerCenterWorld(transform));
}

void OpenRGB3DSpatialTab::SyncAllControllerLinkedReferencePoints()
{
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        SyncControllerLinkedReferencePoint(static_cast<int>(i));
    }
}

void OpenRGB3DSpatialTab::RemoveControllerLinkedReferencePoint(int transform_index)
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* removed_transform = controller_transforms[transform_index].get();
    if(!removed_transform)
    {
        return;
    }

    const int removed_ref_index = removed_transform->linked_reference_point_index;
    if(removed_ref_index < 0 || removed_ref_index >= (int)reference_points.size())
    {
        return;
    }

    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(!ct)
        {
            continue;
        }
        if(ct->linked_reference_point_index == removed_ref_index)
        {
            ct->linked_reference_point_index = -1;
        }
        else if(ct->linked_reference_point_index > removed_ref_index)
        {
            ct->linked_reference_point_index -= 1;
        }
    }

    for(size_t i = 0; i < display_planes.size(); i++)
    {
        DisplayPlane3D* plane = display_planes[i].get();
        if(!plane)
        {
            continue;
        }
        int plane_ref_idx = plane->GetReferencePointIndex();
        if(plane_ref_idx == removed_ref_index)
        {
            plane->SetReferencePointIndex(-1);
        }
        else if(plane_ref_idx > removed_ref_index)
        {
            plane->SetReferencePointIndex(plane_ref_idx - 1);
        }
    }

    RemoveReferencePointControllerEntries(removed_ref_index);
    reference_points.erase(reference_points.begin() + removed_ref_index);
    UpdateReferencePointsList();
}

void OpenRGB3DSpatialTab::availableCardAdd(SpatialControllerEntryKey key,
                                                int                            granularity,
                                                int                            item_index,
                                                float                          spacing_x_mm,
                                                float                          spacing_y_mm,
                                                float                          spacing_z_mm)
{
    const int ctrl_idx = key.first;
    const int item_row = (ctrl_idx >= 0) ? item_index : key.second;
    if(ctrl_idx >= 0)
    {
        RememberAvailableRgbSpacingDraft(ctrl_idx, spacing_x_mm, spacing_y_mm, spacing_z_mm);
    }
    AddControllerEntryToScene(ctrl_idx, granularity, item_row, spacing_x_mm, spacing_y_mm, spacing_z_mm, false);
}

void OpenRGB3DSpatialTab::sceneCardRemove(int scene_list_row)
{
    if(scene_list_row < 0 || scene_list_row >= scene_controllers_.count())
    {
        return;
    }
    scene_controllers_.setCurrentRow(scene_list_row);
    removeControllerClicked();
}

void OpenRGB3DSpatialTab::AddControllerEntryToScene(int  ctrl_idx,
                                                    int  granularity,
                                                    int  item_row,
                                                    float spacing_x_mm,
                                                    float spacing_y_mm,
                                                    float spacing_z_mm,
                                                    bool  show_messages)
{
    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();

    if(ctrl_idx == -1)
    {
        if(AddCustomControllerToScene(item_row) && show_messages)
        {
            QMessageBox::information(this, tr("Custom controller added"),
                tr("Custom controller '%1' added to the 3D scene. You can now position and configure it.")
                    .arg(QString::fromStdString(virtual_controllers[item_row]->GetName())));
        }
        return;
    }

    if(ctrl_idx == -2)
    {
        if(item_row < 0 || item_row >= (int)reference_points.size() || IsDeviceLinkedReferencePoint(item_row))
        {
            return;
        }

        VirtualReferencePoint3D* ref_point = reference_points[item_row].get();
        ref_point->SetVisible(true);

        QString name = QString("[Ref Point] ") + QString::fromStdString(ref_point->GetName());
        scene_controllers_.append(name, qMakePair(-2, item_row));

        if(viewport) viewport->update();
        SetLayoutDirty();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();

        if(show_messages)
        {
            QMessageBox::information(this, tr("Reference point added"),
                                    tr("Reference point '%1' added to the 3D scene.")
                                        .arg(QString::fromStdString(ref_point->GetName())));
        }
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
        scene_controllers_.append(QString("[Display] ") + plane_name, qMakePair(-3, plane->GetId()));

        int linked_ref_idx = plane->GetReferencePointIndex();
        if(linked_ref_idx >= 0 && linked_ref_idx < (int)reference_points.size())
        {
            VirtualReferencePoint3D* ref_pt = reference_points[linked_ref_idx].get();
            if(ref_pt)
            {
                const Transform3D& pt = plane->GetTransform();
                ref_pt->SetPosition({pt.position.x, pt.position.y, pt.position.z});
                ref_pt->SetRotation({pt.rotation.x, pt.rotation.y, pt.rotation.z});
                ref_pt->SetVisible(true);
            }
        }

        if(viewport)
        {
            viewport->SelectDisplayPlane(plane_index);
            viewport->update();
        }
        SetLayoutDirty();
        NotifyDisplayPlaneChanged();
        emit GridLayoutChanged();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();

        if(show_messages)
        {
            QMessageBox::information(this, tr("Display plane added"),
                                    tr("Display plane '%1' added to the 3D scene.")
                                        .arg(plane_name));
        }
        RefreshHiddenControllerStates();
        return;
    }

    if(ctrl_idx < 0 || ctrl_idx >= (int)controllers.size())
    {
        return;
    }

    RGBControllerInterface* controller = controllers[ctrl_idx];

    std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
    ctrl_transform->controller = controller;
    ctrl_transform->virtual_controller = nullptr;
    ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f};
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};
    ctrl_transform->hidden_by_virtual = false;

    ctrl_transform->led_spacing_mm_x = spacing_x_mm;
    ctrl_transform->led_spacing_mm_y = spacing_y_mm;
    ctrl_transform->led_spacing_mm_z = spacing_z_mm;

    ctrl_transform->granularity = granularity;
    ctrl_transform->item_idx = item_row;

    QString name;

    if(granularity == 0)
    {
        ctrl_transform->led_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        name = QString("[Device] ") + ControllerDisplay::FormatRgbControllerTitle(controller);
    }
    else if(granularity == 1)
    {
        if(item_row < 0 || item_row >= (int)controller->GetZoneCount())
        {
            return;
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            if(all_positions[i].zone_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
            }
        }
        

        name = QString("[Zone] ") + ControllerDisplay::FormatRgbControllerTitle(controller) + " - "
               + QString::fromStdString(controller->GetZoneName((unsigned int)item_row));
    }
    else if(granularity == 2)
    {
        if(item_row < 0 || item_row >= (int)controller->GetLEDCount())
        {
            return;
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            unsigned int global_led_idx = 0;
            if(!TryGetObjectCreatorGlobalLedIndex(controller, all_positions[i].zone_idx, all_positions[i].led_idx, &global_led_idx))
            {
                continue;
            }
            if(global_led_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
                break;
            }
        }

        name = QString("[LED] ") + ControllerDisplay::FormatRgbControllerTitle(controller) + " - "
               + QString::fromStdString(controller->GetLEDName((unsigned int)item_row));
    }

    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

    ControllerLayout3D::MarkWorldPositionsDirty(ctrl_transform.get());
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

    controller_transforms.push_back(std::move(ctrl_transform));
    int new_transform_index = (int)controller_transforms.size() - 1;

    scene_controllers_.append(name);

    EnsureControllerLinkedReferencePoint(new_transform_index, name);

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

void OpenRGB3DSpatialTab::removeControllerClicked()
{
    int selected_row = scene_controllers_.currentRow();
    if(selected_row < 0 || selected_row >= scene_controllers_.count())
    {
        return;
    }

    if(scene_controllers_.hasUserRole(selected_row))
    {
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(selected_row);
        const int                     type_code    = metadata.first;
        const int                     object_index = metadata.second;

        if(type_code == -2)
        {
            if(IsDeviceLinkedReferencePoint(object_index))
            {
                return;
            }
            if(object_index >= 0 && object_index < (int)reference_points.size())
            {
                reference_points[object_index]->SetVisible(false);
            }
            if(viewport)
            {
                viewport->SelectReferencePoint(-1);
                viewport->update();
            }
            scene_controllers_.removeAt(selected_row);
            SetLayoutDirty();
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
                }

                if(current_display_plane_index == plane_index)
                {
                    current_display_plane_index = -1;
                    if(displayPlanesList())
                    {
                        QSignalBlocker block(displayPlanesList());
                        displayPlanesList()->setCurrentRow(-1);
                    }
                    RefreshDisplayPlaneDetails();
                }
            }
            scene_controllers_.removeAt(selected_row);
            if(viewport)
            {
                viewport->SelectDisplayPlane(-1);
                viewport->update();
            }
            SetLayoutDirty();
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

    RemoveControllerLinkedReferencePoint(transform_index);
    controller_transforms.erase(controller_transforms.begin() + transform_index);

    scene_controllers_.removeAt(selected_row);

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

void OpenRGB3DSpatialTab::removeControllerFromViewport(int index)
{
    if(index < 0 || index >= (int)controller_transforms.size())
    {
        return;
    }

    int list_row = TransformIndexToControllerListRow(index);
    RemoveControllerLinkedReferencePoint(index);
    controller_transforms.erase(controller_transforms.begin() + index);

    if(list_row >= 0)
    {
        scene_controllers_.removeAt(list_row);
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

void OpenRGB3DSpatialTab::clearAllClicked()
{
    for(int i = (int)controller_transforms.size() - 1; i >= 0; i--)
    {
        RemoveControllerLinkedReferencePoint(i);
    }

    for(unsigned int i = 0; i < reference_points.size(); i++)
    {
        if(reference_points[i] && !IsDeviceLinkedReferencePoint(static_cast<int>(i)))
        {
            reference_points[i]->SetVisible(false);
        }
    }
    for(unsigned int i = 0; i < display_planes.size(); i++)
    {
        display_planes[i]->SetVisible(false);
    }

    controller_transforms.clear();
    scene_controllers_.clear();

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

    controllerSelected(-1);
    refPointSelected(-1);
    displayPlaneSelected(-1);
    if(viewport)
    {
        viewport->ClearSelection();
    }
}

void OpenRGB3DSpatialTab::sceneControllerCardsSelectionChanged(int row)
{
    if(row >= 0 && row < scene_controllers_.count())
    {
        scene_controllers_.setCurrentRow(row);
    }
    else
    {
        scene_controllers_.clearSelection();
    }

    if(sceneControllerCards())
    {
        sceneControllerCards()->setSelectedSceneRow(row);
    }

    controllerSelected(row);

    if(ui->sceneObjectEditHostPanel && IsSceneObjectEditTabActive())
    {
        ui->sceneObjectEditHostPanel->syncFromSceneRow(row);
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

int OpenRGB3DSpatialTab::FindSceneRowForReferencePoint(int ref_index) const
{
    if(ref_index < 0)
    {
        return -1;
    }

    for(int row = 0; row < scene_controllers_.count(); row++)
    {
        if(!scene_controllers_.hasUserRole(row))
        {
            continue;
        }
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(row);
        if(metadata.first == -2 && metadata.second == ref_index)
        {
            return row;
        }
    }
    return -1;
}

int OpenRGB3DSpatialTab::FindSceneRowForDisplayPlane(int plane_index) const
{
    if(plane_index < 0 || plane_index >= (int)display_planes.size())
    {
        return -1;
    }

    const int plane_id = display_planes[plane_index]->GetId();
    for(int row = 0; row < scene_controllers_.count(); row++)
    {
        if(!scene_controllers_.hasUserRole(row))
        {
            continue;
        }
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(row);
        if(metadata.first == -3 && metadata.second == plane_id)
        {
            return row;
        }
    }
    return -1;
}

void OpenRGB3DSpatialTab::RemoveDisplayPlaneControllerEntries(int plane_id)
{
    for(int row = scene_controllers_.count() - 1; row >= 0; row--)
    {
        if(!scene_controllers_.hasUserRole(row))
        {
            continue;
        }
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(row);
        if(metadata.first == -3 && metadata.second == plane_id)
        {
            scene_controllers_.removeAt(row);
        }
    }
}

void OpenRGB3DSpatialTab::RemoveReferencePointControllerEntries(int removed_index)
{
    for(int row = scene_controllers_.count() - 1; row >= 0; row--)
    {
        if(!scene_controllers_.hasUserRole(row))
        {
            continue;
        }
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(row);
        if(metadata.first != -2)
        {
            continue;
        }
        const int ref_idx = metadata.second;
        if(ref_idx == removed_index)
        {
            scene_controllers_.removeAt(row);
        }
        else if(ref_idx > removed_index)
        {
            scene_controllers_.setUserRoleAt(row, qMakePair(-2, ref_idx - 1));
        }
    }
}

SpatialControllerEntryKey OpenRGB3DSpatialTab::sceneControllerRowKey(int row) const
{
    if(row < 0 || row >= scene_controllers_.count())
    {
        return {};
    }
    if(scene_controllers_.hasUserRole(row))
    {
        return scene_controllers_.userRoleAt(row);
    }
    return SpatialControllerEntryKey(0, row);
}

int OpenRGB3DSpatialTab::ControllerListRowToTransformIndex(int row) const
{
    if(row < 0 || row >= scene_controllers_.count() || scene_controllers_.hasUserRole(row))
    {
        return -1;
    }
    int transform_count = 0;
    for(int r = 0; r <= row; r++)
    {
        if(!scene_controllers_.hasUserRole(r))
        {
            if(r == row)
            {
                return transform_count;
            }
            transform_count++;
        }
    }
    return -1;
}

int OpenRGB3DSpatialTab::TransformIndexToControllerListRow(int transform_index) const
{
    if(transform_index < 0 || transform_index >= (int)controller_transforms.size())
    {
        return -1;
    }
    int transform_count = 0;
    for(int row = 0; row < scene_controllers_.count(); row++)
    {
        if(!scene_controllers_.hasUserRole(row))
        {
            if(transform_count == transform_index)
            {
                return row;
            }
            transform_count++;
        }
    }
    return -1;
}

