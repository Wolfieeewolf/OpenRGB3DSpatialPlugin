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
    void setHintText(const QString& text);

    void bindParams(QObject* owner,
                     RoomSpatialLightingUi::RoomSpatialLightParams& params,
                     const std::function<void()>& on_placement_changed,
                     const std::function<void()>& on_tune_changed);

    void syncFromParams(const RoomSpatialLightingUi::RoomSpatialLightParams& params);

private:
    void configureStaticLabels();
    void setCustomPlacementVisible(bool visible);

    Ui::RoomSpatialLightSettingsPanel* ui = nullptr;
};

#endif
