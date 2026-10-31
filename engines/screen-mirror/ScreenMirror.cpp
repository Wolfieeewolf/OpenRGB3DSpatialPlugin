// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "ScreenCaptureDownscale.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "DisplayPlaneCaptureCombo.h"
#include "DisplayPlaneCaptureUi.h"
#include "PluginUiUtils.h"
#include "ScreenMirrorMonitorPanel.h"
#include "ScreenMirror_Internal.h"
#include "VirtualReferencePoint3D.h"
#include "EffectListManager3D.h"
#include "PlayerEngines.h"
#include "ui_ScreenMirrorCapturePanel.h"
#include "ui_ScreenMirrorEffectShell.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QStackedWidget>
#include <QLabel>
#include <QFont>
#include <QString>
#include <QSignalBlocker>
#include <algorithm>

void RegisterScreenMirrorEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "ScreenMirror",
        "Screen Mirror",
        "Ambilight",
        "",
        "",
        []() { return new ScreenMirror; },
        SpatialEffectSettingsLayout::CustomOnly);
}

ScreenMirror::ScreenMirror(QWidget* parent)
    : SpatialEffect3D(parent)
    , capture_quality(5)
    , capture_quality_combo(nullptr)
    , capture_backend_mode(0)
    , capture_backend_combo(nullptr)
    , audience_bias_pct_(0.0f)
    , audience_bias_slider_(nullptr)
    , audience_bias_label_(nullptr)
    , audience_hint_label_(nullptr)
    , audience_reference_points_(nullptr)
    , monitor_status_label(nullptr)
    , monitor_help_label(nullptr)
    , monitors_container(nullptr)
    , monitors_layout(nullptr)
    , monitor_select_combo(nullptr)
    , monitor_stack(nullptr)
    , in_parameter_change_(false)
    , frame_cache_refresh_ms_(0)
    , frame_cache_last_render_seq_(0)
    , tick_scale_mm_(10.0f)
    , smooth_ms_(0.0f)
{
}

void ScreenMirror::SetAudienceReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    audience_reference_points_ = ref_points;
}

ScreenMirror::~ScreenMirror() = default;

EffectInfo3D ScreenMirror::GetEffectInfo() const
{
    EffectInfo3D info           = {};
    info.effect_name            = "Screen Mirror";
    info.effect_description =
        "Maps the display into the room along 3D rays so the grid lights the whole screen, not a square slab.";
    info.category               = "Ambilight";
    info.effect_type            = SPATIAL_EFFECT_SCREEN_MIRROR;
    info.is_reversible          = false;
    info.supports_random        = false;
    info.max_speed = 200;
    info.min_speed              = 0;
    info.user_colors            = 0;
    info.has_custom_settings    = true;
    info.needs_3d_origin        = false;
    info.needs_direction        = false;
    info.needs_frequency        = false;
    info.use_size_parameter     = false;
    info.show_room_output_control = false;

    info.show_color_controls    = false;
    info.show_speed_control     = false;
    info.show_brightness_control = false;
    info.show_frequency_control = false;
    info.show_size_control      = false;
    info.show_scale_control     = false;

    return info;
}

