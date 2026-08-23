// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackEditorDialog.h"
#include "EffectPackCatalog.h"
#include "EffectPackGradientBar.h"
#include "EffectPackTimelineWidget.h"
#include "EffectPackToolBar.h"
#include "EffectPacks/EffectPackApplier.h"
#include "LEDPosition3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"
#include "ZoneManager3D.h"
#include "EffectCollapsibleSection.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

QString ControllerLabel(const ControllerTransform* transform, int index)
{
    if(!transform)
    {
        return QStringLiteral("Controller %1").arg(index);
    }
    if(transform->virtual_controller)
    {
        return QString::fromStdString(transform->virtual_controller->GetName());
    }
    if(transform->controller)
    {
        const std::string display = transform->controller->GetDisplayName();
        if(!display.empty())
        {
            return QString::fromStdString(display);
        }
        return QString::fromStdString(transform->controller->GetName());
    }
    return QStringLiteral("Controller %1").arg(index);
}

std::string ControllerKeyName(const ControllerTransform* transform, int index)
{
    if(transform && transform->virtual_controller)
    {
        return transform->virtual_controller->GetName();
    }
    if(transform && transform->controller)
    {
        const std::string display = transform->controller->GetDisplayName();
        if(!display.empty())
        {
            return display;
        }
        return transform->controller->GetName();
    }
    return std::string("controller_") + std::to_string(index);
}

QString ZoneLabelForLed(RGBControllerInterface* rgb, unsigned int zone_idx)
{
    if(rgb && zone_idx < rgb->GetZoneCount())
    {
        QString name = QString::fromStdString(rgb->GetZoneDisplayName(zone_idx));
        if(name.isEmpty())
        {
            name = QString::fromStdString(rgb->GetZoneName(zone_idx));
        }
        if(!name.isEmpty())
        {
            return name;
        }
    }
    return QStringLiteral("Zone %1").arg(zone_idx);
}

bool TryGlobalLedIndex(RGBControllerInterface* rgb, unsigned int zone_idx, unsigned int led_idx, int* out)
{
    if(!rgb || !out || zone_idx >= rgb->GetZoneCount())
    {
        return false;
    }
    if(led_idx >= rgb->GetZoneLEDsCount(zone_idx))
    {
        return false;
    }
    const unsigned int global = rgb->GetZoneStartIndex(zone_idx) + led_idx;
    if(global >= rgb->GetLEDCount())
    {
        return false;
    }
    *out = (int)global;
    return true;
}

} // namespace

EffectPackTimelineWidget::Node EffectPackEditorDialog::buildControllerNode(ControllerTransform* transform, int index) const
{
    EffectPackTimelineWidget::Node ctrl;
    ctrl.label = ControllerLabel(transform, index);
    const std::string key = ControllerKeyName(transform, index);
    ctrl.target.kind = EffectPack::TargetKind::Device;
    ctrl.target.device_name = key;

    struct ZoneBucket
    {
        QString label;
        QString zone_name;
        std::vector<int> led_globals;
        std::vector<unsigned int> led_in_zone;
    };
    std::map<std::pair<RGBControllerInterface*, unsigned int>, ZoneBucket> zones;

    for(const LEDPosition3D& led : transform->led_positions)
    {
        RGBControllerInterface* rgb = led.controller ? led.controller : transform->controller;
        if(!rgb)
        {
            continue;
        }
        const auto zone_key = std::make_pair(rgb, led.zone_idx);
        ZoneBucket& bucket = zones[zone_key];
        if(bucket.zone_name.isEmpty())
        {
            bucket.zone_name = ZoneLabelForLed(rgb, led.zone_idx);
            bucket.label = bucket.zone_name;
        }
        int global = -1;
        if(TryGlobalLedIndex(rgb, led.zone_idx, led.led_idx, &global))
        {
            if(std::find(bucket.led_globals.begin(), bucket.led_globals.end(), global) == bucket.led_globals.end())
            {
                bucket.led_globals.push_back(global);
                bucket.led_in_zone.push_back(led.led_idx);
            }
        }
    }

    for(auto& entry : zones)
    {
        ZoneBucket& bucket = entry.second;
        int same_label = 0;
        for(const auto& other : zones)
        {
            if(other.second.zone_name == bucket.zone_name)
            {
                ++same_label;
            }
        }
        if(same_label > 1 && entry.first.first)
        {
            QString device = QString::fromStdString(entry.first.first->GetDisplayName());
            if(device.isEmpty())
            {
                device = QString::fromStdString(entry.first.first->GetName());
            }
            if(!device.isEmpty())
            {
                bucket.label = bucket.zone_name + QStringLiteral(" · ") + device;
            }
        }

        EffectPackTimelineWidget::Node zone;
        zone.label = bucket.label;
        zone.target.kind = EffectPack::TargetKind::Zone;
        zone.target.device_name = key;
        zone.target.zone_name = bucket.zone_name.toStdString();

        const size_t led_count = bucket.led_globals.size();
        zone.led_count = (int)std::max<size_t>(1, led_count);
        const size_t led_cap = std::min(led_count, size_t{256});
        for(size_t li = 0; li < led_cap; ++li)
        {
            EffectPackTimelineWidget::Node led_node;
            led_node.label = QStringLiteral("LED %1").arg(bucket.led_in_zone[li]);
            led_node.target.kind = EffectPack::TargetKind::Leds;
            led_node.target.device_name = key;
            led_node.target.zone_name = bucket.zone_name.toStdString();
            led_node.target.led_indices = {bucket.led_globals[li]};
            led_node.led_count = zone.led_count;
            zone.children.push_back(std::move(led_node));
        }
        ctrl.children.push_back(std::move(zone));
    }
    int total_leds = 0;
    for(const EffectPackTimelineWidget::Node& z : ctrl.children)
    {
        total_leds += z.led_count;
    }
    ctrl.led_count = std::max(1, total_leds);
    return ctrl;
}

