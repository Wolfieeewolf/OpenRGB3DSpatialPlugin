// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirrorMonitorPanel.h"
#include "ScreenMirror.h"
#include "DisplayPlane3D.h"
#include "EffectSliderRow.h"
#include "PluginUiUtils.h"
#include "ui_ScreenMirrorMonitorSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int kWhiteRolloffSliderMax = 125;
constexpr int kScreenMapRollTicksPerDegree = 2;
constexpr int kScreenMapRollSliderMax = 180 * kScreenMapRollTicksPerDegree;
} // namespace

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
    PluginUiApplyItalicSecondaryLabel(ui->radialCornerInfo);
}

ScreenMirrorMonitorPanel::~ScreenMirrorMonitorPanel()
{
    delete ui;
}

void ScreenMirrorMonitorPanel::ensureWhiteRolloffAndVibranceWired(ScreenMirror* effect,
                                                                  ScreenMirror::MonitorSettings& settings,
                                                                  bool has_capture_source)
{
    if(!effect || !ui || settings.white_rolloff_slider != nullptr)
    {
        return;
    }

    const auto wire_pct_ticks = [&](EffectSliderRow* row,
                                    QSlider*& slider,
                                    QLabel*& label,
                                    int min,
                                    int max,
                                    int value,
                                    int tick_interval,
                                    const QString& tooltip,
                                    auto label_fn) {
        if(!row)
        {
            return;
        }
        row->configure(min, max, value, tooltip);
        row->slider()->setTickPosition(QSlider::TicksBelow);
        row->slider()->setTickInterval(tick_interval);
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

    wire_pct_ticks(ui->whiteRolloffRow,
                   settings.white_rolloff_slider,
                   settings.white_rolloff_label,
                   0,
                   kWhiteRolloffSliderMax,
                   (int)std::lround(settings.white_rolloff * 100.0f),
                   10,
                   QStringLiteral(
                       "0-125%: strip gray/white (~70-80% sweet spot). 100% = max fog removal. 101-125% = Plus Ultra extra chroma."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->vibranceRow,
                   settings.vibrance_slider,
                   settings.vibrance_label,
                   0,
                   200,
                   (int)std::lround(settings.vibrance * 100.0f),
                   25,
                   QStringLiteral(
                       "Saturation (0-200%). 100% = no change, below 100% = more muted, above 100% = more vivid RGB/CYM."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });
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

    const auto wire_pct_ticks = [&](EffectSliderRow* row,
                                    QSlider*& slider,
                                    QLabel*& label,
                                    int min,
                                    int max,
                                    int value,
                                    int tick_interval,
                                    const QString& tooltip,
                                    auto label_fn) {
        if(!row)
        {
            return;
        }
        row->configure(min, max, value, tooltip);
        row->slider()->setTickPosition(QSlider::TicksBelow);
        row->slider()->setTickInterval(tick_interval);
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
                   25,
                   QStringLiteral("Global reach: 0-100% = fill room, 101-300% = beyond room (extreme)."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    settings.scale_invert_check = ui->scaleInvertCheck;
    ui->scaleInvertCheck->setEnabled(has_capture_source);
    ui->scaleInvertCheck->setChecked(settings.scale_inverted);
    QObject::connect(ui->scaleInvertCheck, &QCheckBox::toggled, effect, &ScreenMirror::OnParameterChanged);

    wire_pct_ticks(ui->falloffCurveRow,
                   settings.falloff_curve_slider,
                   settings.falloff_curve_label,
                   50,
                   200,
                   (int)(settings.falloff_curve_exponent * 100.0f),
                   25,
                   QStringLiteral("Falloff curve: 50% = softer (gradual), 100% = linear, 200% = sharper (sudden edge)."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    settings.ref_point_combo = ui->refPointCombo;
    ui->refPointCombo->addItem(QStringLiteral("Room Center"), QVariant(-1));
    ui->refPointCombo->setItemData(
        0,
        QStringLiteral(
            "Falloff distance is measured from the room center. Named reference points appear here when available."),
        Qt::ToolTipRole);
    ui->refPointCombo->setEnabled(has_capture_source);
    ui->refPointCombo->setToolTip(
        QStringLiteral("Anchor for reach/falloff: room center or a saved reference point. "
                       "The list refreshes when reference points change (see 3D layout / reference points)."));
    QObject::connect(ui->refPointCombo, qOverload<int>(&QComboBox::currentIndexChanged), effect,
                     &ScreenMirror::OnParameterChanged);

    wire_pct_ticks(ui->softnessRow,
                   settings.softness_slider,
                   settings.softness_label,
                   0,
                   100,
                   (int)settings.edge_softness,
                   10,
                   QStringLiteral("Edge feathering (0 = hard, 100 = very soft)."),
                   [](int v) { return QString::number(v); });

    wire_pct_ticks(ui->brightnessRow,
                   settings.brightness_slider,
                   settings.brightness_label,
                   0,
                   200,
                   (int)(settings.brightness_multiplier * 100.0f),
                   25,
                   QStringLiteral(
                       "Overall output level (0-200%). 100% = neutral. Use White rolloff to reduce wash and keep colors vibrant."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->brightnessThresholdRow,
                   settings.brightness_threshold_slider,
                   settings.brightness_threshold_label,
                   0,
                   255,
                   (int)settings.brightness_threshold,
                   25,
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
                   10,
                   QStringLiteral(
                       "0-125%: strip gray/white (~70-80% sweet spot). 100% = max fog removal. 101-125% = Plus Ultra extra chroma."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->vibranceRow,
                   settings.vibrance_slider,
                   settings.vibrance_label,
                   0,
                   200,
                   (int)std::lround(settings.vibrance * 100.0f),
                   25,
                   QStringLiteral("Saturation (0-200%). 100% = no change, below 100% = more muted, above 100% = more vivid RGB/CYM."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ScreenMirrorMonitorPanel::PopulateLedTrimHost(effect, settings, ui->ledTrimHost, has_capture_source);

    wire_pct_ticks(ui->letterboxRow,
                   settings.black_bar_letterbox_slider,
                   settings.black_bar_letterbox_label,
                   0,
                   50,
                   (int)std::lround(settings.black_bar_letterbox_percent),
                   5,
                   QStringLiteral("Crop top and bottom (letterbox). 0 = no crop."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    wire_pct_ticks(ui->pillarboxRow,
                   settings.black_bar_pillarbox_slider,
                   settings.black_bar_pillarbox_label,
                   0,
                   50,
                   (int)std::lround(settings.black_bar_pillarbox_percent),
                   5,
                   QStringLiteral("Crop left and right (pillarbox). 0 = no crop."),
                   [](int v) { return QString::number(v) + QStringLiteral("%"); });

    ui->screenMapRollRow->setCaptionText(QStringLiteral("Map roll:"));
    ui->screenMapRollRow->setValueLabelMinimumWidth(52);
    ui->screenMapRollRow->configure(
        -kScreenMapRollSliderMax,
        kScreenMapRollSliderMax,
        (int)std::lround(std::clamp(settings.screen_map_roll_deg, -180.0f, 180.0f) *
                         (float)kScreenMapRollTicksPerDegree),
        QStringLiteral(
            "Optional extra twist in the capture plane after calibrated directional mapping. 0° = geometry only. Steps 0.5°."));
    ui->screenMapRollRow->setEnabled(has_capture_source);
    BindSliderRow(ui->screenMapRollRow, settings.screen_map_roll_slider, settings.screen_map_roll_label);
    QObject::connect(settings.screen_map_roll_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    QObject::connect(settings.screen_map_roll_slider, &QSlider::valueChanged, effect, [&settings](int v) {
        if(settings.screen_map_roll_label)
        {
            const float deg = (float)v / (float)kScreenMapRollTicksPerDegree;
            settings.screen_map_roll_label->setText(QString::number(deg, 'f', 1) + QChar(0x00B0));
        }
    });

    const auto wire_radial_pct = [&](EffectSliderRow* row,
                                     QSlider*& slider,
                                     QLabel*& label,
                                     const QString& caption,
                                     int stored_ui,
                                     const char* tip) {
        row->setCaptionText(caption);
        row->setValueLabelMinimumWidth(44);
        row->configure(0, 100, std::clamp(stored_ui, 0, 100), QString::fromUtf8(tip));
        row->setEnabled(has_capture_source);
        BindSliderRow(row, slider, label);
        QObject::connect(slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
        QObject::connect(slider, &QSlider::valueChanged, effect, [label](int v) {
            if(label)
            {
                label->setText(QString::number(v) + QStringLiteral("%"));
            }
        });
    };

    wire_radial_pct(ui->radialExpansionRow,
                    settings.radial_corner_expansion_slider,
                    settings.radial_corner_expansion_label,
                    QStringLiteral("Corner expansion:"),
                    settings.radial_corner_expansion_ui,
                    "0% = baseline mapping; raise to push toward corners or lower to pinch (internal −50…+50).");
    wire_radial_pct(ui->radialBiasTlRow,
                    settings.radial_corner_bias_tl_slider,
                    settings.radial_corner_bias_tl_label,
                    QStringLiteral("Bottom-left:"),
                    settings.radial_corner_bias_tl_ui,
                    "Bias toward capture bottom-left in that quadrant; 0% = baseline.");
    wire_radial_pct(ui->radialBiasTrRow,
                    settings.radial_corner_bias_tr_slider,
                    settings.radial_corner_bias_tr_label,
                    QStringLiteral("Bottom-right:"),
                    settings.radial_corner_bias_tr_ui,
                    "Bias toward capture bottom-right in that quadrant; 0% = baseline.");
    wire_radial_pct(ui->radialBiasBlRow,
                    settings.radial_corner_bias_bl_slider,
                    settings.radial_corner_bias_bl_label,
                    QStringLiteral("Top-left:"),
                    settings.radial_corner_bias_bl_ui,
                    "Bias toward capture top-left in that quadrant; 0% = baseline.");
    wire_radial_pct(ui->radialBiasBrRow,
                    settings.radial_corner_bias_br_slider,
                    settings.radial_corner_bias_br_label,
                    QStringLiteral("Top-right:"),
                    settings.radial_corner_bias_br_ui,
                    "Bias toward capture top-right in that quadrant; 0% = baseline.");

    ui->cornerStrengthRow->setCaptionText(QStringLiteral("Strength:"));
    ui->cornerStrengthRow->setValueLabelMinimumWidth(40);
    ui->cornerStrengthRow->configure(
        0,
        100,
        (int)std::lround(settings.corner_blend_strength_pct),
        QStringLiteral(
            "Mix edge colors near frame corners when reading the capture. 0% = off (center sample only); 100% = full blend."));
    ui->cornerStrengthRow->setEnabled(has_capture_source);
    BindSliderRow(ui->cornerStrengthRow, settings.corner_blend_strength_slider, settings.corner_blend_strength_label);
    QObject::connect(settings.corner_blend_strength_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    QObject::connect(settings.corner_blend_strength_slider, &QSlider::valueChanged, effect, [&settings](int v) {
        if(settings.corner_blend_strength_label)
        {
            settings.corner_blend_strength_label->setText(QString::number(v) + QStringLiteral("%"));
        }
    });

    ui->cornerZoneRow->setCaptionText(QStringLiteral("Zone width:"));
    ui->cornerZoneRow->setValueLabelMinimumWidth(40);
    ui->cornerZoneRow->configure(
        0,
        32,
        (int)std::lround(settings.corner_blend_zone_pct),
        QStringLiteral(
            "0% = off. Otherwise corner transition size as % of half the active image (after letterbox). Higher = wider, softer corner."));
    ui->cornerZoneRow->setEnabled(has_capture_source);
    BindSliderRow(ui->cornerZoneRow, settings.corner_blend_zone_slider, settings.corner_blend_zone_label);
    QObject::connect(settings.corner_blend_zone_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    QObject::connect(settings.corner_blend_zone_slider, &QSlider::valueChanged, effect, [&settings](int v) {
        if(settings.corner_blend_zone_label)
        {
            settings.corner_blend_zone_label->setText(QString::number(v) + QStringLiteral("%"));
        }
    });

    wire_pct_ticks(ui->smoothingRow,
                   settings.smoothing_time_slider,
                   settings.smoothing_time_label,
                   0,
                   500,
                   (int)settings.smoothing_time_ms,
                   50,
                   QStringLiteral("Temporal smoothing to reduce flicker (0-500ms)."),
                   [](int v) { return QString::number(v) + QStringLiteral("ms"); });

    wire_pct_ticks(ui->blendRow,
                   settings.blend_slider,
                   settings.blend_label,
                   0,
                   100,
                   (int)settings.blend,
                   10,
                   QStringLiteral("Blend with other monitors (0 = isolated, 100 = fully shared)."),
                   [](int v) { return QString::number(v); });

    settings.calibration_pattern_check = ui->calibrationPatternCheck;
    ui->calibrationPatternCheck->setEnabled(true);
    ui->calibrationPatternCheck->setChecked(settings.show_calibration_pattern);
    ui->calibrationPatternCheck->setToolTip(
        QStringLiteral("LEDs use a grid, rings, spokes, and quadrant colors (same as the zone preview). "
                       "Tune radial corners, map roll, and corner blend until geometry looks straight and corners behave like the preview."));
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QObject::connect(ui->calibrationPatternCheck, &QCheckBox::checkStateChanged, effect, [effect]() {
        effect->OnParameterChanged();
        effect->OnCalibrationPatternChanged();
    });
#else
    QObject::connect(ui->calibrationPatternCheck, &QCheckBox::stateChanged, effect, [effect](int) {
        effect->OnParameterChanged();
        effect->OnCalibrationPatternChanged();
    });
#endif

    settings.screen_preview_check = ui->screenPreviewCheck;
    ui->screenPreviewCheck->setEnabled(has_capture_source);
    ui->screenPreviewCheck->setChecked(settings.show_screen_preview);
    ui->screenPreviewCheck->setToolTip(
        QStringLiteral("Show captured screen on display planes in the 3D viewport. Turn off to save CPU/GPU."));
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QObject::connect(ui->screenPreviewCheck, &QCheckBox::checkStateChanged, effect, [effect]() {
        effect->OnParameterChanged();
        effect->OnScreenPreviewChanged();
    });
#else
    QObject::connect(ui->screenPreviewCheck, &QCheckBox::stateChanged, effect, [effect](int) {
        effect->OnParameterChanged();
        effect->OnScreenPreviewChanged();
    });
#endif

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
        settings.front_back_balance,
        settings.left_right_balance,
        settings.top_bottom_balance,
        ui->captureZonesHost);
    zones_widget->setEnabled(has_capture_source);
    zones_widget->SetValueChangedCallback([effect]() { effect->OnParameterChanged(); });
    QObject::connect(zones_widget, &CaptureZonesWidget::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    zones_layout->addWidget(zones_widget);

    settings.capture_zones_widget = zones_widget;
    settings.propagation_speed_slider = zones_widget->getPropagationSpeedSlider();
    settings.propagation_speed_label = zones_widget->getPropagationSpeedLabel();
    settings.wave_decay_slider       = zones_widget->getWaveDecaySlider();
    settings.wave_decay_label          = zones_widget->getWaveDecayLabel();
    settings.wave_time_to_edge_slider = zones_widget->getWaveTimeToEdgeSlider();
    settings.wave_time_to_edge_label  = zones_widget->getWaveTimeToEdgeLabel();
    settings.front_back_balance_slider = zones_widget->getFrontBackBalanceSlider();
    settings.front_back_balance_label  = zones_widget->getFrontBackBalanceLabel();
    settings.left_right_balance_slider = zones_widget->getLeftRightBalanceSlider();
    settings.left_right_balance_label  = zones_widget->getLeftRightBalanceLabel();
    settings.top_bottom_balance_slider = zones_widget->getTopBottomBalanceSlider();
    settings.top_bottom_balance_label  = zones_widget->getTopBottomBalanceLabel();
    settings.capture_area_preview      = zones_widget->getPreviewWidget();
    settings.add_zone_button           = zones_widget->getAddZoneButton();

    if(settings.propagation_speed_slider)
    {
        QObject::connect(settings.propagation_speed_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    }
    if(settings.wave_decay_slider)
    {
        QObject::connect(settings.wave_decay_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    }
    if(settings.wave_time_to_edge_slider)
    {
        QObject::connect(settings.wave_time_to_edge_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    }
    if(settings.front_back_balance_slider)
    {
        QObject::connect(settings.front_back_balance_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    }
    if(settings.left_right_balance_slider)
    {
        QObject::connect(settings.left_right_balance_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    }
    if(settings.top_bottom_balance_slider)
    {
        QObject::connect(settings.top_bottom_balance_slider, &QSlider::valueChanged, effect, &ScreenMirror::OnParameterChanged);
    }
}
