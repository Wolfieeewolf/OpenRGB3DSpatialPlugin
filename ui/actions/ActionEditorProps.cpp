// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionEditorDialog.h"
#include "ActionCatalog.h"
#include "ActionCurveBar.h"
#include "ActionUserCurves.h"
#include "ActionGradientBar.h"
#include "ActionTimelineWidget.h"
#include "ActionToolBar.h"
#include "ActionUserGradients.h"
#include "PluginSettingsPaths.h"
#include "ActionApplier.h"
#include "ActionMedia.h"
#include "ActionScript.h"
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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
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

void ActionEditorDialog::onToolbarColorClicked(unsigned int rgb)
{
    Action::Clip* b = selectedClip();
    if(!b)
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Select a timeline clip first"));
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
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onToolbarGradientClicked(const QString& preset_id)
{
    applyGradientPresetToClip(selectedClip(), preset_id);
}

void ActionEditorDialog::onToolbarCurveClicked(const QString& preset_id)
{
    Action::Clip* b = selectedClip();
    if(!b)
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Select a timeline clip first"));
        }
        return;
    }
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        ActionUserCurves::Apply(b, preset_id, PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr));
    }
    if(curve_combo_)
    {
        const int idx = curve_combo_->findData(preset_id);
        if(idx >= 0)
        {
            curve_combo_->setCurrentIndex(idx);
        }
    }
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onCurvePresetApplied(int track_index, int clip_index, const QString& preset_id)
{
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& clips = pack_.tracks[(size_t)track_index].clips;
    if(clip_index < 0 || clip_index >= (int)clips.size())
    {
        return;
    }
    selected_track_ = track_index;
    selected_clip_ = clip_index;
    timeline_->setSelectedClip(selected_track_, selected_clip_);
    Action::Clip* b = &clips[(size_t)clip_index];
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        ActionUserCurves::Apply(b, preset_id, PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr));
    }
    markPreviewDirtyIfPlaying();
    applyClipToForm();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onGradientPresetApplied(int track_index, int clip_index, const QString& preset_id)
{
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& clips = pack_.tracks[(size_t)track_index].clips;
    if(clip_index < 0 || clip_index >= (int)clips.size())
    {
        return;
    }
    selected_track_ = track_index;
    selected_clip_ = clip_index;
    timeline_->setSelectedClip(selected_track_, selected_clip_);
    applyGradientPresetToClip(&clips[(size_t)clip_index], preset_id);
    applyClipToForm();
}

void ActionEditorDialog::applyGradientPresetToClip(Action::Clip* b, const QString& preset)
{
    if(!b || preset.isEmpty())
    {
        if(!b && status_label_)
        {
            status_label_->setText(QStringLiteral("Select a timeline clip first"));
        }
        return;
    }
    const RGBColor accent = color_button_
        ? colorFromButton(color_button_)
        : ToRGBColor(255, 80, 40);
    const filesystem::path user_path = PluginSettingsPaths::ActionEditorGradientsFile(tab_ ? tab_->resource_manager : nullptr);
    if(!ActionUserGradients::Apply(b, preset, user_path)
       && !Action::ApplyGradientPresetId(b, preset.toUtf8().constData(), accent))
    {
        return;
    }
    // Only refresh the property panel when this is the currently selected clip
    // (addClipAt applies a preset to a not-yet-selected clip).
    if(b == selectedClip())
    {
        if(!b->gradient.empty())
        {
            setColorButton(color_button_, b->color);
            setColorButton(color_to_button_, b->color_to);
        }
        syncGradientBar();
    }
    markPreviewDirtyIfPlaying();
    if(timeline_)
    {
        timeline_->requestContentRepaint();
    }
}