void EffectPackEditorDialog::onRebuildTimelineModel()
{
    QVector<EffectPackTimelineWidget::Node> roots;

    EffectPackTimelineWidget::Node all;
    all.label = QStringLiteral("All (this pack)");
    all.target.kind = EffectPack::TargetKind::All;
    int pack_leds = 0;

    if(tab_)
    {
        timeline_->setControllerTransforms(tab_->GetControllerTransformsMutable());
        timeline_->setZoneManager(tab_->GetZoneManager());
        const auto& transforms = tab_->GetControllerTransforms();
        ZoneManager3D* zones = tab_->GetZoneManager();

        std::vector<bool> claimed((size_t)transforms.size(), false);

        auto append_controller = [&](int i, EffectPackTimelineWidget::Node* parent,
                                     const QString& scene_zone_name, bool reorderable) {
            if(i < 0 || i >= (int)transforms.size())
            {
                return;
            }
            ControllerTransform* transform = transforms[(size_t)i].get();
            if(!transform || transform->hidden_by_virtual)
            {
                return;
            }
            const std::string key = ControllerKeyName(transform, i);
            if(!deviceSelectedForPack(key))
            {
                return;
            }
            EffectPackTimelineWidget::Node ctrl = buildControllerNode(transform, i);
            ctrl.transform_index = i;
            ctrl.scene_zone_name = scene_zone_name;
            ctrl.reorderable = reorderable;
            pack_leds += ctrl.led_count;
            claimed[(size_t)i] = true;
            if(parent)
            {
                parent->led_count += ctrl.led_count;
                parent->children.push_back(std::move(ctrl));
            }
            else
            {
                roots.push_back(std::move(ctrl));
            }
        };

        if(zones)
        {
            for(int zi = 0; zi < zones->GetZoneCount(); ++zi)
            {
                Zone3D* zone = zones->GetZone(zi);
                if(!zone)
                {
                    continue;
                }
                // Only show zones that intersect pack devices.
                bool any = false;
                for(int ci : zone->GetControllers())
                {
                    if(ci < 0 || ci >= (int)transforms.size())
                    {
                        continue;
                    }
                    ControllerTransform* t = transforms[(size_t)ci].get();
                    if(t && !t->hidden_by_virtual && deviceSelectedForPack(ControllerKeyName(t, ci)))
                    {
                        any = true;
                        break;
                    }
                }
                if(!any)
                {
                    continue;
                }

                EffectPackTimelineWidget::Node zone_node;
                zone_node.label = QString::fromStdString(zone->GetName());
                zone_node.target.kind = EffectPack::TargetKind::SceneZone;
                zone_node.target.scene_zone_name = zone->GetName();
                zone_node.expanded = true;
                zone_node.led_count = 0;

                // All LEDs — flat list of every LED under the zone.
                EffectPackTimelineWidget::Node all_leds;
                all_leds.label = QStringLiteral("All LEDs");
                all_leds.target.kind = EffectPack::TargetKind::SceneZone;
                all_leds.target.scene_zone_name = zone->GetName();
                all_leds.target.flatten_leds = true;
                all_leds.expanded = false;
                all_leds.led_count = 0;

                for(int ci : zone->GetControllers())
                {
                    if(ci < 0 || ci >= (int)transforms.size())
                    {
                        continue;
                    }
                    ControllerTransform* transform = transforms[(size_t)ci].get();
                    if(!transform || transform->hidden_by_virtual)
                    {
                        continue;
                    }
                    const std::string key = ControllerKeyName(transform, ci);
                    if(!deviceSelectedForPack(key))
                    {
                        continue;
                    }
                    // Individual LED children under All LEDs.
                    int led_slot = 0;
                    for(const LEDPosition3D& led : transform->led_positions)
                    {
                        RGBControllerInterface* rgb = led.controller ? led.controller : transform->controller;
                        int global = -1;
                        if(!rgb || !TryGlobalLedIndex(rgb, led.zone_idx, led.led_idx, &global))
                        {
                            continue;
                        }
                        EffectPackTimelineWidget::Node led_node;
                        led_node.label = ControllerLabel(transform, ci)
                            + QStringLiteral(" · LED %1").arg(led.led_idx);
                        led_node.target.kind = EffectPack::TargetKind::Leds;
                        led_node.target.device_name = key;
                        led_node.target.zone_name = ZoneLabelForLed(rgb, led.zone_idx).toStdString();
                        led_node.target.led_indices = {global};
                        led_node.led_count = 1;
                        all_leds.children.push_back(std::move(led_node));
                        ++led_slot;
                    }
                    all_leds.led_count += led_slot;
                }
                if(all_leds.led_count > 0)
                {
                    zone_node.led_count += all_leds.led_count;
                    zone_node.children.push_back(std::move(all_leds));
                }

                for(int ci : zone->GetControllers())
                {
                    append_controller(ci, &zone_node, zone_node.label, true);
                }

                if(!zone_node.children.isEmpty())
                {
                    zone_node.led_count = std::max(1, zone_node.led_count);
                    roots.push_back(std::move(zone_node));
                }
            }
        }

        EffectPackTimelineWidget::Node ungrouped;
        ungrouped.label = QStringLiteral("Ungrouped");
        ungrouped.target.kind = EffectPack::TargetKind::All; // not used for painting; containers only
        ungrouped.expanded = true;
        for(int i = 0; i < (int)transforms.size(); ++i)
        {
            if(claimed[(size_t)i])
            {
                continue;
            }
            append_controller(i, &ungrouped, QString(), false);
        }
        // Prefer real device targets under Ungrouped — if we used a fake All target on the
        // folder itself, only promote children to roots when the folder has no identity.
        if(!ungrouped.children.isEmpty())
        {
            // Re-parent: Ungrouped is a UI folder; children keep Device targets.
            // Use a distinct scene_zone_name marker so we don't collide with All.
            ungrouped.target.kind = EffectPack::TargetKind::SceneZone;
            ungrouped.target.scene_zone_name = std::string("__ungrouped__");
            // Don't allow placing blocks on the fake ungrouped zone — strip by making
            // it non-trackable: keep as folder only via empty scene zone that applier ignores.
            // Blocks on Ungrouped row would not match any Zone3D — skip creating that track
            // by not exposing a paintable target. Use Device-less label folder:
            for(EffectPackTimelineWidget::Node& child : ungrouped.children)
            {
                roots.push_back(std::move(child));
            }
        }
    }
    all.led_count = std::max(1, pack_leds);
    roots.prepend(std::move(all));

    timeline_->setModel(std::move(roots));
}

