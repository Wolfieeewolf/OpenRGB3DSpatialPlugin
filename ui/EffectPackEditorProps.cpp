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
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
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

void EffectPackEditorDialog::onToolbarColorClicked(unsigned int rgb)
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    b->color = (RGBColor)rgb;
    b->color_from = (RGBColor)rgb;
    if(!b->gradient.empty())
    {
        b->gradient.front().color = (RGBColor)rgb;
    }
    EffectPack::EnsureBlockGradient(b);
    setColorButton(color_button_, b->color);
    syncGradientBar();
    timeline_->update();
}

void EffectPackEditorDialog::onToolbarGradientClicked(const QString& preset_id)
{
    applyGradientPresetToBlock(selectedBlock(), preset_id);
}

void EffectPackEditorDialog::onToolbarCurveClicked(const QString& preset_id)
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        EffectPack::ApplyBuiltinIntensityCurve(b, preset_id.toUtf8().constData());
    }
    if(curve_combo_)
    {
        const int idx = curve_combo_->findData(preset_id);
        if(idx >= 0)
        {
            curve_combo_->setCurrentIndex(idx);
        }
    }
    timeline_->update();
}

void EffectPackEditorDialog::onCurvePresetApplied(int track_index, int block_index, const QString& preset_id)
{
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)track_index].blocks;
    if(block_index < 0 || block_index >= (int)blocks.size())
    {
        return;
    }
    selected_track_ = track_index;
    selected_block_ = block_index;
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    EffectPack::Block* b = &blocks[(size_t)block_index];
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        EffectPack::ApplyBuiltinIntensityCurve(b, preset_id.toUtf8().constData());
    }
    applyBlockToForm();
    timeline_->update();
}

void EffectPackEditorDialog::onGradientPresetApplied(int track_index, int block_index, const QString& preset_id)
{
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)track_index].blocks;
    if(block_index < 0 || block_index >= (int)blocks.size())
    {
        return;
    }
    selected_track_ = track_index;
    selected_block_ = block_index;
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    applyGradientPresetToBlock(&blocks[(size_t)block_index], preset_id);
    applyBlockToForm();
}

void EffectPackEditorDialog::applyGradientPresetToBlock(EffectPack::Block* b, const QString& preset)
{
    if(!b || preset.isEmpty())
    {
        return;
    }
    const RGBColor accent = color_button_
        ? colorFromButton(color_button_)
        : ToRGBColor(255, 80, 40);
    if(!EffectPack::ApplyGradientPresetId(b, preset.toUtf8().constData(), accent))
    {
        return;
    }
    if(!b->gradient.empty())
    {
        setColorButton(color_button_, b->color);
        setColorButton(color_to_button_, b->color_to);
    }
    syncGradientBar();
    if(timeline_)
    {
        timeline_->update();
    }
}