void ScreenMirror::SetupCustomUI(QWidget* parent)
{
    capture_quality_combo = nullptr;
    capture_backend_combo = nullptr;
    monitor_status_label = nullptr;
    monitor_help_label = nullptr;
    monitors_container = nullptr;
    monitors_layout = nullptr;
    monitor_select_combo = nullptr;
    monitor_stack = nullptr;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& s = it->second;
        s.group_box = nullptr;
        s.scale_slider = nullptr;
        s.scale_label = nullptr;
        s.scale_invert_check = nullptr;
        s.smoothing_time_slider = nullptr;
        s.smoothing_time_label = nullptr;
        s.brightness_slider = nullptr;
        s.brightness_label = nullptr;
        s.brightness_threshold_slider = nullptr;
        s.brightness_threshold_label = nullptr;
        s.white_rolloff_slider = nullptr;
        s.white_rolloff_label = nullptr;
        s.vibrance_slider = nullptr;
        s.vibrance_label = nullptr;
        s.led_output_gain_r_slider = nullptr;
        s.led_output_gain_r_label = nullptr;
        s.led_output_gain_g_slider = nullptr;
        s.led_output_gain_g_label = nullptr;
        s.led_output_gain_b_slider = nullptr;
        s.led_output_gain_b_label = nullptr;
        s.black_bar_letterbox_slider = nullptr;
        s.black_bar_letterbox_label = nullptr;
        s.black_bar_pillarbox_slider = nullptr;
        s.black_bar_pillarbox_label = nullptr;
        s.softness_slider = nullptr;
        s.softness_label = nullptr;
        s.propagation_speed_slider = nullptr;
        s.propagation_speed_label = nullptr;
        s.wave_decay_slider = nullptr;
        s.wave_decay_label = nullptr;
        s.wave_time_to_edge_slider = nullptr;
        s.wave_time_to_edge_label = nullptr;
        s.falloff_curve_slider = nullptr;
        s.falloff_curve_label = nullptr;
        s.front_back_balance_slider = nullptr;
        s.front_back_balance_label = nullptr;
        s.left_right_balance_slider = nullptr;
        s.left_right_balance_label = nullptr;
        s.top_bottom_balance_slider = nullptr;
        s.top_bottom_balance_label = nullptr;
        s.calibration_pattern_check = nullptr;
        s.screen_preview_check = nullptr;
        s.capture_area_preview = nullptr;
        s.add_zone_button = nullptr;
        s.capture_zones_widget = nullptr;
        s.screen_map_roll_slider = nullptr;
        s.screen_map_roll_label = nullptr;
        s.radial_corner_expansion_slider = nullptr;
        s.radial_corner_expansion_label = nullptr;
        s.radial_corner_bias_tl_slider = nullptr;
        s.radial_corner_bias_tl_label = nullptr;
        s.radial_corner_bias_tr_slider = nullptr;
        s.radial_corner_bias_tr_label = nullptr;
        s.radial_corner_bias_bl_slider = nullptr;
        s.radial_corner_bias_bl_label = nullptr;
        s.radial_corner_bias_br_slider = nullptr;
        s.radial_corner_bias_br_label = nullptr;
    }

    if(rotation_yaw_slider)
    {
        QWidget* rotation_group = rotation_yaw_slider->parentWidget();
        while(rotation_group && !qobject_cast<QGroupBox*>(rotation_group))
        {
            rotation_group = rotation_group->parentWidget();
        }
        if(rotation_group && rotation_group != effect_controls_group)
        {
            rotation_group->setVisible(false);
        }
    }
    if(scale_x_slider)
    {
        QWidget* scale_group = scale_x_slider->parentWidget();
        while(scale_group && !qobject_cast<QGroupBox*>(scale_group))
        {
            scale_group = scale_group->parentWidget();
        }
        if(scale_group && scale_group != effect_controls_group)
        {
            scale_group->setVisible(false);
        }
    }
    if(axis_scale_rot_yaw_slider)
    {
        QWidget* asr_group = axis_scale_rot_yaw_slider->parentWidget();
        while(asr_group && !qobject_cast<QGroupBox*>(asr_group))
        {
            asr_group = asr_group->parentWidget();
        }
        if(asr_group && asr_group != effect_controls_group)
        {
            asr_group->setVisible(false);
        }
    }
    if(intensity_slider)
    {
        QWidget* intensity_widget = intensity_slider->parentWidget();
        if(intensity_widget && intensity_widget != effect_controls_group)
        {
            intensity_widget->setVisible(false);
        }
    }
    if(sharpness_slider)
    {
        QWidget* sharpness_widget = sharpness_slider->parentWidget();
        if(sharpness_widget && sharpness_widget != effect_controls_group)
        {
            sharpness_widget->setVisible(false);
        }
    }
    
    QWidget* container = new QWidget();
    Ui::ScreenMirrorEffectShell shell_ui;
    shell_ui.setupUi(container);

    auto* capture_panel = new QWidget(shell_ui.capturePanelHost);
    Ui::ScreenMirrorCapturePanel capture_ui;
    capture_ui.setupUi(capture_panel);
    capture_ui.statusInfoLabel->setText(DisplayPlaneCaptureUi::ScreenMirrorAssignHint());
    PluginUiApplyItalicSecondaryLabel(capture_ui.statusInfoLabel);
    monitor_status_label = capture_ui.monitorStatusLabel;
    monitor_help_label   = nullptr;
    {
        QFont sf = monitor_status_label->font();
        sf.setBold(true);
        sf.setPointSizeF(sf.pointSizeF() * 1.15);
        monitor_status_label->setFont(sf);
    }

    capture_quality_combo = capture_ui.captureQualityCombo;
    capture_quality_combo->clear();
    capture_quality_combo->addItem("Native (match display)", QVariant(kScreenMirrorQualityNative));
    capture_quality_combo->addItem("Low (320×180)", QVariant(0));
    capture_quality_combo->addItem("Medium (480×270)", QVariant(1));
    capture_quality_combo->addItem("High (640×360)", QVariant(2));
    capture_quality_combo->addItem("Ultra (960×540)", QVariant(3));
    capture_quality_combo->addItem("Maximum (1280×720)", QVariant(4));
    capture_quality_combo->addItem("1080p (1920×1080)", QVariant(5));
    capture_quality_combo->addItem("1440p (2560×1440)", QVariant(6));
    capture_quality_combo->addItem("4K (3840×2160)", QVariant(7));
    capture_quality_combo->setToolTip(
        "Windows DXGI: capped sizes GPU-scale on D3D11, then CPU-read for LED mapping "
        "(default 1080p). Native = full display copy to CPU (heavier). "
        "Never upscales. Linux/macOS scale on CPU after QScreen grab. "
        "LED colors always sample on CPU (plane UV, zones, falloff, wave) — that is intentional.");
    SelectCaptureQualityCombo(capture_quality);
    {
        int w = 1920;
        int h = 1080;
        ScreenMirrorQualityToSize(capture_quality, w, h);
        ScreenCaptureManager::Instance().SetDownscaleResolution(w, h);
    }

    capture_backend_combo = capture_ui.captureBackendCombo;
    capture_backend_combo->addItem("Auto (DXGI, GDI on stall)", QVariant(0));
    capture_backend_combo->addItem("DXGI only", QVariant(1));
    capture_backend_combo->addItem("GDI only", QVariant(2));
    capture_backend_combo->setCurrentIndex(std::clamp(capture_backend_mode, 0, 2));
    capture_backend_combo->setToolTip(
        "Windows: Auto stays on DXGI and reuses the last frame when the desktop is idle; "
        "GDI only after a real stall/access loss, then a dwell before DXGI retry. "
        "DXGI requests SDR BGRA (HDR duplication is refused). "
        "No effect on Linux/macOS (QScreen grab).");

    QVBoxLayout* capture_host_layout = new QVBoxLayout(shell_ui.capturePanelHost);
    capture_host_layout->setContentsMargins(0, 0, 0, 0);
    capture_host_layout->addWidget(capture_panel);

    audience_hint_label_ = shell_ui.audienceHintLabel;
    PluginUiApplyItalicSecondaryLabel(audience_hint_label_);
    audience_bias_slider_ = shell_ui.audienceBiasSlider;
    audience_bias_label_ = shell_ui.audienceBiasLabel;
    audience_bias_slider_->setValue((int)std::lround(audience_bias_pct_));
    audience_bias_label_->setText(QString::number((int)std::lround(audience_bias_pct_)) + QStringLiteral("%"));
    connect(audience_bias_slider_, &QSlider::valueChanged, this, [this](int v) {
        audience_bias_pct_ = (float)std::clamp(v, 0, 100);
        if(audience_bias_label_)
        {
            audience_bias_label_->setText(QString::number(v) + QStringLiteral("%"));
        }
        OnParameterChanged();
    });

    ScreenCaptureManager::Instance().SetWindowsCaptureBackendMode(capture_backend_mode);
    connect(capture_backend_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        capture_backend_mode = std::clamp(index, 0, 2);
        ScreenCaptureManager::Instance().SetWindowsCaptureBackendMode(capture_backend_mode);
        OnParameterChanged();
    });
    connect(capture_quality_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        capture_quality = ScreenMirrorClampQuality(capture_quality_combo->currentData().toInt());
        int w = 320;
        int h = 180;
        ScreenMirrorQualityToSize(capture_quality, w, h);
        ScreenCaptureManager::Instance().SetDownscaleResolution(w, h);
        OnParameterChanged();
    });

    monitors_container = shell_ui.monitorsGroup;
    monitors_layout    = shell_ui.monitorsLayout;

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();

        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            MonitorSettings new_settings;
            new_settings.enabled = DefaultMonitorEnabledForPlane(plane);
            settings_it = monitor_settings.emplace(plane_name, new_settings).first;
        }
        MonitorSettings& settings = settings_it->second;

        if(!settings.group_box)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
    }

    if(monitor_settings.empty())
    {
        QLabel* no_monitors_label = new QLabel(
            QStringLiteral("No monitors configured. Set up Display Planes first."));
        PluginUiApplyItalicSecondaryLabel(no_monitors_label);
        monitors_layout->addWidget(no_monitors_label);
    }

    RefreshMonitorStatus();

    AddWidgetToParent(container, parent);
}

