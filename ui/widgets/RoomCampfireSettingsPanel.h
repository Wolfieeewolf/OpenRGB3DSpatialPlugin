// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMCAMPFIRESETTINGSPANEL_H
#define ROOMCAMPFIRESETTINGSPANEL_H

#include <QWidget>
#include <functional>

#include "SpatialLighting/RoomCampfireParams.h"

class SpatialEffect3D;

namespace RoomSpatialLightingUi {
struct RoomSpatialLightParams;
}

namespace Ui {
class RoomCampfireSettingsPanel;
}

class RoomCampfireSettingsPanel : public QWidget
{
public:
    explicit RoomCampfireSettingsPanel(QWidget* parent = nullptr);
    ~RoomCampfireSettingsPanel() override;

    void bind(SpatialEffect3D* effect,
              RoomSpatialLightingUi::RoomSpatialLightParams& light_params,
              RoomCampfireParams& campfire_params,
              const std::function<void()>& on_placement_changed,
              const std::function<void()>& on_tune_changed);

    void syncFromState(SpatialEffect3D* effect,
                       const RoomSpatialLightingUi::RoomSpatialLightParams& light_params,
                       const RoomCampfireParams& campfire_params);

private:
    void configureStaticLabels();

    Ui::RoomCampfireSettingsPanel* ui = nullptr;
};

#endif
