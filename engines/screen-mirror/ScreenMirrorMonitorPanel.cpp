// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirrorMonitorPanel.h"
#include "DisplayPlaneCaptureUi.h"
#include "ScreenMirror.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "DisplayPlaneCaptureCombo.h"
#include "EffectSliderRow.h"
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
#include <QSlider>
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
        channel->setEnabled(has_capture);
        BindSliderRow(channel, slider, label);
        h->addWidget(channel, 1);
        QObject::connect(slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
        QObject::connect(slider, &QSlider::valueChanged, effect, [label](int v) {
            if(label)
            {
                label->setText(QString::number(v) + QStringLiteral("%"));
            }
        });
        if(label)
        {
            label->setText(QString::number(slider->value()) + QStringLiteral("%"));
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
                                    auto label_fn) {
        if(!row)
        {
            return;
        }
        row->configure(min, max, value, tooltip);
        row->setEnabled(has_capture_source);
        BindSliderRow(row, slider, label);
        QObject::connect(slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
        QObject::connect(slider, &QSlider::valueChanged, effect, [label, label_fn](int v) {
            if(label)
            {
                label->setText(label_fn(v));
            }
        });
        if(label)
        {
            label->setText(label_fn(slider->value()));
        }
    };

    wire_pct_ticks(ui->scaleRow,
                   settings.scale_slider,
                   settings.scale_label,
                   0,
                   300,
                   (int)(settings.scale * 100.0f),
                   QStringLiteral("How far Reach extends from this display plane's centre "
                                  "(0–100% of room corner distance; 101–300% = beyond)."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

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
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->softnessRow,
                   settings.softness_slider,
                   settings.softness_label,
                   0,
                   100,
                   (int)settings.edge_softness,
                   QStringLiteral("Flat bright core around this display plane's centre before fading "
                                  "(0 = pure gradient, 100 ≈ room-wide fill with a soft rim)."),
                   [](int v) { return QString::number(v); });

    wire_pct_ticks(ui->brightnessRow,
                   settings.brightness_slider,
                   settings.brightness_label,
                   0,
                   200,
                   (int)(settings.brightness_multiplier * 100.0f),
                   QStringLiteral(
                       "Overall output level (0-200%). 100% = neutral. Use White rolloff to reduce wash and keep colors vibrant."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->brightnessThresholdRow,
                   settings.brightness_threshold_slider,
                   settings.brightness_threshold_label,
                   0,
                   255,
                   (int)settings.brightness_threshold,
                   QStringLiteral(
                       "Floor for dim pixels (0-255). Uses peak RGB and luma so saturated reds/greens/blues are not crushed. "
                       "0 = off, higher = only brighter content passes at full strength."),
                   [](int v) { return QString::number(v); });

    wire_pct_ticks(ui->whiteRolloffRow,
                   settings.white_rolloff_slider,
                   settings.white_rolloff_label,
                   0,
                   kWhiteRolloffSliderMax,
                   (int)std::lround(settings.white_rolloff * 100.0f),
                   QStringLiteral(
                       "0-125%: strip gray/white (~70-80% sweet spot). 100% = max fog removal. 101-125% = Plus Ultra extra chroma."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->vibranceRow,
                   settings.vibrance_slider,
                   settings.vibrance_label,
                   0,
                   200,
                   (int)std::lround(settings.vibrance * 100.0f),
                   QStringLiteral("Saturation (0-200%). 100% = no change, below 100% = more muted, above 100% = more vivid RGB/CYM."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ScreenMirrorMonitorPanel::PopulateLedTrimHost(effect, settings, ui->ledTrimHost, has_capture_source);

    wire_pct_ticks(ui->letterboxRow,
                   settings.black_bar_letterbox_slider,
                   settings.black_bar_letterbox_label,
                   0,
                   50,
                   (int)std::lround(settings.black_bar_letterbox_percent),
                   QStringLiteral("Crop top and bottom (letterbox). 0 = no crop."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->pillarboxRow,
                   settings.black_bar_pillarbox_slider,
                   settings.black_bar_pillarbox_label,
                   0,
                   50,
                   (int)std::lround(settings.black_bar_pillarbox_percent),
                   QStringLiteral("Crop left and right (pillarbox). 0 = no crop."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->smoothingRow,
                   settings.smoothing_time_slider,
                   settings.smoothing_time_label,
                   0,
                   500,
                   (int)settings.smoothing_time_ms,
                   QStringLiteral("Temporal smoothing to reduce flicker (0-500ms)."),
                   [](int v) { return QString::number(v) + QStringLiteral("ms"); });

    settings.calibration_pattern_check = ui->calibrationPatternCheck;
    ui->calibrationPatternCheck->setEnabled(true);
    ui->calibrationPatternCheck->setChecked(settings.show_calibration_pattern);
    ui->calibrationPatternCheck->setToolTip(
        QStringLiteral("LEDs use a grid, rings, spokes, and quadrant colors (same as the zone preview)."));
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
        &settings.black_bar_letterbox_percent,
        &settings.black_bar_pillarbox_percent,
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
    settings.wave_time_to_edge_label  = zones_widget->getWaveTimeToEdgeLabel();
    settings.capture_area_preview      = zones_widget->getPreviewWidget();
    settings.add_zone_button           = zones_widget->getAddZoneButton();

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
