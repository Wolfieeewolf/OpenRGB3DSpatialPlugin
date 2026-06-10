// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTROOMOUTPUTPANEL_H
#define EFFECTROOMOUTPUTPANEL_H

#include "SpatialRoom/SpatialRoomTypes.h"

#include <QWidget>
#include <functional>
#include <vector>

namespace RoomSpatialLightingUi {
struct RoomSpatialLightParams;
}

class SpatialEffect3D;
class QComboBox;
class QGroupBox;
class QLabel;
class QShowEvent;
class QVBoxLayout;
class RoomSpatialLightSettingsPanel;

class EffectRoomOutputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EffectRoomOutputPanel(QWidget* parent = nullptr);

    void bind(SpatialEffect3D* effect,
              SpatialRoom::SpatialRoomCoordinateMode& coordinate_mode,
              SpatialRoom::SpatialRoomOutputRole& output_role,
              RoomSpatialLightingUi::RoomSpatialLightParams& relay_params,
              std::vector<int>& emitter_controllers,
              std::vector<int>& receiver_controllers,
              const std::function<void()>& on_changed);

    void syncFromState(SpatialRoom::SpatialRoomCoordinateMode coordinate_mode,
                       SpatialRoom::SpatialRoomOutputRole output_role,
                       const RoomSpatialLightingUi::RoomSpatialLightParams& relay_params,
                       const std::vector<int>& emitter_controllers,
                       const std::vector<int>& receiver_controllers);

    void refreshControllerLists();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refreshRolePanels();
    void rebuildControllerLists();
    void clearLayout(QVBoxLayout* layout);
    bool isEmitterIndex(int index) const;
    bool isReceiverIndex(int index) const;

    QComboBox* coordinate_combo_ = nullptr;
    QComboBox* output_combo_ = nullptr;
    QLabel* zone_hint_ = nullptr;
    QGroupBox* emitters_group_ = nullptr;
    QVBoxLayout* emitters_layout_ = nullptr;
    QGroupBox* receivers_group_ = nullptr;
    QVBoxLayout* receivers_layout_ = nullptr;
    RoomSpatialLightSettingsPanel* relay_panel_ = nullptr;
    SpatialRoom::SpatialRoomOutputRole* bound_role_ = nullptr;
    std::vector<int>* bound_emitters_ = nullptr;
    std::vector<int>* bound_receivers_ = nullptr;
    SpatialEffect3D* bound_effect_ = nullptr;
    std::function<void()> on_changed_;
};

#endif
