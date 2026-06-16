// SPDX-License-Identifier: GPL-2.0-only

#include "EffectRoomOutputPanel.h"

#include "SpatialEffect3D.h"
#include "ControllerDisplayUtils.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "RoomSpatialLightSettingsPanel.h"
#include "Effects3D/SpatialLighting/RoomSpatialLightingUi.h"
#include "RoomOutputDeviceCard.h"
#include "LEDPosition3D.h"
#include "ControllerLayout3D.h"
#include "PluginUiUtils.h"

#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>

EffectRoomOutputPanel::EffectRoomOutputPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    output_combo_ = new QComboBox();
    output_combo_->addItem(tr("Default"), (int)SpatialRoom::SpatialRoomOutputRole::Direct);
    output_combo_->addItem(tr("Emitter + relay"),
                           (int)SpatialRoom::SpatialRoomOutputRole::EmitterRelay);
    output_combo_->setToolTip(tr(
        "Default: normal effect on this layer's zone.\n"
        "Emitter + relay: the effect is painted once across all emitters (like one image "
        "stretched over multiple monitors). Relays never run the effect — they only "
        "catch light rays from emitter surfaces.\n\n"
        "Use Spatial anchor (above) for pattern origin on the emitter group."));
    layout->addWidget(output_combo_);

    zone_hint_ = new QLabel(tr(
        "Set the stack zone to All so emitters and receivers on this layer are not limited by the top-bar zone."));
    zone_hint_->setWordWrap(true);
    zone_hint_->setVisible(false);
    layout->addWidget(zone_hint_);

    emitters_group_ = new QGroupBox(tr("Emitters"));
    auto* emitters_outer = new QVBoxLayout(emitters_group_);
    emitters_outer->setContentsMargins(0, 0, 0, 0);
    emitters_scroll_ = new QScrollArea(emitters_group_);
    emitters_scroll_->setWidgetResizable(true);
    emitters_scroll_->setFrameShape(QFrame::NoFrame);
    emitters_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    emitters_host_ = new QWidget();
    emitters_layout_ = new QVBoxLayout(emitters_host_);
    emitters_layout_->setContentsMargins(0, 0, 0, 0);
    emitters_layout_->setSpacing(2);
    emitters_scroll_->setWidget(emitters_host_);
    emitters_outer->addWidget(emitters_scroll_);
    emitters_group_->setVisible(false);
    layout->addWidget(emitters_group_);

    receivers_group_ = new QGroupBox(tr("Receivers"));
    auto* receivers_outer = new QVBoxLayout(receivers_group_);
    receivers_outer->setContentsMargins(0, 0, 0, 0);
    receivers_scroll_ = new QScrollArea(receivers_group_);
    receivers_scroll_->setWidgetResizable(true);
    receivers_scroll_->setFrameShape(QFrame::NoFrame);
    receivers_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    receivers_host_ = new QWidget();
    receivers_layout_ = new QVBoxLayout(receivers_host_);
    receivers_layout_->setContentsMargins(0, 0, 0, 0);
    receivers_layout_->setSpacing(2);
    receivers_scroll_->setWidget(receivers_host_);
    receivers_outer->addWidget(receivers_scroll_);
    receivers_group_->setVisible(false);
    layout->addWidget(receivers_group_);

    relay_panel_ = new RoomSpatialLightSettingsPanel();
    relay_panel_->setShowPlacement(false);
    relay_panel_->setShowRoomFill(true);
    relay_panel_->setHintText(tr(
        "Reach/glow: distance falloff from emitters. "
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

QString EffectRoomOutputPanel::titleForTransform(int transform_index,
                                                 const ControllerTransform* transform) const
{
    if(transform_label_fn_)
    {
        const QString label = transform_label_fn_(transform_index);
        if(!label.isEmpty())
        {
            return label;
        }
    }
    return ControllerDisplay::FormatControllerTransformLabel(transform, transform_index);
}

void EffectRoomOutputPanel::appendDeviceCards(QVBoxLayout* layout,
                                              const std::vector<std::pair<int, QString>>& devices,
                                              bool emitter_role)
{
    if(!layout)
    {
        return;
    }

    if(devices.empty())
    {
        auto* empty_label = new QLabel(tr("No devices in the 3D scene."));
        PluginUiApplyMutedSecondaryLabel(empty_label);
        empty_label->setWordWrap(true);
        layout->addWidget(empty_label);
        return;
    }

    for(const std::pair<int, QString>& device : devices)
    {
        const int ctrl_idx = device.first;
        const bool is_emitter = isEmitterIndex(ctrl_idx);
        const bool explicit_receivers = bound_receivers_ && !bound_receivers_->empty();
        const bool is_explicit_receiver =
            explicit_receivers &&
            std::find(bound_receivers_->begin(), bound_receivers_->end(), ctrl_idx) != bound_receivers_->end();

        bool added = false;
        bool enabled = true;
        if(emitter_role)
        {
            added   = is_emitter;
            enabled = !is_explicit_receiver;
        }
        else
        {
            added   = is_emitter ? false : isReceiverIndex(ctrl_idx);
            enabled = !is_emitter;
        }

        auto* card = new RoomOutputDeviceCard(device.second);
        card->setAdded(added);
        card->setInteractionEnabled(enabled);
        connect(card, &RoomOutputDeviceCard::actionToggled, this, [this, ctrl_idx, emitter_role](bool on) {
            if(!bound_effect_)
            {
                return;
            }

            if(emitter_role)
            {
                bound_effect_->setRoomEmitterControllerIndex(ctrl_idx, on);
            }
            else if(bound_receivers_)
            {
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
                            const int idx = static_cast<int>(j);
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
            }

            refreshControllerLists();
            if(on_changed_)
            {
                on_changed_();
            }
        });
        layout->addWidget(card);
    }

    layout->addStretch();
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

    std::vector<std::pair<int, QString>> devices;
    devices.reserve(transforms->size());
    for(size_t i = 0; i < transforms->size(); ++i)
    {
        const ControllerTransform* t = (*transforms)[i].get();
        if(!t || t->hidden_by_virtual)
        {
            continue;
        }
        const int ctrl_idx = static_cast<int>(i);
        devices.emplace_back(ctrl_idx, titleForTransform(ctrl_idx, t));
    }

    appendDeviceCards(emitters_layout_, devices, true);
    appendDeviceCards(receivers_layout_, devices, false);
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

void EffectRoomOutputPanel::syncFromState(SpatialRoom::SpatialRoomOutputRole output_role,
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
                                 SpatialRoom::SpatialRoomOutputRole& output_role,
                                 RoomSpatialLightingUi::RoomSpatialLightParams& relay_params,
                                 std::vector<int>& emitter_controllers,
                                 std::vector<int>& receiver_controllers,
                                 const std::function<void()>& on_changed,
                                 const std::function<QString(int)>& transform_label)
{
    if(output_combo_)
    {
        output_combo_->disconnect(this);
    }

    bound_effect_         = effect;
    bound_role_           = &output_role;
    bound_emitters_       = &emitter_controllers;
    bound_receivers_      = &receiver_controllers;
    on_changed_           = on_changed;
    transform_label_fn_     = transform_label;
    syncFromState(output_role, relay_params, emitter_controllers, receiver_controllers);

    connect(output_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&output_role, on_changed, this](int index) {
                output_role = (SpatialRoom::SpatialRoomOutputRole)output_combo_->itemData(index).toInt();
                refreshRolePanels();
                on_changed();
            });

    relay_panel_->bindParams(effect, relay_params, on_changed, on_changed);
}
