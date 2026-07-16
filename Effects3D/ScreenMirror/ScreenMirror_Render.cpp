// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "MediaTextureEffectUtils.h"
#include "VirtualReferencePoint3D.h"
#include "ScreenMirror/ScreenMirrorCalibrationPattern.h"
#include "ScreenMirror/ScreenMirror_Internal.h"

#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const uint8_t* GetCalibrationPatternBuffer(int& out_w, int& out_h)
{
    static std::vector<uint8_t> buffer;
    static std::once_flag init_once;
    std::call_once(init_once, []() { ScreenMirrorFillCalibrationPatternBuffer(buffer); });
    out_w = kScreenMirrorCalibrationPatternW;
    out_h = kScreenMirrorCalibrationPatternH;
    return buffer.data();
}

namespace
{
    inline RGBColor SampleFrameWithCornerBlend(const uint8_t* frame_data,
                                               int frame_w,
                                               int frame_h,
                                               float u_s,
                                               float v_s,
                                               float u_min,
                                               float u_max,
                                               float v_min,
                                               float v_max,
                                               unsigned int samp,
                                               float corner_strength_01,
                                               float corner_zone_01)
    {
        if(!frame_data || frame_w <= 0 || frame_h <= 0)
        {
            return ToRGBColor(0, 0, 0);
        }

        auto quant = [&](float& u, float& v) {
            if(samp < 100u)
            {
                Geometry3D::QuantizeMediaUV01(u, v, frame_w, frame_h, samp);
            }
        };

        const float strength = std::clamp(corner_strength_01, 0.0f, 1.0f);
        if(strength <= 0.004f)
        {
            float um = u_s, vm = v_s;
            quant(um, vm);
            return Geometry3D::SampleFrame(frame_data, frame_w, frame_h, um, vm, true);
        }

        const float zone = std::clamp(corner_zone_01, 0.0f, 0.32f);
        const float span_u = std::max(1e-6f, u_max - u_min);
        const float span_v = std::max(1e-6f, v_max - v_min);
        const float u_n = std::clamp((u_s - u_min) / span_u, 0.0f, 1.0f);
        const float v_n = std::clamp((v_s - v_min) / span_v, 0.0f, 1.0f);
        const float edge_u = std::min(u_n, 1.0f - u_n);
        const float edge_v = std::min(v_n, 1.0f - v_n);
        const float near_u = 1.0f - MediaTextureEffect::Smoothstep(0.0f, zone, edge_u);
        const float near_v = 1.0f - MediaTextureEffect::Smoothstep(0.0f, zone, edge_v);
        const float corner_w = std::min(near_u, near_v);

        float um = u_s, vm = v_s;
        quant(um, vm);
        RGBColor c_mid = Geometry3D::SampleFrame(frame_data, frame_w, frame_h, um, vm, true);
        if(corner_w <= 0.004f)
        {
            return c_mid;
        }

        const float w = corner_w * strength;
        if(w <= 0.004f)
        {
            return c_mid;
        }

        const float snap_u = (u_n < 0.5f) ? u_min : u_max;
        const float snap_v = (v_n < 0.5f) ? v_min : v_max;

        float uv_u = snap_u, uv_v = v_s;
        quant(uv_u, uv_v);
        RGBColor c_vert = Geometry3D::SampleFrame(frame_data, frame_w, frame_h, uv_u, uv_v, true);

        float uh_u = u_s, uh_v = snap_v;
        quant(uh_u, uh_v);
        RGBColor c_horiz = Geometry3D::SampleFrame(frame_data, frame_w, frame_h, uh_u, uh_v, true);

        const float r =
            (1.0f - w) * (float)RGBGetRValue(c_mid) + w * 0.5f * ((float)RGBGetRValue(c_vert) + (float)RGBGetRValue(c_horiz));
        const float g =
            (1.0f - w) * (float)RGBGetGValue(c_mid) + w * 0.5f * ((float)RGBGetGValue(c_vert) + (float)RGBGetGValue(c_horiz));
        const float b =
            (1.0f - w) * (float)RGBGetBValue(c_mid) + w * 0.5f * ((float)RGBGetBValue(c_vert) + (float)RGBGetBValue(c_horiz));
        return ToRGBColor((uint8_t)std::clamp((int)std::lround(r), 0, 255),
                          (uint8_t)std::clamp((int)std::lround(g), 0, 255),
                          (uint8_t)std::clamp((int)std::lround(b), 0, 255));
    }

