// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureDownscale.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "ScreenMirror_Internal.h"

#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

nlohmann::json ScreenMirror::SaveSettings() const
{
    nlohmann::json settings;
    settings["capture_quality"] = ScreenMirrorClampQuality(capture_quality);
    settings["capture_backend_mode"] = std::clamp(capture_backend_mode, 0, 2);
    settings["audience_bias_pct"] = std::clamp(audience_bias_pct_, 0.0f, 100.0f);
    nlohmann::json monitors = nlohmann::json::object();
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        nlohmann::json mon;
        mon["enabled"] = mon_settings.enabled;
        mon["scale"] = mon_settings.scale;
        mon["scale_inverted"] = mon_settings.scale_inverted;
        mon["smoothing_time_ms"] = mon_settings.smoothing_time_ms;
        mon["brightness_multiplier"] = mon_settings.brightness_multiplier;
        mon["brightness_threshold"] = mon_settings.brightness_threshold;
        mon["white_rolloff"] = mon_settings.white_rolloff;
        mon["vibrance"] = mon_settings.vibrance;
        mon["led_output_gain_r"] = mon_settings.led_output_gain_r;
        mon["led_output_gain_g"] = mon_settings.led_output_gain_g;
        mon["led_output_gain_b"] = mon_settings.led_output_gain_b;
        mon["black_bar_letterbox_percent"] = mon_settings.black_bar_letterbox_percent;
        mon["black_bar_pillarbox_percent"] = mon_settings.black_bar_pillarbox_percent;
        mon["edge_softness"] = mon_settings.edge_softness;
        mon["propagation_speed_mm_per_ms"] = mon_settings.propagation_speed_mm_per_ms;
        mon["wave_decay_ms"] = mon_settings.wave_decay_ms;
        mon["wave_time_to_edge_sec"] = mon_settings.wave_time_to_edge_sec;
        mon["falloff_curve_exponent"] = mon_settings.falloff_curve_exponent;
        mon["show_calibration_pattern"] = mon_settings.show_calibration_pattern;
        mon["show_screen_preview"] = mon_settings.show_screen_preview;

        nlohmann::json zones_array = nlohmann::json::array();
        for(const CaptureZone& zone : mon_settings.capture_zones)
        {
            nlohmann::json zone_json;
            zone_json["u_min"] = zone.u_min;
            zone_json["u_max"] = zone.u_max;
            zone_json["v_min"] = zone.v_min;
            zone_json["v_max"] = zone.v_max;
            zone_json["enabled"] = zone.enabled;
            zone_json["name"] = zone.name;
            zones_array.push_back(zone_json);
        }
        mon["capture_zones"] = zones_array;
        monitors[it->first] = mon;
    }
    settings["monitor_settings"] = monitors;

    return settings;
}

