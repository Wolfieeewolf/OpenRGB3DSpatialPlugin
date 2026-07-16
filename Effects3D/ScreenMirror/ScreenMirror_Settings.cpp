// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "VirtualReferencePoint3D.h"
#include "ScreenMirror/ScreenMirror_Internal.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QSlider>
#include <QSignalBlocker>
#include <QPushButton>
#include <algorithm>
#include <cmath>

nlohmann::json ScreenMirror::SaveSettings() const
{
    nlohmann::json settings;
    settings["capture_quality"] = std::clamp(capture_quality, 0, 7);
    settings["capture_backend_mode"] = std::clamp(capture_backend_mode, 0, 2);
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
        mon["blend"] = mon_settings.blend;
        mon["propagation_speed_mm_per_ms"] = mon_settings.propagation_speed_mm_per_ms;
        mon["wave_decay_ms"] = mon_settings.wave_decay_ms;
        mon["wave_time_to_edge_sec"] = mon_settings.wave_time_to_edge_sec;
        mon["falloff_curve_exponent"] = mon_settings.falloff_curve_exponent;
        mon["front_back_balance"] = mon_settings.front_back_balance;
        mon["left_right_balance"] = mon_settings.left_right_balance;
        mon["top_bottom_balance"] = mon_settings.top_bottom_balance;
        
        mon["reference_point_id"] = mon_settings.reference_point_id;
        mon["show_calibration_pattern"] = mon_settings.show_calibration_pattern;
        mon["show_screen_preview"] = mon_settings.show_screen_preview;

        mon["radial_map_roll_deg"] = std::clamp(mon_settings.screen_map_roll_deg, -180.0f, 180.0f);
        mon["radial_map_expansion"] = std::clamp(mon_settings.radial_corner_expansion_ui, 0, 100);
        mon["radial_map_bias_tl"] = std::clamp(mon_settings.radial_corner_bias_tl_ui, 0, 100);
        mon["radial_map_bias_tr"] = std::clamp(mon_settings.radial_corner_bias_tr_ui, 0, 100);
        mon["radial_map_bias_bl"] = std::clamp(mon_settings.radial_corner_bias_bl_ui, 0, 100);
        mon["radial_map_bias_br"] = std::clamp(mon_settings.radial_corner_bias_br_ui, 0, 100);
        mon["sample_corner_blend_strength_pct"] = std::clamp(mon_settings.corner_blend_strength_pct, 0.0f, 100.0f);
        mon["sample_corner_blend_zone_pct"] = std::clamp(mon_settings.corner_blend_zone_pct, 0.0f, 32.0f);
        
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
    if(msettings.blend_slider)
    {
        QSignalBlocker blocker(msettings.blend_slider);
        msettings.blend_slider->setValue((int)std::lround(msettings.blend));
    }
    if(msettings.blend_label)
    {
        msettings.blend_label->setText(QString::number((int)msettings.blend));
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
    if(msettings.front_back_balance_slider)
    {
        QSignalBlocker blocker(msettings.front_back_balance_slider);
        msettings.front_back_balance_slider->setValue((int)std::lround(msettings.front_back_balance));
    }
    if(msettings.front_back_balance_label)
    {
        msettings.front_back_balance_label->setText(QString::number((int)std::lround(msettings.front_back_balance)));
    }
    if(msettings.left_right_balance_slider)
    {
        QSignalBlocker blocker(msettings.left_right_balance_slider);
        msettings.left_right_balance_slider->setValue((int)std::lround(msettings.left_right_balance));
    }
    if(msettings.left_right_balance_label)
    {
        msettings.left_right_balance_label->setText(QString::number((int)std::lround(msettings.left_right_balance)));
    }
    if(msettings.top_bottom_balance_slider)
    {
        QSignalBlocker blocker(msettings.top_bottom_balance_slider);
        msettings.top_bottom_balance_slider->setValue((int)std::lround(msettings.top_bottom_balance));
    }
    if(msettings.top_bottom_balance_label)
    {
        msettings.top_bottom_balance_label->setText(QString::number((int)std::lround(msettings.top_bottom_balance)));
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
    if(msettings.screen_map_roll_slider)
    {
        QSignalBlocker blocker(msettings.screen_map_roll_slider);
        msettings.screen_map_roll_slider->setValue(
            (int)std::lround(std::clamp(msettings.screen_map_roll_deg, -180.0f, 180.0f) *
                             (float)kScreenMapRollTicksPerDegree));
    }
    if(msettings.screen_map_roll_label)
    {
        msettings.screen_map_roll_label->setText(
            QString::number((double)std::clamp(msettings.screen_map_roll_deg, -180.0f, 180.0f), 'f', 1) +
            QChar(0x00B0));
    }
    if(msettings.radial_corner_expansion_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_expansion_slider);
        msettings.radial_corner_expansion_slider->setValue(msettings.radial_corner_expansion_ui);
    }
    if(msettings.radial_corner_expansion_label)
    {
        msettings.radial_corner_expansion_label->setText(QString::number(msettings.radial_corner_expansion_ui) + "%");
    }
    if(msettings.radial_corner_bias_tl_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_tl_slider);
        msettings.radial_corner_bias_tl_slider->setValue(msettings.radial_corner_bias_tl_ui);
    }
    if(msettings.radial_corner_bias_tl_label)
    {
        msettings.radial_corner_bias_tl_label->setText(QString::number(msettings.radial_corner_bias_tl_ui) + "%");
    }
    if(msettings.radial_corner_bias_tr_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_tr_slider);
        msettings.radial_corner_bias_tr_slider->setValue(msettings.radial_corner_bias_tr_ui);
    }
    if(msettings.radial_corner_bias_tr_label)
    {
        msettings.radial_corner_bias_tr_label->setText(QString::number(msettings.radial_corner_bias_tr_ui) + "%");
    }
    if(msettings.radial_corner_bias_bl_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_bl_slider);
        msettings.radial_corner_bias_bl_slider->setValue(msettings.radial_corner_bias_bl_ui);
    }
    if(msettings.radial_corner_bias_bl_label)
    {
        msettings.radial_corner_bias_bl_label->setText(QString::number(msettings.radial_corner_bias_bl_ui) + "%");
    }
    if(msettings.radial_corner_bias_br_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_br_slider);
        msettings.radial_corner_bias_br_slider->setValue(msettings.radial_corner_bias_br_ui);
    }
    if(msettings.radial_corner_bias_br_label)
    {
        msettings.radial_corner_bias_br_label->setText(QString::number(msettings.radial_corner_bias_br_ui) + "%");
    }
    if(msettings.corner_blend_strength_slider)
    {
        QSignalBlocker blocker(msettings.corner_blend_strength_slider);
        msettings.corner_blend_strength_slider->setValue((int)std::lround(msettings.corner_blend_strength_pct));
    }
    if(msettings.corner_blend_strength_label)
    {
        msettings.corner_blend_strength_label->setText(
            QString::number((int)std::lround(msettings.corner_blend_strength_pct)) + "%");
    }
    if(msettings.corner_blend_zone_slider)
    {
        QSignalBlocker blocker(msettings.corner_blend_zone_slider);
        msettings.corner_blend_zone_slider->setValue((int)std::lround(msettings.corner_blend_zone_pct));
    }
    if(msettings.corner_blend_zone_label)
    {
        msettings.corner_blend_zone_label->setText(
            QString::number((int)std::lround(msettings.corner_blend_zone_pct)) + "%");
    }
    if(msettings.ref_point_combo)
    {
        QSignalBlocker blocker(msettings.ref_point_combo);
        int desired = msettings.reference_point_id;
        int idx = msettings.ref_point_combo->findData(desired);
        if(idx < 0)
        {
            idx = msettings.ref_point_combo->findData(-1);
            msettings.reference_point_id = -1;
        }
        if(idx >= 0) msettings.ref_point_combo->setCurrentIndex(idx);
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
    static const float default_front_back_balance = 0.0f;
    static const float default_left_right_balance = 0.0f;
    static const float default_top_bottom_balance = 0.0f;

    if(settings.contains("capture_quality"))
    {
        capture_quality = std::clamp(settings["capture_quality"].get<int>(), 0, 7);
        if(capture_quality_combo)
        {
            capture_quality_combo->setCurrentIndex(capture_quality);
        }
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
            else msettings.edge_softness = 0.0f;
            if(mon.contains("blend")) msettings.blend = mon["blend"].get<float>();
            else msettings.blend = 0.0f;
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
            if(mon.contains("front_back_balance")) msettings.front_back_balance = mon["front_back_balance"].get<float>();
            else msettings.front_back_balance = default_front_back_balance;
            if(mon.contains("left_right_balance")) msettings.left_right_balance = mon["left_right_balance"].get<float>();
            else msettings.left_right_balance = default_left_right_balance;
            if(mon.contains("top_bottom_balance")) msettings.top_bottom_balance = mon["top_bottom_balance"].get<float>();
            else msettings.top_bottom_balance = default_top_bottom_balance;
            
            if(mon.contains("show_calibration_pattern"))
            {
                msettings.show_calibration_pattern = mon["show_calibration_pattern"].get<bool>();
            }
            if(mon.contains("show_screen_preview")) msettings.show_screen_preview = mon["show_screen_preview"].get<bool>();
            
            if(mon.contains("capture_zones") && mon["capture_zones"].is_array())
            {
                msettings.capture_zones.clear();
                const nlohmann::json& zones_arr = mon["capture_zones"];
                for(size_t zi = 0; zi < zones_arr.size(); zi++)
                {
                    const nlohmann::json& zone_json = zones_arr[zi];
                    CaptureZone zone;
                    zone.u_min = zone_json.value("u_min", 0.0f);
                    zone.u_max = zone_json.value("u_max", 1.0f);
                    zone.v_min = zone_json.value("v_min", 0.0f);
                    zone.v_max = zone_json.value("v_max", 1.0f);
                    zone.enabled = zone_json.value("enabled", true);
                    zone.name = zone_json.value("name", "Zone");
                    
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
            
            if(mon.contains("reference_point_id"))
            {
                msettings.reference_point_id = mon["reference_point_id"].get<int>();
            }

            if(mon.contains("radial_map_roll_deg"))
            {
                msettings.screen_map_roll_deg =
                    std::clamp(static_cast<float>(mon["radial_map_roll_deg"].get<double>()), -180.0f, 180.0f);
            }
            if(mon.contains("radial_map_expansion"))
            {
                msettings.radial_corner_expansion_ui = std::clamp(mon["radial_map_expansion"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_tl"))
            {
                msettings.radial_corner_bias_tl_ui = std::clamp(mon["radial_map_bias_tl"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_tr"))
            {
                msettings.radial_corner_bias_tr_ui = std::clamp(mon["radial_map_bias_tr"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_bl"))
            {
                msettings.radial_corner_bias_bl_ui = std::clamp(mon["radial_map_bias_bl"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_br"))
            {
                msettings.radial_corner_bias_br_ui = std::clamp(mon["radial_map_bias_br"].get<int>(), 0, 100);
            }
            if(mon.contains("sample_corner_blend_strength_pct"))
            {
                msettings.corner_blend_strength_pct = std::clamp(
                    static_cast<float>(mon["sample_corner_blend_strength_pct"].get<double>()), 0.0f, 100.0f);
            }
            if(mon.contains("sample_corner_blend_zone_pct"))
            {
                msettings.corner_blend_zone_pct = std::clamp(
                    static_cast<float>(mon["sample_corner_blend_zone_pct"].get<double>()), 0.0f, 32.0f);
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
            msettings.blend = std::clamp(msettings.blend, 0.0f, 100.0f);
            msettings.propagation_speed_mm_per_ms = std::clamp(msettings.propagation_speed_mm_per_ms, 0.0f, 100.0f);
            msettings.wave_decay_ms = std::clamp(msettings.wave_decay_ms, 0.0f, 3000.0f);
            msettings.wave_time_to_edge_sec = std::clamp(msettings.wave_time_to_edge_sec, 0.0f, 10.0f);
            msettings.falloff_curve_exponent = std::clamp(msettings.falloff_curve_exponent, 0.5f, 2.0f);
            msettings.front_back_balance = std::clamp(msettings.front_back_balance, -100.0f, 100.0f);
            msettings.left_right_balance = std::clamp(msettings.left_right_balance, -100.0f, 100.0f);
            msettings.top_bottom_balance = std::clamp(msettings.top_bottom_balance, -100.0f, 100.0f);
            msettings.screen_map_roll_deg = std::clamp(msettings.screen_map_roll_deg, -180.0f, 180.0f);
            msettings.radial_corner_expansion_ui = std::clamp(msettings.radial_corner_expansion_ui, 0, 100);
            msettings.radial_corner_bias_tl_ui = std::clamp(msettings.radial_corner_bias_tl_ui, 0, 100);
            msettings.radial_corner_bias_tr_ui = std::clamp(msettings.radial_corner_bias_tr_ui, 0, 100);
            msettings.radial_corner_bias_bl_ui = std::clamp(msettings.radial_corner_bias_bl_ui, 0, 100);
            msettings.radial_corner_bias_br_ui = std::clamp(msettings.radial_corner_bias_br_ui, 0, 100);
            msettings.corner_blend_strength_pct = std::clamp(msettings.corner_blend_strength_pct, 0.0f, 100.0f);
            msettings.corner_blend_zone_pct = std::clamp(msettings.corner_blend_zone_pct, 0.0f, 32.0f);
    }

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        SyncMonitorSettingsToUI(it->second);
    }

    RefreshReferencePointDropdowns();
    
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
        if(settings.blend_slider) settings.blend = (float)settings.blend_slider->value();
        if(settings.propagation_speed_slider) settings.propagation_speed_mm_per_ms = std::clamp((float)settings.propagation_speed_slider->value(), 0.0f, 100.0f);
        if(settings.wave_decay_slider) settings.wave_decay_ms = (float)settings.wave_decay_slider->value();
        if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_sec = (float)settings.wave_time_to_edge_slider->value() / 10.0f;
        if(settings.falloff_curve_slider) settings.falloff_curve_exponent = std::clamp((float)settings.falloff_curve_slider->value() / 100.0f, 0.5f, 2.0f);
        if(settings.front_back_balance_slider) settings.front_back_balance = std::clamp((float)settings.front_back_balance_slider->value(), -100.0f, 100.0f);
        if(settings.left_right_balance_slider) settings.left_right_balance = std::clamp((float)settings.left_right_balance_slider->value(), -100.0f, 100.0f);
        if(settings.top_bottom_balance_slider) settings.top_bottom_balance = std::clamp((float)settings.top_bottom_balance_slider->value(), -100.0f, 100.0f);

        if(settings.screen_map_roll_slider)
        {
            settings.screen_map_roll_deg = std::clamp(
                (float)settings.screen_map_roll_slider->value() / (float)kScreenMapRollTicksPerDegree, -180.0f, 180.0f);
        }
        if(settings.radial_corner_expansion_slider)
        {
            settings.radial_corner_expansion_ui = std::clamp(settings.radial_corner_expansion_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_tl_slider)
        {
            settings.radial_corner_bias_tl_ui = std::clamp(settings.radial_corner_bias_tl_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_tr_slider)
        {
            settings.radial_corner_bias_tr_ui = std::clamp(settings.radial_corner_bias_tr_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_bl_slider)
        {
            settings.radial_corner_bias_bl_ui = std::clamp(settings.radial_corner_bias_bl_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_br_slider)
        {
            settings.radial_corner_bias_br_ui = std::clamp(settings.radial_corner_bias_br_slider->value(), 0, 100);
        }
        if(settings.corner_blend_strength_slider)
        {
            settings.corner_blend_strength_pct =
                std::clamp((float)settings.corner_blend_strength_slider->value(), 0.0f, 100.0f);
        }
        if(settings.corner_blend_zone_slider)
        {
            settings.corner_blend_zone_pct =
                std::clamp((float)settings.corner_blend_zone_slider->value(), 0.0f, 32.0f);
        }
        
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

        if(settings.ref_point_combo)
        {
            int index = settings.ref_point_combo->currentIndex();
            if(index >= 0)
            {
                settings.reference_point_id = settings.ref_point_combo->itemData(index).toInt();
            }
        }
    }

    RefreshMonitorStatus();
    RefreshReferencePointDropdowns();

    emit ParametersChanged();
    in_parameter_change_ = false;
}

void ScreenMirror::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
    if(monitors_layout && monitor_settings.size() > 0)
    {
        bool has_ui_widgets = false;
        for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin(); it != monitor_settings.end(); ++it)
        {
            if(it->second.ref_point_combo)
            {
                has_ui_widgets = true;
                break;
            }
        }
        if(has_ui_widgets)
        {
            RefreshReferencePointDropdowns();
        }
    }
}

void ScreenMirror::RefreshReferencePointDropdowns()
{
    if(!reference_points || !monitors_layout)
    {
        return;
    }

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(!settings.ref_point_combo)
        {
            continue;
        }

        int desired_id = settings.reference_point_id;

        settings.ref_point_combo->blockSignals(true);
        settings.ref_point_combo->clear();

        settings.ref_point_combo->addItem("Room Center", QVariant(-1));
        settings.ref_point_combo->setItemData(0,
            "Falloff distance is measured from the room center.",
            Qt::ToolTipRole);

        for(size_t i = 0; i < reference_points->size(); i++)
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
            if(!ref_point) continue;

            QString name = QString::fromStdString(ref_point->GetName());
            QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
            QString display = QString("%1 (%2)").arg(name, type);
            settings.ref_point_combo->addItem(display, QVariant(ref_point->GetId()));
            const int row = settings.ref_point_combo->count() - 1;
            settings.ref_point_combo->setItemData(row,
                QStringLiteral("Measure reach/falloff from \"%1\" for this monitor.").arg(name),
                Qt::ToolTipRole);
        }

        int restore_index = settings.ref_point_combo->findData(QVariant(desired_id));
        if(restore_index < 0)
        {
            settings.reference_point_id = -1;
            restore_index = settings.ref_point_combo->findData(QVariant(-1));
        }
        if(restore_index >= 0)
        {
            settings.ref_point_combo->setCurrentIndex(restore_index);
        }

        settings.ref_point_combo->blockSignals(false);
    }
}

bool ScreenMirror::ResolveReferencePointById(int id, Vector3D& out) const
{
    if(!reference_points || id <= 0)
    {
        return false;
    }

    for(size_t i = 0; i < reference_points->size(); i++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
        if(ref_point && ref_point->GetId() == id)
        {
            out = ref_point->GetPosition();
            return true;
        }
    }
    return false;
}

int ScreenMirror::LookupReferencePointIdByIndex(int index) const
{
    if(!reference_points || index < 0 || index >= (int)reference_points->size())
    {
        return -1;
    }
    VirtualReferencePoint3D* ref_point = (*reference_points)[index].get();
    return ref_point ? ref_point->GetId() : -1;
}