void EffectPackEditorDialog::updatePropVisibility()
{
    EffectPack::Block* b = selectedBlock();
    const bool ok = b != nullptr;
    const EffectPack::BlockType type = b
        ? b->type
        : (EffectPack::BlockType)type_combo_->currentData().toInt();
    const bool fade = type == EffectPack::BlockType::Fade;
    const bool wipe_chase = type == EffectPack::BlockType::Wipe || type == EffectPack::BlockType::Chase
        || type == EffectPack::BlockType::Wave || type == EffectPack::BlockType::Scanner;
    const bool pulse_twinkle = type == EffectPack::BlockType::Pulse || type == EffectPack::BlockType::Twinkle;
    const bool alternating = type == EffectPack::BlockType::Alternating;
    const bool strobe = type == EffectPack::BlockType::Strobe;
    const bool spin = type == EffectPack::BlockType::Spin;
    const bool candle = type == EffectPack::BlockType::Candle;
    const bool dissolve = type == EffectPack::BlockType::Dissolve;
    const bool chase = type == EffectPack::BlockType::Chase || type == EffectPack::BlockType::Scanner;
    const bool colorwash = type == EffectPack::BlockType::ColorWash;
    const bool worldish = EffectPack::BlockNeedsWorldEval(type);
    const bool needs_period = pulse_twinkle || alternating || strobe;
    const bool needs_speed = wipe_chase || colorwash || pulse_twinkle || alternating || strobe || spin
        || dissolve || worldish;
    const bool needs_direction = EffectPack::BlockNeedsDirection(type);
    const bool needs_pulse_length = chase || spin || strobe
        || type == EffectPack::BlockType::Wave
        || type == EffectPack::BlockType::Orbit
        || type == EffectPack::BlockType::Ripple
        || type == EffectPack::BlockType::Meteor
        || type == EffectPack::BlockType::Balls
        || type == EffectPack::BlockType::Burst;
    const bool needs_min_intensity = pulse_twinkle || candle;
    const bool custom_axis = axis_mode_combo_
        && (EffectPack::AxisMode)axis_mode_combo_->currentData().toInt() == EffectPack::AxisMode::Custom;

    if(color_to_row_)
    {
        color_to_row_->setVisible(fade);
    }
    if(color_to_button_)
    {
        color_to_button_->setEnabled(ok && fade);
    }
    if(direction_section_)
    {
        direction_section_->setVisible(needs_direction);
    }
    if(direction_combo_)
    {
        direction_combo_->setEnabled(ok && needs_direction && !custom_axis);
        direction_combo_->setVisible(!custom_axis);
    }
    if(axis_yaw_spin_)
    {
        axis_yaw_spin_->setVisible(custom_axis && needs_direction);
        axis_yaw_spin_->setEnabled(ok && custom_axis && needs_direction);
    }
    if(axis_pitch_spin_)
    {
        axis_pitch_spin_->setVisible(custom_axis && needs_direction);
        axis_pitch_spin_->setEnabled(ok && custom_axis && needs_direction);
    }
    if(speed_section_)
    {
        speed_section_->setVisible(needs_speed);
    }
    if(period_row_)
    {
        period_row_->setVisible(needs_period);
    }
    if(period_spin_)
    {
        period_spin_->setEnabled(ok && needs_period);
    }
    if(pulse_section_)
    {
        pulse_section_->setVisible(needs_pulse_length);
    }
    if(min_intensity_row_)
    {
        min_intensity_row_->setVisible(needs_min_intensity);
    }
    if(min_intensity_spin_)
    {
        min_intensity_spin_->setEnabled(ok && needs_min_intensity);
    }
    updateSelectionActions();
}
void EffectPackEditorDialog::syncGradientBar()
{
    if(!gradient_bar_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        gradient_bar_->setEnabled(false);
        return;
    }
    EffectPack::EnsureBlockGradient(b);
    gradient_bar_->setEnabled(true);
    suppress_ui_ = true;
    gradient_bar_->setStops(b->gradient);
    suppress_ui_ = false;
}

void EffectPackEditorDialog::onGradientStopsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b || !gradient_bar_)
    {
        return;
    }
    b->gradient = gradient_bar_->stops();
    if(!b->gradient.empty() && !gradient_bar_->isDragging())
    {
        b->color = b->gradient.front().color;
        b->color_from = b->gradient.front().color;
        b->color_to = b->gradient.back().color;
        setColorButton(color_button_, b->color);
        setColorButton(color_to_button_, b->color_to);
    }
    timeline_->update();
}