void ActionEditorDialog::refillGradientPresets()
{
    if(!gradient_preset_)
    {
        return;
    }
    const QString current = gradient_preset_->currentData().toString();
    gradient_preset_->blockSignals(true);
    gradient_preset_->clear();
    gradient_preset_->addItem(QStringLiteral("Preset\u2026"), QString());
    const filesystem::path path = PluginSettingsPaths::ActionEditorGradientsFile(tab_ ? tab_->resource_manager : nullptr);
    bool separated = false;
    for(const ActionUserGradients::Entry& entry : ActionUserGradients::Visible(path))
    {
        if(!separated && !ActionUserGradients::IsBuiltin(entry.id))
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

void ActionEditorDialog::onSaveUserGradient()
{
    Action::Clip* b = selectedClip();
    if(!b || b->gradient.empty())
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Set a gradient on the selected clip before saving"));
        }
        return;
    }
    if(!tab_ || !gradient_preset_)
    {
        return;
    }

    const filesystem::path path = PluginSettingsPaths::ActionEditorGradientsFile(tab_->resource_manager);
    const QString selected_id = gradient_preset_->currentData().toString();
    QString selected_label = selected_id;
    for(const ActionUserGradients::Entry& entry : ActionUserGradients::Visible(path))
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
        box.setText(QStringLiteral("Preset \u201c%1\u201d is selected.").arg(selected_label));
        box.setInformativeText(QStringLiteral("Overwrite it (including renaming), or save as a new preset?"));
        QPushButton* overwrite_btn = box.addButton(QStringLiteral("Overwrite\u2026"), QMessageBox::AcceptRole);
        QPushButton* new_btn = box.addButton(QStringLiteral("Save as new\u2026"), QMessageBox::ActionRole);
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
        if(!ActionUserGradients::Replace(path, selected_id, name, b->gradient))
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

    const QString id = ActionUserGradients::MakeId(name);
    bool exists = false;
    QString existing_label = name;
    for(const ActionUserGradients::Entry& entry : ActionUserGradients::Visible(path))
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
            QStringLiteral("A preset named \u201c%1\u201d already exists. Overwrite it?").arg(existing_label),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Yes);
        if(reply != QMessageBox::Yes)
        {
            return;
        }
        if(!ActionUserGradients::Replace(path, id, name, b->gradient))
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
        std::vector<ActionUserGradients::Entry> entries = ActionUserGradients::Load(path);
        entries.push_back({id, name, b->gradient});
        if(!ActionUserGradients::Save(path, entries))
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

void ActionEditorDialog::onDeleteUserGradient()
{
    if(!gradient_preset_)
    {
        return;
    }
    onDeleteGradientPreset(gradient_preset_->currentData().toString());
}

void ActionEditorDialog::onOverwriteGradientPreset(const QString& preset_id)
{
    Action::Clip* b = selectedClip();
    if(!b || b->gradient.empty() || preset_id.isEmpty() || !tab_)
    {
        if(status_label_)
        {
            status_label_->setText(QStringLiteral("Select a clip with a gradient, then double-click a preset to replace it"));
        }
        return;
    }
    QString label = preset_id;
    const filesystem::path path = PluginSettingsPaths::ActionEditorGradientsFile(tab_->resource_manager);
    for(const ActionUserGradients::Entry& entry : ActionUserGradients::Visible(path))
    {
        if(entry.id == preset_id)
        {
            label = entry.label;
            break;
        }
    }
    if(!ActionUserGradients::Replace(path, preset_id, label, b->gradient))
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

void ActionEditorDialog::onDeleteGradientPreset(const QString& preset_id)
{
    if(preset_id.isEmpty() || !tab_)
    {
        return;
    }
    QString label = preset_id;
    const filesystem::path path = PluginSettingsPaths::ActionEditorGradientsFile(tab_->resource_manager);
    for(const ActionUserGradients::Entry& entry : ActionUserGradients::Visible(path))
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
        QStringLiteral("Remove gradient \u201c%1\u201d?").arg(label));
    if(answer != QMessageBox::Yes)
    {
        return;
    }
    if(!ActionUserGradients::Remove(path, preset_id))
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
        status_label_->setText(ActionUserGradients::IsBuiltin(preset_id)
            ? QStringLiteral("Removed gradient preset %1. Right-click the faded preset to restore it.").arg(label)
            : QStringLiteral("Removed gradient preset %1").arg(label));
    }
}

void ActionEditorDialog::onResetGradientPreset(const QString& preset_id)
{
    if(!tab_ || !ActionUserGradients::IsBuiltin(preset_id))
    {
        return;
    }
    const filesystem::path path = PluginSettingsPaths::ActionEditorGradientsFile(tab_->resource_manager);
    if(!ActionUserGradients::Reset(path, preset_id))
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

ActionCatalog::Entry KnobsForSelection(const Action::Clip* clip, QComboBox* type_combo, const filesystem::path& dir)
{
    const QList<ActionCatalog::Entry> entries = ActionCatalog::LoadEntries(dir);
    QString id;
    if(clip && !clip->effect_id.empty())
    {
        id = QString::fromStdString(clip->effect_id);
    }
    else if(type_combo)
    {
        id = type_combo->currentData().toString();
    }
    for(const ActionCatalog::Entry& entry : entries)
    {
        if(!id.isEmpty() && entry.id == id)
        {
            return entry;
        }
    }
    return {};
}

}

