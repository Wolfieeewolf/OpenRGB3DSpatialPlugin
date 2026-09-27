// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackEditorDialog.h"
#include "EffectPackCatalog.h"
#include "EffectPackCurveBar.h"
#include "EffectPackUserCurves.h"
#include "EffectPackGradientBar.h"
#include "EffectPackTimelineWidget.h"
#include "EffectPackToolBar.h"
#include "EffectPackUserGradients.h"
#include "PluginSettingsPaths.h"
#include "EffectPacks/EffectPackApplier.h"
#include "EffectPacks/EffectPackMedia.h"
#include "LEDPosition3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"
#include "ZoneManager3D.h"
#include "EffectCollapsibleSection.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
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

} // namespace

void EffectPackEditorDialog::onToolbarColorClicked(unsigned int rgb)
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Select a timeline block first"));
        }
        return;
    }
    const RGBColor color = (RGBColor)rgb;
    b->color = color;
    b->color_from = color;
    b->color_to = color;
    b->gradient = {{0.0f, color}};
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
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Select a timeline block first"));
        }
        return;
    }
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        EffectPackUserCurves::Apply(b, preset_id, PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr));
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
        EffectPackUserCurves::Apply(b, preset_id, PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr));
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
        if(!b && status_label_)
        {
            status_label_->setText(QStringLiteral("Select a timeline block first"));
        }
        return;
    }
    const RGBColor accent = color_button_
        ? colorFromButton(color_button_)
        : ToRGBColor(255, 80, 40);
    const filesystem::path user_path = PluginSettingsPaths::UserGradientsFile(tab_ ? tab_->resource_manager : nullptr);
    if(!EffectPackUserGradients::Apply(b, preset, user_path)
       && !EffectPack::ApplyGradientPresetId(b, preset.toUtf8().constData(), accent))
    {
        return;
    }
    // Only refresh the property panel when this is the currently selected block
    // (addBlockAt applies a preset to a not-yet-selected block).
    if(b == selectedBlock())
    {
        if(!b->gradient.empty())
        {
            setColorButton(color_button_, b->color);
            setColorButton(color_to_button_, b->color_to);
        }
        syncGradientBar();
    }
    if(timeline_)
    {
        timeline_->update();
    }
}

void EffectPackEditorDialog::refillGradientPresets()
{
    if(!gradient_preset_)
    {
        return;
    }
    const QString current = gradient_preset_->currentData().toString();
    gradient_preset_->blockSignals(true);
    gradient_preset_->clear();
    gradient_preset_->addItem(QStringLiteral("Preset…"), QString());
    const filesystem::path path = PluginSettingsPaths::UserGradientsFile(tab_ ? tab_->resource_manager : nullptr);
    bool separated = false;
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Visible(path))
    {
        if(!separated && !EffectPackUserGradients::IsBuiltin(entry.id))
        {
            gradient_preset_->insertSeparator(gradient_preset_->count());
            separated = true;
        }
        gradient_preset_->addItem(entry.label, entry.id);
    }
    const int idx = gradient_preset_->findData(current);
    gradient_preset_->setCurrentIndex(idx >= 0 ? idx : 0);
    gradient_preset_->blockSignals(false);
    updateSelectionActions();
}

