// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMWASHLIGHTSETTINGSPANEL_H
#define ROOMWASHLIGHTSETTINGSPANEL_H

#include <QWidget>
#include <functional>

class SpatialEffect3D;

namespace RoomSpatialLightingUi {
struct RoomSpatialLightParams;
}

namespace Ui {
class RoomWashLightSettingsPanel;
}

class RoomWashLightSettingsPanel : public QWidget
{
public:
    explicit RoomWashLightSettingsPanel(QWidget* parent = nullptr);
    ~RoomWashLightSettingsPanel() override;

    void bind(SpatialEffect3D* effect,
              RoomSpatialLightingUi::RoomSpatialLightParams& light_params,
              const std::function<void()>& on_placement_changed,
              const std::function<void()>& on_tune_changed);

    void syncFromState(SpatialEffect3D* effect, const RoomSpatialLightingUi::RoomSpatialLightParams& light_params);

private:
    void configureStaticLabels();

    Ui::RoomWashLightSettingsPanel* ui = nullptr;
};

#endif