void ScreenMirror::SyncMonitorSettingsToUI(MonitorSettings& msettings)
{
    if(msettings.group_box)
    {
        QSignalBlocker blocker(msettings.group_box);
        msettings.group_box->setChecked(msettings.enabled);
    }
    if(msettings.scale_slider)
    {
        QSignalBlocker blocker(msettings.scale_slider);
        msettings.scale_slider->setValue((int)std::lround(msettings.scale * 100.0f));
    }
    if(msettings.scale_label)
    {
        msettings.scale_label->setText(QString::number((int)std::lround(msettings.scale * 100.0f)) + "%");
    }
    if(msettings.scale_invert_check)
    {
        QSignalBlocker blocker(msettings.scale_invert_check);
        msettings.scale_invert_check->setChecked(msettings.scale_inverted);
    }
    if(msettings.smoothing_time_slider)
    {
        QSignalBlocker blocker(msettings.smoothing_time_slider);
        msettings.smoothing_time_slider->setValue((int)std::lround(msettings.smoothing_time_ms));
    }
    if(msettings.smoothing_time_label)
    {
        msettings.smoothing_time_label->setText(QString::number((int)msettings.smoothing_time_ms) + "ms");
    }
    if(msettings.brightness_slider)
    {
        QSignalBlocker blocker(msettings.brightness_slider);
        msettings.brightness_slider->setValue((int)std::lround(msettings.brightness_multiplier * 100.0f));
    }
    if(msettings.brightness_label)
    {
        msettings.brightness_label->setText(QString::number((int)std::lround(msettings.brightness_multiplier * 100.0f)) + "%");
    }
    if(msettings.brightness_threshold_slider)
    {
        QSignalBlocker blocker(msettings.brightness_threshold_slider);
        msettings.brightness_threshold_slider->setValue((int)msettings.brightness_threshold);
    }
    if(msettings.brightness_threshold_label)
    {
        msettings.brightness_threshold_label->setText(QString::number((int)msettings.brightness_threshold));
    }
    if(msettings.white_rolloff_slider)
    {
        QSignalBlocker blocker(msettings.white_rolloff_slider);
        msettings.white_rolloff_slider->setValue((int)std::lround(msettings.white_rolloff * 100.0f));
    }
    if(msettings.white_rolloff_label)
    {
        msettings.white_rolloff_label->setText(QString::number((int)std::lround(msettings.white_rolloff * 100.0f)) + "%");
    }
    if(msettings.vibrance_slider)
    {
        QSignalBlocker blocker(msettings.vibrance_slider);
        msettings.vibrance_slider->setValue((int)std::lround(msettings.vibrance * 100.0f));
    }
    if(msettings.vibrance_label)
    {
        msettings.vibrance_label->setText(QString::number((int)std::lround(msettings.vibrance * 100.0f)) + "%");
    }
    if(msettings.led_output_gain_r_slider)
    {
        QSignalBlocker blocker(msettings.led_output_gain_r_slider);
        msettings.led_output_gain_r_slider->setValue(
            (int)std::lround(std::clamp(msettings.led_output_gain_r, 0.5f, 2.0f) * 100.0f));
    }
    if(msettings.led_output_gain_r_label)
    {
        msettings.led_output_gain_r_label->setText(
            QString::number((int)std::lround(std::clamp(msettings.led_output_gain_r, 0.5f, 2.0f) * 100.0f)) + "%");
    }
    if(msettings.led_output_gain_g_slider)
    {
        QSignalBlocker blocker(msettings.led_output_gain_g_slider);
        msettings.led_output_gain_g_slider->setValue(
            (int)std::lround(std::clamp(msettings.led_output_gain_g, 0.5f, 2.0f) * 100.0f));
    }
    if(msettings.led_output_gain_g_label)
    {
        msettings.led_output_gain_g_label->setText(
            QString::number((int)std::lround(std::clamp(msettings.led_output_gain_g, 0.5f, 2.0f) * 100.0f)) + "%");
    }
    if(msettings.led_output_gain_b_slider)
    {
        QSignalBlocker blocker(msettings.led_output_gain_b_slider);
        msettings.led_output_gain_b_slider->setValue(
            (int)std::lround(std::clamp(msettings.led_output_gain_b, 0.5f, 2.0f) * 100.0f));
    }
    if(msettings.led_output_gain_b_label)
    {
        msettings.led_output_gain_b_label->setText(
            QString::number((int)std::lround(std::clamp(msettings.led_output_gain_b, 0.5f, 2.0f) * 100.0f)) + "%");
    }
    if(msettings.black_bar_letterbox_slider)
    {
        QSignalBlocker blocker(msettings.black_bar_letterbox_slider);
        msettings.black_bar_letterbox_slider->setValue((int)std::lround(msettings.black_bar_letterbox_percent));
    }
    if(msettings.black_bar_letterbox_label)
    {
        msettings.black_bar_letterbox_label->setText(QString::number((int)std::lround(msettings.black_bar_letterbox_percent)));
    }
    if(msettings.black_bar_pillarbox_slider)
    {
        QSignalBlocker blocker(msettings.black_bar_pillarbox_slider);
        msettings.black_bar_pillarbox_slider->setValue((int)std::lround(msettings.black_bar_pillarbox_percent));
    }
    if(msettings.black_bar_pillarbox_label)
    {
        msettings.black_bar_pillarbox_label->setText(QString::number((int)std::lround(msettings.black_bar_pillarbox_percent)));
    }
    if(msettings.softness_slider)
    {
        QSignalBlocker blocker(msettings.softness_slider);
        msettings.softness_slider->setValue((int)std::lround(msettings.edge_softness));
    }
    if(msettings.softness_label)
    {
        msettings.softness_label->setText(QString::number((int)msettings.edge_softness));
    }
    if(msettings.propagation_speed_slider)
    {
        QSignalBlocker blocker(msettings.propagation_speed_slider);
        msettings.propagation_speed_slider->setValue((int)std::lround(msettings.propagation_speed_mm_per_ms));
    }
    if(msettings.propagation_speed_label)
    {
        msettings.propagation_speed_label->setText(QString::number((int)std::lround(msettings.propagation_speed_mm_per_ms)) + "%");
    }
    if(msettings.wave_decay_slider)
    {
        QSignalBlocker blocker(msettings.wave_decay_slider);
        msettings.wave_decay_slider->setValue((int)std::lround(msettings.wave_decay_ms));
    }
    if(msettings.wave_decay_label)
    {
        msettings.wave_decay_label->setText(QString::number((int)msettings.wave_decay_ms) + "ms");
    }
    if(msettings.wave_time_to_edge_slider)
    {
        QSignalBlocker blocker(msettings.wave_time_to_edge_slider);
        msettings.wave_time_to_edge_slider->setValue((int)(msettings.wave_time_to_edge_sec * 10.0f));
    }
    if(msettings.wave_time_to_edge_label)
    {
        msettings.wave_time_to_edge_label->setText(msettings.wave_time_to_edge_sec <= 0.0f ? "Off" : QString::number(msettings.wave_time_to_edge_sec, 'f', 1) + "s");
    }
    if(msettings.falloff_curve_slider)
    {
        QSignalBlocker blocker(msettings.falloff_curve_slider);
        msettings.falloff_curve_slider->setValue((int)(msettings.falloff_curve_exponent * 100.0f));
    }
    if(msettings.falloff_curve_label)
    {
        msettings.falloff_curve_label->setText(QString::number((int)(msettings.falloff_curve_exponent * 100.0f)) + "%");
    }
    if(msettings.calibration_pattern_check)
    {
        QSignalBlocker blocker(msettings.calibration_pattern_check);
        msettings.calibration_pattern_check->setChecked(msettings.show_calibration_pattern);
    }
    if(msettings.screen_preview_check)
    {
        QSignalBlocker blocker(msettings.screen_preview_check);
        msettings.screen_preview_check->setChecked(msettings.show_screen_preview);
    }
    if(msettings.capture_area_preview)
    {
        msettings.capture_area_preview->update();
    }
}

