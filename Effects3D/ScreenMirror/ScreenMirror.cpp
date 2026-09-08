// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "ScreenCaptureDownscale.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "DisplayPlaneCaptureCombo.h"
#include "PluginUiUtils.h"
#include "ScreenMirror/ScreenMirrorMonitorPanel.h"
#include "ScreenMirror/ScreenMirror_Internal.h"
#include "ui_ScreenMirrorCapturePanel.h"
#include "ui_ScreenMirrorEffectShell.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QString>
#include <QSignalBlocker>
#include <algorithm>

REGISTER_EFFECT_3D(ScreenMirror);

ScreenMirror::ScreenMirror(QWidget* parent)
    : SpatialEffect3D(parent)
    , capture_quality(kScreenMirrorQualityNative)
    , capture_quality_combo(nullptr)
    , capture_backend_mode(1)
    , capture_backend_combo(nullptr)
    , monitor_status_label(nullptr)
    , monitor_help_label(nullptr)
    , monitors_container(nullptr)
    , monitors_layout(nullptr)
    , in_parameter_change_(false)
    , reference_points(nullptr)
    , frame_cache_refresh_ms_(0)
    , frame_cache_last_render_seq_(0)
    , tick_scale_mm_(10.0f)
    , gpu_smooth_ms_(0.0f)
{
}

ScreenMirror::~ScreenMirror() = default;