void ScreenMirror::OnScreenPreviewChanged()
{
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_screen_preview && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit ScreenPreviewChanged(any_enabled);
}

void ScreenMirror::OnCalibrationPatternChanged()
{
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_calibration_pattern && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit CalibrationPatternChanged(any_enabled);
    
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }
    }
}

bool ScreenMirror::ShouldShowCalibrationPattern(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        const MonitorSettings& m = it->second;
        const bool row_on = m.group_box ? m.group_box->isChecked() : m.enabled;
        return m.show_calibration_pattern && row_on;
    }
    return false;
}

bool ScreenMirror::ShouldShowScreenPreview(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        const MonitorSettings& m = it->second;
        const bool row_on = m.group_box ? m.group_box->isChecked() : m.enabled;
        return m.show_screen_preview && row_on;
    }
    return false;
}

void ScreenMirror::EnsureMonitorPicker()
{
    if(!monitors_layout || monitor_stack)
    {
        return;
    }

    auto* picker_row = new QWidget();
    auto* picker_layout = new QHBoxLayout(picker_row);
    picker_layout->setContentsMargins(0, 0, 0, 4);
    picker_layout->setSpacing(8);
    auto* picker_label = new QLabel(QStringLiteral("Display:"));
    monitor_select_combo = new QComboBox();
    monitor_select_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    monitor_select_combo->setToolTip(
        QStringLiteral("Choose which display plane's settings to edit. Only that display's controls are shown."));
    picker_layout->addWidget(picker_label);
    picker_layout->addWidget(monitor_select_combo, 1);

    monitor_stack = new QStackedWidget();
    monitors_layout->addWidget(picker_row);
    monitors_layout->addWidget(monitor_stack);

    connect(monitor_select_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(!monitor_stack || !monitor_select_combo || index < 0)
        {
            return;
        }
        const QString plane_key = monitor_select_combo->itemData(index).toString();
        ShowMonitorSettingsPage(plane_key.toStdString());
    });
}

