// SPDX-License-Identifier: GPL-2.0-only

#include "EffectRoomOutputPanel.h"

#include "SpatialEffect3D.h"
#include "ControllerDisplayUtils.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "RoomSpatialLightSettingsPanel.h"
#include "Effects3D/SpatialLighting/RoomSpatialLightingUi.h"
#include "LEDPosition3D.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>

EffectRoomOutputPanel::EffectRoomOutputPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* coord_label = new QLabel(tr("Room coordinates:"));
    coordinate_combo_ = new QComboBox();
    coordinate_combo_->addItem(tr("Effect origin"), (int)SpatialRoom::SpatialRoomCoordinateMode::EffectOrigin);
    coordinate_combo_->addItem(tr("Room mapped (full bounds)"),
                               (int)SpatialRoom::SpatialRoomCoordinateMode::RoomMapped);
    coordinate_combo_->setToolTip(tr(
        "Effect origin: classic distance/angle from reference and offset.\n"
        "Room mapped: pattern tied to room box center and walls."));
    layout->addWidget(coord_label);
    layout->addWidget(coordinate_combo_);

    auto* output_label = new QLabel(tr("Room output:"));
    output_combo_ = new QComboBox();
    output_combo_->addItem(tr("Direct (paint LEDs)"), (int)SpatialRoom::SpatialRoomOutputRole::Direct);
    output_combo_->addItem(tr("Emitter + relay (screen mirror)"),
                           (int)SpatialRoom::SpatialRoomOutputRole::EmitterRelay);
    output_combo_->setToolTip(tr(
        "Direct: normal effect on this layer's zone.\n"
        "Emitter + relay: emitters run the pattern; receivers sample it like Screen Mirror "
        "(project each LED toward the emitter plane and read the color at that UV)."));
    layout->addWidget(output_label);
    layout->addWidget(output_combo_);

    zone_hint_ = new QLabel(tr(
        "Set the stack zone to All so emitters and receivers on this layer are not limited by the top-bar zone."));
    zone_hint_->setWordWrap(true);
    zone_hint_->setVisible(false);
    layout->addWidget(zone_hint_);

    emitters_group_ = new QGroupBox(tr("Emitter devices"));
    emitters_layout_ = new QVBoxLayout(emitters_group_);
    emitters_group_->setVisible(false);
    layout->addWidget(emitters_group_);

    receivers_group_ = new QGroupBox(tr("Receiver devices"));
    receivers_layout_ = new QVBoxLayout(receivers_group_);
    receivers_group_->setVisible(false);
    layout->addWidget(receivers_group_);

    relay_panel_ = new RoomSpatialLightSettingsPanel();
    relay_panel_->setShowPlacement(false);
    relay_panel_->setShowRoomFill(true);
    relay_panel_->setHintText(tr(
        "Reach/glow: Screen Mirror–style distance falloff. "
        "Shadows and ambient occlusion darken receivers behind controllers and in corners."));
    relay_panel_->setVisible(false);
    layout->addWidget(relay_panel_);
}

void EffectRoomOutputPanel::clearLayout(QVBoxLayout* layout)
{
    if(!layout)
    {
        return;
    }
    while(QLayoutItem* item = layout->takeAt(0))
    {
        if(QWidget* w = item->widget())
        {
            w->deleteLater();
        }
        delete item;
    }
}

bool EffectRoomOutputPanel::isEmitterIndex(int index) const
{
    if(!bound_emitters_)
    {
        return false;
    }
    return std::find(bound_emitters_->begin(), bound_emitters_->end(), index) != bound_emitters_->end();
}

bool EffectRoomOutputPanel::isReceiverIndex(int index) const
{
    if(!bound_receivers_)
    {
        return false;
    }
    if(!bound_receivers_->empty())
    {
        return std::find(bound_receivers_->begin(), bound_receivers_->end(), index) != bound_receivers_->end();
    }
    return !isEmitterIndex(index);
}

void EffectRoomOutputPanel::refreshControllerLists()
{
    rebuildControllerLists();
}

void EffectRoomOutputPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshControllerLists();
}