    float ComputeMaxReferenceDistanceMm(const GridContext3D& grid, const Vector3D& reference, float grid_scale_mm)
    {
        std::array<float, 2> xs = {grid.min_x, grid.max_x};
        std::array<float, 2> ys = {grid.min_y, grid.max_y};
        std::array<float, 2> zs = {grid.min_z, grid.max_z};

        float max_distance_sq = 0.0f;
        for(size_t x_index = 0; x_index < xs.size(); x_index++)
        {
            float cx = xs[x_index];
            for(size_t y_index = 0; y_index < ys.size(); y_index++)
            {
                float cy = ys[y_index];
                for(size_t z_index = 0; z_index < zs.size(); z_index++)
                {
                    float cz = zs[z_index];
                    float dx = GridUnitsToMM(cx - reference.x, grid_scale_mm);
                    float dy = GridUnitsToMM(cy - reference.y, grid_scale_mm);
                    float dz = GridUnitsToMM(cz - reference.z, grid_scale_mm);
                    float dist_sq = dx * dx + dy * dy + dz * dz;
                    if(dist_sq > max_distance_sq)
                    {
                        max_distance_sq = dist_sq;
                    }
                }
            }
        }
        if(max_distance_sq <= 0.0f)
        {
            return 0.0f;
        }
        return sqrtf(max_distance_sq);
    }

    float WaveIntensityToSpeedMmPerMs(float intensity_0_to_100)
    {
        if(intensity_0_to_100 < 0.5f) return 0.0f;
        float p = std::clamp(intensity_0_to_100, 1.0f, 100.0f) / 100.0f;
        float speed = 200.0f * powf(0.01f, p);
        return std::clamp(speed, 0.5f, 500.0f);
    }

    float ComputeInvertedShellFalloff(float distance_mm,
                                      float max_distance_mm,
                                      float coverage,
                                      float softness_percent,
                                      float curve_exponent = 1.0f)
    {
        coverage = std::max(0.0f, coverage);
        if(coverage <= 0.0001f || max_distance_mm <= 0.0f)
        {
            return 0.0f;
        }

        if(coverage >= 0.999f)
        {
            return 1.0f;
        }

        float normalized_distance = std::clamp(distance_mm / std::max(max_distance_mm, 1.0f), 0.0f, 1.0f);
        float exp_val = std::clamp(curve_exponent, 0.25f, 4.0f);
        normalized_distance = powf(normalized_distance, exp_val);
        float boundary = std::max(0.0f, 1.0f - coverage);
        if(boundary <= 0.0005f)
        {
            return 1.0f;
        }

        float softness_ratio = std::clamp(softness_percent / 100.0f, 0.0f, 0.95f);
        float feather_band = softness_ratio * 0.5f;
        float fade_start = std::max(0.0f, boundary - feather_band);
        float fade_end = boundary;

        if(normalized_distance <= fade_start)
        {
            return 0.0f;
        }
        if(normalized_distance >= fade_end)
        {
            return 1.0f;
        }
        return MediaTextureEffect::Smoothstep(fade_start, fade_end, normalized_distance);
    }
}

void ScreenMirror::RefreshFrameCacheForRenderSequence(const GridContext3D& grid)
{
    const uint64_t now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    bool need_frame_cache_refresh = false;
    if(grid.render_sequence != 0)
    {
        if(grid.render_sequence != frame_cache_last_render_seq_)
        {
            need_frame_cache_refresh = true;
            frame_cache_last_render_seq_ = grid.render_sequence;
        }
    }
    else
    {
        const uint64_t cache_max_age_ms = 8;
        if(frame_cache_refresh_ms_ == 0 || (now_ms - frame_cache_refresh_ms_) >= cache_max_age_ms)
        {
            need_frame_cache_refresh = true;
        }
    }

    if(!need_frame_cache_refresh)
    {
        return;
    }

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsCaptureSessionActive())
    {
        return;
    }

    frame_cache_planes_ = DisplayPlaneManager::instance()->GetDisplayPlanes();
    std::unordered_set<std::string> seen_capture_ids;
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }
    capture_mgr.SetTargetFPS(120);
    int cap_w = 320, cap_h = 180;
    int q = std::clamp(capture_quality, 0, 7);
    if(q == 1) { cap_w = 480; cap_h = 270; }
    else if(q == 2) { cap_w = 640; cap_h = 360; }
    else if(q == 3) { cap_w = 960; cap_h = 540; }
    else if(q == 4) { cap_w = 1280; cap_h = 720; }
    else if(q == 5) { cap_w = 1920; cap_h = 1080; }
    else if(q == 6) { cap_w = 2560; cap_h = 1440; }
    else if(q == 7) { cap_w = 3840; cap_h = 2160; }
    capture_mgr.SetDownscaleResolution(cap_w, cap_h);
    for(size_t i = 0; i < frame_cache_planes_.size(); i++)
    {
        DisplayPlane3D* plane = frame_cache_planes_[i];
        if(!plane) continue;
        std::string capture_id = plane->GetCaptureSourceId();
        if(capture_id.empty()) continue;
        seen_capture_ids.insert(capture_id);
        if(!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
        }
        std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(capture_id);
        if(frame && frame->valid && !frame->data.empty())
        {
            frame_cache_[capture_id] = frame;
            AddFrameToHistory(capture_id, frame);
        }
    }
    for(std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>::iterator it = frame_cache_.begin();
        it != frame_cache_.end(); )
    {
        if(seen_capture_ids.find(it->first) == seen_capture_ids.end())
        {
            it = frame_cache_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    frame_cache_refresh_ms_ = now_ms;
}

RGBColor ScreenMirror::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    RefreshFrameCacheForRenderSequence(grid);
    return CalculateColorGridInternal(x, y, z, time, grid, &frame_cache_, &frame_cache_planes_);
}