void ScreenMirror::ShowMonitorSettingsPage(const std::string& plane_name)
{
    if(!monitor_stack || plane_name.empty())
    {
        return;
    }
    std::map<std::string, MonitorSettings>::iterator it = monitor_settings.find(plane_name);
    if(it == monitor_settings.end() || !it->second.group_box)
    {
        return;
    }
    const int page = monitor_stack->indexOf(it->second.group_box);
    if(page >= 0)
    {
        monitor_stack->setCurrentIndex(page);
    }
}

void ScreenMirror::SyncMonitorPickerLabels()
{
    if(!monitor_select_combo)
    {
        return;
    }
    QSignalBlocker block(monitor_select_combo);
    for(int i = 0; i < monitor_select_combo->count(); ++i)
    {
        const std::string plane_name = monitor_select_combo->itemData(i).toString().toStdString();
        DisplayPlane3D* plane = FindDisplayPlaneByName(plane_name);
        QString label = QString::fromStdString(plane_name);
        if(plane && plane->GetCaptureSourceId().empty())
        {
            label += QStringLiteral(" (No Capture Source)");
        }
        monitor_select_combo->setItemText(i, label);
    }
}

void ScreenMirror::CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings)
{
    if(!plane || !monitors_layout)
    {
        return;
    }

    EnsureMonitorPicker();
    if(!monitor_stack || !monitor_select_combo)
    {
        return;
    }

    const bool has_capture_source = !plane->GetCaptureSourceId().empty();
    QString display_name = QString::fromStdString(plane->GetName());
    if(!has_capture_source)
    {
        display_name += QStringLiteral(" (No Capture Source)");
    }

    settings.group_box = new QGroupBox(QStringLiteral("Enabled"));
    settings.group_box->setCheckable(true);
    settings.group_box->setChecked(settings.enabled &&
                                   (has_capture_source || settings.show_calibration_pattern || settings.show_screen_preview));
    settings.group_box->setEnabled(true);
    settings.group_box->setFlat(true);
    settings.group_box->setToolTip(has_capture_source
                                      ? QStringLiteral("Enable or disable this display's influence on LEDs.")
                                      : QStringLiteral("Pick the Windows display this plane should capture."));
    connect(settings.group_box, &QGroupBox::toggled, this, &ScreenMirror::OnParameterChanged);

    auto* panel = new ScreenMirrorMonitorPanel(settings.group_box);
    panel->initialize(this, settings, plane, has_capture_source);

    auto* outer = new QVBoxLayout(settings.group_box);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);
    outer->addWidget(panel);

    monitor_stack->addWidget(settings.group_box);
    monitor_select_combo->addItem(display_name, QString::fromStdString(plane->GetName()));
    if(monitor_select_combo->count() == 1)
    {
        monitor_select_combo->setCurrentIndex(0);
        monitor_stack->setCurrentIndex(0);
    }
}

