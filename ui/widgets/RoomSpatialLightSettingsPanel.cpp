// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSpatialLightSettingsPanel.h"
#include "ui_RoomSpatialLightSettingsPanel.h"

#include "SpatialLighting/RoomSpatialLightingUi.h"
#include "EffectSliderRow.h"
#include "EffectInfoLabel.h"

RoomSpatialLightSettingsPanel::RoomSpatialLightSettingsPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RoomSpatialLightSettingsPanel)
{
    ui->setupUi(this);
    configureStaticLabels();
}

RoomSpatialLightSettingsPanel::~RoomSpatialLightSettingsPanel()
{
    delete ui;
}

void RoomSpatialLightSettingsPanel::configureStaticLabels()
{
    ui->glowSizeRow->setCaptionText(QStringLiteral("Glow size:"));
    ui->lightReachRow->setCaptionText(QStringLiteral("Light reach:"));
    ui->roomFillRow->setCaptionText(QStringLiteral("Room fill:"));
}

void RoomSpatialLightSettingsPanel::setShowRoomFill(bool show)
{
    ui->roomFillRow->setVisible(show);
}

void RoomSpatialLightSettingsPanel::setHintText(const QString& text)
{
    ui->hintLabel->setVisible(!text.isEmpty());
    ui->hintLabel->setText(text);
}

void RoomSpatialLightSettingsPanel::syncRelayTuneFromParams(const RoomSpatialLightingUi::RoomSpatialLightParams& params)
{
    ui->glowSizeRow->syncSliderValue(static_cast<int>(params.glow_radius_mm),
                                     [](int v) { return QString::number(v) + QStringLiteral(" mm"); });
    ui->lightReachRow->syncSliderValue(static_cast<int>(params.light_reach_mm),
                                       [](int v) { return QString::number(v) + QStringLiteral(" mm"); });
    ui->roomFillRow->syncSliderValue(static_cast<int>(params.room_fill),
                                     [](int v) { return QString::number(v) + QStringLiteral("%"); });
}

void RoomSpatialLightSettingsPanel::bindRelayTuneSliders(QObject* owner,
                                                         RoomSpatialLightingUi::RoomSpatialLightParams& params,
                                                         const std::function<void()>& tuneChanged)
{
    ui->glowSizeRow->configure(10, 400, static_cast<int>(params.glow_radius_mm));
    ui->glowSizeRow->bindValueChanged(
        owner,
        [&params, tuneChanged](int v) {
            params.glow_radius_mm = static_cast<float>(v);
            if(tuneChanged)
            {
                tuneChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral(" mm"); });

    ui->lightReachRow->configure(50, 5000, static_cast<int>(params.light_reach_mm));
    ui->lightReachRow->bindValueChanged(
        owner,
        [&params, tuneChanged](int v) {
            params.light_reach_mm = static_cast<float>(v);
            if(tuneChanged)
            {
                tuneChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral(" mm"); });

    ui->roomFillRow->configure(0, 100, static_cast<int>(params.room_fill));
    ui->roomFillRow->bindValueChanged(
        owner,
        [&params, tuneChanged](int v) {
            params.room_fill = static_cast<float>(v);
            if(tuneChanged)
            {
                tuneChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });
}

void RoomSpatialLightSettingsPanel::bindRelayTuneParams(QObject* owner,
                                                        RoomSpatialLightingUi::RoomSpatialLightParams& params,
                                                        const std::function<void()>& tuneChanged)
{
    syncRelayTuneFromParams(params);
    bindRelayTuneSliders(owner, params, tuneChanged);
}

void RoomSpatialLightSettingsPanel::syncFromParams(const RoomSpatialLightingUi::RoomSpatialLightParams& params)
{
    syncRelayTuneFromParams(params);
}