void EffectPackEditorDialog::onSaveUserGradient()
{
    EffectPack::Block* b = selectedBlock();
    if(!b || b->gradient.empty())
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Set a gradient on the selected block before saving"));
        }
        return;
    }
    if(!tab_ || !gradient_preset_)
    {
        return;
    }

    const filesystem::path path = PluginSettingsPaths::UserGradientsFile(tab_->resource_manager);
    const QString selected_id = gradient_preset_->currentData().toString();
    QString selected_label = selected_id;
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Visible(path))
    {
        if(entry.id == selected_id)
        {
            selected_label = entry.label;
            break;
        }
    }

    enum class Mode
    {
        Overwrite,
        SaveAsNew,
    };
    Mode mode = Mode::SaveAsNew;
    if(!selected_id.isEmpty())
    {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(QStringLiteral("Save gradient"));
        box.setText(QStringLiteral("Preset “%1” is selected.").arg(selected_label));
        box.setInformativeText(QStringLiteral("Overwrite it (including renaming), or save as a new preset?"));
        QPushButton* overwrite_btn = box.addButton(QStringLiteral("Overwrite…"), QMessageBox::AcceptRole);
        QPushButton* new_btn = box.addButton(QStringLiteral("Save as new…"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if(box.clickedButton() == overwrite_btn)
        {
            mode = Mode::Overwrite;
        }
        else if(box.clickedButton() == new_btn)
        {
            mode = Mode::SaveAsNew;
        }
        else
        {
            return;
        }
    }

    bool ok = false;
    const QString default_name = (mode == Mode::Overwrite) ? selected_label : QString();
    const QString name = QInputDialog::getText(
                             this,
                             (mode == Mode::Overwrite) ? QStringLiteral("Overwrite gradient")
                                                       : QStringLiteral("Save gradient"),
                             (mode == Mode::Overwrite) ? QStringLiteral("Preset name (can rename defaults)")
                                                       : QStringLiteral("New preset name"),
                             QLineEdit::Normal,
                             default_name,
                             &ok)
                             .trimmed();
    if(!ok || name.isEmpty())
    {
        return;
    }

    if(mode == Mode::Overwrite)
    {
        if(!EffectPackUserGradients::Replace(path, selected_id, name, b->gradient))
        {
            if(status_label_)
            {
                status_label_->setText(QStringLiteral("Could not update gradient preset"));
            }
            return;
        }
        refillGradientPresets();
        const int idx = gradient_preset_->findData(selected_id);
        if(idx >= 0)
        {
            gradient_preset_->setCurrentIndex(idx);
        }
        if(effect_toolbar_)
        {
            effect_toolbar_->reloadUserGradients();
        }
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Updated gradient preset %1").arg(name));
        }
        return;
    }

    const QString id = EffectPackUserGradients::MakeId(name);
    bool exists = false;
    QString existing_label = name;
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Visible(path))
    {
        if(entry.id == id)
        {
            exists = true;
            existing_label = entry.label;
            break;
        }
    }
    if(exists)
    {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            QStringLiteral("Save gradient"),
            QStringLiteral("A preset named “%1” already exists. Overwrite it?").arg(existing_label),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Yes);
        if(reply != QMessageBox::Yes)
        {
            return;
        }
        if(!EffectPackUserGradients::Replace(path, id, name, b->gradient))
        {
            if(status_label_)
            {
                status_label_->setText(QStringLiteral("Could not update gradient preset"));
            }
            return;
        }
    }
    else
    {
        std::vector<EffectPackUserGradients::Entry> entries = EffectPackUserGradients::Load(path);
        entries.push_back({id, name, b->gradient});
        if(!EffectPackUserGradients::Save(path, entries))
        {
            if(status_label_)
            {
                status_label_->setText(QStringLiteral("Could not save gradient preset"));
            }
            return;
        }
    }

    refillGradientPresets();
    const int idx = gradient_preset_->findData(id);
    if(idx >= 0)
    {
        gradient_preset_->setCurrentIndex(idx);
    }
    if(effect_toolbar_)
    {
        effect_toolbar_->reloadUserGradients();
    }
    if(status_label_)
    {
        status_label_->setText(exists ? QStringLiteral("Updated gradient preset %1").arg(name)
                                      : QStringLiteral("Saved gradient preset %1").arg(name));
    }
}

void EffectPackEditorDialog::onDeleteUserGradient()
{
    if(!gradient_preset_)
    {
        return;
    }
    onDeleteGradientPreset(gradient_preset_->currentData().toString());
}

void EffectPackEditorDialog::onOverwriteGradientPreset(const QString& preset_id)
{
    EffectPack::Block* b = selectedBlock();
    if(!b || b->gradient.empty() || preset_id.isEmpty() || !tab_)
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Select a block with a gradient, then double-click a preset to replace it"));
        }
        return;
    }
    QString label = preset_id;
    const filesystem::path path = PluginSettingsPaths::UserGradientsFile(tab_->resource_manager);
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Visible(path))
    {
        if(entry.id == preset_id)
        {
            label = entry.label;
            break;
        }
    }
    if(!EffectPackUserGradients::Replace(path, preset_id, label, b->gradient))
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Could not update gradient preset"));
        }
        return;
    }
    refillGradientPresets();
    if(effect_toolbar_)
    {
        effect_toolbar_->reloadUserGradients();
    }
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Updated gradient preset %1").arg(label));
    }
}