void EffectPackEditorDialog::onSceneZoneControllersReordered(const QString& scene_zone_name,
                                                             const QVector<int>& controller_indices)
{
    if(!tab_ || scene_zone_name.isEmpty() || scene_zone_name == QStringLiteral("__ungrouped__"))
    {
        return;
    }
    ZoneManager3D* zones = tab_->GetZoneManager();
    if(!zones)
    {
        return;
    }
    Zone3D* zone = zones->GetZoneByName(scene_zone_name.toStdString());
    if(!zone)
    {
        return;
    }
    std::vector<int> order;
    order.reserve((size_t)controller_indices.size());
    for(int idx : controller_indices)
    {
        order.push_back(idx);
    }
    zone->SetControllers(std::move(order));
    onRebuildTimelineModel();
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Reordered controllers in “%1” (Sequence space uses this order)")
                                   .arg(scene_zone_name));
    }
}

int EffectPackEditorDialog::ensureTrackForTarget(const EffectPack::Target& target, const QString& label)
{
    for(int i = 0; i < (int)pack_.tracks.size(); ++i)
    {
        const EffectPack::Target& existing = pack_.tracks[(size_t)i].target;
        if(existing.kind == target.kind
           && existing.device_name == target.device_name
           && existing.zone_name == target.zone_name
           && existing.led_indices == target.led_indices)
        {
            return i;
        }
    }
    EffectPack::Track track;
    track.name = label.toStdString();
    track.target = target;
    pack_.tracks.push_back(std::move(track));
    return (int)pack_.tracks.size() - 1;
}

