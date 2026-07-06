// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSPATIALLIGHTSETTINGSPANEL_H
#define ROOMSPATIALLIGHTSETTINGSPANEL_H

#include <QWidget>
#include <functional>

namespace RoomSpatialLightingUi {
struct RoomSpatialLightParams;
}

namespace Ui {
class RoomSpatialLightSettingsPanel;
}

class RoomSpatialLightSettingsPanel : public QWidget
{
public:
    explicit RoomSpatialLightSettingsPanel(QWidget* parent = nullptr);
    ~RoomSpatialLightSettingsPanel() override;

    void setShowRoomFill(bool show);
    void setShowPlacement(bool show);
    void setShowAmbientOcclusion(bool show);
    void setShowBlockerControls(bool show);
    void setHintText(const QString& text);

    void bindParams(QObject* owner,
                     RoomSpatialLightingUi::RoomSpatialLightParams& params,
                     const std::function<void()>& placementChanged,
                     const std::function<void()>& tuneChanged);

    void bindRelayTuneParams(QObject* owner,
                             RoomSpatialLightingUi::RoomSpatialLightParams& params,
                             const std::function<void()>& tuneChanged);

    void syncFromParams(const RoomSpatialLightingUi::RoomSpatialLightParams& params);

private:
    void configureStaticLabels();
    void setCustomPlacementVisible(bool visible);
    void syncRelayTuneFromParams(const RoomSpatialLightingUi::RoomSpatialLightParams& params);
    void bindRelayTuneSliders(QObject* owner,
                              RoomSpatialLightingUi::RoomSpatialLightParams& params,
                              const std::function<void()>& tuneChanged);

    Ui::RoomSpatialLightSettingsPanel* ui = nullptr;
};

#endif