void ScreenMirror::LoadSettings(const nlohmann::json& settings)
{
    static const float default_scale = 1.0f;
    static const bool default_scale_inverted = true;
    static const float default_smoothing_time_ms = 0.0f;
    static const float default_brightness_multiplier = 1.0f;
    static const float default_brightness_threshold = 0.0f;
    static const float default_propagation_speed_mm_per_ms = 0.0f;
    static const float default_wave_decay_ms = 0.0f;
    static const float default_wave_time_to_edge_sec = 0.0f;
    static const float default_falloff_curve_exponent = 1.0f;

    if(settings.contains("capture_quality"))
    {
        capture_quality = ScreenMirrorClampQuality(settings["capture_quality"].get<int>());
        SelectCaptureQualityCombo(capture_quality);
    }
    if(settings.contains("audience_bias_pct"))
    {
        audience_bias_pct_ = std::clamp(settings["audience_bias_pct"].get<float>(), 0.0f, 100.0f);
    }
    if(audience_bias_slider_)
    {
        QSignalBlocker blocker(audience_bias_slider_);
        audience_bias_slider_->setValue((int)std::lround(audience_bias_pct_));
    }
    if(audience_bias_label_)
    {
        audience_bias_label_->setText(
            QString::number((int)std::lround(audience_bias_pct_)) + QStringLiteral("%"));
    }

    if(settings.contains("capture_backend_mode"))
    {
        capture_backend_mode = std::clamp(settings["capture_backend_mode"].get<int>(), 0, 2);
        if(capture_backend_combo)
        {
            capture_backend_combo->setCurrentIndex(capture_backend_mode);
        }
        ScreenCaptureManager::Instance().SetWindowsCaptureBackendMode(capture_backend_mode);
    }

    if(!settings.contains("monitor_settings"))
    {
        return;
    }

    const nlohmann::json& monitors = settings["monitor_settings"];
    for(nlohmann::json::const_iterator it = monitors.begin(); it != monitors.end(); ++it)
    {
            const std::string& monitor_name = it.key();
            const nlohmann::json& mon = it.value();

            std::map<std::string, MonitorSettings>::iterator existing_it = monitor_settings.find(monitor_name);
            const bool had_entry = (existing_it != monitor_settings.end());
            if(!had_entry)
            {
                monitor_settings.emplace(monitor_name, MonitorSettings());
                existing_it = monitor_settings.find(monitor_name);
            }
            MonitorSettings& msettings = existing_it->second;

            if(mon.contains("enabled"))
            {
                msettings.enabled = mon["enabled"].get<bool>();
            }
            else if(!had_entry)
            {
                DisplayPlane3D* loaded_plane = FindDisplayPlaneByName(monitor_name);
                msettings.enabled = loaded_plane ? DefaultMonitorEnabledForPlane(loaded_plane) : false;
            }

            if(mon.contains("scale")) msettings.scale = mon["scale"].get<float>();
            else msettings.scale = default_scale;
            if(mon.contains("scale_inverted")) msettings.scale_inverted = mon["scale_inverted"].get<bool>();
            else msettings.scale_inverted = default_scale_inverted;

            if(mon.contains("smoothing_time_ms")) msettings.smoothing_time_ms = mon["smoothing_time_ms"].get<float>();
            else msettings.smoothing_time_ms = default_smoothing_time_ms;
            if(mon.contains("brightness_multiplier")) msettings.brightness_multiplier = mon["brightness_multiplier"].get<float>();
            else msettings.brightness_multiplier = default_brightness_multiplier;
            if(mon.contains("brightness_threshold"))
            {
                msettings.brightness_threshold = mon["brightness_threshold"].get<float>();
            }
            else msettings.brightness_threshold = default_brightness_threshold;
            if(mon.contains("white_rolloff")) msettings.white_rolloff = mon["white_rolloff"].get<float>();
            else msettings.white_rolloff = 0.0f;
            if(mon.contains("vibrance")) msettings.vibrance = mon["vibrance"].get<float>();
            else msettings.vibrance = 1.0f;
            if(mon.contains("led_output_gain_r"))
            {
                msettings.led_output_gain_r = mon["led_output_gain_r"].get<float>();
            }
            else
            {
                msettings.led_output_gain_r = 1.0f;
            }
            if(mon.contains("led_output_gain_g"))
            {
                msettings.led_output_gain_g = mon["led_output_gain_g"].get<float>();
            }
            else
            {
                msettings.led_output_gain_g = 1.0f;
            }
            if(mon.contains("led_output_gain_b"))
            {
                msettings.led_output_gain_b = mon["led_output_gain_b"].get<float>();
            }
            else
            {
                msettings.led_output_gain_b = 1.0f;
            }
            if(mon.contains("black_bar_letterbox_percent")) msettings.black_bar_letterbox_percent = mon["black_bar_letterbox_percent"].get<float>();
            else msettings.black_bar_letterbox_percent = 0.0f;
            if(mon.contains("black_bar_pillarbox_percent")) msettings.black_bar_pillarbox_percent = mon["black_bar_pillarbox_percent"].get<float>();
            else msettings.black_bar_pillarbox_percent = 0.0f;

            if(mon.contains("edge_softness")) msettings.edge_softness = mon["edge_softness"].get<float>();
            else msettings.edge_softness = 70.0f;
            if(mon.contains("propagation_speed_mm_per_ms"))
            {
                msettings.propagation_speed_mm_per_ms = mon["propagation_speed_mm_per_ms"].get<float>();
            }
            else msettings.propagation_speed_mm_per_ms = default_propagation_speed_mm_per_ms;
            if(mon.contains("wave_decay_ms")) msettings.wave_decay_ms = mon["wave_decay_ms"].get<float>();
            else msettings.wave_decay_ms = default_wave_decay_ms;
            if(mon.contains("wave_time_to_edge_sec")) msettings.wave_time_to_edge_sec = mon["wave_time_to_edge_sec"].get<float>();
            else msettings.wave_time_to_edge_sec = default_wave_time_to_edge_sec;
            if(mon.contains("falloff_curve_exponent")) msettings.falloff_curve_exponent = mon["falloff_curve_exponent"].get<float>();
            else msettings.falloff_curve_exponent = default_falloff_curve_exponent;

            if(mon.contains("show_calibration_pattern"))
            {
                msettings.show_calibration_pattern = mon["show_calibration_pattern"].get<bool>();
            }
            if(mon.contains("show_screen_preview")) msettings.show_screen_preview = mon["show_screen_preview"].get<bool>();
            
            if(mon.contains("capture_zones") && mon["capture_zones"].is_array())
            {
                msettings.capture_zones.clear();
                const nlohmann::json& zones_arr = mon["capture_zones"];
                for(size_t zi = 0; zi < zones_arr.size() &&
                                   msettings.capture_zones.size() < kMaxCaptureZonesPerMonitor;
                     zi++)
                {
                    const nlohmann::json& zone_json = zones_arr[zi];
                    CaptureZone zone;
                    zone.u_min = zone_json.value("u_min", 0.0f);
                    zone.u_max = zone_json.value("u_max", 1.0f);
                    zone.v_min = zone_json.value("v_min", 0.0f);
                    zone.v_max = zone_json.value("v_max", 1.0f);
                    zone.enabled = zone_json.value("enabled", true);
                    zone.name = zone_json.value("name", "Zone");
                    if(zone.name.size() > kMaxCaptureZoneNameChars)
                    {
                        zone.name.resize(kMaxCaptureZoneNameChars);
                    }
                    
                    zone.u_min = std::clamp(zone.u_min, 0.0f, 1.0f);
                    zone.u_max = std::clamp(zone.u_max, 0.0f, 1.0f);
                    zone.v_min = std::clamp(zone.v_min, 0.0f, 1.0f);
                    zone.v_max = std::clamp(zone.v_max, 0.0f, 1.0f);
                    if(zone.u_min > zone.u_max) { float temp = zone.u_min; zone.u_min = zone.u_max; zone.u_max = temp; }
                    if(zone.v_min > zone.v_max) { float temp = zone.v_min; zone.v_min = zone.v_max; zone.v_max = temp; }
                    
                    msettings.capture_zones.push_back(zone);
                }
                
                if(msettings.capture_zones.empty())
                {
                    msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }
            else
            {
                msettings.capture_zones.clear();
                msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
            }
            
            msettings.scale = std::clamp(msettings.scale, 0.0f, 3.0f);
            msettings.smoothing_time_ms = std::clamp(msettings.smoothing_time_ms, 0.0f, 500.0f);
            msettings.brightness_multiplier = std::clamp(msettings.brightness_multiplier, 0.0f, 2.0f);
            msettings.brightness_threshold = std::clamp(msettings.brightness_threshold, 0.0f, 255.0f);
            msettings.white_rolloff = std::clamp(msettings.white_rolloff, 0.0f, kWhiteRolloffStoredMax);
            msettings.vibrance = std::clamp(msettings.vibrance, 0.0f, 2.0f);
            msettings.led_output_gain_r = std::clamp(msettings.led_output_gain_r, 0.5f, 2.0f);
            msettings.led_output_gain_g = std::clamp(msettings.led_output_gain_g, 0.5f, 2.0f);
            msettings.led_output_gain_b = std::clamp(msettings.led_output_gain_b, 0.5f, 2.0f);
            msettings.black_bar_letterbox_percent = std::clamp(msettings.black_bar_letterbox_percent, 0.0f, 50.0f);
            msettings.black_bar_pillarbox_percent = std::clamp(msettings.black_bar_pillarbox_percent, 0.0f, 50.0f);
            msettings.edge_softness = std::clamp(msettings.edge_softness, 0.0f, 100.0f);
            msettings.propagation_speed_mm_per_ms = std::clamp(msettings.propagation_speed_mm_per_ms, 0.0f, 100.0f);
            msettings.wave_decay_ms = std::clamp(msettings.wave_decay_ms, 0.0f, 3000.0f);
            msettings.wave_time_to_edge_sec = std::clamp(msettings.wave_time_to_edge_sec, 0.0f, 10.0f);
            msettings.falloff_curve_exponent = std::clamp(msettings.falloff_curve_exponent, 0.5f, 2.0f);
    }

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        SyncMonitorSettingsToUI(it->second);
    }

    RefreshMonitorStatus();
    
    OnScreenPreviewChanged();
    OnCalibrationPatternChanged();
    
    OnParameterChanged();
}