void ActionEditorDialog::updatePropVisibility()
{
    Action::Clip* b = selectedClip();
    const bool ok = b != nullptr;
    const filesystem::path effect_dir = PluginSettingsPaths::ActionToolboxDir(tab_ ? tab_->resource_manager : nullptr);
    const ActionCatalog::Entry knobs = KnobsForSelection(b, type_combo_, effect_dir);
    const bool fade = knobs.knob_color_to;
    const bool needs_direction = knobs.knob_direction;
    const bool needs_pulse_length = knobs.knob_pulse;
    const std::string effect_id = b ? Action::script::FileId(*b) : std::string();
    const bool needs_placement = b && (Action::script::UsesWorld(effect_id)
                                       || Action::script::UsesField(effect_id)
                                       || Action::script::UsesParticles(effect_id)
                                       || Action::script::UsesRibbons(effect_id)
                                       || Action::IsMediaClip(*b)
                                       || b->axis_space == Action::AxisSpace::Room
                                       || b->play_bounds == Action::PlayBounds::RoomBox);
    const bool needs_media_path = knobs.knob_media_path
        || (b && (b->effect_id == "media_image" || b->effect_id == "media_gif"));
    const bool needs_media_text = knobs.knob_media_text || (b && b->effect_id == "media_text");
    const bool needs_media = needs_media_path || needs_media_text;
    const bool custom_axis = axis_mode_combo_
        && (Action::AxisMode)axis_mode_combo_->currentData().toInt() == Action::AxisMode::Custom;

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
    if(play_bounds_combo_)
    {
        play_bounds_combo_->setVisible(ok);
        play_bounds_combo_->setEnabled(ok);
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
    if(placement_section_)
    {
        placement_section_->setVisible(ok && needs_placement);
    }
    if(bright_form_)
    {
        bright_form_->setRowVisible(min_intensity_row_, ok);
        bright_form_->setRowVisible(max_intensity_row_, ok);
    }
    if(min_intensity_spin_)
    {
        min_intensity_spin_->setEnabled(ok);
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
void ActionEditorDialog::syncGradientBar()
{
    if(!gradient_bar_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
    if(!b)
    {
        gradient_bar_->setEnabled(false);
        return;
    }
    Action::EnsureClipGradient(b);
    gradient_bar_->setEnabled(true);
    suppress_ui_ = true;
    gradient_bar_->setStops(b->gradient);
    suppress_ui_ = false;
}

void ActionEditorDialog::syncCurveBar()
{
    if(!curve_bar_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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

void ActionEditorDialog::syncPeriodCurveBar()
{
    if(!period_curve_bar_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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

void ActionEditorDialog::refillCurvePresets()
{
    if(!curve_combo_)
    {
        return;
    }
    const QString current = curve_combo_->currentData().toString();
    curve_combo_->blockSignals(true);
    curve_combo_->clear();
    const filesystem::path curves_path = PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr);
    for(const ActionUserCurves::Entry& c : ActionUserCurves::Load(curves_path))
    {
        curve_combo_->addItem(c.label.isEmpty() ? c.id : c.label, c.id);
    }
    curve_combo_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
    const int idx = curve_combo_->findData(current);
    curve_combo_->setCurrentIndex(idx >= 0 ? idx : curve_combo_->findData(QStringLiteral("custom")));
    curve_combo_->blockSignals(false);
}

void ActionEditorDialog::refillPeriodCurvePresets()
{
    if(!period_curve_combo_)
    {
        return;
    }
    const QString current = period_curve_combo_->currentData().toString();
    period_curve_combo_->blockSignals(true);
    period_curve_combo_->clear();
    for(const ActionCatalog::CurveEntry& c : ActionCatalog::PeriodCurveEntries())
    {
        period_curve_combo_->addItem(QString::fromUtf8(c.label), QString::fromUtf8(c.id));
    }
    period_curve_combo_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
    const int idx = period_curve_combo_->findData(current);
    period_curve_combo_->setCurrentIndex(idx >= 0 ? idx : period_curve_combo_->findData(QStringLiteral("custom")));
    period_curve_combo_->blockSignals(false);
}

void ActionEditorDialog::onCurvePointsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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
    markPreviewDirtyIfPlaying();
    scheduleTimelineRefresh();
}

void ActionEditorDialog::onPeriodCurvePointsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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
        const char* matched = Action::MatchBuiltinPeriodCurve(b->period_curve);
        const QString id = matched ? QString::fromUtf8(matched) : QStringLiteral("custom");
        const int idx = period_curve_combo_->findData(id);
        suppress_ui_ = true;
        period_curve_combo_->setCurrentIndex(idx >= 0 ? idx : period_curve_combo_->findData(QStringLiteral("custom")));
        suppress_ui_ = false;
    }
    markPreviewDirtyIfPlaying();
    scheduleTimelineRefresh();
}

void ActionEditorDialog::onCurvePresetChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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
        ActionUserCurves::Apply(b, cid, PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr));
    }
    syncCurveBar();
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onPeriodCurvePresetChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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
        Action::ApplyBuiltinPeriodCurve(b, cid.toUtf8().constData());
    }
    syncPeriodCurveBar();
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onBrowseMediaPath()
{
    Action::Clip* b = selectedClip();
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
    Action::InvalidateMediaCache(b->media_path);
    b->media_path = path.toStdString();
    if(b->effect_id == "media_image" || b->effect_id == "media_gif")
    {
        if(path.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive))
        {
            b->effect_id = "media_gif";
        }
    }
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
    applyClipToForm();
}