void EffectPackEditorDialog::onDeleteGradientPreset(const QString& preset_id)
{
    if(preset_id.isEmpty() || !tab_)
    {
        return;
    }
    QString label = preset_id;
    const filesystem::path path = PluginSettingsPaths::UserGradientsFile(tab_->resource_manager);
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Visible(path))
    {
        if(entry.id == preset_id)
        {
            label = entry.label;
            break;
        }
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("Delete gradient"),
        QStringLiteral("Remove gradient \"%1\"?").arg(label));
    if(answer != QMessageBox::Yes)
    {
        return;
    }
    if(!EffectPackUserGradients::Remove(path, preset_id))
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Could not delete gradient preset"));
        }
        return;
    }
    refillGradientPresets();
    if(effect_toolbar_)
    {
        effect_toolbar_->reloadUserGradients();
    }
    updateSelectionActions();
    if(status_label_)
    {
        status_label_->setText(EffectPackUserGradients::IsBuiltin(preset_id)
            ? QStringLiteral("Removed gradient preset %1. Right-click the faded preset to restore it.").arg(label)
            : QStringLiteral("Removed gradient preset %1").arg(label));
    }
}

void EffectPackEditorDialog::onResetGradientPreset(const QString& preset_id)
{
    if(!tab_ || !EffectPackUserGradients::IsBuiltin(preset_id))
    {
        return;
    }
    const filesystem::path path = PluginSettingsPaths::UserGradientsFile(tab_->resource_manager);
    if(!EffectPackUserGradients::Reset(path, preset_id))
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Could not reset gradient preset"));
        }
        return;
    }
    refillGradientPresets();
    if(effect_toolbar_)
    {
        effect_toolbar_->reloadUserGradients();
    }
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Restored the default gradient"));
    }
}

namespace
{

EffectPackCatalog::Entry KnobsForSelection(const EffectPack::Block* block, QComboBox* type_combo, const filesystem::path& dir)
{
    const QList<EffectPackCatalog::Entry> entries = EffectPackCatalog::LoadEntries(dir);
    QString id;
    if(block && !block->effect_id.empty())
    {
        id = QString::fromStdString(block->effect_id);
    }
    else if(type_combo)
    {
        id = type_combo->currentData().toString();
    }
    for(const EffectPackCatalog::Entry& entry : entries)
    {
        if(!id.isEmpty() && entry.id == id)
        {
            return entry;
        }
    }
    return {};
}

}