void ScreenMirror::RefreshMonitorStatus()
{
    if(!monitor_status_label) return;

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    int total_count = 0;
    int active_count = 0;
    
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;
        
        total_count++;
        bool has_capture_source = !plane->GetCaptureSourceId().empty();
        if(has_capture_source)
        {
            active_count++;
        }
        
        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            MonitorSettings new_settings;
            new_settings.enabled = DefaultMonitorEnabledForPlane(plane);
            settings_it = monitor_settings.emplace(plane_name, new_settings).first;
        }
        MonitorSettings& settings = settings_it->second;

        if(!settings.group_box && monitors_container && monitors_layout)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
        else if(settings.group_box)
        {
            settings.group_box->setTitle(QStringLiteral("Enabled"));
            settings.group_box->setEnabled(true);

            if(has_capture_source)
            {
                settings.group_box->setToolTip(
                    QStringLiteral("Enable or disable this display's influence on LEDs."));
            }
            else
            {
                settings.group_box->setToolTip(
                    QStringLiteral("Pick the Windows display this plane should capture."));
            }
            if(settings.capture_combo)
            {
                FillDisplayPlaneCaptureCombo(settings.capture_combo, plane->GetCaptureSourceId());
                settings.capture_combo->setEnabled(true);
            }
            if(settings.capture_refresh_button)
            {
                settings.capture_refresh_button->setEnabled(true);
            }
            
            if(settings.scale_slider) settings.scale_slider->setEnabled(has_capture_source);
            if(settings.white_rolloff_slider) settings.white_rolloff_slider->setEnabled(has_capture_source);
            if(settings.vibrance_slider) settings.vibrance_slider->setEnabled(has_capture_source);
            if(settings.led_output_gain_r_slider) settings.led_output_gain_r_slider->setEnabled(has_capture_source);
            if(settings.led_output_gain_g_slider) settings.led_output_gain_g_slider->setEnabled(has_capture_source);
            if(settings.led_output_gain_b_slider) settings.led_output_gain_b_slider->setEnabled(has_capture_source);
            if(settings.screen_map_roll_slider) settings.screen_map_roll_slider->setEnabled(has_capture_source);
            if(settings.radial_corner_expansion_slider) settings.radial_corner_expansion_slider->setEnabled(has_capture_source);
            if(settings.radial_corner_bias_tl_slider) settings.radial_corner_bias_tl_slider->setEnabled(has_capture_source);
            if(settings.radial_corner_bias_tr_slider) settings.radial_corner_bias_tr_slider->setEnabled(has_capture_source);
            if(settings.radial_corner_bias_bl_slider) settings.radial_corner_bias_bl_slider->setEnabled(has_capture_source);
            if(settings.radial_corner_bias_br_slider) settings.radial_corner_bias_br_slider->setEnabled(has_capture_source);
            if(settings.scale_invert_check) settings.scale_invert_check->setEnabled(has_capture_source);
            if(settings.smoothing_time_slider) settings.smoothing_time_slider->setEnabled(has_capture_source);
            if(settings.brightness_slider) settings.brightness_slider->setEnabled(has_capture_source);
            if(settings.brightness_threshold_slider) settings.brightness_threshold_slider->setEnabled(has_capture_source);
            if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_slider->setEnabled(has_capture_source);
            if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_slider->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.propagation_speed_slider) settings.propagation_speed_slider->setEnabled(has_capture_source);
            if(settings.wave_decay_slider) settings.wave_decay_slider->setEnabled(has_capture_source);
            if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_slider->setEnabled(has_capture_source);
            if(settings.falloff_curve_slider) settings.falloff_curve_slider->setEnabled(has_capture_source);
            if(settings.front_back_balance_slider) settings.front_back_balance_slider->setEnabled(has_capture_source);
            if(settings.left_right_balance_slider) settings.left_right_balance_slider->setEnabled(has_capture_source);
            if(settings.top_bottom_balance_slider) settings.top_bottom_balance_slider->setEnabled(has_capture_source);
            if(settings.calibration_pattern_check) settings.calibration_pattern_check->setEnabled(true);
            if(settings.screen_preview_check) settings.screen_preview_check->setEnabled(has_capture_source);
            if(settings.capture_zones_widget)
            {
                settings.capture_zones_widget->setEnabled(has_capture_source);
                settings.capture_zones_widget->SetDisplayPlane(plane);
                settings.capture_zones_widget->SetCaptureZones(&settings.capture_zones);
            }
            if(settings.add_zone_button)
                settings.add_zone_button->setEnabled(has_capture_source);
            
        }
    }

    SyncMonitorPickerLabels();

    QString status_text;
    if(total_count == 0)
    {
        status_text = "No Display Planes configured";
    }
    else if(active_count == 0)
    {
        status_text = QString("Display Planes: %1 (none have capture sources)").arg(total_count);
    }
    else
    {
        status_text = QString("Display Planes: %1 total, %2 active").arg(total_count).arg(active_count);
    }
    monitor_status_label->setText(status_text);

    QWidget* parent = monitor_status_label->parentWidget();
    if(parent)
    {
        QGroupBox* status_group = qobject_cast<QGroupBox*>(parent);
        if(status_group && status_group->layout())
        {
            if(total_count > 0 && active_count == 0)
            {
                if(!monitor_help_label)
                {
                    monitor_help_label = new QLabel("Tip: Pick a Windows display on each plane (Monitor 1, Monitor 2, …).");
                    monitor_help_label->setWordWrap(true);
                    PluginUiApplyItalicSecondaryLabel(monitor_help_label);
                    status_group->layout()->addWidget(monitor_help_label);
                }
            }
            else
            {
                if(monitor_help_label)
                {
                    monitor_help_label->deleteLater();
                    monitor_help_label = nullptr;
                }
            }
        }
    }
}

void ScreenMirror::SelectCaptureQualityCombo(int quality)
{
    if(!capture_quality_combo)
    {
        return;
    }
    quality = ScreenMirrorClampQuality(quality);
    for(int i = 0; i < capture_quality_combo->count(); ++i)
    {
        if(capture_quality_combo->itemData(i).toInt() == quality)
        {
            QSignalBlocker block(capture_quality_combo);
            capture_quality_combo->setCurrentIndex(i);
            return;
        }
    }
}