void ActionEditorDialog::onSaveUserCurve()
{
    Action::Clip* b = selectedClip();
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
    ActionUserCurves::Entry entry;
    entry.label = label.trimmed();
    entry.id = sanitizeId(entry.label);
    if(entry.id.isEmpty())
    {
        entry.id = QStringLiteral("curve");
    }
    entry.points = curve_bar_->points();
    b->intensity_curve = entry.points;
    const filesystem::path path = PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr);
    if(!ActionUserCurves::Upsert(path, entry))
    {
        QMessageBox::warning(this, QStringLiteral("Intensity Curve"), QStringLiteral("Could not save curve preset."));
        return;
    }
    refillCurvePresets();
    if(effect_toolbar_)
    {
        effect_toolbar_->reloadUserCurves();
    }
    const int idx = curve_combo_->findData(entry.id);
    if(idx >= 0)
    {
        suppress_ui_ = true;
        curve_combo_->setCurrentIndex(idx);
        suppress_ui_ = false;
    }
    markPreviewDirtyIfPlaying();
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Saved curve preset %1").arg(entry.label));
    }
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onFlipDirection()
{
    if(!direction_combo_ || suppress_ui_)
    {
        return;
    }
    const Action::Direction current = (Action::Direction)direction_combo_->currentData().toInt();
    const Action::Direction flipped = Action::OppositeDirection(current);
    const int idx = direction_combo_->findData((int)flipped);
    if(idx >= 0)
    {
        direction_combo_->setCurrentIndex(idx);
    }
}

void ActionEditorDialog::onGradientStopsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
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
    markPreviewDirtyIfPlaying();
    scheduleTimelineRefresh();
}