void EffectPackEditorDialog::updatePropVisibility()
{
    EffectPack::Block* b = selectedBlock();
    const bool ok = b != nullptr;
    const filesystem::path effect_dir = PluginSettingsPaths::TimelineBlocksDir(tab_ ? tab_->resource_manager : nullptr);
    const EffectPackCatalog::Entry knobs = KnobsForSelection(b, type_combo_, effect_dir);
    const bool fade = knobs.knob_color_to;
    const bool needs_direction = knobs.knob_direction;
    const bool needs_pulse_length = knobs.knob_pulse;
    const bool needs_media_path = knobs.knob_media_path
        || (b && (b->effect_id == "media_image" || b->effect_id == "media_gif"));
    const bool needs_media_text = knobs.knob_media_text || (b && b->effect_id == "media_text");
    const bool needs_media = needs_media_path || needs_media_text;
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
        // Reverse time is useful for fades/dissolves even when direction knobs are hidden.
        direction_section_->setVisible(ok);
    }
    if(direction_combo_)
    {
        direction_combo_->setEnabled(ok && needs_direction && !custom_axis);
        direction_combo_->setVisible(needs_direction && !custom_axis);
    }
    if(flip_direction_button_)
    {
        flip_direction_button_->setEnabled(ok && needs_direction && !custom_axis);
        flip_direction_button_->setVisible(needs_direction && !custom_axis);
    }
    if(reverse_check_)
    {
        reverse_check_->setEnabled(ok);
        reverse_check_->setVisible(true);
    }
    if(flip_h_check_)
    {
        flip_h_check_->setEnabled(ok);
        flip_h_check_->setVisible(true);
    }
    if(flip_v_check_)
    {
        flip_v_check_->setEnabled(ok);
        flip_v_check_->setVisible(true);
    }
    if(rotate_combo_)
    {
        rotate_combo_->setEnabled(ok);
        rotate_combo_->setVisible(true);
    }
    if(axis_space_combo_)
    {
        axis_space_combo_->setVisible(needs_direction);
        axis_space_combo_->setEnabled(ok && needs_direction);
    }
    if(axis_mode_combo_)
    {
        axis_mode_combo_->setVisible(needs_direction);
        axis_mode_combo_->setEnabled(ok && needs_direction);
    }
    if(axis_yaw_spin_)
    {
        const bool show = custom_axis && needs_direction;
        QWidget* host = axis_yaw_spin_->parentWidget() ? axis_yaw_spin_->parentWidget() : static_cast<QWidget*>(axis_yaw_spin_);
        host->setVisible(show);
        axis_yaw_spin_->setEnabled(ok && show);
        for(QSlider* slider : host->findChildren<QSlider*>(QString(), Qt::FindDirectChildrenOnly))
        {
            slider->setEnabled(ok && show);
        }
    }
    if(axis_pitch_spin_)
    {
        const bool show = custom_axis && needs_direction;
        QWidget* host = axis_pitch_spin_->parentWidget() ? axis_pitch_spin_->parentWidget() : static_cast<QWidget*>(axis_pitch_spin_);
        host->setVisible(show);
        axis_pitch_spin_->setEnabled(ok && show);
        for(QSlider* slider : host->findChildren<QSlider*>(QString(), Qt::FindDirectChildrenOnly))
        {
            slider->setEnabled(ok && show);
        }
    }
    if(speed_section_)
    {
        speed_section_->setVisible(ok);
    }
    if(speed_row_)
    {
        speed_row_->setVisible(ok);
    }
    if(speed_spin_)
    {
        speed_spin_->setEnabled(ok);
    }
    if(period_row_)
    {
        period_row_->setVisible(ok);
    }
    if(period_spin_)
    {
        period_spin_->setEnabled(ok);
    }
    if(pulse_section_)
    {
        pulse_section_->setVisible(needs_pulse_length);
    }
    if(min_intensity_row_)
    {
        min_intensity_row_->setVisible(ok);
    }
    if(min_intensity_spin_)
    {
        min_intensity_spin_->setEnabled(ok);
    }
    if(max_intensity_row_)
    {
        max_intensity_row_->setVisible(ok);
    }
    if(max_intensity_spin_)
    {
        max_intensity_spin_->setEnabled(ok);
    }
    if(media_section_)
    {
        media_section_->setVisible(ok && needs_media);
    }
    if(media_path_edit_)
    {
        media_path_edit_->setVisible(needs_media_path);
        media_path_edit_->setEnabled(ok && needs_media_path);
    }
    if(media_browse_button_)
    {
        media_browse_button_->setVisible(needs_media_path);
        media_browse_button_->setEnabled(ok && needs_media_path);
    }
    if(media_text_edit_)
    {
        media_text_edit_->setVisible(needs_media_text);
        media_text_edit_->setEnabled(ok && needs_media_text);
    }
    if(media_scroll_check_)
    {
        media_scroll_check_->setVisible(needs_media);
        media_scroll_check_->setEnabled(ok && needs_media);
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

void EffectPackEditorDialog::syncCurveBar()
{
    if(!curve_bar_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        curve_bar_->setEnabled(false);
        if(period_curve_bar_)
        {
            period_curve_bar_->setEnabled(false);
        }
        return;
    }
    curve_bar_->setEnabled(true);
    suppress_ui_ = true;
    if(b->intensity_curve.empty())
    {
        curve_bar_->setPoints({{0.0f, 1.0f}, {1.0f, 1.0f}});
    }
    else
    {
        curve_bar_->setPoints(b->intensity_curve);
    }
    suppress_ui_ = false;
    syncPeriodCurveBar();
}

void EffectPackEditorDialog::syncPeriodCurveBar()
{
    if(!period_curve_bar_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        period_curve_bar_->setEnabled(false);
        return;
    }
    period_curve_bar_->setEnabled(true);
    suppress_ui_ = true;
    if(b->period_curve.empty())
    {
        period_curve_bar_->setPoints({{0.0f, 1.0f}, {1.0f, 1.0f}});
    }
    else
    {
        period_curve_bar_->setPoints(b->period_curve);
    }
    suppress_ui_ = false;
}

void EffectPackEditorDialog::refillCurvePresets()
{
    if(!curve_combo_)
    {
        return;
    }
    const QString current = curve_combo_->currentData().toString();
    curve_combo_->blockSignals(true);
    curve_combo_->clear();
    const filesystem::path curves_path = PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr);
    for(const EffectPackUserCurves::Entry& c : EffectPackUserCurves::Load(curves_path))
    {
        curve_combo_->addItem(c.label.isEmpty() ? c.id : c.label, c.id);
    }
    curve_combo_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
    const int idx = curve_combo_->findData(current);
    curve_combo_->setCurrentIndex(idx >= 0 ? idx : curve_combo_->findData(QStringLiteral("custom")));
    curve_combo_->blockSignals(false);
}