RGBColor ScreenMirror::CalculateColorGridInternal(float x, float y, float z, float time, const GridContext3D& grid,
                                                     const std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* frame_cache,
                                                     const std::vector<DisplayPlane3D*>* pre_fetched_planes,
                                                     bool apply_led_smoothing)
{
    (void)time;
    std::vector<DisplayPlane3D*> all_planes;
    if(pre_fetched_planes)
        all_planes = *pre_fetched_planes;
    else
        all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    if(all_planes.empty())
    {
        return ToRGBColor(0, 0, 0);
    }

    Vector3D led_pos = {x, y, z};
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    struct MonitorContribution
    {
        DisplayPlane3D* plane;
        Geometry3D::PlaneProjection proj;
        std::shared_ptr<CapturedFrame> frame;
        std::shared_ptr<CapturedFrame> frame_blend;
        float blend_t;
        float weight;
        float blend;
        float brightness_multiplier;
        float brightness_threshold;
        float white_rolloff;
        float vibrance;
        float led_output_gain_r;
        float led_output_gain_g;
        float led_output_gain_b;
        float black_bar_letterbox_percent;
        float black_bar_pillarbox_percent;
        float smoothing_time_ms;
        bool use_calibration_pattern;
        float corner_blend_strength_01;
        float corner_blend_zone_01;
    };

    std::vector<MonitorContribution> contributions;
    contributions.reserve(all_planes.size());
    Vector3D grid_anchor_ref = GetReferencePointGrid(grid);

    const float scale_mm = SafeGridScaleMm(grid.grid_scale_mm);
    float base_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, grid_anchor_ref, scale_mm);
    if(base_max_distance_mm <= 0.0f)
    {
        base_max_distance_mm = 3000.0f;
    }

    std::map<std::string, std::unordered_map<std::string, FrameHistory>::iterator> history_cache;

    for(size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = all_planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            settings_it = monitor_settings.emplace(plane_name, MonitorSettings()).first;
            settings_it->second.enabled = DefaultMonitorEnabledForPlane(plane);
            if(settings_it->second.capture_zones.empty())
            {
                settings_it->second.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
            }
        }

        MonitorSettings& mon_settings = settings_it->second;
        
        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        bool has_enabled_zone = false;
        for(size_t zone_idx = 0; zone_idx < mon_settings.capture_zones.size(); zone_idx++)
        {
            if(mon_settings.capture_zones[zone_idx].enabled)
            {
                has_enabled_zone = true;
                break;
            }
        }
        if(!has_enabled_zone && !mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones[0].enabled = true;
        }
        
        bool monitor_enabled = mon_settings.group_box ? mon_settings.group_box->isChecked() : mon_settings.enabled;
        if(!monitor_enabled)
        {
            continue;
        }

        bool monitor_calibration_pattern = mon_settings.show_calibration_pattern;
        
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if(!monitor_calibration_pattern)
        {
            if(capture_id.empty())
            {
                continue;
            }
            if(frame_cache)
            {
                std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>::const_iterator it =
                    frame_cache->find(capture_id);
                frame = (it != frame_cache->end()) ? it->second : nullptr;
            }
            else
            {
                if(!capture_mgr.IsCapturing(capture_id))
                {
                    capture_mgr.StartCapture(capture_id);
                    if(!capture_mgr.IsCapturing(capture_id))
                    {
                        continue;
                    }
                }
                frame = capture_mgr.GetLatestFrame(capture_id);
                if(!frame || !frame->valid || frame->data.empty())
                {
                    continue;
                }
                AddFrameToHistory(capture_id, frame);
            }
            if(!frame || !frame->valid || frame->data.empty())
            {
                continue;
            }
        }

        const Vector3D* falloff_ref = &grid_anchor_ref;
        Vector3D custom_ref;
        if(mon_settings.reference_point_id > 0 &&
           ResolveReferencePointById(mon_settings.reference_point_id, custom_ref))
        {
            falloff_ref = &custom_ref;
        }

        float reference_max_distance_mm = base_max_distance_mm;
        if(falloff_ref != &grid_anchor_ref)
        {
            reference_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, *falloff_ref, scale_mm);
            if(reference_max_distance_mm <= 0.0f)
            {
                reference_max_distance_mm = base_max_distance_mm;
            }
        }

        Geometry3D::PlaneProjection proj =
            Geometry3D::SpatialMapToScreen(led_pos, *plane, 0.0f, falloff_ref, scale_mm);

        if(!proj.is_valid) continue;

        float u = proj.u;
        float v = proj.v;

        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        bool in_zone = false;
        for(size_t zone_idx = 0; zone_idx < mon_settings.capture_zones.size(); zone_idx++)
        {
            const CaptureZone& zone = mon_settings.capture_zones[zone_idx];
            if(zone.Contains(u, v))
            {
                in_zone = true;
                break;
            }
        }
        if(!in_zone)
        {
            continue;
        }

        Geometry3D::ApplyRadialCornerMapping01(u, v,
                                               RadialMapUiToInternal(mon_settings.radial_corner_expansion_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_tl_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_tr_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_bl_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_br_ui));

        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        Geometry3D::ApplyUVRotationDegrees01(u, v, mon_settings.screen_map_roll_deg);

        proj.u = u;
        proj.v = v;

        float monitor_scale = std::clamp(mon_settings.scale, 0.0f, 3.0f);
        float coverage = monitor_scale;
        float curve_exp = std::clamp(mon_settings.falloff_curve_exponent, 0.5f, 2.0f);
        float distance_falloff = 0.0f;
        const float edge_softness = mon_settings.edge_softness;

        if(mon_settings.scale_inverted)
        {
            if(coverage > 0.0001f)
            {
                float effective_range = reference_max_distance_mm * coverage;
                effective_range = std::max(effective_range, 10.0f);
                distance_falloff = Geometry3D::ComputeFalloff(proj.distance, effective_range, edge_softness);
            }
        }
        else
        {
            distance_falloff = ComputeInvertedShellFalloff(proj.distance, reference_max_distance_mm, coverage, edge_softness, curve_exp);

            if(coverage >= 1.0f && distance_falloff < 1.0f)
            {
                distance_falloff = std::max(distance_falloff, std::min(coverage - 0.99f, 1.0f));
            }
        }

        std::shared_ptr<CapturedFrame> sampling_frame = frame;
        std::shared_ptr<CapturedFrame> sampling_frame_blend;
        float sampling_blend_t = 0.0f;
        float delay_ms = 0.0f;
        float speed_mm_per_ms = 0.0f;
        bool use_wave = !monitor_calibration_pattern && !capture_id.empty();
        if(use_wave && mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
            speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
            speed_mm_per_ms = std::max(speed_mm_per_ms, 0.1f);
            delay_ms = proj.distance / speed_mm_per_ms;
            delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
        }
        else if(use_wave && mon_settings.propagation_speed_mm_per_ms >= 5.0f)
        {
            speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
            delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.5f);
            delay_ms = std::clamp(delay_ms, 0.0f, 15000.0f);
        }

        if(use_wave && (mon_settings.wave_time_to_edge_sec > 0.4f || mon_settings.propagation_speed_mm_per_ms >= 5.0f))
        {
            std::unordered_map<std::string, FrameHistory>::iterator history_it = capture_history.end();
            std::map<std::string, std::unordered_map<std::string, FrameHistory>::iterator>::iterator cache_it = history_cache.find(capture_id);
            if(cache_it != history_cache.end())
            {
                history_it = cache_it->second;
            }
            else
            {
                history_it = capture_history.find(capture_id);
                if(history_it != capture_history.end())
                {
                    history_cache[capture_id] = history_it;
                }
            }
            
            if(history_it != capture_history.end() && history_it->second.frames.size() >= 2)
            {
                FrameHistory& history = history_it->second;
                const std::deque<std::shared_ptr<CapturedFrame>>& frames = history.frames;
                
                float avg_frame_time_ms = history.cached_avg_frame_time_ms > 0.0f
                    ? history.cached_avg_frame_time_ms : 16.67f;
                uint64_t latest_timestamp = frames.back()->timestamp_ms;
                const uint64_t frame_rate_stale_ms = 200;

                if(history.last_frame_rate_update == 0 ||
                   (latest_timestamp - history.last_frame_rate_update) > frame_rate_stale_ms)
                {
                    if(frames.size() >= 2)
                    {
                        size_t check_frames = std::min(frames.size() - 1, (size_t)10);
                        uint64_t total_time = 0;
                        size_t valid_pairs = 0;
                        const uint64_t min_delta_ms = 8;
                        const uint64_t max_delta_ms = 80;

                        for(size_t i = frames.size() - check_frames; i < frames.size(); i++)
                        {
                            if(i > 0 && i < frames.size())
                            {
                                uint64_t frame_time = frames[i]->timestamp_ms;
                                uint64_t prev_time = frames[i-1]->timestamp_ms;
                                uint64_t delta = (frame_time > prev_time) ? (frame_time - prev_time) : 0;
                                if(delta >= min_delta_ms && delta <= max_delta_ms)
                                {
                                    total_time += delta;
                                    valid_pairs++;
                                }
                            }
                        }

                        if(valid_pairs > 0 && total_time > 0)
                        {
                            float measured_ms = (float)total_time / (float)valid_pairs;
                            measured_ms = std::clamp(measured_ms, 12.0f, 50.0f);
                            if(history.cached_avg_frame_time_ms > 0.0f)
                                avg_frame_time_ms = 0.75f * history.cached_avg_frame_time_ms + 0.25f * measured_ms;
                            else
                                avg_frame_time_ms = measured_ms;
                        }
                    }
                    history.cached_avg_frame_time_ms = avg_frame_time_ms;
                    history.last_frame_rate_update = latest_timestamp;
                }
                else
                {
                    avg_frame_time_ms = history.cached_avg_frame_time_ms;
                }

                float frame_offset_f = delay_ms / std::max(avg_frame_time_ms, 1.0f);
                frame_offset_f = std::max(0.0f, frame_offset_f);
                int frame_offset_int = (int)(frame_offset_f + 0.5f);
                frame_offset_int = std::max(0, frame_offset_int);

                if(frame_offset_int < (int)frames.size())
                {
                    size_t frame_index_lo = frames.size() - 1 - (size_t)frame_offset_int;
                    float frac = frame_offset_f - std::floor(frame_offset_f);
                    if(frame_index_lo < frames.size())
                    {
                        sampling_frame = frames[frame_index_lo];
                        if(frac > 0.01f && frame_index_lo + 1 < frames.size())
                        {
                            sampling_frame_blend = frames[frame_index_lo + 1];
                            sampling_blend_t = frac;
                        }
                    }
                }
            }
            else
            {
            }
        }

        float wave_envelope = 1.0f;
        if((mon_settings.wave_time_to_edge_sec > 0.4f || mon_settings.propagation_speed_mm_per_ms >= 5.0f) && mon_settings.wave_decay_ms > 0.1f)
        {
            if(delay_ms <= 0.0f && use_wave)
            {
                if(mon_settings.wave_time_to_edge_sec > 0.4f)
                {
                    float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
                    speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.1f);
                }
                else
                {
                    speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.5f);
                }
                delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
            }
            wave_envelope = std::exp(-delay_ms / std::max(mon_settings.wave_decay_ms, 0.1f));
        }

        float weight = distance_falloff * wave_envelope;

        const float ref_max_units = MMToGridUnits(reference_max_distance_mm, scale_mm);
        if(ref_max_units > 0.001f && (std::fabs(mon_settings.front_back_balance) > 0.5f || std::fabs(mon_settings.left_right_balance) > 0.5f || std::fabs(mon_settings.top_bottom_balance) > 0.5f))
        {
            Vector3D ref_to_led = { led_pos.x - falloff_ref->x, led_pos.y - falloff_ref->y, led_pos.z - falloff_ref->z };
            const Transform3D& transform = plane->GetTransform();
            float rot[9];
            Geometry3D::ComputeRotationMatrix(transform.rotation, rot);
            Vector3D plane_right  = { rot[0], rot[3], rot[6] };
            Vector3D plane_up     = { rot[1], rot[4], rot[7] };
            Vector3D plane_normal = { rot[2], rot[5], rot[8] };
            float lateral = ref_to_led.x * plane_right.x + ref_to_led.y * plane_right.y + ref_to_led.z * plane_right.z;
            float vertical = ref_to_led.x * plane_up.x + ref_to_led.y * plane_up.y + ref_to_led.z * plane_up.z;
            float depth = ref_to_led.x * plane_normal.x + ref_to_led.y * plane_normal.y + ref_to_led.z * plane_normal.z;
            float lat_norm = std::clamp(lateral / ref_max_units, -1.0f, 1.0f);
            float vert_norm = std::clamp(vertical / ref_max_units, -1.0f, 1.0f);
            float depth_norm = std::clamp(depth / ref_max_units, -1.0f, 1.0f);
            float dir_fb = std::clamp(1.0f + (mon_settings.front_back_balance / 100.0f) * depth_norm, 0.0f, 2.0f);
            float dir_lr = std::clamp(1.0f + (mon_settings.left_right_balance / 100.0f) * lat_norm, 0.0f, 2.0f);
            float dir_tb = std::clamp(1.0f + (mon_settings.top_bottom_balance / 100.0f) * vert_norm, 0.0f, 2.0f);
            weight *= dir_fb * dir_lr * dir_tb;
        }

        if(weight > 0.01f)
        {
            MonitorContribution contrib;
            contrib.plane = plane;
            contrib.proj = proj;
            contrib.frame = sampling_frame;
            contrib.frame_blend = sampling_frame_blend;
            contrib.blend_t = sampling_blend_t;
            contrib.weight = weight;
            contrib.blend = mon_settings.blend;
            contrib.brightness_multiplier = mon_settings.brightness_multiplier;
            contrib.brightness_threshold = mon_settings.brightness_threshold;
            contrib.white_rolloff = mon_settings.white_rolloff;
            contrib.vibrance = mon_settings.vibrance;
            contrib.led_output_gain_r = mon_settings.led_output_gain_r;
            contrib.led_output_gain_g = mon_settings.led_output_gain_g;
            contrib.led_output_gain_b = mon_settings.led_output_gain_b;
            contrib.black_bar_letterbox_percent = mon_settings.black_bar_letterbox_percent;
            contrib.black_bar_pillarbox_percent = mon_settings.black_bar_pillarbox_percent;
            contrib.smoothing_time_ms = mon_settings.smoothing_time_ms;
            contrib.use_calibration_pattern = mon_settings.show_calibration_pattern;
            contrib.corner_blend_strength_01 =
                std::clamp(mon_settings.corner_blend_strength_pct / 100.0f, 0.0f, 1.0f);
            contrib.corner_blend_zone_01 =
                std::clamp(mon_settings.corner_blend_zone_pct / 100.0f, 0.0f, 0.32f);
            contributions.push_back(contrib);
        }
    }

    if(contributions.empty())
    {
        if(show_calibration_pattern)
        {
            return ToRGBColor(0, 0, 0);
        }
        
        int capturing_count = 0;
        for(size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
        {
            DisplayPlane3D* plane = all_planes[plane_index];
            if(plane && !plane->GetCaptureSourceId().empty())
            {
                if(capture_mgr.IsCapturing(plane->GetCaptureSourceId()))
                {
                    capturing_count++;
                }
            }
        }

        if(capturing_count > 0)
        {
            return ToRGBColor(0, 0, 0);
        }
        else
        {
            return ToRGBColor(128, 0, 128);
        }
    }

    float avg_blend = 0.0f;
    for(size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        avg_blend += contributions[contrib_index].blend;
    }
    avg_blend /= (float)contributions.size();
    float blend_factor = avg_blend / 100.0f;

    if(blend_factor < 0.01f && contributions.size() > 1)
    {
        size_t strongest_idx = 0;
        float max_weight = contributions[0].weight;
        for(size_t i = 1; i < contributions.size(); i++)
        {
            if(contributions[i].weight > max_weight)
            {
                max_weight = contributions[i].weight;
                strongest_idx = i;
            }
        }
        if(strongest_idx != 0)
        {
            contributions[0] = contributions[strongest_idx];
        }
        contributions.resize(1);
    }

    float total_r = 0.0f, total_g = 0.0f, total_b = 0.0f;
    float total_weight = 0.0f;
    std::vector<float> per_contrib_r;
    std::vector<float> per_contrib_g;
    std::vector<float> per_contrib_b;
    std::vector<float> per_contrib_w;
    per_contrib_r.reserve(contributions.size());
    per_contrib_g.reserve(contributions.size());
    per_contrib_b.reserve(contributions.size());
    per_contrib_w.reserve(contributions.size());

    for(size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        MonitorContribution& contrib = contributions[contrib_index];
        float sample_u = contrib.proj.u;
        float sample_v = contrib.proj.v;
        const float corner_strength_01 = contrib.corner_blend_strength_01;
        const float corner_zone_01 = contrib.corner_blend_zone_01;

        float r, g, b;

        if(contrib.use_calibration_pattern)
        {
            int cal_w = 0;
            int cal_h = 0;
            const uint8_t* cal_data = GetCalibrationPatternBuffer(cal_w, cal_h);
            float lp = std::clamp(contrib.black_bar_letterbox_percent, 0.0f, 49.0f) / 100.0f;
            float pp = std::clamp(contrib.black_bar_pillarbox_percent, 0.0f, 49.0f) / 100.0f;
            float u_min = pp;
            float u_max = 1.0f - pp;
            float v_min = lp;
            float v_max = 1.0f - lp;
            float sample_u_clamped = std::clamp(sample_u, u_min, u_max);
            float tex_v = std::clamp(sample_v, v_min, v_max);
            const unsigned int samp = GetSamplingResolution();
            RGBColor sampled_cal = SampleFrameWithCornerBlend(cal_data,
                                                              cal_w,
                                                              cal_h,
                                                              sample_u_clamped,
                                                              tex_v,
                                                              u_min,
                                                              u_max,
                                                              v_min,
                                                              v_max,
                                                              samp,
                                                              corner_strength_01,
                                                              corner_zone_01);
            r = (float)RGBGetRValue(sampled_cal);
            g = (float)RGBGetGValue(sampled_cal);
            b = (float)RGBGetBValue(sampled_cal);
        }
        else
        {
            if(!contrib.frame || contrib.frame->data.empty())
            {
                continue;
            }

            float lp = std::clamp(contrib.black_bar_letterbox_percent, 0.0f, 49.0f) / 100.0f;
            float pp = std::clamp(contrib.black_bar_pillarbox_percent, 0.0f, 49.0f) / 100.0f;
            float u_min = pp, u_max = 1.0f - pp;
            float v_min = lp, v_max = 1.0f - lp;
            float sample_u_clamped = std::clamp(sample_u, u_min, u_max);
            float flipped_v = 1.0f - sample_v;
            flipped_v = std::clamp(flipped_v, v_min, v_max);
            const unsigned int samp = GetSamplingResolution();
            const float u_s = sample_u_clamped;
            const float v_s = flipped_v;

            RGBColor sampled_color = SampleFrameWithCornerBlend(contrib.frame->data.data(),
                                                                contrib.frame->width,
                                                                contrib.frame->height,
                                                                u_s,
                                                                v_s,
                                                                u_min,
                                                                u_max,
                                                                v_min,
                                                                v_max,
                                                                samp,
                                                                corner_strength_01,
                                                                corner_zone_01);

            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);

            if(contrib.frame_blend && !contrib.frame_blend->data.empty() && contrib.blend_t > 0.01f)
            {
                const float u_b = sample_u_clamped;
                const float v_b = flipped_v;
                RGBColor sampled_blend = SampleFrameWithCornerBlend(contrib.frame_blend->data.data(),
                                                                      contrib.frame_blend->width,
                                                                      contrib.frame_blend->height,
                                                                      u_b,
                                                                      v_b,
                                                                      u_min,
                                                                      u_max,
                                                                      v_min,
                                                                      v_max,
                                                                      samp,
                                                                      corner_strength_01,
                                                                      corner_zone_01);
                float r2 = (float)RGBGetRValue(sampled_blend);
                float g2 = (float)RGBGetGValue(sampled_blend);
                float b2 = (float)RGBGetBValue(sampled_blend);
                float t = std::clamp(contrib.blend_t, 0.0f, 1.0f);
                r = (1.0f - t) * r + t * r2;
                g = (1.0f - t) * g + t * g2;
                b = (1.0f - t) * b + t * b2;
            }

            if(contrib.brightness_threshold > 0.0f)
            {
                float thr = std::min(255.0f, contrib.brightness_threshold);
                float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
                float peak = std::max(r, std::max(g, b));
                float level = std::max(luminance, peak);
                if(level <= thr)
                {
                    float t = (thr <= 0.0f) ? 1.0f : std::max(0.0f, level / thr);
                    t = t * t;
                    contrib.weight *= t;
                    r *= t;
                    g *= t;
                    b *= t;
                }
            }
        }

        float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        float max_rgb = std::max(r, std::max(g, b));
        float min_rgb = std::min(r, std::min(g, b));
        float sat = (max_rgb > 0.001f) ? ((max_rgb - min_rgb) / max_rgb) : 0.0f;

        const float wr_full = std::clamp(contrib.white_rolloff, 0.0f, kWhiteRolloffStoredMax);
        const float wr_sub = std::min(1.0f, wr_full);

        if(lum > 242.0f && sat < 0.20f)
        {
            float t = std::clamp((lum - 242.0f) / 13.0f, 0.0f, 1.0f);
            t *= (1.0f - wr_sub);
            float gray = lum;
            r = (1.0f - t) * r + t * gray;
            g = (1.0f - t) * g + t * gray;
            b = (1.0f - t) * b + t * gray;
        }
        if(wr_sub > 0.0f)
        {
            float wmin = std::min(r, std::min(g, b));
            float sub = wr_sub * wmin;
            r -= sub;
            g -= sub;
            b -= sub;
        }
        const float ultra01 = std::clamp((wr_full - 1.0f) / (kWhiteRolloffStoredMax - 1.0f), 0.0f, 1.0f);
        if(ultra01 > 0.0f)
        {
            float gray = (r + g + b) / 3.0f;
            const float boost = 1.0f + ultra01 * 0.95f;
            r = gray + (r - gray) * boost;
            g = gray + (g - gray) * boost;
            b = gray + (b - gray) * boost;
            r = std::clamp(r, 0.0f, 255.0f);
            g = std::clamp(g, 0.0f, 255.0f);
            b = std::clamp(b, 0.0f, 255.0f);
        }

        r *= contrib.brightness_multiplier;
        g *= contrib.brightness_multiplier;
        b *= contrib.brightness_multiplier;

        float vib = std::max(0.0f, std::min(2.0f, contrib.vibrance));
        if(std::fabs(vib - 1.0f) > 0.001f)
        {
            float gray = (r + g + b) / 3.0f;
            r = gray + (r - gray) * vib;
            g = gray + (g - gray) * vib;
            b = gray + (b - gray) * vib;
            r = std::max(0.0f, std::min(255.0f, r));
            g = std::max(0.0f, std::min(255.0f, g));
            b = std::max(0.0f, std::min(255.0f, b));
        }

        r = std::clamp(r * contrib.led_output_gain_r, 0.0f, 255.0f);
        g = std::clamp(g * contrib.led_output_gain_g, 0.0f, 255.0f);
        b = std::clamp(b * contrib.led_output_gain_b, 0.0f, 255.0f);

        float adjusted_weight = contrib.weight * (0.5f + 0.5f * blend_factor);

        total_r += r * adjusted_weight;
        total_g += g * adjusted_weight;
        total_b += b * adjusted_weight;
        total_weight += adjusted_weight;

        per_contrib_r.push_back(r);
        per_contrib_g.push_back(g);
        per_contrib_b.push_back(b);
        per_contrib_w.push_back(adjusted_weight);
    }

    if(total_weight > 0.0f)
    {
        total_r /= total_weight;
        total_g /= total_weight;
        total_b /= total_weight;
    }

    if(per_contrib_w.size() >= 2 && total_weight > 1e-6f)
    {
        size_t best = 0;
        for(size_t i = 1; i < per_contrib_w.size(); i++)
        {
            if(per_contrib_w[i] > per_contrib_w[best])
            {
                best = i;
            }
        }
        const float dominance = per_contrib_w[best] / total_weight;
        if(dominance >= 0.50f)
        {
            total_r = per_contrib_r[best];
            total_g = per_contrib_g[best];
            total_b = per_contrib_b[best];
        }
        else
        {
            float maxc = std::max(total_r, std::max(total_g, total_b));
            float minc = std::min(total_r, std::min(total_g, total_b));
            if(maxc > 1.0f)
            {
                float sat_mix = (maxc - minc) / maxc;
                if(sat_mix < 0.30f)
                {
                    float gray = (total_r + total_g + total_b) / 3.0f;
                    float k = 1.0f + (0.30f - sat_mix) * 1.15f;
                    k = std::min(k, 1.48f);
                    total_r = gray + (total_r - gray) * k;
                    total_g = gray + (total_g - gray) * k;
                    total_b = gray + (total_b - gray) * k;
                    total_r = std::clamp(total_r, 0.0f, 255.0f);
                    total_g = std::clamp(total_g, 0.0f, 255.0f);
                    total_b = std::clamp(total_b, 0.0f, 255.0f);
                }
            }
        }
    }

    if(total_r > 255.0f) total_r = 255.0f;
    if(total_g > 255.0f) total_g = 255.0f;
    if(total_b > 255.0f) total_b = 255.0f;

    if(apply_led_smoothing)
    {
        float max_smoothing_time = 0.0f;
        if(contributions.size() == 1)
        {
            max_smoothing_time = contributions[0].smoothing_time_ms;
        }
        else
        {
            for(size_t i = 0; i < contributions.size(); i++)
            {
                if(contributions[i].smoothing_time_ms > max_smoothing_time)
                {
                    max_smoothing_time = contributions[i].smoothing_time_ms;
                }
            }
        }
        
        if(max_smoothing_time > 0.1f)
        {
            LEDKey key = MakeLEDKey(x, y, z);
            LEDState& state = led_states[key];

            static const std::chrono::steady_clock::time_point smooth_clock_start = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point now_tp = std::chrono::steady_clock::now();
            uint64_t tick_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now_tp - smooth_clock_start)
                                     .count();

            if(state.smooth_last_tick_ms == 0)
            {
                state.r = total_r;
                state.g = total_g;
                state.b = total_b;
                state.smooth_last_tick_ms = tick_ms;
            }
            else
            {
                uint64_t dt_ms_u64 = (tick_ms > state.smooth_last_tick_ms) ? (tick_ms - state.smooth_last_tick_ms) : 0;
                if(dt_ms_u64 == 0)
                {
                    dt_ms_u64 = 1;
                }
                float dt = (float)dt_ms_u64;
                float tau = max_smoothing_time;
                float alpha = dt / (tau + dt);

                state.r += alpha * (total_r - state.r);
                state.g += alpha * (total_g - state.g);
                state.b += alpha * (total_b - state.b);
                state.smooth_last_tick_ms = tick_ms;

                total_r = state.r;
                total_g = state.g;
                total_b = state.b;
            }
        }
        else
        {
            LEDKey key = MakeLEDKey(x, y, z);
            led_states.erase(key);
        }
    }

    return ToRGBColor((uint8_t)total_r, (uint8_t)total_g, (uint8_t)total_b);
}