void ActionEditorDialog::onAddColorStop()
{
    Action::Clip* b = selectedClip();
    if(!b)
    {
        return;
    }
    Action::EnsureClipGradient(b);
    Action::GradientStop stop;
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
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onRemoveColorStop()
{
    Action::Clip* b = selectedClip();
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
    markPreviewDirtyIfPlaying();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::applyClipToForm()
{
    suppress_ui_ = true;
    Action::Clip* b = selectedClip();
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
    setSpinEnabled(trail_spin_, ok);
    if(blend_combo_)
    {
        blend_combo_->setEnabled(ok);
    }
    setSpinEnabled(speed_spin_, ok);
    setSpinEnabled(period_spin_, ok);
    direction_combo_->setEnabled(ok);
    setSpinEnabled(pulse_length_spin_, ok);
    setSpinEnabled(origin_offset_x_spin_, ok);
    setSpinEnabled(origin_offset_y_spin_, ok);
    setSpinEnabled(origin_offset_z_spin_, ok);
    setSpinEnabled(effect_size_spin_, ok);
    if(origin_ref_combo_)
    {
        origin_ref_combo_->setEnabled(ok);
    }
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
        refreshMotionTable();
        updatePropVisibility();
        updateCentreReadout();
        return;
    }
    Action::EnsureClipGradient(b);
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
    if(blend_combo_)
    {
        const int blend_idx = blend_combo_->findData((int)b->blend);
        blend_combo_->setCurrentIndex(blend_idx >= 0 ? blend_idx : 0);
    }
    if(trail_spin_)
    {
        trail_spin_->setValue(std::clamp(b->trail_ms, 0, Action::kMaxTrailMs));
    }
    speed_spin_->setValue(b->speed);
    pulse_length_spin_->setValue((int)std::lround(std::clamp(b->pulse_length, 0.02f, 1.0f) * 100.0f));
    if(origin_ref_combo_)
    {
        origin_ref_combo_->blockSignals(true);
        origin_ref_combo_->clear();
        origin_ref_combo_->addItem(QStringLiteral("Room center"), QString());
        if(tab_)
        {
            Action::ApplySpatialContext room;
            if(tab_->GetActionRoomWorldBounds(&room))
            {
                for(const Action::NamedWorldPoint& p : room.reference_points)
                {
                    const QString name = QString::fromStdString(p.name);
                    origin_ref_combo_->addItem(name, name);
                }
            }
        }
        const QString want = QString::fromStdString(b->origin_ref);
        int oidx = want.isEmpty() ? 0 : origin_ref_combo_->findData(want);
        if(oidx < 0)
        {
            origin_ref_combo_->addItem(want + QStringLiteral(" (missing)"), want);
            oidx = origin_ref_combo_->count() - 1;
        }
        origin_ref_combo_->setCurrentIndex(oidx);
        origin_ref_combo_->blockSignals(false);
    }
    if(origin_offset_x_spin_)
    {
        origin_offset_x_spin_->setValue((int)std::lround(std::clamp(b->origin_offset_x, -100.0f, 100.0f)));
    }
    if(origin_offset_y_spin_)
    {
        origin_offset_y_spin_->setValue((int)std::lround(std::clamp(b->origin_offset_y, -100.0f, 100.0f)));
    }
    if(origin_offset_z_spin_)
    {
        origin_offset_z_spin_->setValue((int)std::lround(std::clamp(b->origin_offset_z, -100.0f, 100.0f)));
    }
    if(effect_size_spin_)
    {
        effect_size_spin_->setValue((int)std::lround(std::clamp(b->effect_size, 0.05f, 2.0f) * 100.0f));
    }
    const int dir_idx = direction_combo_->findData((int)b->direction);
    direction_combo_->setCurrentIndex(dir_idx >= 0 ? dir_idx : 1);
    if(play_bounds_combo_)
    {
        const int pidx = play_bounds_combo_->findData((int)b->play_bounds);
        play_bounds_combo_->setCurrentIndex(pidx >= 0 ? pidx : 0);
    }
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
        const char* matched = Action::MatchBuiltinIntensityCurve(b->intensity_curve);
        const QString id = matched ? QString::fromUtf8(matched) : QStringLiteral("custom");
        const int cidx = curve_combo_->findData(id);
        curve_combo_->setCurrentIndex(cidx >= 0 ? cidx : curve_combo_->findData(QStringLiteral("custom")));
    }
    if(period_curve_combo_)
    {
        const char* matched = Action::MatchBuiltinPeriodCurve(b->period_curve);
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
    const ActionCatalog::Entry shown = KnobsForSelection(b, type_combo_, PluginSettingsPaths::ActionToolboxDir(tab_ ? tab_->resource_manager : nullptr));
    setColorButton(color_button_, shown.knob_color_to ? b->color_from : b->color);
    setColorButton(color_to_button_, b->color_to);
    refreshMotionTable();
    showMotionAtPlayhead();
    suppress_ui_ = false;
    syncGradientBar();
    syncCurveBar();
    updatePropVisibility();
    updateCentreReadout();
}

void ActionEditorDialog::applyFormToSelectedClip()
{
    if(suppress_ui_)
    {
        return;
    }
    Action::Clip* b = selectedClip();
    if(!b)
    {
        return;
    }
    /* Clip timing before this edit: the placement fields show motion at this fraction. */
    const int motion_prev_start = b->start_ms;
    const int motion_prev_end = b->end_ms;
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
    if(blend_combo_)
    {
        b->blend = (Action::ClipBlend)blend_combo_->currentData().toInt();
    }
    if(trail_spin_)
    {
        b->trail_ms = trail_spin_->value();
    }
    b->speed = (float)speed_spin_->value();
    b->pulse_length = pulse_length_spin_->value() / 100.0f;
    if(origin_ref_combo_)
    {
        b->origin_ref = origin_ref_combo_->currentData().toString().toStdString();
    }
    /* Auto-key: with motion keys on the clip the placement fields show the keyed values at
       the playhead, so a change there sets (or adds) the key at the playhead. */
    if(!b->motion.empty() && origin_offset_x_spin_ && effect_size_spin_ && axis_yaw_spin_ && axis_pitch_spin_)
    {
        const int playhead = timeline_ ? timeline_->playheadMs() : motion_prev_start;
        const float at = std::clamp((float)(playhead - motion_prev_start)
                                        / (float)std::max(1, motion_prev_end - motion_prev_start),
                                    0.0f, 1.0f);
        const Action::MotionKey now = Action::SampleClipMotion(*b, at);
        const Action::MotionKey form = motionKeyFromForm(at);
        auto same = [](float x, float y, float scale) { return std::lround(x * scale) == std::lround(y * scale); };
        if(!same(now.offset_x, form.offset_x, 1.0f) || !same(now.offset_y, form.offset_y, 1.0f)
           || !same(now.offset_z, form.offset_z, 1.0f) || !same(now.size, form.size, 100.0f)
           || !same(now.yaw_deg, form.yaw_deg, 10.0f) || !same(now.pitch_deg, form.pitch_deg, 10.0f))
        {
            Action::UpsertMotionKey(b, form);
            refreshMotionTable();
        }
    }
    if(origin_offset_x_spin_)
    {
        b->origin_offset_x = (float)origin_offset_x_spin_->value();
    }
    if(origin_offset_y_spin_)
    {
        b->origin_offset_y = (float)origin_offset_y_spin_->value();
    }
    if(origin_offset_z_spin_)
    {
        b->origin_offset_z = (float)origin_offset_z_spin_->value();
    }
    if(effect_size_spin_)
    {
        b->effect_size = effect_size_spin_->value() / 100.0f;
    }
    b->direction = (Action::Direction)direction_combo_->currentData().toInt();
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
    if(play_bounds_combo_)
    {
        b->play_bounds = (Action::PlayBounds)play_bounds_combo_->currentData().toInt();
    }
    if(axis_space_combo_)
    {
        b->axis_space = (Action::AxisSpace)axis_space_combo_->currentData().toInt();
    }
    if(axis_mode_combo_)
    {
        b->axis_mode = (Action::AxisMode)axis_mode_combo_->currentData().toInt();
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
            ActionUserCurves::Apply(b, cid, PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr));
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
            Action::ApplyBuiltinPeriodCurve(b, cid.toUtf8().constData());
        }
    }
    if(media_path_edit_)
    {
        const std::string new_path = media_path_edit_->text().trimmed().toStdString();
        if(new_path != b->media_path)
        {
            Action::InvalidateMediaCache(b->media_path);
            b->media_path = new_path;
        }
    }
    if(media_text_edit_)
    {
        const std::string new_text = media_text_edit_->text().toStdString();
        if(new_text != b->media_text)
        {
            b->media_text = new_text;
            Action::InvalidateMediaTextCache();
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
    const ActionCatalog::Entry edited = KnobsForSelection(b, type_combo_, PluginSettingsPaths::ActionToolboxDir(tab_ ? tab_->resource_manager : nullptr));
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
    markPreviewDirtyIfPlaying();
    scheduleTimelineRefresh();
}

void ActionEditorDialog::onTypeChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    applyFormToSelectedClip();
    Action::Clip* b = selectedClip();
    if(b)
    {
        const ActionCatalog::Entry picked = KnobsForSelection(b, type_combo_, PluginSettingsPaths::ActionToolboxDir(tab_ ? tab_->resource_manager : nullptr));
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
        Action::EnsureClipGradient(b);
        markPreviewDirtyIfPlaying();
    }
    updatePropVisibility();
    syncGradientBar();
    scheduleTimelineRefresh();
}