void EffectPackEditorDialog::refillPeriodCurvePresets()
{
    if(!period_curve_combo_)
    {
        return;
    }
    const QString current = period_curve_combo_->currentData().toString();
    period_curve_combo_->blockSignals(true);
    period_curve_combo_->clear();
    for(const EffectPackCatalog::CurveEntry& c : EffectPackCatalog::PeriodCurveEntries())
    {
        period_curve_combo_->addItem(QString::fromUtf8(c.label), QString::fromUtf8(c.id));
    }
    period_curve_combo_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
    const int idx = period_curve_combo_->findData(current);
    period_curve_combo_->setCurrentIndex(idx >= 0 ? idx : period_curve_combo_->findData(QStringLiteral("custom")));
    period_curve_combo_->blockSignals(false);
}

void EffectPackEditorDialog::onCurvePointsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b || !curve_bar_)
    {
        return;
    }
    b->intensity_curve = curve_bar_->points();
    if(curve_combo_)
    {
        const int custom = curve_combo_->findData(QStringLiteral("custom"));
        if(custom >= 0)
        {
            suppress_ui_ = true;
            curve_combo_->setCurrentIndex(custom);
            suppress_ui_ = false;
        }
    }
    timeline_->update();
}

void EffectPackEditorDialog::onPeriodCurvePointsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b || !period_curve_bar_)
    {
        return;
    }
    b->period_curve = period_curve_bar_->points();
    // Flat line → treat as no waveform modulation.
    if(b->period_curve.size() == 2
       && std::abs(b->period_curve.front().value - 1.0f) < 1e-3f
       && std::abs(b->period_curve.back().value - 1.0f) < 1e-3f)
    {
        b->period_curve.clear();
    }
    if(period_curve_combo_)
    {
        const char* matched = EffectPack::MatchBuiltinPeriodCurve(b->period_curve);
        const QString id = matched ? QString::fromUtf8(matched) : QStringLiteral("custom");
        const int idx = period_curve_combo_->findData(id);
        suppress_ui_ = true;
        period_curve_combo_->setCurrentIndex(idx >= 0 ? idx : period_curve_combo_->findData(QStringLiteral("custom")));
        suppress_ui_ = false;
    }
    timeline_->update();
}

void EffectPackEditorDialog::onCurvePresetChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b || !curve_combo_)
    {
        return;
    }
    const QString cid = curve_combo_->currentData().toString();
    if(cid == QStringLiteral("custom"))
    {
        return;
    }
    if(cid.isEmpty() || cid == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        EffectPackUserCurves::Apply(b, cid, PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr));
    }
    syncCurveBar();
    timeline_->update();
}

void EffectPackEditorDialog::onPeriodCurvePresetChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b || !period_curve_combo_)
    {
        return;
    }
    const QString cid = period_curve_combo_->currentData().toString();
    if(cid == QStringLiteral("custom"))
    {
        return;
    }
    if(cid.isEmpty() || cid == QStringLiteral("flat"))
    {
        b->period_curve.clear();
    }
    else
    {
        EffectPack::ApplyBuiltinPeriodCurve(b, cid.toUtf8().constData());
    }
    syncPeriodCurveBar();
    timeline_->update();
}

