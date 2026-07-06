// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSpatialLightSettingsPanel.h"
#include "ui_RoomSpatialLightSettingsPanel.h"

#include "SpatialLighting/RoomSpatialLightingUi.h"
#include "EffectLabeledComboRow.h"
#include "EffectSliderRow.h"
#include "EffectCheckRow.h"
#include "EffectInfoLabel.h"

#include <QCheckBox>
#include <QComboBox>

#include <algorithm>

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
    ui->placementRow->setCaptionText(QStringLiteral("Placement:"));
    QComboBox* place = ui->placementRow->combo();
    place->addItem(QStringLiteral("Near corner (room min)"));
    place->addItem(QStringLiteral("Room center"));
    place->addItem(QStringLiteral("Far corner (room max)"));
    place->addItem(QStringLiteral("Custom (room %)"));

    ui->customXRow->setCaptionText(QStringLiteral("Custom X %:"));
    ui->customYRow->setCaptionText(QStringLiteral("Custom Y %:"));
    ui->customZRow->setCaptionText(QStringLiteral("Custom Z %:"));
    ui->customXRow->configure(0, 100, 15);
    ui->customYRow->configure(0, 100, 15);
    ui->customZRow->configure(0, 100, 12);
    ui->glowSizeRow->setCaptionText(QStringLiteral("Glow size:"));
    ui->lightReachRow->setCaptionText(QStringLiteral("Light reach:"));
    ui->roomFillRow->setCaptionText(QStringLiteral("Room fill:"));
    ui->ambientOcclusionRow->setCaptionText(QStringLiteral("Ambient occlusion:"));

    ui->shadowsRow->configure(QStringLiteral("Blockers (shadows)"), false);
    ui->roomWallsRow->configure(QStringLiteral("Include room walls as blockers"), false);
}

void RoomSpatialLightSettingsPanel::setShowRoomFill(bool show)
{
    ui->roomFillRow->setVisible(show);
}

void RoomSpatialLightSettingsPanel::setShowPlacement(bool show)
{
    ui->placementRow->setVisible(show);
    if(!show)
    {
        setCustomPlacementVisible(false);
    }
}

void RoomSpatialLightSettingsPanel::setShowAmbientOcclusion(bool show)
{
    ui->ambientOcclusionRow->setVisible(show);
}

void RoomSpatialLightSettingsPanel::setShowBlockerControls(bool show)
{
    ui->shadowsRow->setVisible(show);
    ui->roomWallsRow->setVisible(show);
}

void RoomSpatialLightSettingsPanel::setHintText(const QString& text)
{
    ui->hintLabel->setVisible(!text.isEmpty());
    ui->hintLabel->setText(text);
}

void RoomSpatialLightSettingsPanel::setCustomPlacementVisible(bool visible)
{
    ui->customXRow->setVisible(visible);
    ui->customYRow->setVisible(visible);
    ui->customZRow->setVisible(visible);
}

void RoomSpatialLightSettingsPanel::syncRelayTuneFromParams(const RoomSpatialLightingUi::RoomSpatialLightParams& params)
{
    ui->glowSizeRow->syncSliderValue(static_cast<int>(params.glow_radius_mm),
                                     [](int v) { return QString::number(v) + QStringLiteral(" mm"); });
    ui->lightReachRow->syncSliderValue(static_cast<int>(params.light_reach_mm),
                                       [](int v) { return QString::number(v) + QStringLiteral(" mm"); });
    ui->roomFillRow->syncSliderValue(static_cast<int>(params.room_fill),
                                     [](int v) { return QString::number(v) + QStringLiteral("%"); });
    ui->ambientOcclusionRow->syncSliderValue(static_cast<int>(params.ao_strength),
                                             [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->shadowsRow->checkBox()->setChecked(params.use_occlusion);
    ui->roomWallsRow->checkBox()->setChecked(params.use_room_walls);
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

    ui->ambientOcclusionRow->configure(0, 100, static_cast<int>(params.ao_strength));
    ui->ambientOcclusionRow->bindValueChanged(
        owner,
        [&params, tuneChanged](int v) {
            params.ao_strength = static_cast<float>(v);
            if(tuneChanged)
            {
                tuneChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->roomWallsRow->bindToggled(owner, [&params, tuneChanged](bool on) {
        params.use_room_walls = on;
        if(tuneChanged)
        {
            tuneChanged();
        }
    });

    ui->shadowsRow->bindToggled(owner, [&params, tuneChanged](bool on) {
        params.use_occlusion = on;
        params.use_controller_occlusion = on;
        if(tuneChanged)
        {
            tuneChanged();
        }
    });
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
    QComboBox* place = ui->placementRow->combo();
    place->setCurrentIndex(std::max(0, std::min(params.placement_mode, 3)));
    setCustomPlacementVisible(params.placement_mode == 3);

    ui->customXRow->syncSliderValue(static_cast<int>(params.custom_u * 100.0f),
                                    [](int v) { return QString::number(v) + QStringLiteral("%"); });
    ui->customYRow->syncSliderValue(static_cast<int>(params.custom_v * 100.0f),
                                    [](int v) { return QString::number(v) + QStringLiteral("%"); });
    ui->customZRow->syncSliderValue(static_cast<int>(params.custom_w * 100.0f),
                                    [](int v) { return QString::number(v) + QStringLiteral("%"); });
    syncRelayTuneFromParams(params);
}

void RoomSpatialLightSettingsPanel::bindParams(QObject* owner,
                                               RoomSpatialLightingUi::RoomSpatialLightParams& params,
                                               const std::function<void()>& placementChanged,
                                               const std::function<void()>& tuneChanged)
{
    syncFromParams(params);

    QComboBox* place = ui->placementRow->combo();
    setCustomPlacementVisible(params.placement_mode == 3);
    QObject::connect(place, QOverload<int>::of(&QComboBox::currentIndexChanged), owner, [this, &params, placementChanged](int idx) {
        params.placement_mode = idx;
        setCustomPlacementVisible(idx == 3);
        if(placementChanged)
        {
            placementChanged();
        }
    });

    ui->customXRow->configure(0, 100, static_cast<int>(params.custom_u * 100.0f));
    ui->customYRow->configure(0, 100, static_cast<int>(params.custom_v * 100.0f));
    ui->customZRow->configure(0, 100, static_cast<int>(params.custom_w * 100.0f));

    ui->customXRow->bindValueChanged(
        owner,
        [&params, placementChanged](int v) {
            params.custom_u = static_cast<float>(v) / 100.0f;
            if(placementChanged)
            {
                placementChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->customYRow->bindValueChanged(
        owner,
        [&params, placementChanged](int v) {
            params.custom_v = static_cast<float>(v) / 100.0f;
            if(placementChanged)
            {
                placementChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->customZRow->bindValueChanged(
        owner,
        [&params, placementChanged](int v) {
            params.custom_w = static_cast<float>(v) / 100.0f;
            if(placementChanged)
            {
                placementChanged();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    bindRelayTuneSliders(owner, params, tuneChanged);
}