void ScreenMirror::OnParameterChanged()
{
    if(in_parameter_change_)
    {
        return;
    }
    in_parameter_change_ = true;

    if(audience_bias_slider_)
    {
        audience_bias_pct_ = (float)std::clamp(audience_bias_slider_->value(), 0, 100);
    }

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.group_box) settings.enabled = settings.group_box->isChecked();
        
        if(settings.scale_slider) settings.scale = std::clamp(settings.scale_slider->value() / 100.0f, 0.0f, 3.0f);
        if(settings.scale_invert_check) settings.scale_inverted = settings.scale_invert_check->isChecked();
        
        if(settings.smoothing_time_slider) settings.smoothing_time_ms = (float)settings.smoothing_time_slider->value();
        if(settings.brightness_slider) settings.brightness_multiplier = std::clamp(settings.brightness_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.brightness_threshold_slider) settings.brightness_threshold = (float)settings.brightness_threshold_slider->value();
        if(settings.white_rolloff_slider)
            settings.white_rolloff =
                std::clamp((float)settings.white_rolloff_slider->value() / 100.0f, 0.0f, kWhiteRolloffStoredMax);
        if(settings.vibrance_slider) settings.vibrance = std::clamp((float)settings.vibrance_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.led_output_gain_r_slider)
            settings.led_output_gain_r =
                std::clamp((float)settings.led_output_gain_r_slider->value() / 100.0f, 0.5f, 2.0f);
        if(settings.led_output_gain_g_slider)
            settings.led_output_gain_g =
                std::clamp((float)settings.led_output_gain_g_slider->value() / 100.0f, 0.5f, 2.0f);
        if(settings.led_output_gain_b_slider)
            settings.led_output_gain_b =
                std::clamp((float)settings.led_output_gain_b_slider->value() / 100.0f, 0.5f, 2.0f);
        if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_percent = (float)settings.black_bar_letterbox_slider->value();
        if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_percent = (float)settings.black_bar_pillarbox_slider->value();
        
        if(settings.softness_slider) settings.edge_softness = (float)settings.softness_slider->value();
        if(settings.propagation_speed_slider) settings.propagation_speed_mm_per_ms = std::clamp((float)settings.propagation_speed_slider->value(), 0.0f, 100.0f);
        if(settings.wave_decay_slider) settings.wave_decay_ms = (float)settings.wave_decay_slider->value();
        if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_sec = (float)settings.wave_time_to_edge_slider->value() / 10.0f;
        if(settings.falloff_curve_slider) settings.falloff_curve_exponent = std::clamp((float)settings.falloff_curve_slider->value() / 100.0f, 0.5f, 2.0f);

        bool old_calibration_pattern = settings.show_calibration_pattern;
        bool old_screen_preview = settings.show_screen_preview;
        if(settings.calibration_pattern_check) settings.show_calibration_pattern = settings.calibration_pattern_check->isChecked();
        if(settings.screen_preview_check) settings.show_screen_preview = settings.screen_preview_check->isChecked();
        if(settings.show_calibration_pattern && settings.group_box && !settings.group_box->isChecked())
        {
            QSignalBlocker block(settings.group_box);
            settings.group_box->setChecked(true);
            settings.enabled = true;
        }
        if(settings.show_screen_preview && settings.group_box && !settings.group_box->isChecked())
        {
            QSignalBlocker block(settings.group_box);
            settings.group_box->setChecked(true);
            settings.enabled = true;
        }

        if((old_calibration_pattern != settings.show_calibration_pattern || old_screen_preview != settings.show_screen_preview) 
           && settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }

    }

    RefreshMonitorStatus();

    emit ParametersChanged();
    in_parameter_change_ = false;
}
