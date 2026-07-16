// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ProfilesTabPanel.h"
#include "PluginSettingsPaths.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <exception>
#include "LogManager.h"

void OpenRGB3DSpatialTab::MergePluginUiIntoSettings(nlohmann::json& settings) const
{
    if(viewport)
    {
        float dist, yaw, pitch, tx, ty, tz;
        viewport->GetCamera(dist, yaw, pitch, tx, ty, tz);
        settings["Camera"]["Distance"] = dist;
        settings["Camera"]["Yaw"]      = yaw;
        settings["Camera"]["Pitch"]    = pitch;
        settings["Camera"]["TargetX"]  = tx;
        settings["Camera"]["TargetY"]  = ty;
        settings["Camera"]["TargetZ"]  = tz;

        settings["RoomGrid"]["Show"]       = viewport->GetShowRoomGridOverlay();
        settings["RoomGrid"]["Brightness"] = viewport->GetRoomGridBrightness();
        settings["RoomGrid"]["PointSize"]  = viewport->GetRoomGridPointSize();
        settings["RoomGrid"]["Step"]       = viewport->GetRoomGridStep();

        settings["Grid"]["SnapEnabled"] = viewport->IsGridSnapEnabled();

        settings["Viewport"]["ShowRoomGuideLabels"] = viewport->GetShowRoomGuideLabels();
    }

    int grid_x = custom_grid_x;
    int grid_y = custom_grid_y;
    int grid_z = custom_grid_z;
    float scale_mm = grid_scale_mm;
    bool manual_room = use_manual_room_size;
    float room_w = manual_room_width;
    float room_h = manual_room_height;
    float room_d = manual_room_depth;

    if(gridXSpin())
    {
        grid_x = gridXSpin()->value();
    }
    if(gridYSpin())
    {
        grid_y = gridYSpin()->value();
    }
    if(gridZSpin())
    {
        grid_z = gridZSpin()->value();
    }
    if(gridScaleSpin())
    {
        scale_mm = (float)gridScaleSpin()->value();
    }
    if(useManualRoomSizeCheckbox())
    {
        manual_room = useManualRoomSizeCheckbox()->isChecked();
    }
    if(roomWidthSpin())
    {
        room_w = (float)roomWidthSpin()->value();
    }
    if(roomHeightSpin())
    {
        room_h = (float)roomHeightSpin()->value();
    }
    if(roomDepthSpin())
    {
        room_d = (float)roomDepthSpin()->value();
    }

    settings["Grid"]["X"]       = grid_x;
    settings["Grid"]["Y"]       = grid_y;
    settings["Grid"]["Z"]       = grid_z;
    settings["Grid"]["ScaleMM"] = scale_mm;

    settings["Room"]["UseManual"] = manual_room;
    settings["Room"]["WidthMM"]  = room_w;
    settings["Room"]["HeightMM"] = room_h;
    settings["Room"]["DepthMM"]  = room_d;

    settings["LEDSpacing"]["X"] = default_led_spacing_x_;
    settings["LEDSpacing"]["Y"] = default_led_spacing_y_;
    settings["LEDSpacing"]["Z"] = default_led_spacing_z_;

    if(layoutProfilesCombo() && layoutProfilesCombo()->currentIndex() >= 0)
    {
        settings["SelectedProfile"] = layoutProfilesCombo()->currentText().toStdString();
    }
    if(autoLoadLayoutCheckbox())
    {
        settings["AutoLoadEnabled"] = autoLoadLayoutCheckbox()->isChecked();
    }
    if(effectProfilesCombo() && effectProfilesCombo()->currentIndex() >= 0)
    {
        settings["EffectSelectedProfile"] = effectProfilesCombo()->currentText().toStdString();
    }
    if(autoLoadEffectProfileCheckbox())
    {
        settings["EffectAutoLoadEnabled"] = autoLoadEffectProfileCheckbox()->isChecked();
    }

    if(ui && ui->leftModeTabs)
    {
        settings["Ui"]["LeftModeTab"] = ui->leftModeTabs->currentIndex();
    }
}