void ActionEditorDialog::onGradientPreset()
{
    if(suppress_ui_)
    {
        return;
    }
    const QString preset = gradient_preset_->currentData().toString();
    applyGradientPresetToClip(selectedClip(), preset);
    updateSelectionActions();
}

void ActionEditorDialog::onClipFieldChanged()
{
    applyFormToSelectedClip();
    if(!suppress_ui_)
    {
        syncGradientBar();
        updateCentreReadout();
        showMotionAtPlayhead();
        // applyFormToSelectedClip already scheduled a coalesced timeline refresh.
    }
}

Action::MotionKey ActionEditorDialog::motionKeyFromForm(float at) const
{
    Action::MotionKey k;
    k.at = std::clamp(at, 0.0f, 1.0f);
    k.offset_x = origin_offset_x_spin_ ? (float)origin_offset_x_spin_->value() : 0.0f;
    k.offset_y = origin_offset_y_spin_ ? (float)origin_offset_y_spin_->value() : 0.0f;
    k.offset_z = origin_offset_z_spin_ ? (float)origin_offset_z_spin_->value() : 0.0f;
    k.size = effect_size_spin_ ? effect_size_spin_->value() / 100.0f : 1.0f;
    k.yaw_deg = axis_yaw_spin_ ? (float)axis_yaw_spin_->value() : 0.0f;
    k.pitch_deg = axis_pitch_spin_ ? (float)axis_pitch_spin_->value() : 0.0f;
    return k;
}