void EffectRoomOutputPanel::rebuildControllerLists()
{
    clearLayout(emitters_layout_);
    clearLayout(receivers_layout_);

    const auto* transforms = SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms || !bound_effect_)
    {
        return;
    }

    for(size_t i = 0; i < transforms->size(); ++i)
    {
        const ControllerTransform* t = (*transforms)[i].get();
        if(!t || t->hidden_by_virtual)
        {
            continue;
        }
        const int ctrl_idx = static_cast<int>(i);
        const QString label = ControllerDisplay::FormatControllerTransformLabel(t, ctrl_idx);

        const bool is_emitter = isEmitterIndex(ctrl_idx);
        const bool explicit_receivers = bound_receivers_ && !bound_receivers_->empty();
        const bool is_explicit_receiver =
            explicit_receivers &&
            std::find(bound_receivers_->begin(), bound_receivers_->end(), ctrl_idx) != bound_receivers_->end();

        auto* emitter_box = new QCheckBox(label);
        emitter_box->setChecked(is_emitter);
        emitter_box->setEnabled(!is_explicit_receiver);
        connect(emitter_box, &QCheckBox::toggled, this, [this, ctrl_idx](bool on) {
            if(!bound_effect_)
            {
                return;
            }
            bound_effect_->setRoomEmitterControllerIndex(ctrl_idx, on);
            refreshControllerLists();
            if(on_changed_)
            {
                on_changed_();
            }
        });
        emitters_layout_->addWidget(emitter_box);

        auto* receiver_box = new QCheckBox(label);
        receiver_box->setChecked(is_emitter ? false : isReceiverIndex(ctrl_idx));
        receiver_box->setEnabled(!is_emitter);
        connect(receiver_box, &QCheckBox::toggled, this, [this, ctrl_idx](bool on) {
            if(!bound_effect_ || !bound_receivers_)
            {
                return;
            }
            if(bound_receivers_->empty() && !on)
            {
                const auto* transforms = SpatialLightingSceneProvider::instance()->controllers();
                if(transforms)
                {
                    for(size_t j = 0; j < transforms->size(); ++j)
                    {
                        const ControllerTransform* t = (*transforms)[j].get();
                        if(!t || t->hidden_by_virtual)
                        {
                            continue;
                        }
                        const int idx = (int)j;
                        if(!bound_effect_->isRoomEmitterController(idx) && idx != ctrl_idx)
                        {
                            bound_receivers_->push_back(idx);
                        }
                    }
                }
            }
            else
            {
                bound_effect_->setRoomReceiverControllerIndex(ctrl_idx, on);
            }
            refreshControllerLists();
            if(on_changed_)
            {
                on_changed_();
            }
        });
        receivers_layout_->addWidget(receiver_box);
    }
}

void EffectRoomOutputPanel::refreshRolePanels()
{
    const SpatialRoom::SpatialRoomOutputRole role =
        bound_role_ ? *bound_role_ : SpatialRoom::SpatialRoomOutputRole::Direct;
    const bool emitter_relay = role == SpatialRoom::SpatialRoomOutputRole::EmitterRelay;
    if(zone_hint_)
    {
        zone_hint_->setVisible(emitter_relay);
    }
    emitters_group_->setVisible(emitter_relay);
    receivers_group_->setVisible(emitter_relay);
    relay_panel_->setVisible(emitter_relay);
    if(emitter_relay)
    {
        rebuildControllerLists();
    }
}

void EffectRoomOutputPanel::syncFromState(SpatialRoom::SpatialRoomCoordinateMode coordinate_mode,
                                          SpatialRoom::SpatialRoomOutputRole output_role,
                                          const RoomSpatialLightingUi::RoomSpatialLightParams& relay_params,
                                          const std::vector<int>& emitter_controllers,
                                          const std::vector<int>& receiver_controllers)
{
    (void)emitter_controllers;
    (void)receiver_controllers;

    SpatialRoom::SpatialRoomOutputRole display_role = output_role;
    if(display_role == SpatialRoom::SpatialRoomOutputRole::Emitter ||
       display_role == SpatialRoom::SpatialRoomOutputRole::RelayShade)
    {
        display_role = SpatialRoom::SpatialRoomOutputRole::EmitterRelay;
    }

    for(int i = 0; i < coordinate_combo_->count(); ++i)
    {
        if(coordinate_combo_->itemData(i).toInt() == (int)coordinate_mode)
        {
            coordinate_combo_->setCurrentIndex(i);
            break;
        }
    }
    for(int i = 0; i < output_combo_->count(); ++i)
    {
        if(output_combo_->itemData(i).toInt() == (int)display_role)
        {
            output_combo_->setCurrentIndex(i);
            break;
        }
    }
    relay_panel_->syncFromParams(relay_params);
    if(bound_role_)
    {
        *bound_role_ = output_role;
    }
    refreshRolePanels();
}

void EffectRoomOutputPanel::bind(SpatialEffect3D* effect,
                                 SpatialRoom::SpatialRoomCoordinateMode& coordinate_mode,
                                 SpatialRoom::SpatialRoomOutputRole& output_role,
                                 RoomSpatialLightingUi::RoomSpatialLightParams& relay_params,
                                 std::vector<int>& emitter_controllers,
                                 std::vector<int>& receiver_controllers,
                                 const std::function<void()>& on_changed)
{
    bound_effect_ = effect;
    bound_role_ = &output_role;
    bound_emitters_ = &emitter_controllers;
    bound_receivers_ = &receiver_controllers;
    on_changed_ = on_changed;
    syncFromState(coordinate_mode, output_role, relay_params, emitter_controllers, receiver_controllers);

    connect(coordinate_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&coordinate_mode, on_changed, this](int index) {
                coordinate_mode =
                    (SpatialRoom::SpatialRoomCoordinateMode)coordinate_combo_->itemData(index).toInt();
                on_changed();
            });

    connect(output_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&output_role, on_changed, this](int index) {
                output_role = (SpatialRoom::SpatialRoomOutputRole)output_combo_->itemData(index).toInt();
                refreshRolePanels();
                on_changed();
            });

    relay_panel_->bindParams(effect, relay_params, on_changed, on_changed);
}