void OpenRGB3DSpatialTab::SavePluginUiSettings()
{
    if(!resource_manager)
    {
        return;
    }

    try
    {
        nlohmann::json settings = GetPluginSettings();
        MergePluginUiIntoSettings(settings);
        SetPluginSettings(settings);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to save plugin UI settings: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::RestoreProfileUiFromSettings()
{
    try
    {
        const nlohmann::json settings = GetPluginSettings();

        if(autoLoadLayoutCheckbox() && settings.contains("AutoLoadEnabled"))
        {
            const QSignalBlocker block(autoLoadLayoutCheckbox());
            autoLoadLayoutCheckbox()->setChecked(settings["AutoLoadEnabled"].get<bool>());
        }

        if(autoLoadEffectProfileCheckbox() && settings.contains("EffectAutoLoadEnabled"))
        {
            const QSignalBlocker block(autoLoadEffectProfileCheckbox());
            autoLoadEffectProfileCheckbox()->setChecked(settings["EffectAutoLoadEnabled"].get<bool>());
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to restore profile UI from settings: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::BindSettingsPanels()
{
    if(!ui)
    {
        return;
    }

    InitSceneObjectEditTab();

    ui->profilesTabPanel->connectTab(this);
    PopulateLayoutDropdown();
    PopulateEffectProfileDropdown();
    RestoreProfileUiFromSettings();

    if(ui->sceneObjectEditHostPanel)
    {
        ui->sceneObjectEditHostPanel->bindTab(this);
    }
    ui->gridSettingsPanel->bindTab(this);
    ui->objectCreatorTabPanel->bindTab(this);
}

void OpenRGB3DSpatialTab::SetLayoutDirty(bool dirty)
{
    if(layout_dirty == dirty)
    {
        return;
    }
    
    layout_dirty = dirty;
    
    if(QPushButton* save_btn = saveLayoutButton())
    {
        save_btn->setEnabled(dirty);
        if(dirty)
        {
            save_btn->setText("Save *");
            save_btn->setToolTip("Save changes to current layout profile (unsaved changes)");
        }
        else
        {
            save_btn->setText("Save");
            save_btn->setToolTip("Save changes to current layout profile");
        }
    }
}

void OpenRGB3DSpatialTab::ClearLayoutDirty()
{
    SetLayoutDirty(false);
}

void OpenRGB3DSpatialTab::openConfigFolderClicked()
{
    if(!resource_manager)
    {
        return;
    }
    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    const filesystem::path plugin_dir = PluginSettingsPaths::PluginRoot(resource_manager);
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(plugin_dir.string())));
}

bool OpenRGB3DSpatialTab::PromptSaveIfDirty()
{
    if(!layout_dirty)
    {
        return true;
    }
    
    QMessageBox msgBox;
    msgBox.setWindowTitle("Unsaved Changes");
    msgBox.setText("The current layout has unsaved changes.");
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setIcon(QMessageBox::Warning);
    
    int ret = msgBox.exec();
    
    switch(ret)
    {
        case QMessageBox::Save:
            quickSaveLayoutClicked();
            return !layout_dirty;
            
        case QMessageBox::Discard:
            return true;
            
        case QMessageBox::Cancel:
        default:
            return false;
    }
}

void OpenRGB3DSpatialTab::quickSaveLayoutClicked()
{
    QComboBox* profiles = layoutProfilesCombo();
    if(!profiles || profiles->currentIndex() < 0)
    {
        QMessageBox::warning(this, "No Profile Selected",
                           "Please select a layout profile first, or use 'Save As...' to create a new one.");
        return;
    }
    
    QString profile_name = profiles->currentText();
    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected",
                           "Please select a layout profile first, or use 'Save As...' to create a new one.");
        return;
    }
    
    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    
    if(QSpinBox* gx = gridXSpin()) custom_grid_x = gx->value();
    if(QSpinBox* gy = gridYSpin()) custom_grid_y = gy->value();
    if(QSpinBox* gz = gridZSpin()) custom_grid_z = gz->value();
    if(QDoubleSpinBox* rw = roomWidthSpin()) manual_room_width = rw->value();
    if(QDoubleSpinBox* rh = roomHeightSpin()) manual_room_height = rh->value();
    if(QDoubleSpinBox* rd = roomDepthSpin()) manual_room_depth = rd->value();
    
    SaveLayout(layout_path);
    ClearLayoutDirty();
    
    if(QPushButton* save_btn = saveLayoutButton())
    {
        QString original_text = save_btn->text();
        save_btn->setText("Saved!");
        save_btn->setEnabled(false);
        QTimer::singleShot(1500, this, [this, original_text]()
        {
            if(QPushButton* btn = saveLayoutButton())
            {
                if(!layout_dirty)
                {
                    btn->setText(original_text);
                }
            }
        });
    }
}

