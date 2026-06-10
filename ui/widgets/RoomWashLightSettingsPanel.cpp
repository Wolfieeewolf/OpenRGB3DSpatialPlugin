// SPDX-License-Identifier: GPL-2.0-only

#include "RoomWashLightSettingsPanel.h"
#include "ui_RoomWashLightSettingsPanel.h"

#include "RoomSpatialLightSettingsPanel.h"
#include "EffectColorSwatchRow.h"
#include "EffectSectionHeading.h"
#include "Colors.h"
#include "SpatialEffect3D.h"

#include <vector>

namespace
{

constexpr const char* kWashHint =
    "Even ambient wash across the room (high Room fill). "
    "For a localized fire with sparks, use Room campfire instead.";

} // namespace

RoomWashLightSettingsPanel::RoomWashLightSettingsPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RoomWashLightSettingsPanel)
{
    ui->setupUi(this);
    configureStaticLabels();
}

RoomWashLightSettingsPanel::~RoomWashLightSettingsPanel()
{
    delete ui;
}

void RoomWashLightSettingsPanel::configureStaticLabels()
{
    ui->washColorHeading->setTitle(QStringLiteral("Wash color"));
    ui->placementHeading->setTitle(QStringLiteral("Placement & light"));
    ui->washColorRow->setCaptionText(QStringLiteral("Wash color:"));

    ui->lightSettingsPanel->setShowRoomFill(true);
    ui->lightSettingsPanel->setHintText(QString::fromUtf8(kWashHint));
}

void RoomWashLightSettingsPanel::syncFromState(SpatialEffect3D* effect,
                                               const RoomSpatialLightingUi::RoomSpatialLightParams& light_params)
{
    if(!effect)
    {
        return;
    }

    std::vector<RGBColor> palette = effect->GetColors();
    if(palette.empty())
    {
        palette.push_back(COLOR_WHITE);
    }
    ui->washColorRow->setSwatchColor(palette[0]);
    ui->lightSettingsPanel->syncFromParams(light_params);
}

void RoomWashLightSettingsPanel::bind(SpatialEffect3D* effect,
                                      RoomSpatialLightingUi::RoomSpatialLightParams& light_params,
                                      const std::function<void()>& on_placement_changed,
                                      const std::function<void()>& on_tune_changed)
{
    syncFromState(effect, light_params);

    std::vector<RGBColor> palette = effect->GetColors();
    if(palette.empty())
    {
        palette.push_back(COLOR_WHITE);
    }
    effect->SetColors(palette);
    effect->SetRainbowMode(false);

    ui->washColorRow->bindColorIndex(effect, 0, on_tune_changed);
    ui->lightSettingsPanel->bindParams(effect, light_params, on_placement_changed, on_tune_changed);
}