void EffectPackEditorDialog::applyBlockToForm()
{
    suppress_ui_ = true;
    EffectPack::Block* b = selectedBlock();
    const bool ok = b != nullptr;
    type_combo_->setEnabled(ok);
    start_spin_->setEnabled(ok);
    end_spin_->setEnabled(ok);
    color_button_->setEnabled(ok);
    color_to_button_->setEnabled(ok);
    intensity_spin_->setEnabled(ok);
    min_intensity_spin_->setEnabled(ok);
    speed_spin_->setEnabled(ok);
    period_spin_->setEnabled(ok);
    direction_combo_->setEnabled(ok);
    pulse_length_spin_->setEnabled(ok);
    if(gradient_bar_)
    {
        gradient_bar_->setEnabled(ok);
    }
    gradient_preset_->setEnabled(ok);
    if(!ok)
    {
        suppress_ui_ = false;
        updatePropVisibility();
        return;
    }
    EffectPack::EnsureBlockGradient(b);
    const int type_idx = type_combo_->findData((int)b->type);
    type_combo_->setCurrentIndex(type_idx >= 0 ? type_idx : 0);
    start_spin_->setValue(b->start_ms);
    end_spin_->setValue(b->end_ms);
    period_spin_->setValue(std::max(50, b->period_ms));
    intensity_spin_->setValue((int)std::lround(std::clamp(b->intensity, 0.0f, 1.0f) * 100.0f));
    min_intensity_spin_->setValue((int)std::lround(std::clamp(b->min_intensity, 0.0f, 1.0f) * 100.0f));
    speed_spin_->setValue(b->speed);
    pulse_length_spin_->setValue((int)std::lround(std::clamp(b->pulse_length, 0.02f, 1.0f) * 100.0f));
    const int dir_idx = direction_combo_->findData((int)b->direction);
    direction_combo_->setCurrentIndex(dir_idx >= 0 ? dir_idx : 1);
    if(axis_space_combo_)
    {
        const int sidx = axis_space_combo_->findData((int)b->axis_space);
        axis_space_combo_->setCurrentIndex(sidx >= 0 ? sidx : 0);
    }
    if(axis_mode_combo_)
    {
        const int midx = axis_mode_combo_->findData((int)b->axis_mode);
        axis_mode_combo_->setCurrentIndex(midx >= 0 ? midx : 0);
    }
    if(axis_yaw_spin_)
    {
        axis_yaw_spin_->setValue(b->axis_yaw_deg);
    }
    if(axis_pitch_spin_)
    {
        axis_pitch_spin_->setValue(b->axis_pitch_deg);
    }
    if(curve_combo_)
    {
        const char* matched = EffectPack::MatchBuiltinIntensityCurve(b->intensity_curve);
        const QString id = matched ? QString::fromUtf8(matched) : QStringLiteral("custom");
        const int cidx = curve_combo_->findData(id);
        curve_combo_->setCurrentIndex(cidx >= 0 ? cidx : curve_combo_->findData(QStringLiteral("custom")));
    }
    setColorButton(color_button_, b->type == EffectPack::BlockType::Fade ? b->color_from : b->color);
    setColorButton(color_to_button_, b->color_to);
    suppress_ui_ = false;
    syncGradientBar();
    updatePropVisibility();
}

void EffectPackEditorDialog::applyFormToSelectedBlock()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    b->type = (EffectPack::BlockType)type_combo_->currentData().toInt();
    b->start_ms = start_spin_->value();
    b->end_ms = std::max(b->start_ms + 1, end_spin_->value());
    if(end_spin_->value() != b->end_ms)
    {
        const bool prev = suppress_ui_;
        suppress_ui_ = true;
        end_spin_->setValue(b->end_ms);
        suppress_ui_ = prev;
    }
    b->period_ms = period_spin_->value();
    b->intensity = intensity_spin_->value() / 100.0f;
    b->min_intensity = min_intensity_spin_->value() / 100.0f;
    b->speed = (float)speed_spin_->value();
    b->pulse_length = pulse_length_spin_->value() / 100.0f;
    b->direction = (EffectPack::Direction)direction_combo_->currentData().toInt();
    if(axis_space_combo_)
    {
        b->axis_space = (EffectPack::AxisSpace)axis_space_combo_->currentData().toInt();
    }
    if(axis_mode_combo_)
    {
        b->axis_mode = (EffectPack::AxisMode)axis_mode_combo_->currentData().toInt();
    }
    if(axis_yaw_spin_)
    {
        b->axis_yaw_deg = (float)axis_yaw_spin_->value();
    }
    if(axis_pitch_spin_)
    {
        b->axis_pitch_deg = (float)axis_pitch_spin_->value();
    }
    if(curve_combo_)
    {
        const QString cid = curve_combo_->currentData().toString();
        if(cid == QStringLiteral("custom"))
        {
        }
        else if(cid.isEmpty() || cid == QStringLiteral("flat"))
        {
            b->intensity_curve.clear();
        }
        else
        {
            EffectPack::ApplyBuiltinIntensityCurve(b, cid.toUtf8().constData());
        }
    }
    const RGBColor c = colorFromButton(color_button_);
    const RGBColor c2 = colorFromButton(color_to_button_);
    b->color = c;
    b->color_from = c;
    b->color_to = c2;
    if(b->type == EffectPack::BlockType::Fade)
    {
        if(b->gradient.size() < 2)
        {
            b->gradient = {{0.0f, c}, {1.0f, c2}};
        }
        else
        {
            b->gradient.front().color = c;
            b->gradient.back().color = c2;
        }
    }
    else if(b->type == EffectPack::BlockType::Solid || b->gradient.empty() || b->gradient.size() == 1)
    {
        b->gradient = {{0.0f, c}, {1.0f, c}};
    }
    else
    {
        // Multi-stop: keep shape, sync primary colour to the first stop.
        b->gradient.front().color = c;
    }
    timeline_->update();
}