void EffectPackEditorDialog::onBrowseMediaPath()
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    const QString start = media_path_edit_ ? media_path_edit_->text() : QString();
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Choose image or GIF"),
        start,
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif);;GIF (*.gif);;All files (*.*)"));
    if(path.isEmpty())
    {
        return;
    }
    if(media_path_edit_)
    {
        media_path_edit_->setText(path);
    }
    EffectPack::InvalidateMediaCache(b->media_path);
    b->media_path = path.toStdString();
    if(b->effect_id == "media_image" || b->effect_id == "media_gif")
    {
        if(path.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive))
        {
            b->effect_id = "media_gif";
        }
    }
    timeline_->update();
    applyBlockToForm();
}

void EffectPackEditorDialog::onSaveUserCurve()
{
    EffectPack::Block* b = selectedBlock();
    if(!b || !curve_bar_)
    {
        return;
    }
    bool ok = false;
    const QString label = QInputDialog::getText(
        this,
        QStringLiteral("Save intensity curve"),
        QStringLiteral("Preset name:"),
        QLineEdit::Normal,
        QStringLiteral("My curve"),
        &ok);
    if(!ok || label.trimmed().isEmpty())
    {
        return;
    }
    EffectPackUserCurves::Entry entry;
    entry.label = label.trimmed();
    entry.id = sanitizeId(entry.label);
    if(entry.id.isEmpty())
    {
        entry.id = QStringLiteral("curve");
    }
    entry.points = curve_bar_->points();
    b->intensity_curve = entry.points;
    const filesystem::path path = PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr);
    if(!EffectPackUserCurves::Upsert(path, entry))
    {
        QMessageBox::warning(this, QStringLiteral("Intensity Curve"), QStringLiteral("Could not save curve preset."));
        return;
    }
    refillCurvePresets();
    const int idx = curve_combo_->findData(entry.id);
    if(idx >= 0)
    {
        suppress_ui_ = true;
        curve_combo_->setCurrentIndex(idx);
        suppress_ui_ = false;
    }
    timeline_->update();
}

void EffectPackEditorDialog::onFlipDirection()
{
    if(!direction_combo_ || suppress_ui_)
    {
        return;
    }
    const EffectPack::Direction current = (EffectPack::Direction)direction_combo_->currentData().toInt();
    const EffectPack::Direction flipped = EffectPack::OppositeDirection(current);
    const int idx = direction_combo_->findData((int)flipped);
    if(idx >= 0)
    {
        direction_combo_->setCurrentIndex(idx);
    }
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
    if(remove_color_button_)
    {
        remove_color_button_->setEnabled(b->gradient.size() > 1);
    }
    timeline_->update();
}

void EffectPackEditorDialog::onAddColorStop()
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    EffectPack::EnsureBlockGradient(b);
    EffectPack::GradientStop stop;
    if(b->gradient.empty())
    {
        stop.pos = 0.0f;
        stop.color = b->color;
    }
    else
    {
        stop.pos = 1.0f;
        stop.color = b->gradient.back().color;
        // Spread existing stops evenly so new swatches stay discrete.
        const int n = (int)b->gradient.size() + 1;
        for(int i = 0; i < (int)b->gradient.size(); ++i)
        {
            b->gradient[(size_t)i].pos = (n <= 1) ? 0.0f : (float)i / (float)(n - 1);
        }
        stop.pos = 1.0f;
    }
    b->gradient.push_back(stop);
    b->color = b->gradient.front().color;
    b->color_from = b->gradient.front().color;
    b->color_to = b->gradient.back().color;
    syncGradientBar();
    setColorButton(color_button_, b->color);
    setColorButton(color_to_button_, b->color_to);
    if(remove_color_button_)
    {
        remove_color_button_->setEnabled(b->gradient.size() > 1);
    }
    timeline_->update();
}

void EffectPackEditorDialog::onRemoveColorStop()
{
    EffectPack::Block* b = selectedBlock();
    if(!b || b->gradient.size() <= 1)
    {
        return;
    }
    b->gradient.pop_back();
    if(b->gradient.size() == 1)
    {
        b->gradient.front().pos = 0.0f;
    }
    else
    {
        const int n = (int)b->gradient.size();
        for(int i = 0; i < n; ++i)
        {
            b->gradient[(size_t)i].pos = (n <= 1) ? 0.0f : (float)i / (float)(n - 1);
        }
    }
    b->color = b->gradient.front().color;
    b->color_from = b->gradient.front().color;
    b->color_to = b->gradient.back().color;
    syncGradientBar();
    setColorButton(color_button_, b->color);
    setColorButton(color_to_button_, b->color_to);
    if(remove_color_button_)
    {
        remove_color_button_->setEnabled(b->gradient.size() > 1);
    }
    timeline_->update();
}

