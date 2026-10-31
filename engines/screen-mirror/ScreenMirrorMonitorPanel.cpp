// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirrorMonitorPanel.h"
#include "DisplayPlaneCaptureUi.h"
#include "ScreenMirror.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "DisplayPlaneCaptureCombo.h"
#include "EffectSliderRow.h"
#include "EffectSliderPromote.h"
#include "EffectUiRows.h"
#include "PluginUiUtils.h"
#include "ScreenMirror_Internal.h"
#include "ui_ScreenMirrorMonitorSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
void BindSliderRow(EffectSliderRow* row, QSlider*& slider_out, QLabel*& label_out)
{
    if(!row)
    {
        return;
    }
    slider_out = row->slider();
    label_out  = row->valueLabel();
    /* Spin shows the value; keep optional format label hidden unless a caller shows it. */
    if(row->valueLabel())
    {
        row->valueLabel()->hide();
    }
}

void MoveFormRowTo(QFormLayout* src, QWidget* field, QFormLayout* dest)
{
    if(!src || !field || !dest)
    {
        return;
    }
    QFormLayout::TakeRowResult taken = src->takeRow(field);
    QWidget* label = taken.labelItem ? taken.labelItem->widget() : nullptr;
    QWidget* field_w = taken.fieldItem ? taken.fieldItem->widget() : field;
    dest->addRow(label, field_w);
    delete taken.labelItem;
    delete taken.fieldItem;
}
} // namespace

void ScreenMirrorMonitorPanel::PopulateLedTrimHost(ScreenMirror* effect,
                                                   ScreenMirror::MonitorSettings& settings,
                                                   QWidget* host,
                                                   bool has_capture)
{
    if(!host || !effect)
    {
        return;
    }

    auto* row = new QWidget(host);
    auto* h   = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    const auto add_channel = [&](const QString& title, QSlider*& slider, QLabel*& label, float gain) {
        auto* channel = new EffectSliderRow(row);
        channel->setCaptionText(title);
        channel->setValueLabelMinimumWidth(36);
        channel->configure(50, 200, (int)std::lround(std::clamp(gain, 0.5f, 2.0f) * 100.0f),
                           QStringLiteral(
                               "Per-channel multiplier for this display (50-200%). 100% = neutral. "
                               "Lower green slightly if oranges read yellow on the strip."));
        channel->setSuffix(QStringLiteral("%"));
        channel->setEnabled(has_capture);
        BindSliderRow(channel, slider, label);
        h->addWidget(channel, 1);
        if(QSpinBox* spin = channel->spinBox())
        {
            QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), effect,
                             &ScreenMirror::OnParameterChanged);
        }
        else if(slider)
        {
            QObject::connect(slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
        }
    };

    add_channel(QStringLiteral("R"), settings.led_output_gain_r_slider, settings.led_output_gain_r_label,
                settings.led_output_gain_r);
    add_channel(QStringLiteral("G"), settings.led_output_gain_g_slider, settings.led_output_gain_g_label,
                settings.led_output_gain_g);
    add_channel(QStringLiteral("B"), settings.led_output_gain_b_slider, settings.led_output_gain_b_label,
                settings.led_output_gain_b);

    auto* host_layout = new QVBoxLayout(host);
    host_layout->setContentsMargins(0, 0, 0, 0);
    host_layout->addWidget(row);
}

ScreenMirrorMonitorPanel::ScreenMirrorMonitorPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ScreenMirrorMonitorSettings)
{
    ui->setupUi(this);
    ui->captureCaption->setText(DisplayPlaneCaptureUi::Caption());
    ui->captureCombo->setToolTip(DisplayPlaneCaptureUi::ComboToolTip());
    ui->captureRefreshButton->setToolTip(DisplayPlaneCaptureUi::RefreshToolTip());
    ApplyRightAlignedCaption(ui->captureCaption, 120);
    AlignFormCaptions(ui->reachForm);
    AlignFormCaptions(ui->brightnessForm);
    AlignFormCaptions(ui->blackBarsForm);
    AlignFormCaptions(ui->blendForm);
    AlignFormCaptions(ui->previewForm);
}

ScreenMirrorMonitorPanel::~ScreenMirrorMonitorPanel()
{
    delete ui;
}