void EffectPackEditorDialog::onTypeChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    applyFormToSelectedBlock();
    EffectPack::Block* b = selectedBlock();
    if(b)
    {
        const bool spatial = b->type == EffectPack::BlockType::Wipe
            || b->type == EffectPack::BlockType::Chase
            || b->type == EffectPack::BlockType::Wave
            || b->type == EffectPack::BlockType::Scanner
            || b->type == EffectPack::BlockType::ColorWash
            || b->type == EffectPack::BlockType::Spin
            || b->type == EffectPack::BlockType::Alternating
            || b->type == EffectPack::BlockType::Dissolve
            || EffectPack::BlockNeedsWorldEval(b->type);
        if(b->type == EffectPack::BlockType::Twinkle)
        {
            // Floor between flashes — keep sparky by default when switching type.
            if(b->min_intensity > 0.25f)
            {
                b->min_intensity = 0.0f;
                min_intensity_spin_->blockSignals(true);
                min_intensity_spin_->setValue(0);
                min_intensity_spin_->blockSignals(false);
            }
            if(b->period_ms < 80)
            {
                b->period_ms = 700;
            }
        }
        if(spatial && b->gradient.size() >= 2)
        {
            const bool flat = b->gradient.front().color == b->gradient.back().color
                && b->gradient.size() <= 2;
            if(flat)
            {
                b->gradient = {
                    {0.0f, ToRGBColor(255, 0, 0)},
                    {0.2f, ToRGBColor(255, 128, 0)},
                    {0.4f, ToRGBColor(255, 255, 0)},
                    {0.6f, ToRGBColor(0, 255, 0)},
                    {0.8f, ToRGBColor(0, 128, 255)},
                    {1.0f, ToRGBColor(128, 0, 255)},
                };
                b->color = b->gradient.front().color;
                b->color_from = b->gradient.front().color;
                b->color_to = b->gradient.back().color;
                setColorButton(color_button_, b->color);
                setColorButton(color_to_button_, b->color_to);
            }
        }
        EffectPack::EnsureBlockGradient(b);
    }
    updatePropVisibility();
    syncGradientBar();
    timeline_->update();
}

void EffectPackEditorDialog::onGradientPreset()
{
    if(suppress_ui_)
    {
        return;
    }
    const QString preset = gradient_preset_->currentData().toString();
    gradient_preset_->blockSignals(true);
    gradient_preset_->setCurrentIndex(0);
    gradient_preset_->blockSignals(false);
    applyGradientPresetToBlock(selectedBlock(), preset);
}

void EffectPackEditorDialog::onBlockFieldChanged()
{
    applyFormToSelectedBlock();
    // Keep gradient bar + timeline preview live for both new and existing blocks.
    if(!suppress_ui_)
    {
        syncGradientBar();
        timeline_->update();
    }
}

void EffectPackEditorDialog::setColorButton(QPushButton* button, RGBColor color)
{
    if(!button)
    {
        return;
    }
    button->setProperty("rgbColor", (uint)color);
    // Compact leading swatch + hex label (do not use full-button swatch sizing).
    const QColor qc = RgbToQColor(color);
    QPixmap pm(22, 16);
    pm.fill(qc);
    button->setIcon(QIcon(pm));
    button->setIconSize(QSize(22, 16));
    button->setText(qc.name().toUpper());
}

RGBColor EffectPackEditorDialog::colorFromButton(QPushButton* button) const
{
    if(!button)
    {
        return ToRGBColor(255, 0, 0);
    }
    return (RGBColor)button->property("rgbColor").toUInt();
}

void EffectPackEditorDialog::onPickColor()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_button_)), this, QStringLiteral("Block color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_button_, QColorToRgb(picked));
    applyFormToSelectedBlock();
    syncGradientBar();
}

void EffectPackEditorDialog::onPickColorTo()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_to_button_)), this, QStringLiteral("Fade end color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_to_button_, QColorToRgb(picked));
    applyFormToSelectedBlock();
    syncGradientBar();
}