void ActionEditorDialog::showMotionAtPlayhead()
{
    const Action::Clip* b = selectedClip();
    if(!b || b->motion.empty() || !timeline_)
    {
        return;
    }
    const Action::MotionKey k = Action::SampleClipMotion(*b, Action::ClipFractionAt(*b, timeline_->playheadMs()));
    const bool was = suppress_ui_;
    suppress_ui_ = true;
    if(origin_offset_x_spin_) origin_offset_x_spin_->setValue((int)std::lround(k.offset_x));
    if(origin_offset_y_spin_) origin_offset_y_spin_->setValue((int)std::lround(k.offset_y));
    if(origin_offset_z_spin_) origin_offset_z_spin_->setValue((int)std::lround(k.offset_z));
    if(effect_size_spin_) effect_size_spin_->setValue((int)std::lround(k.size * 100.0f));
    if(axis_yaw_spin_) axis_yaw_spin_->setValue(k.yaw_deg);
    if(axis_pitch_spin_) axis_pitch_spin_->setValue(k.pitch_deg);
    suppress_ui_ = was;
}

void ActionEditorDialog::refreshMotionTable()
{
    if(!motion_table_)
    {
        return;
    }
    const Action::Clip* b = selectedClip();
    const QSignalBlocker block(motion_table_);
    const int n = b ? (int)b->motion.size() : 0;
    motion_table_->setRowCount(n);
    for(int i = 0; i < n; ++i)
    {
        const Action::MotionKey& k = b->motion[(size_t)i];
        const double values[7] = {k.at * 100.0, k.offset_x, k.offset_y, k.offset_z, k.size * 100.0, k.yaw_deg, k.pitch_deg};
        for(int c = 0; c < 7; ++c)
        {
            auto* item = new QTableWidgetItem(QString::number(values[c], 'f', (c == 0 || c >= 5) ? 1 : 0));
            item->setTextAlignment(Qt::AlignCenter);
            motion_table_->setItem(i, c, item);
        }
    }
    const bool ok = b != nullptr;
    motion_key_button_->setEnabled(ok);
    motion_remove_button_->setEnabled(n > 0);
    motion_clear_button_->setEnabled(n > 0);
    motion_table_->setEnabled(ok);
    motion_linear_check_->setEnabled(n > 1);
    {
        const QSignalBlocker block_check(motion_linear_check_);
        motion_linear_check_->setChecked(b && b->motion_linear);
    }
    motion_hint_->setText(n == 0
        ? QStringLiteral("No keys: the clip stays where Placement puts it. Put the playhead in the clip, set "
                         "Offset / Size (and Yaw / Pitch for a custom axis), press Key at playhead, then move the "
                         "playhead and change them again to add the next key.")
        : QStringLiteral("%1 key%2. Offset, Size, Yaw and Pitch show the moving values at the playhead; changing "
                         "one sets a key there.").arg(n).arg(n == 1 ? QString() : QStringLiteral("s")));
}

void ActionEditorDialog::motionChanged()
{
    refreshMotionTable();
    showMotionAtPlayhead();
    if(timeline_)
    {
        timeline_->requestContentRepaint();
    }
    markPreviewDirtyIfPlaying();
    scheduleHistoryCommit();
}

void ActionEditorDialog::onMotionKeyAtPlayhead()
{
    applyFormToSelectedClip();
    Action::Clip* b = selectedClip();
    if(!b || !timeline_)
    {
        return;
    }
    const float at = Action::ClipFractionAt(*b, timeline_->playheadMs());
    if(Action::UpsertMotionKey(b, motionKeyFromForm(at)) < 0)
    {
        status_label_->setText(QStringLiteral("This clip already has the most keys it can hold"));
        return;
    }
    motionChanged();
}

void ActionEditorDialog::onMotionRemoveKey()
{
    Action::Clip* b = selectedClip();
    if(!b || b->motion.empty())
    {
        return;
    }
    int idx = motion_table_ ? motion_table_->currentRow() : -1;
    if(idx < 0 || idx >= (int)b->motion.size())
    {
        const float at = timeline_ ? Action::ClipFractionAt(*b, timeline_->playheadMs()) : 0.0f;
        idx = 0;
        for(int i = 1; i < (int)b->motion.size(); ++i)
        {
            if(std::fabs(b->motion[(size_t)i].at - at) < std::fabs(b->motion[(size_t)idx].at - at))
            {
                idx = i;
            }
        }
    }
    b->motion.erase(b->motion.begin() + idx);
    motionChanged();
}

