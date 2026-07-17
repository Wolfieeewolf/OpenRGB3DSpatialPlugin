// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <exception>
#include "PluginLog.h"

void OpenRGB3DSpatialTab::MergePluginUiIntoSettings(nlohmann::json& settings) const
{
    if(viewport)
    {
        settings["RoomGrid"]["Show"]       = viewport->GetShowRoomGridOverlay();
        settings["RoomGrid"]["Brightness"] = viewport->GetRoomGridBrightness();
        settings["RoomGrid"]["PointSize"]  = viewport->GetRoomGridPointSize();
        settings["RoomGrid"]["Step"]       = viewport->GetRoomGridStep();

        settings["Viewport"]["ShowRoomGuideLabels"] = viewport->GetShowRoomGuideLabels();
    }

    settings["LEDSpacing"]["X"] = default_led_spacing_x_;
    settings["LEDSpacing"]["Y"] = default_led_spacing_y_;
    settings["LEDSpacing"]["Z"] = default_led_spacing_z_;

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

void OpenRGB3DSpatialTab::BindSettingsPanels()
{
    if(!ui)
    {
        return;
    }

    if(!profile_unsaved_banner_ && ui->rootVerticalLayout)
    {
        profile_unsaved_banner_ = new QLabel(this);
        profile_unsaved_banner_->setObjectName(QStringLiteral("profileUnsavedBanner"));
        profile_unsaved_banner_->setWordWrap(false);
        profile_unsaved_banner_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        profile_unsaved_banner_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        profile_unsaved_banner_->setToolTip(
            tr("Save in OpenRGB using File → Profiles, or layout and effect changes will be lost."));
        profile_unsaved_banner_->setVisible(false);
        profile_unsaved_banner_->setStyleSheet(
            QStringLiteral("QLabel#profileUnsavedBanner {"
                           " background-color: #5c4a12;"
                           " color: #ffe9a8;"
                           " font-size: 11px;"
                           " padding: 2px 6px;"
                           " border-bottom: 1px solid #8a7020;"
                           "}"));
        ui->rootVerticalLayout->insertWidget(0, profile_unsaved_banner_);
    }

    InitSceneObjectEditTab();

    if(ui->sceneObjectEditHostPanel)
    {
        ui->sceneObjectEditHostPanel->bindTab(this);
    }
    ui->gridSettingsPanel->bindTab(this);
    ui->objectCreatorTabPanel->bindTab(this);
}

void OpenRGB3DSpatialTab::SetLayoutDirty(bool dirty)
{
    layout_dirty = dirty;
    UpdateProfileDirtyBanner();
}

void OpenRGB3DSpatialTab::ClearLayoutDirty()
{
    SetLayoutDirty(false);
}

void OpenRGB3DSpatialTab::UpdateProfileDirtyBanner()
{
    if(!profile_unsaved_banner_)
    {
        return;
    }

    profile_unsaved_banner_->setVisible(layout_dirty);
    if(layout_dirty)
    {
        profile_unsaved_banner_->setText(
            tr("Unsaved Spatial changes — save your OpenRGB profile."));
    }
}