void EffectPackEditorDialog::applyBlockToForm()
{
    suppress_ui_ = true;
    EffectPack::Block* b = selectedBlock();
    const bool ok = b != nullptr;
    auto setSpinEnabled = [](QWidget* spin, bool enabled) {
        if(!spin)
        {
            return;
        }
        spin->setEnabled(enabled);
        if(QWidget* host = spin->parentWidget())
        {
            const QList<QSlider*> sliders = host->findChildren<QSlider*>(QString(), Qt::FindDirectChildrenOnly);
            for(QSlider* slider : sliders)
            {
                slider->setEnabled(enabled);
            }
        }
    };
    type_combo_->setEnabled(ok);
    setSpinEnabled(start_spin_, ok);
    setSpinEnabled(end_spin_, ok);
    if(start_slider_)
    {
        start_slider_->setEnabled(ok);
    }
    if(end_slider_)
    {
        end_slider_->setEnabled(ok);
    }
    color_button_->setEnabled(ok);
    color_to_button_->setEnabled(ok);
    setSpinEnabled(intensity_spin_, ok);
    setSpinEnabled(min_intensity_spin_, ok);
    setSpinEnabled(max_intensity_spin_, ok);
    setSpinEnabled(speed_spin_, ok);
    setSpinEnabled(period_spin_, ok);
    direction_combo_->setEnabled(ok);
    setSpinEnabled(pulse_length_spin_, ok);
    if(gradient_bar_)
    {
        gradient_bar_->setEnabled(ok);
    }
    if(add_color_button_)
    {
        add_color_button_->setEnabled(ok);
    }
    if(remove_color_button_)
    {
        remove_color_button_->setEnabled(ok && b && b->gradient.size() > 1);
    }
    gradient_preset_->setEnabled(ok);
    if(save_gradient_button_)
    {
        save_gradient_button_->setEnabled(ok);
    }
    if(!ok)
    {
        suppress_ui_ = false;
        updatePropVisibility();
        return;
    }
    EffectPack::EnsureBlockGradient(b);
    const QString effect_id = QString::fromStdString(b->effect_id);
    int type_idx = effect_id.isEmpty() ? -1 : type_combo_->findData(effect_id);
    type_combo_->setCurrentIndex(type_idx >= 0 ? type_idx : 0);
    start_spin_->setValue(b->start_ms);
    end_spin_->setValue(b->end_ms);
    period_spin_->setValue(std::max(50, b->period_ms));
    intensity_spin_->setValue((int)std::lround(std::clamp(b->intensity, 0.0f, 1.0f) * 100.0f));
    min_intensity_spin_->setValue((int)std::lround(std::clamp(b->min_intensity, 0.0f, 1.0f) * 100.0f));
    if(max_intensity_spin_)
    {
        max_intensity_spin_->setValue((int)std::lround(std::clamp(b->max_intensity, 0.0f, 1.0f) * 100.0f));
    }
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
    if(period_curve_combo_)
    {
        const char* matched = EffectPack::MatchBuiltinPeriodCurve(b->period_curve);
        const QString id = matched ? QString::fromUtf8(matched) : QStringLiteral("custom");
        const int cidx = period_curve_combo_->findData(id);
        period_curve_combo_->setCurrentIndex(cidx >= 0 ? cidx : period_curve_combo_->findData(QStringLiteral("custom")));
    }
    if(media_path_edit_)
    {
        media_path_edit_->setText(QString::fromStdString(b->media_path));
    }
    if(media_text_edit_)
    {
        media_text_edit_->setText(QString::fromStdString(b->media_text));
    }
    if(media_scroll_check_)
    {
        media_scroll_check_->setChecked(b->media_scroll);
    }
    if(reverse_check_)
    {
        reverse_check_->setChecked(b->reverse);
    }
    if(flip_h_check_)
    {
        flip_h_check_->setChecked(b->flip_h);
    }
    if(flip_v_check_)
    {
        flip_v_check_->setChecked(b->flip_v);
    }
    if(rotate_combo_)
    {
        const int q = ((b->rotate_quarters % 4) + 4) % 4;
        const int ridx = rotate_combo_->findData(q);
        rotate_combo_->setCurrentIndex(ridx >= 0 ? ridx : 0);
    }
    const EffectPackCatalog::Entry shown = KnobsForSelection(b, type_combo_, PluginSettingsPaths::TimelineBlocksDir(tab_ ? tab_->resource_manager : nullptr));
    setColorButton(color_button_, shown.knob_color_to ? b->color_from : b->color);
    setColorButton(color_to_button_, b->color_to);
    suppress_ui_ = false;
    syncGradientBar();
    syncCurveBar();
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
    const QString picked_id = type_combo_->currentData().toString();
    if(!picked_id.isEmpty())
    {
        b->effect_id = picked_id.toStdString();
    }
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
    if(max_intensity_spin_)
    {
        b->max_intensity = max_intensity_spin_->value() / 100.0f;
    }
    b->speed = (float)speed_spin_->value();
    b->pulse_length = pulse_length_spin_->value() / 100.0f;
    b->direction = (EffectPack::Direction)direction_combo_->currentData().toInt();
    if(reverse_check_)
    {
        b->reverse = reverse_check_->isChecked();
    }
    if(flip_h_check_)
    {
        b->flip_h = flip_h_check_->isChecked();
    }
    if(flip_v_check_)
    {
        b->flip_v = flip_v_check_->isChecked();
    }
    if(rotate_combo_)
    {
        b->rotate_quarters = rotate_combo_->currentData().toInt();
    }
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
            if(curve_bar_)
            {
                b->intensity_curve = curve_bar_->points();
            }
        }
        else if(cid.isEmpty() || cid == QStringLiteral("flat"))
        {
            b->intensity_curve.clear();
        }
        else
        {
            EffectPackUserCurves::Apply(b, cid, PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr));
        }
    }
    if(period_curve_combo_)
    {
        const QString cid = period_curve_combo_->currentData().toString();
        if(cid == QStringLiteral("custom"))
        {
            if(period_curve_bar_)
            {
                b->period_curve = period_curve_bar_->points();
            }
        }
        else if(cid.isEmpty() || cid == QStringLiteral("flat"))
        {
            b->period_curve.clear();
        }
        else
        {
            EffectPack::ApplyBuiltinPeriodCurve(b, cid.toUtf8().constData());
        }
    }
    if(media_path_edit_)
    {
        const std::string new_path = media_path_edit_->text().trimmed().toStdString();
        if(new_path != b->media_path)
        {
            EffectPack::InvalidateMediaCache(b->media_path);
            b->media_path = new_path;
        }
    }
    if(media_text_edit_)
    {
        const std::string new_text = media_text_edit_->text().toStdString();
        if(new_text != b->media_text)
        {
            b->media_text = new_text;
            EffectPack::InvalidateMediaCache();
        }
    }
    if(media_scroll_check_)
    {
        b->media_scroll = media_scroll_check_->isChecked();
    }
    const RGBColor c = colorFromButton(color_button_);
    const RGBColor c2 = colorFromButton(color_to_button_);
    b->color = c;
    b->color_from = c;
    b->color_to = c2;
    const EffectPackCatalog::Entry edited = KnobsForSelection(b, type_combo_, PluginSettingsPaths::TimelineBlocksDir(tab_ ? tab_->resource_manager : nullptr));
    if(edited.knob_color_to)
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
    else if(b->gradient.empty() || b->gradient.size() == 1)
    {
        b->gradient = {{0.0f, c}};
    }
    else
    {
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
        const EffectPackCatalog::Entry picked = KnobsForSelection(b, type_combo_, PluginSettingsPaths::TimelineBlocksDir(tab_ ? tab_->resource_manager : nullptr));
        // Only auto-expand a flat gradient for spatial travel effects — not blink/alternating.
        const bool travels = picked.knob_direction || picked.knob_pulse;
        if(travels && b->gradient.size() >= 2)
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
    applyGradientPresetToBlock(selectedBlock(), preset);
    updateSelectionActions();
}

void EffectPackEditorDialog::onBlockFieldChanged()
{
    applyFormToSelectedBlock();
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