void ActionEditorDialog::onMotionClear()
{
    Action::Clip* b = selectedClip();
    if(!b || b->motion.empty())
    {
        return;
    }
    /* Keep the clip where it is at the playhead, so clearing never makes it jump. */
    const Action::MotionKey k = Action::SampleClipMotion(*b, timeline_ ? Action::ClipFractionAt(*b, timeline_->playheadMs()) : 0.0f);
    b->origin_offset_x = k.offset_x;
    b->origin_offset_y = k.offset_y;
    b->origin_offset_z = k.offset_z;
    b->effect_size = k.size;
    b->axis_yaw_deg = k.yaw_deg;
    b->axis_pitch_deg = k.pitch_deg;
    b->motion.clear();
    motionChanged();
}

void ActionEditorDialog::onMotionTableEdited(int row, int column)
{
    Action::Clip* b = selectedClip();
    if(!b || !motion_table_ || row < 0 || row >= (int)b->motion.size())
    {
        return;
    }
    QTableWidgetItem* item = motion_table_->item(row, column);
    bool ok = false;
    const double v = item ? item->text().trimmed().remove(QLatin1Char('%')).toDouble(&ok) : 0.0;
    if(ok)
    {
        Action::MotionKey& k = b->motion[(size_t)row];
        switch(column)
        {
            case 0: k.at = (float)std::clamp(v / 100.0, 0.0, 1.0); break;
            case 1: k.offset_x = (float)std::clamp(v, -100.0, 100.0); break;
            case 2: k.offset_y = (float)std::clamp(v, -100.0, 100.0); break;
            case 3: k.offset_z = (float)std::clamp(v, -100.0, 100.0); break;
            case 4: k.size = (float)std::clamp(v / 100.0, 0.05, 2.0); break;
            case 5: k.yaw_deg = (float)std::clamp(v, -180.0, 180.0); break;
            case 6: k.pitch_deg = (float)std::clamp(v, -90.0, 90.0); break;
            default: break;
        }
        std::stable_sort(b->motion.begin(), b->motion.end(),
                         [](const Action::MotionKey& a, const Action::MotionKey& c) { return a.at < c.at; });
    }
    motionChanged();
}

void ActionEditorDialog::updateCentreReadout()
{
    if(!placement_centre_label_)
    {
        return;
    }
    const Action::Clip* b = selectedClip();
    if(!b || selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        placement_centre_label_->clear();
        return;
    }
    const Action::Target& target = pack_.tracks[(size_t)selected_track_].target;
    const bool room = b->play_bounds == Action::PlayBounds::RoomBox;
    const bool zone = target.kind == Action::TargetKind::SceneZone;
    const bool device_frame = !room && b->axis_space == Action::AxisSpace::Device;

    /* One centre per clip, decided by its row: a picked reference point wins, then the
       room anchor (whole room), the zone's middle, or the row's own lights. */
    QString default_centre;
    QString scale;
    if(room)
    {
        default_centre = QStringLiteral("room anchor");
        scale = QStringLiteral("the room");
    }
    else if(zone)
    {
        default_centre = QStringLiteral("middle of zone \u201c%1\u201d")
            .arg(QString::fromStdString(target.scene_zone_name));
        scale = QStringLiteral("the zone");
    }
    else if(device_frame)
    {
        default_centre = QStringLiteral("this device, in its own frame");
        scale = QStringLiteral("this device");
    }
    else
    {
        default_centre = QStringLiteral("middle of this row\u2019s lights");
        scale = QStringLiteral("this row\u2019s lights");
    }
    QString centre = default_centre;
    if(!b->origin_ref.empty())
    {
        centre = QStringLiteral("reference point \u201c%1\u201d")
            .arg(QString::fromStdString(b->origin_ref));
    }
    placement_centre_label_->setText(QStringLiteral("Centre: %1. Offsets and size are a percentage of %2.")
                                         .arg(centre, scale));
    if(origin_ref_combo_ && origin_ref_combo_->count() > 0)
    {
        QString first = default_centre;
        if(!first.isEmpty())
        {
            first[0] = first[0].toUpper();
        }
        origin_ref_combo_->setItemText(0, first);
    }
}

void ActionEditorDialog::setColorButton(QPushButton* button, RGBColor color)
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

RGBColor ActionEditorDialog::colorFromButton(QPushButton* button) const
{
    if(!button)
    {
        return ToRGBColor(255, 0, 0);
    }
    return (RGBColor)button->property("rgbColor").toUInt();
}

void ActionEditorDialog::onPickColor()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_button_)), this, QStringLiteral("Clip color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_button_, QColorToRgb(picked));
    applyFormToSelectedClip();
    syncGradientBar();
}

void ActionEditorDialog::onPickColorTo()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_to_button_)), this, QStringLiteral("Fade end color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_to_button_, QColorToRgb(picked));
    applyFormToSelectedClip();
    syncGradientBar();
}

