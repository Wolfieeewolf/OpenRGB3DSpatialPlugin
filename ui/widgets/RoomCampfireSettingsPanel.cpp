// SPDX-License-Identifier: GPL-2.0-only

#include "RoomCampfireSettingsPanel.h"
#include "ui_RoomCampfireSettingsPanel.h"

#include "RoomSpatialLightSettingsPanel.h"
#include "EffectColorSwatchRow.h"
#include "EffectSliderRow.h"
#include "EffectSectionHeading.h"
#include "Colors.h"
#include "SpatialLighting/RoomSpatialLightingUi.h"
#include "SpatialEffect3D.h"

#include <vector>

namespace
{

constexpr const char* kCampfireHint =
    "Flame tongues use partial coverage (dark gaps between tongues), not a room-wide wash. "
    "Speed = flicker; Turbulence = tongue motion; Sparks = ember pops. "
    "Ember glow adds a faint room halo (default ~22%). Raise Glow size / Light reach if flames look tiny.";

} // namespace

RoomCampfireSettingsPanel::RoomCampfireSettingsPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RoomCampfireSettingsPanel)
{
    ui->setupUi(this);
    configureStaticLabels();
}

RoomCampfireSettingsPanel::~RoomCampfireSettingsPanel()
{
    delete ui;
}

void RoomCampfireSettingsPanel::configureStaticLabels()
{
    ui->fireColorsHeading->setTitle(QStringLiteral("Fire colors"));
    ui->fireMotionHeading->setTitle(QStringLiteral("Fire motion"));
    ui->placementHeading->setTitle(QStringLiteral("Placement & light"));

    ui->coreColorRow->setCaptionText(QStringLiteral("Core (hot):"));
    ui->flameColorRow->setCaptionText(QStringLiteral("Flame:"));
    ui->outerColorRow->setCaptionText(QStringLiteral("Outer spill:"));

    ui->flameRiseRow->setCaptionText(QStringLiteral("Flame rise:"));
    ui->flameTurbulenceRow->setCaptionText(QStringLiteral("Flame turbulence:"));
    ui->sparkRow->setCaptionText(QStringLiteral("Sparks:"));
    ui->roomSpillRow->setCaptionText(QStringLiteral("Ember glow:"));

    ui->flameRiseRow->configure(0, 100, 75);
    ui->flameTurbulenceRow->configure(0, 100, 62);
    ui->sparkRow->configure(0, 100, 45);
    ui->roomSpillRow->configure(0, 40, 22);

    ui->lightSettingsPanel->setShowRoomFill(false);
    ui->lightSettingsPanel->setHintText(QString::fromUtf8(kCampfireHint));
}

void RoomCampfireSettingsPanel::syncFromState(SpatialEffect3D* effect,
                                              const RoomSpatialLightingUi::RoomSpatialLightParams& light_params,
                                              const RoomCampfireParams& campfire_params)
{
    if(!effect)
    {
        return;
    }

    std::vector<RGBColor> palette = effect->GetColors();
    while(palette.size() < 3)
    {
        palette.push_back(COLOR_WHITE);
    }
    ui->coreColorRow->setSwatchColor(palette[0]);
    ui->flameColorRow->setSwatchColor(palette.size() > 1 ? palette[1] : palette[0]);
    ui->outerColorRow->setSwatchColor(palette.size() > 2 ? palette[2] : palette[0]);

    ui->flameRiseRow->syncSliderValue(static_cast<int>(campfire_params.flame_rise),
                                      [](int v) { return QString::number(v) + QStringLiteral("%"); });
    ui->flameTurbulenceRow->syncSliderValue(static_cast<int>(campfire_params.flame_turbulence),
                                            [](int v) { return QString::number(v) + QStringLiteral("%"); });
    ui->sparkRow->syncSliderValue(static_cast<int>(campfire_params.spark_amount),
                                  [](int v) { return QString::number(v) + QStringLiteral("%"); });
    ui->roomSpillRow->syncSliderValue(static_cast<int>(campfire_params.spill_fill),
                                      [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->lightSettingsPanel->syncFromParams(light_params);
}

void RoomCampfireSettingsPanel::bind(SpatialEffect3D* effect,
                                     RoomSpatialLightingUi::RoomSpatialLightParams& light_params,
                                     RoomCampfireParams& campfire_params,
                                     const std::function<void()>& on_placement_changed,
                                     const std::function<void()>& on_tune_changed)
{
    syncFromState(effect, light_params, campfire_params);

    ui->coreColorRow->bindColorIndex(effect, 0, on_tune_changed);
    ui->flameColorRow->bindColorIndex(effect, 1, on_tune_changed);
    ui->outerColorRow->bindColorIndex(effect, 2, on_tune_changed);

    ui->flameRiseRow->bindValueChanged(
        effect,
        [&campfire_params, on_tune_changed](int v) {
            campfire_params.flame_rise = static_cast<float>(v);
            if(on_tune_changed)
            {
                on_tune_changed();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->flameTurbulenceRow->bindValueChanged(
        effect,
        [&campfire_params, on_tune_changed](int v) {
            campfire_params.flame_turbulence = static_cast<float>(v);
            if(on_tune_changed)
            {
                on_tune_changed();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->sparkRow->bindValueChanged(
        effect,
        [&campfire_params, on_tune_changed](int v) {
            campfire_params.spark_amount = static_cast<float>(v);
            if(on_tune_changed)
            {
                on_tune_changed();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->roomSpillRow->bindValueChanged(
        effect,
        [&campfire_params, on_tune_changed](int v) {
            campfire_params.spill_fill = static_cast<float>(v);
            if(on_tune_changed)
            {
                on_tune_changed();
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->lightSettingsPanel->bindParams(effect, light_params, on_placement_changed, on_tune_changed);
}
