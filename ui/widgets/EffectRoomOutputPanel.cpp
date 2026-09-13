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
#include "EffectCheckRow.h"
#include "EffectSliderRow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>

EffectRoomOutputPanel::EffectRoomOutputPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    output_combo_ = new QComboBox();
    output_combo_->addItem(tr("Default"), (int)SpatialRoom::SpatialRoomOutputRole::Direct);
    output_combo_->addItem(tr("Emitter + relay"),
                           (int)SpatialRoom::SpatialRoomOutputRole::EmitterRelay);
    output_combo_->setToolTip(tr(
        "Default: normal effect on this layer's zone.\n"
        "Emitter + relay: the effect is painted on emitter devices. Relays never run the "
        "effect — they only catch light from emitter LEDs.\n\n"
        "Without blockers: light spreads in all directions from each emitter LED.\n"
        "With blockers: painted housing cells stop light like the real device.\n\n"
        "Use Spatial anchor (above) for pattern origin on the emitter group."));
    layout->addWidget(output_combo_);

    blockers_row_ = new EffectCheckRow();
    blockers_row_->configure(
        tr("Blockers"),
        false,
        tr("Off: emitter light spreads in all directions.\n"
           "On: custom-controller blocker cells stop light like the real device "
           "(tube, speaker body, keyboard plate)."));
    blockers_row_->setVisible(false);
    layout->addWidget(blockers_row_);

    walls_row_ = new EffectCheckRow();
    walls_row_->configure(
        tr("Include room walls as blockers"),
        false,
        tr("Treat room bounds as solid walls that stop emitter light."));
    walls_row_->setVisible(false);
    layout->addWidget(walls_row_);

    ao_row_ = new EffectSliderRow();
    ao_row_->setCaptionText(tr("Room ambient occlusion:"));
    ao_row_->configure(
        0,
        100,
        65,
        tr("Extra contact darkening on receivers when blockers are on."));
    ao_row_->setVisible(false);
    layout->addWidget(ao_row_);

    connect(blockers_row_->checkBox(), &QCheckBox::toggled, this, [this](bool on) {
        if(bound_relay_params_)
        {
            bound_relay_params_->use_occlusion = on;
        }
        updateBlockerChildVisibility();
        if(changed_callback_)
        {
            changed_callback_();
        }
    });
    connect(walls_row_->checkBox(), &QCheckBox::toggled, this, [this](bool on) {
        if(bound_relay_params_)
        {
            bound_relay_params_->use_room_walls = on;
        }
        if(changed_callback_)
        {
            changed_callback_();
        }
    });
    ao_row_->bindValueChanged(
        this,
        [this](int v) {
            if(bound_relay_params_)
            {
                bound_relay_params_->ao_strength = static_cast<float>(v);
            }
        },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        [this]() {
            if(changed_callback_)
            {
                changed_callback_();
            }
        });

    zone_hint_ = new QLabel(tr(
        "Set the stack zone to All so emitters and receivers on this layer are not limited by the top-bar zone. "
        "Use one Emitter + relay layer; pattern layers below it paint emitters."));
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
    emitters_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    emitters_scroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    emitters_scroll_->setMinimumHeight(170);
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
    receivers_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    receivers_scroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    receivers_scroll_->setMinimumHeight(170);
    receivers_host_ = new QWidget();
    receivers_layout_ = new QVBoxLayout(receivers_host_);
    receivers_layout_->setContentsMargins(0, 0, 0, 0);
    receivers_layout_->setSpacing(2);
    receivers_scroll_->setWidget(receivers_host_);
    receivers_outer->addWidget(receivers_scroll_);
    receivers_group_->setVisible(false);
    layout->addWidget(receivers_group_);

    relay_panel_ = new RoomSpatialLightSettingsPanel();
    relay_panel_->setShowRoomFill(true);
    relay_panel_->setHintText(tr(
        "Reach/glow: how far light travels from each emitter LED. "
        "Leave Blockers off for a 360° wash. Turn Blockers on so housing cells "
        "stop light like the real device."));
    relay_panel_->setVisible(false);
    layout->addWidget(relay_panel_);
    layout->setStretch(5, 1);
    layout->setStretch(6, 1);
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
            delete w;
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
            if(changed_callback_)
            {
                changed_callback_();
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

void EffectRoomOutputPanel::updateBlockerChildVisibility()
{
    const SpatialRoom::SpatialRoomOutputRole role =
        bound_role_ ? *bound_role_ : SpatialRoom::SpatialRoomOutputRole::Direct;
    const bool emitter_relay = role == SpatialRoom::SpatialRoomOutputRole::EmitterRelay;
    const bool blockers_on =
        emitter_relay && bound_relay_params_ && bound_relay_params_->use_occlusion;
    if(blockers_row_)
    {
        blockers_row_->setVisible(emitter_relay);
    }
    if(walls_row_)
    {
        walls_row_->setVisible(blockers_on);
    }
    if(ao_row_)
    {
        ao_row_->setVisible(blockers_on);
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
    updateBlockerChildVisibility();
    if(emitter_relay)
    {
        rebuildControllerLists();
    }

    if(emitters_host_ && receivers_host_)
    {
        emitters_host_->adjustSize();
        receivers_host_->adjustSize();
    }
    if(emitters_scroll_ && receivers_scroll_)
    {
        emitters_scroll_->updateGeometry();
        receivers_scroll_->updateGeometry();
    }
    updateGeometry();
}

void EffectRoomOutputPanel::syncFromState(SpatialRoom::SpatialRoomOutputRole output_role,
                                          const RoomSpatialLightingUi::RoomSpatialLightParams& relay_params)
{
    const SpatialRoom::SpatialRoomOutputRole display_role =
        (output_role == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
            ? SpatialRoom::SpatialRoomOutputRole::EmitterRelay
            : SpatialRoom::SpatialRoomOutputRole::Direct;

    int matched = -1;
    for(int i = 0; i < output_combo_->count(); ++i)
    {
        if(output_combo_->itemData(i).toInt() == (int)display_role)
        {
            matched = i;
            break;
        }
    }
    if(matched >= 0)
    {
        output_combo_->setCurrentIndex(matched);
    }
    else if(output_combo_->count() > 0)
    {
        output_combo_->setCurrentIndex(0);
    }
    relay_panel_->syncFromParams(relay_params);
    if(blockers_row_ && blockers_row_->checkBox())
    {
        QSignalBlocker block(blockers_row_->checkBox());
        blockers_row_->checkBox()->setChecked(relay_params.use_occlusion);
    }
    if(walls_row_ && walls_row_->checkBox())
    {
        QSignalBlocker block(walls_row_->checkBox());
        walls_row_->checkBox()->setChecked(relay_params.use_room_walls);
    }
    if(ao_row_)
    {
        ao_row_->syncSliderValue(static_cast<int>(relay_params.ao_strength),
                                 [](int v) { return QString::number(v) + QStringLiteral("%"); });
    }
    if(bound_role_)
    {
        *bound_role_ = display_role;
    }
    refreshRolePanels();
}

void EffectRoomOutputPanel::bind(SpatialEffect3D* effect,
                                 SpatialRoom::SpatialRoomOutputRole& output_role,
                                 RoomSpatialLightingUi::RoomSpatialLightParams& relay_params,
                                 std::vector<int>& emitter_controllers,
                                 std::vector<int>& receiver_controllers,
                                 const std::function<void()>& changed,
                                 const std::function<QString(int)>& transform_label)
{
    if(output_combo_)
    {
        output_combo_->disconnect(this);
    }

    bound_effect_         = effect;
    bound_role_           = &output_role;
    bound_relay_params_   = &relay_params;
    bound_emitters_       = &emitter_controllers;
    bound_receivers_      = &receiver_controllers;
    changed_callback_     = changed;
    transform_label_fn_     = transform_label;
    syncFromState(output_role, relay_params);

    connect(output_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&output_role, changed, this](int index) {
                const int raw = output_combo_->itemData(index).toInt();
                output_role = (raw == (int)SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
                                  ? SpatialRoom::SpatialRoomOutputRole::EmitterRelay
                                  : SpatialRoom::SpatialRoomOutputRole::Direct;
                refreshRolePanels();
                changed();
            });

    relay_panel_->bindRelayTuneParams(effect, relay_params, changed);
}