void ScreenMirrorMonitorPanel::initialize(ScreenMirror* effect,
                                          ScreenMirror::MonitorSettings& settings,
                                          DisplayPlane3D* plane,
                                          bool has_capture_source)
{
    if(!effect)
    {
        return;
    }

    settings.capture_combo = ui->captureCombo;
    settings.capture_refresh_button = ui->captureRefreshButton;
    FillDisplayPlaneCaptureCombo(ui->captureCombo, plane ? plane->GetCaptureSourceId() : std::string());
    ui->captureCombo->setEnabled(true);
    ui->captureRefreshButton->setEnabled(true);
    QComboBox* capture_combo = ui->captureCombo;
    QObject::connect(capture_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     effect, [effect, plane, capture_combo](int) {
                         if(!effect || !plane)
                         {
                             return;
                         }
                         ApplyCaptureComboToPlane(plane, capture_combo);
                         DisplayPlaneManager::instance()->NotifyEdited();
                         effect->RefreshMonitorStatus();
                     });
    QObject::connect(ui->captureRefreshButton, &QPushButton::clicked, effect, [plane, capture_combo]() {
        FillDisplayPlaneCaptureCombo(capture_combo, plane ? plane->GetCaptureSourceId() : std::string());
    });

    const auto wire_pct_ticks = [&](EffectSliderRow* row,
                                    QSlider*& slider,
                                    QLabel*& label,
                                    int min,
                                    int max,
                                    int value,
                                    const QString& tooltip,
                                    const QString& spin_suffix) {
        if(!row)
        {
            return;
        }
        row->configure(min, max, value, tooltip);
        if(!spin_suffix.isEmpty())
        {
            row->setSuffix(spin_suffix);
        }
        row->setEnabled(has_capture_source);
        BindSliderRow(row, slider, label);
        if(QSpinBox* spin = row->spinBox())
        {
            QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), effect,
                             &ScreenMirror::OnParameterChanged);
        }
        else if(slider)
        {
            QObject::connect(slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
        }
    };

    wire_pct_ticks(ui->scaleRow,
                   settings.scale_slider,
                   settings.scale_label,
                   0,
                   100,
                   (int)std::lround(std::clamp(settings.scale, 0.0f, 1.0f) * 100.0f),
                   QStringLiteral("How far Reach extends from this display plane's centre "
                                  "(0–100% of room corner distance). Use Softness / Falloff curve "
                                  "for the edge shape — overshoot past 100% is not needed."),
                   QStringLiteral("%"));

    settings.scale_invert_check = ui->scaleInvertCheck;
    ui->scaleInvertCheck->setEnabled(has_capture_source);
    ui->scaleInvertCheck->setChecked(settings.scale_inverted);
    ui->scaleInvertCheck->setToolTip(
        QStringLiteral("On (default): bright near this display plane's centre, darker with distance. "
                       "Off: outer-shell mode (far LEDs stay brighter)."));
    QObject::connect(ui->scaleInvertCheck, &QCheckBox::toggled, effect, &ScreenMirror::OnParameterChanged);

    wire_pct_ticks(ui->falloffCurveRow,
                   settings.falloff_curve_slider,
                   settings.falloff_curve_label,
                   50,
                   200,
                   (int)(settings.falloff_curve_exponent * 100.0f),
                   QStringLiteral("Shapes distance falloff from this display's centre: 50% = gradual, "
                                  "100% = linear, 200% = sharper edge."),
                   QStringLiteral("%"));

    wire_pct_ticks(ui->softnessRow,
                   settings.softness_slider,
                   settings.softness_label,
                   0,
                   100,
                   (int)settings.edge_softness,
                   QStringLiteral("Flat bright core around this display plane's centre before fading "
                                  "(0 = pure gradient, 100 ≈ room-wide fill with a soft rim)."),
                   QString());

    wire_pct_ticks(ui->brightnessRow,
                   settings.brightness_slider,
                   settings.brightness_label,
                   0,
                   100,
                   (int)std::lround(std::clamp(settings.brightness_multiplier, 0.0f, 1.0f) * 100.0f),
                   QStringLiteral(
                       "Overall output level (0–100%). 100% = full. For more punch, raise Vibrance instead of overdriving brightness."),
                   QStringLiteral("%"));

    wire_pct_ticks(ui->brightnessThresholdRow,
                   settings.brightness_threshold_slider,
                   settings.brightness_threshold_label,
                   0,
                   255,
                   (int)settings.brightness_threshold,
                   QStringLiteral(
                       "Floor for dim pixels (0-255). Uses peak RGB and luma so saturated reds/greens/blues are not crushed. "
                       "0 = off, higher = only brighter content passes at full strength."),
                   QString());

    wire_pct_ticks(ui->whiteRolloffRow,
                   settings.white_rolloff_slider,
                   settings.white_rolloff_label,
                   0,
                   kWhiteRolloffSliderMax,
                   (int)std::lround(settings.white_rolloff * 100.0f),
                   QStringLiteral(
                       "0-125%: strip gray/white (~70-80% sweet spot). 100% = max fog removal. 101-125% = Plus Ultra extra chroma."),
                   QStringLiteral("%"));

    wire_pct_ticks(ui->vibranceRow,
                   settings.vibrance_slider,
                   settings.vibrance_label,
                   0,
                   200,
                   (int)std::lround(settings.vibrance * 100.0f),
                   QStringLiteral("Saturation (0-200%). 100% = no change, below 100% = more muted, above 100% = more vivid RGB/CYM."),
                   QStringLiteral("%"));

    ScreenMirrorMonitorPanel::PopulateLedTrimHost(effect, settings, ui->ledTrimHost, has_capture_source);

    auto wire_black_bar = [&](EffectSliderRow* row,
                              QSlider*& slider,
                              QLabel*& label,
                              float value,
                              const QString& tip) {
        wire_pct_ticks(row,
                       slider,
                       label,
                       0,
                       49,
                       (int)std::lround(std::clamp(value, 0.0f, 49.0f)),
                       tip,
                       QStringLiteral("%"));
    };
    wire_black_bar(ui->blackBarTopRow,
                   settings.black_bar_top_slider,
                   settings.black_bar_top_label,
                   settings.black_bar_top_percent,
                   QStringLiteral("Crop from the top edge (0–49%)."));
    wire_black_bar(ui->blackBarBottomRow,
                   settings.black_bar_bottom_slider,
                   settings.black_bar_bottom_label,
                   settings.black_bar_bottom_percent,
                   QStringLiteral("Crop from the bottom edge (0–49%)."));
    wire_black_bar(ui->blackBarLeftRow,
                   settings.black_bar_left_slider,
                   settings.black_bar_left_label,
                   settings.black_bar_left_percent,
                   QStringLiteral("Crop from the left edge (0–49%)."));
    wire_black_bar(ui->blackBarRightRow,
                   settings.black_bar_right_slider,
                   settings.black_bar_right_label,
                   settings.black_bar_right_percent,
                   QStringLiteral("Crop from the right edge (0–49%)."));

    settings.black_bar_link_vertical_check = ui->blackBarLinkVerticalCheck;
    ui->blackBarLinkVerticalCheck->setChecked(settings.black_bar_link_vertical);
    settings.black_bar_link_horizontal_check = ui->blackBarLinkHorizontalCheck;
    ui->blackBarLinkHorizontalCheck->setChecked(settings.black_bar_link_horizontal);

    auto sync_linked_pair = [effect](QSpinBox* a, QSpinBox* b, QCheckBox* link) {
        if(!a || !b || !link)
        {
            return;
        }
        /* Disconnect the generic OnParameterChanged from wire_pct_ticks — this
           handler owns change notification for linked pairs. */
        QObject::disconnect(a, QOverload<int>::of(&QSpinBox::valueChanged), effect,
                            &ScreenMirror::OnParameterChanged);
        QObject::disconnect(b, QOverload<int>::of(&QSpinBox::valueChanged), effect,
                            &ScreenMirror::OnParameterChanged);
        QObject::connect(a, QOverload<int>::of(&QSpinBox::valueChanged), effect,
                         [effect, a, b, link](int v) {
                             if(link->isChecked() && b->value() != v)
                             {
                                 QSignalBlocker block(b);
                                 b->setValue(v);
                             }
                             (void)a;
                             effect->OnParameterChanged();
                         });
        QObject::connect(b, QOverload<int>::of(&QSpinBox::valueChanged), effect,
                         [effect, a, b, link](int v) {
                             if(link->isChecked() && a->value() != v)
                             {
                                 QSignalBlocker block(a);
                                 a->setValue(v);
                             }
                             (void)b;
                             effect->OnParameterChanged();
                         });
        QObject::connect(link, &QCheckBox::toggled, effect, [effect, a, b](bool on) {
            if(on)
            {
                const int v = std::max(a->value(), b->value());
                QSignalBlocker ba(a);
                QSignalBlocker bb(b);
                a->setValue(v);
                b->setValue(v);
            }
            effect->OnParameterChanged();
        });
    };
    sync_linked_pair(ui->blackBarTopRow ? ui->blackBarTopRow->spinBox() : nullptr,
                     ui->blackBarBottomRow ? ui->blackBarBottomRow->spinBox() : nullptr,
                     settings.black_bar_link_vertical_check);
    sync_linked_pair(ui->blackBarLeftRow ? ui->blackBarLeftRow->spinBox() : nullptr,
                     ui->blackBarRightRow ? ui->blackBarRightRow->spinBox() : nullptr,
                     settings.black_bar_link_horizontal_check);

    wire_pct_ticks(ui->smoothingRow,
                   settings.smoothing_time_slider,
                   settings.smoothing_time_label,
                   0,
                   500,
                   (int)settings.smoothing_time_ms,
                   QStringLiteral("Temporal smoothing to reduce flicker (0-500ms)."),
                   QStringLiteral(" ms"));

    settings.calibration_pattern_check = ui->calibrationPatternCheck;
    ui->calibrationPatternCheck->setEnabled(true);
    ui->calibrationPatternCheck->setChecked(settings.show_calibration_pattern);
    ui->calibrationPatternCheck->setToolTip(
        QStringLiteral(
            "Drive LEDs from a 3D-friendly R/G/B/Y test image (same as the zone preview). "
            "Brightness, threshold, Reach / falloff, vibrance, and related sliders all apply."));
    QObject::connect(ui->calibrationPatternCheck, &QCheckBox::toggled, effect, [effect](bool) {
        effect->OnParameterChanged();
        effect->OnCalibrationPatternChanged();
    });

    settings.screen_preview_check = ui->screenPreviewCheck;
    ui->screenPreviewCheck->setEnabled(has_capture_source);
    ui->screenPreviewCheck->setChecked(settings.show_screen_preview);
    ui->screenPreviewCheck->setToolTip(
        QStringLiteral("Show captured screen on display planes in the 3D viewport. Turn off to save CPU/GPU."));
    QObject::connect(ui->screenPreviewCheck, &QCheckBox::toggled, effect, [effect](bool) {
        effect->OnParameterChanged();
        effect->OnScreenPreviewChanged();
    });
    auto* zones_layout = new QVBoxLayout(ui->captureZonesHost);
    zones_layout->setContentsMargins(0, 0, 0, 0);
    auto* zones_widget = new CaptureZonesWidget(
        &settings.capture_zones,
        plane,
        &settings.show_calibration_pattern,
        &settings.show_screen_preview,
        &settings.black_bar_top_percent,
        &settings.black_bar_bottom_percent,
        &settings.black_bar_left_percent,
        &settings.black_bar_right_percent,
        settings.propagation_speed_mm_per_ms,
        settings.wave_decay_ms,
        settings.wave_time_to_edge_sec,
        ui->captureZonesHost);
    zones_widget->setEnabled(has_capture_source);
    QObject::connect(zones_widget, &CaptureZonesWidget::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    zones_layout->addWidget(zones_widget);

    settings.capture_zones_widget = zones_widget;
    settings.propagation_speed_slider = zones_widget->getPropagationSpeedSlider();
    settings.propagation_speed_label = zones_widget->getPropagationSpeedLabel();
    settings.wave_decay_slider       = zones_widget->getWaveDecaySlider();
    settings.wave_decay_label          = zones_widget->getWaveDecayLabel();
    settings.wave_time_to_edge_slider = zones_widget->getWaveTimeToEdgeSlider();
    settings.wave_time_to_edge_spin   = zones_widget->getWaveTimeToEdgeSpin();
    settings.capture_area_preview      = zones_widget->getPreviewWidget();
    settings.add_zone_button           = zones_widget->getAddZoneButton();
    settings.remove_zone_button        = zones_widget->getRemoveZoneButton();

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if(!root)
    {
        return;
    }

    auto* extras_box = new QGroupBox(QStringLiteral("Color & falloff extras"), this);
    auto* extras_form = new QFormLayout(extras_box);
    extras_form->setContentsMargins(8, 12, 8, 8);
    MoveFormRowTo(qobject_cast<QFormLayout*>(ui->reachGroup->layout()), ui->falloffCurveRow, extras_form);
    MoveFormRowTo(qobject_cast<QFormLayout*>(ui->brightnessGroup->layout()), ui->brightnessThresholdRow, extras_form);
    MoveFormRowTo(qobject_cast<QFormLayout*>(ui->brightnessGroup->layout()), ui->whiteRolloffRow, extras_form);
    MoveFormRowTo(qobject_cast<QFormLayout*>(ui->brightnessGroup->layout()), ui->vibranceRow, extras_form);
    MoveFormRowTo(qobject_cast<QFormLayout*>(ui->brightnessGroup->layout()), ui->ledTrimHost, extras_form);
    MoveFormRowTo(qobject_cast<QFormLayout*>(ui->previewGroup->layout()), ui->calibrationPatternCheck, extras_form);

    EffectCollapsibleSection* advanced = EffectUiRows::AppendCollapsibleSection(
        root,
        QStringLiteral("Advanced"),
        QStringLiteral("Black bars, capture zones, and extra color/falloff."),
        false);
    if(!advanced)
    {
        return;
    }
    QVBoxLayout* advanced_body = advanced->bodyLayout();
    advanced_body->addWidget(extras_box);
    advanced_body->addWidget(ui->blackBarsGroup);
    if(QGroupBox* zones = zones_widget->getZonesGroup())
    {
        advanced_body->addWidget(zones);
    }
    if(QGroupBox* wave = zones_widget->getWaveGroup())
    {
        const int advanced_index = root->indexOf(advanced);
        if(advanced_index >= 0)
        {
            root->insertWidget(advanced_index, wave);
        }
        else
        {
            root->addWidget(wave);
        }
    }
    root->removeWidget(ui->captureZonesHost);
    ui->captureZonesHost->hide();
}