void ScreenMirror::AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame)
{
    if(capture_id.empty() || !frame || !frame->valid)
    {
        return;
    }

    FrameHistory& history = capture_history[capture_id];
    if(!history.frames.empty() && history.frames.back()->frame_id == frame->frame_id)
    {
        return;
    }

    history.frames.push_back(frame);

    uint64_t retention_ms = (uint64_t)GetHistoryRetentionMs();
    uint64_t cutoff = (frame->timestamp_ms > retention_ms) ? frame->timestamp_ms - retention_ms : 0;

    while(history.frames.size() > 1 && history.frames.front()->timestamp_ms < cutoff)
    {
        history.frames.pop_front();
    }

    const size_t max_frames = 180;
    if(history.frames.size() > max_frames)
    {
        history.frames.pop_front();
    }
}

float ScreenMirror::GetHistoryRetentionMs() const
{
    float max_retention = 600.0f;
    
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        if(!mon_settings.enabled) continue;
        
        float monitor_retention = std::max(mon_settings.wave_decay_ms * 3.0f, mon_settings.smoothing_time_ms * 3.0f);
        if(mon_settings.propagation_speed_mm_per_ms >= 5.0f || mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float max_distance_mm = 5000.0f;
            if(mon_settings.wave_time_to_edge_sec > 0.4f)
            {
                float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
                monitor_retention = std::max(monitor_retention, t_sec * 1000.0f);
            }
            else
            {
                float speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
                if(speed_mm_per_ms > 0.0f)
                    monitor_retention = std::max(monitor_retention, max_distance_mm / speed_mm_per_ms);
            }
            monitor_retention = std::max(monitor_retention, mon_settings.wave_decay_ms * 2.0f);
        }
        max_retention = std::max(max_retention, monitor_retention);
    }
    
    return std::max(max_retention, 600.0f);
}

ScreenMirror::LEDKey ScreenMirror::MakeLEDKey(float x, float y, float z) const
{
    const float quantize_scale = 1000.0f;
    LEDKey key;
    key.x = (int)std::lround(x * quantize_scale);
    key.y = (int)std::lround(y * quantize_scale);
    key.z = (int)std::lround(z * quantize_scale);
    return key;
}