void EffectPackEditorDialog::addBlockAt(int row_index, int ms, EffectPack::BlockType type)
{
    if(timeline_)
    {
        timeline_->cancelDrag();
    }
    // Flush props into the previously selected block before creating another.
    applyFormToSelectedBlock();

    const QVector<EffectPackTimelineWidget::Row>& built = timeline_->rows();
    if(row_index < 0 || row_index >= built.size())
    {
        if(built.isEmpty())
        {
            return;
        }
        row_index = 0;
    }

    const auto& row = built[row_index];
    const int track = ensureTrackForTarget(row.target, row.label);
    EffectPack::Block block;
    block.type = type;
    block.start_ms = std::clamp(ms, 0, std::max(0, pack_.duration_ms - 1));
    const int default_len = (type == EffectPack::BlockType::Fade || type == EffectPack::BlockType::ColorWash) ? 2000 : 1000;
    block.end_ms = std::min(pack_.duration_ms, block.start_ms + default_len);
    if(block.end_ms <= block.start_ms)
    {
        block.end_ms = block.start_ms + 1;
    }
    block.color = ToRGBColor(255, 0, 0);
    block.color_from = ToRGBColor(255, 0, 0);
    block.color_to = ToRGBColor(0, 128, 255);
    block.period_ms = 800;
    block.min_intensity = (type == EffectPack::BlockType::Twinkle) ? 0.0f : 0.15f;
    block.max_intensity = 1.0f;
    block.intensity = 1.0f;
    block.direction = EffectPack::Direction::Right;
    block.axis_space = EffectPack::TargetIsMultiDeviceGroup(row.target)
        ? EffectPack::AxisSpace::Room
        : EffectPack::AxisSpace::Device;
    block.axis_mode = EffectPack::AxisMode::Preset;
    block.axis_yaw_deg = 0.0f;
    block.axis_pitch_deg = 0.0f;
    block.speed = 1.0f;
    block.pulse_length = 0.25f;
    if(type == EffectPack::BlockType::Twinkle)
    {
        block.period_ms = 700;
        block.gradient = {
            {0.0f, ToRGBColor(255, 220, 120)},
            {0.5f, ToRGBColor(255, 255, 255)},
            {1.0f, ToRGBColor(120, 180, 255)},
        };
        block.color = block.gradient.front().color;
        block.color_from = block.color;
        block.color_to = block.gradient.back().color;
    }
    else if(type == EffectPack::BlockType::Candle)
    {
        block.period_ms = 120;
        block.min_intensity = 0.35f;
        block.gradient = {
            {0.0f, ToRGBColor(180, 40, 0)},
            {0.45f, ToRGBColor(255, 120, 20)},
            {1.0f, ToRGBColor(255, 220, 80)},
        };
        block.color = block.gradient[1].color;
    }
    else if(type == EffectPack::BlockType::Strobe)
    {
        block.period_ms = 120;
        block.pulse_length = 0.35f;
        block.gradient = {
            {0.0f, ToRGBColor(255, 255, 255)},
            {1.0f, ToRGBColor(255, 255, 255)},
        };
        block.color = block.gradient.front().color;
    }
    else if(type == EffectPack::BlockType::Alternating)
    {
        block.period_ms = 400;
        block.gradient = {
            {0.0f, ToRGBColor(255, 40, 40)},
            {1.0f, ToRGBColor(40, 80, 255)},
        };
        block.color = block.gradient.front().color;
        block.color_to = block.gradient.back().color;
    }
    else if(type == EffectPack::BlockType::Spin)
    {
        block.direction = EffectPack::Direction::Up; // spin around room vertical
        block.pulse_length = 0.18f;
        block.speed = 1.5f;
        block.gradient = {
            {0.0f, ToRGBColor(40, 200, 255)},
            {0.5f, ToRGBColor(255, 255, 255)},
            {1.0f, ToRGBColor(40, 80, 255)},
        };
        block.color = block.gradient.front().color;
    }
    else if(type == EffectPack::BlockType::Dissolve)
    {
        const int default_dissolve = 2500;
        block.end_ms = std::min(pack_.duration_ms, block.start_ms + default_dissolve);
        block.gradient = {
            {0.0f, ToRGBColor(255, 0, 0)},
            {0.5f, ToRGBColor(255, 128, 0)},
            {1.0f, ToRGBColor(255, 255, 120)},
        };
        block.color = block.gradient.front().color;
    }
    else if(type == EffectPack::BlockType::Fire)
    {
        block.gradient = {
            {0.0f, ToRGBColor(20, 0, 0)},
            {0.35f, ToRGBColor(255, 40, 0)},
            {0.7f, ToRGBColor(255, 160, 0)},
            {1.0f, ToRGBColor(255, 255, 180)},
        };
        block.color = block.gradient[2].color;
    }
    else if(type == EffectPack::BlockType::Snow)
    {
        block.gradient = {
            {0.0f, ToRGBColor(200, 220, 255)},
            {1.0f, ToRGBColor(255, 255, 255)},
        };
        block.color = block.gradient.back().color;
    }
    else if(type == EffectPack::BlockType::Wave)
    {
        block.end_ms = std::min(pack_.duration_ms, block.start_ms + 2500);
        block.pulse_length = 0.35f;
        block.speed = 1.25f;
        EffectPack::ApplyGradientPresetId(&block, "cyber");
    }
    else if(type == EffectPack::BlockType::Scanner)
    {
        block.end_ms = std::min(pack_.duration_ms, block.start_ms + 2200);
        block.pulse_length = 0.12f;
        block.speed = 1.0f;
        EffectPack::ApplyGradientPresetId(&block, "fire");
    }
    else if(type == EffectPack::BlockType::Burst)
    {
        block.end_ms = std::min(pack_.duration_ms, block.start_ms + 1800);
        block.pulse_length = 0.2f;
        EffectPack::ApplyGradientPresetId(&block, "sunset");
        EffectPack::ApplyBuiltinIntensityCurve(&block, "snap");
    }
    else if(type == EffectPack::BlockType::SphereWipe || type == EffectPack::BlockType::Ripple
            || type == EffectPack::BlockType::Orbit || type == EffectPack::BlockType::Meteor
            || type == EffectPack::BlockType::Noise3D || type == EffectPack::BlockType::Plasma
            || type == EffectPack::BlockType::Balls || type == EffectPack::BlockType::Bars
            || type == EffectPack::BlockType::ColorWash || type == EffectPack::BlockType::Wipe
            || type == EffectPack::BlockType::Chase)
    {
        if(type == EffectPack::BlockType::SphereWipe || type == EffectPack::BlockType::Ripple)
        {
            block.end_ms = std::min(pack_.duration_ms, block.start_ms + 2500);
        }
        if(type == EffectPack::BlockType::Meteor)
        {
            block.direction = EffectPack::Direction::Down;
            block.pulse_length = 0.2f;
        }
        if(type == EffectPack::BlockType::Balls)
        {
            block.pulse_length = 0.28f;
            block.end_ms = std::min(pack_.duration_ms, block.start_ms + 3000);
        }
        if(type == EffectPack::BlockType::Bars)
        {
            block.pulse_length = 0.2f;
            block.end_ms = std::min(pack_.duration_ms, block.start_ms + 2000);
        }
        if(type == EffectPack::BlockType::Plasma || type == EffectPack::BlockType::Noise3D
           || type == EffectPack::BlockType::ColorWash)
        {
            block.end_ms = std::min(pack_.duration_ms, block.start_ms + 3000);
        }
        EffectPack::ApplyGradientPresetId(&block, "rainbow");
    }
    else
    {
        EffectPack::EnsureBlockGradient(&block);
    }
    pack_.tracks[(size_t)track].blocks.push_back(block);
    selected_track_ = track;
    selected_block_ = (int)pack_.tracks[(size_t)track].blocks.size() - 1;
    timeline_->setPack(&pack_);
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    if(player_.IsPlaying())
    {
        player_.UpdatePack(pack_);
    }
    applyBlockToForm();
    updateSelectionActions();
    timeline_->update();
    if(status_label_)
    {
        QString space_tip;
        if(block.axis_space == EffectPack::AxisSpace::Sequence)
        {
            space_tip = QStringLiteral("Sequence space — marches along controller order (drag rows to reorder)");
        }
        else if(block.axis_space == EffectPack::AxisSpace::Room)
        {
            space_tip = QStringLiteral("Room space — shared 3D box across devices in this target");
        }
        else
        {
            space_tip = QStringLiteral("Device space — per-controller local axes");
        }
        status_label_->setText(QStringLiteral("Added %1 — %2")
            .arg(QString::fromUtf8(EffectPack::BlockTypeDisplayName(type)))
            .arg(space_tip));
    }
}