EffectInfo3D ScreenMirror::GetEffectInfo() const
{
    EffectInfo3D info           = {};
    info.effect_name            = "Screen Mirror";
    info.effect_description =
        "Maps each display plane onto nearby LEDs. Capture is sampled per LED from the plane rectangle.";
    info.category               = "Ambilight";
    info.effect_type            = SPATIAL_EFFECT_SCREEN_MIRROR;
    info.is_reversible          = false;
    info.supports_random        = false;
    info.max_speed = 200;
    info.min_speed              = 0;
    info.user_colors            = 0;
    info.has_custom_settings    = true;
    info.needs_3d_origin        = true;
    info.needs_direction        = false;
    info.needs_thickness        = false;
    info.needs_arms             = false;
    info.needs_frequency        = false;
    info.use_size_parameter     = false;
    
    info.show_color_controls    = false;
    info.show_speed_control     = false;
    info.show_brightness_control = false;
    info.show_frequency_control = false;
    info.show_size_control      = false;
    info.show_scale_control     = false;
    info.show_axis_control      = false;

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
        s.blend_slider = nullptr;
        s.blend_label = nullptr;
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
        s.ref_point_combo = nullptr;
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
        s.corner_blend_strength_slider = nullptr;
        s.corner_blend_strength_label = nullptr;
        s.corner_blend_zone_slider = nullptr;
        s.corner_blend_zone_label = nullptr;
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
        "Native copies each monitor at its current resolution (never upscales). "
        "Lower entries are caps: any Direct3D 11 GPU scales in VRAM before CPU readback. "
        "A 1080p video on a 4K desktop is still 4K pixels — capture sees the display, not the file.");
    capture_quality_combo->setItemData(0, "1:1 copy of whatever the monitor is right now. 1080p screen → 1080p, 4K screen → 4K, ultrawide → ultrawide. Works on any GPU.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(1, "Lightest load; fine for small planes or testing.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(2, "Low bandwidth; useful on integrated GPUs if Native feels heavy.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(3, "Balanced cap for many setups.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(4, "Sharper color detail on wide monitors, still a cap.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(5, "720p cap. GPU-scales larger desktops before CPU Map.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(6, "1080p cap. GPU-scales native pixels into this size before CPU Map.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(7, "1440p cap. GPU scales in VRAM; CPU only maps 1440p.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(8, "4K cap (never larger than the display). Use if you want a hard ceiling below Native.", Qt::ToolTipRole);
    SelectCaptureQualityCombo(capture_quality);

    capture_backend_combo = capture_ui.captureBackendCombo;
    capture_backend_combo->addItem("Auto (GDI if DXGI stalls)", QVariant(0));
    capture_backend_combo->addItem("DXGI only", QVariant(1));
    capture_backend_combo->addItem("GDI only", QVariant(2));
    capture_backend_combo->setCurrentIndex(std::clamp(capture_backend_mode, 0, 2));

    QVBoxLayout* capture_host_layout = new QVBoxLayout(shell_ui.capturePanelHost);
    capture_host_layout->setContentsMargins(0, 0, 0, 0);
    capture_host_layout->addWidget(capture_panel);

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
        if(settings.reference_point_id <= 0)
        {
            settings.reference_point_id = -1;
        }

        if(!settings.group_box)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
    }

    if(monitor_settings.empty())
    {
        QLabel* no_monitors_label = new QLabel("No monitors configured. Set up Display Planes first.");
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

void ScreenMirror::CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings)
{
    if(!plane || !monitors_layout)
    {
        return;
    }

    const bool has_capture_source = !plane->GetCaptureSourceId().empty();
    QString display_name = QString::fromStdString(plane->GetName());
    if(!has_capture_source)
    {
        display_name += QStringLiteral(" (No Capture Source)");
    }

    settings.group_box = new QGroupBox(display_name);
    settings.group_box->setCheckable(true);
    settings.group_box->setChecked(settings.enabled &&
                                   (has_capture_source || settings.show_calibration_pattern || settings.show_screen_preview));
    settings.group_box->setEnabled(true);
    settings.group_box->setToolTip(has_capture_source
                                      ? QStringLiteral("Enable or disable this monitor's influence.")
                                      : QStringLiteral("Pick the Windows display this plane should capture."));
    connect(settings.group_box, &QGroupBox::toggled, this, &ScreenMirror::OnParameterChanged);

    auto* panel = new ScreenMirrorMonitorPanel(settings.group_box);
    panel->initialize(this, settings, plane, has_capture_source);

    auto* outer = new QVBoxLayout(settings.group_box);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(panel);

    monitors_layout->addWidget(settings.group_box);
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

        if(settings.reference_point_id <= 0)
        {
            settings.reference_point_id = -1;
        }

        if(!settings.group_box && monitors_container && monitors_layout)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
        else if(settings.group_box)
        {
            QString display_name = QString::fromStdString(plane_name);
            if(!has_capture_source)
            {
                display_name += " (No Capture Source)";
            }
            settings.group_box->setTitle(display_name);
            settings.group_box->setEnabled(true);
            
            if(has_capture_source)
            {
                settings.group_box->setToolTip("Enable or disable this monitor's influence.");
            }
            else
            {
                settings.group_box->setToolTip("Pick the Windows display this plane should capture.");
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
            if(settings.corner_blend_strength_slider) settings.corner_blend_strength_slider->setEnabled(has_capture_source);
            if(settings.corner_blend_zone_slider) settings.corner_blend_zone_slider->setEnabled(has_capture_source);
            if(settings.scale_invert_check) settings.scale_invert_check->setEnabled(has_capture_source);
            if(settings.smoothing_time_slider) settings.smoothing_time_slider->setEnabled(has_capture_source);
            if(settings.brightness_slider) settings.brightness_slider->setEnabled(has_capture_source);
            if(settings.brightness_threshold_slider) settings.brightness_threshold_slider->setEnabled(has_capture_source);
            if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_slider->setEnabled(has_capture_source);
            if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_slider->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.blend_slider) settings.blend_slider->setEnabled(has_capture_source);
            if(settings.propagation_speed_slider) settings.propagation_speed_slider->setEnabled(has_capture_source);
            if(settings.wave_decay_slider) settings.wave_decay_slider->setEnabled(has_capture_source);
            if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_slider->setEnabled(has_capture_source);
            if(settings.falloff_curve_slider) settings.falloff_curve_slider->setEnabled(has_capture_source);
            if(settings.front_back_balance_slider) settings.front_back_balance_slider->setEnabled(has_capture_source);
            if(settings.left_right_balance_slider) settings.left_right_balance_slider->setEnabled(has_capture_source);
            if(settings.top_bottom_balance_slider) settings.top_bottom_balance_slider->setEnabled(has_capture_source);
            if(settings.ref_point_combo) settings.ref_point_combo->setEnabled(has_capture_source);
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
