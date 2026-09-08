// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "ScreenCaptureDownscale.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "LEDPosition3D.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "ScreenMirror/ScreenMirrorCalibrationPattern.h"
#include "ScreenMirror/ScreenMirror_Internal.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    void GradeRgb(float& r, float& g, float& b,
                  float brightness,
                  float threshold,
                  float white_rolloff,
                  float vibrance,
                  float gain_r,
                  float gain_g,
                  float gain_b,
                  bool apply_threshold)
    {
        if(apply_threshold && threshold > 0.0f)
        {
            float thr = std::min(255.0f, threshold);
            float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
            float peak = std::max(r, std::max(g, b));
            float level = std::max(luminance, peak);
            if(level <= thr)
            {
                float t = (thr <= 0.0f) ? 1.0f : std::max(0.0f, level / thr);
                t = t * t;
                r *= t;
                g *= t;
                b *= t;
            }
        }

        float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        float max_rgb = std::max(r, std::max(g, b));
        float min_rgb = std::min(r, std::min(g, b));
        float sat = (max_rgb > 0.001f) ? ((max_rgb - min_rgb) / max_rgb) : 0.0f;
        const float wr_full = std::clamp(white_rolloff, 0.0f, kWhiteRolloffStoredMax);
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

        r *= brightness;
        g *= brightness;
        b *= brightness;

        float vib = std::max(0.0f, std::min(2.0f, vibrance));
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

        r = std::clamp(r * gain_r, 0.0f, 255.0f);
        g = std::clamp(g * gain_g, 0.0f, 255.0f);
        b = std::clamp(b * gain_b, 0.0f, 255.0f);
    }

    void SampleNearestRgba(const uint8_t* data, int w, int h, float u, float v,
                           float& r, float& g, float& b)
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        if(!data || w <= 0 || h <= 0)
        {
            return;
        }
        v = 1.0f - v;
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        const int ix = std::clamp((int)(u * (float)(w - 1) + 0.5f), 0, w - 1);
        const int iy = std::clamp((int)(v * (float)(h - 1) + 0.5f), 0, h - 1);
        const uint8_t* p = data + ((size_t)iy * (size_t)w + (size_t)ix) * 4u;
        r = (float)p[0];
        g = (float)p[1];
        b = (float)p[2];
    }

    struct SurfaceUvCache
    {
        uint64_t seq = 0;
        int ctrl = -2;
        const DisplayPlane3D* plane = nullptr;
        int u_axis = 0;
        int v_axis = 1;
    };
    thread_local SurfaceUvCache g_surface_uv;

    void ResolveSurfaceUvAxes(const DisplayPlane3D& plane,
                              float grid_scale_mm,
                              uint64_t render_sequence,
                              int& u_axis,
                              int& v_axis)
    {
        const int ctrl = SpatialLightingSceneProvider::instance()->shadingControllerIndex();
        if(g_surface_uv.seq == render_sequence &&
           g_surface_uv.ctrl == ctrl &&
           g_surface_uv.plane == &plane)
        {
            u_axis = g_surface_uv.u_axis;
            v_axis = g_surface_uv.v_axis;
            return;
        }

        u_axis = 0;
        v_axis = 1;
        const std::vector<std::unique_ptr<ControllerTransform>>* transforms =
            SpatialLightingSceneProvider::instance()->controllers();
        if(transforms && ctrl >= 0 && ctrl < (int)transforms->size())
        {
            const ControllerTransform* transform = (*transforms)[(size_t)ctrl].get();
            if(transform && transform->led_positions.size() >= 2)
            {
                std::vector<Vector3D> local_pts;
                local_pts.reserve(transform->led_positions.size());
                for(const LEDPosition3D& led : transform->led_positions)
                {
                    local_pts.push_back(Geometry3D::TransformDisplayPlaneWorldToLocal(
                        led.world_position, plane.GetTransform()));
                }
                Geometry3D::ChooseSurfaceUvAxes(local_pts.data(), (int)local_pts.size(), u_axis, v_axis);
            }
        }
        (void)grid_scale_mm;
        g_surface_uv.seq = render_sequence;
        g_surface_uv.ctrl = ctrl;
        g_surface_uv.plane = &plane;
        g_surface_uv.u_axis = u_axis;
        g_surface_uv.v_axis = v_axis;
    }

    float FalloffWeight(float dist_mm, float max_mm, float coverage, float softness, float curve, bool inverted)
    {
        coverage = std::max(coverage, 0.0f);
        if(coverage <= 0.0001f || max_mm <= 0.0f)
        {
            return 0.0f;
        }
        if(inverted)
        {
            float range = std::max(max_mm * coverage, 10.0f);
            if(dist_mm <= range * (1.0f - softness / 100.0f))
            {
                return 1.0f;
            }
            if(dist_mm >= range)
            {
                return 0.0f;
            }
            float feather = range * (softness / 100.0f);
            float core = range - feather;
            float t = std::clamp((dist_mm - core) / std::max(feather, 1e-4f), 0.0f, 1.0f);
            t = t * t * (3.0f - 2.0f * t);
            return 1.0f - t;
        }
        if(coverage >= 0.999f)
        {
            return 1.0f;
        }
        float nd = std::clamp(dist_mm / std::max(max_mm, 1.0f), 0.0f, 1.0f);
        nd = powf(nd, std::clamp(curve, 0.25f, 4.0f));
        float boundary = std::max(0.0f, 1.0f - coverage);
        if(boundary <= 0.0005f)
        {
            return 1.0f;
        }
        float feather_band = std::clamp(softness / 100.0f, 0.0f, 0.95f) * 0.5f;
        float fade_start = std::max(0.0f, boundary - feather_band);
        float t = std::clamp((nd - fade_start) / std::max(boundary - fade_start, 1e-5f), 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);
        return t;
    }

    void EnsureDefaultZones(ScreenMirror::MonitorSettings& mon)
    {
        if(mon.capture_zones.empty())
        {
            mon.capture_zones.push_back(ScreenMirror::CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        bool has_enabled = false;
        for(size_t i = 0; i < mon.capture_zones.size(); ++i)
        {
            if(mon.capture_zones[i].enabled)
            {
                has_enabled = true;
                break;
            }
        }
        if(!has_enabled && !mon.capture_zones.empty())
        {
            mon.capture_zones[0].enabled = true;
        }
    }

    void ZoneUnion(const ScreenMirror::MonitorSettings& mon,
                   float& u0, float& u1, float& v0, float& v1)
    {
        u0 = 1.0f;
        u1 = 0.0f;
        v0 = 1.0f;
        v1 = 0.0f;
        bool any = false;
        for(size_t i = 0; i < mon.capture_zones.size(); ++i)
        {
            const CaptureZone& z = mon.capture_zones[i];
            if(!z.enabled)
            {
                continue;
            }
            any = true;
            u0 = std::min(u0, z.u_min);
            u1 = std::max(u1, z.u_max);
            v0 = std::min(v0, z.v_min);
            v1 = std::max(v1, z.v_max);
        }
        if(!any)
        {
            u0 = 0.0f;
            u1 = 1.0f;
            v0 = 0.0f;
            v1 = 1.0f;
        }
    }

    std::shared_ptr<CapturedFrame> ClosestFrameAtOrBefore(
        const std::deque<std::shared_ptr<CapturedFrame>>& dq,
        uint64_t target_ms)
    {
        std::shared_ptr<CapturedFrame> best;
        uint64_t best_delta = std::numeric_limits<uint64_t>::max();
        for(size_t i = 0; i < dq.size(); ++i)
        {
            const std::shared_ptr<CapturedFrame>& fr = dq[i];
            if(!fr || !fr->valid)
            {
                continue;
            }
            if(fr->timestamp_ms > target_ms)
            {
                continue;
            }
            uint64_t delta = target_ms - fr->timestamp_ms;
            if(delta < best_delta)
            {
                best_delta = delta;
                best = fr;
            }
        }
        if(best)
        {
            return best;
        }
        return dq.empty() ? std::shared_ptr<CapturedFrame>() : dq.front();
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
    capture_mgr.SetTargetFPS(240);
    int cap_w = 320;
    int cap_h = 180;
    ScreenMirrorQualityToSize(ScreenMirrorClampQuality(capture_quality), cap_w, cap_h);
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

void ScreenMirror::PrepareGpuFields(std::uint64_t, float, const GridContext3D& grid)
{
    RefreshFrameCacheForRenderSequence(grid);
    smooth_ms_ = 0.0f;
    sample_tick_.clear();
    tick_scale_mm_ = SafeGridScaleMm(grid.grid_scale_mm);

    const float span_x = std::max(grid.max_x - grid.min_x, 1e-4f);
    const float span_y = std::max(grid.max_y - grid.min_y, 1e-4f);
    const float span_z = std::max(grid.max_z - grid.min_z, 1e-4f);
    const Vector3D grid_anchor_ref = GetEffectOriginGrid(grid);

    std::vector<DisplayPlane3D*> planes = frame_cache_planes_;
    if(planes.empty())
    {
        planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    }

    for(size_t plane_index = 0; plane_index < planes.size(); ++plane_index)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane)
        {
            continue;
        }
        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            settings_it = monitor_settings.emplace(plane_name, MonitorSettings()).first;
            settings_it->second.enabled = DefaultMonitorEnabledForPlane(plane);
        }
        MonitorSettings& mon_settings = settings_it->second;
        EnsureDefaultZones(mon_settings);

        bool monitor_enabled = mon_settings.group_box ? mon_settings.group_box->isChecked() : mon_settings.enabled;
        if(!monitor_enabled)
        {
            continue;
        }

        SampleMonitor sm;
        sm.plane = plane;
        sm.settings = &mon_settings;
        sm.calibration = mon_settings.show_calibration_pattern;
        sm.cal_rgba = nullptr;
        sm.cal_w = 0;
        sm.cal_h = 0;
        sm.wave_speed = 0.0f;
        ZoneUnion(mon_settings, sm.zone_u0, sm.zone_u1, sm.zone_v0, sm.zone_v1);

        sm.falloff_origin = grid_anchor_ref;
        Vector3D custom_ref;
        if(mon_settings.reference_point_id > 0 && ResolveReferencePointById(mon_settings.reference_point_id, custom_ref))
        {
            sm.falloff_origin = custom_ref;
            sm.falloff_origin.x += (effect_offset_x / 100.0f) * (grid.width * 0.5f);
            sm.falloff_origin.y += (effect_offset_y / 100.0f) * (grid.height * 0.5f);
            sm.falloff_origin.z += (effect_offset_z / 100.0f) * (grid.depth * 0.5f);
        }

        const float span_mm_x = span_x * tick_scale_mm_;
        const float span_mm_y = span_y * tick_scale_mm_;
        const float span_mm_z = span_z * tick_scale_mm_;
        const float fux = (sm.falloff_origin.x - grid.min_x) / span_x;
        const float fuy = (sm.falloff_origin.y - grid.min_y) / span_y;
        const float fuz = (sm.falloff_origin.z - grid.min_z) / span_z;
        sm.max_distance_mm = RoomCornerMaxDistanceMm(span_mm_x, span_mm_y, span_mm_z, fux, fuy, fuz);

        const std::string capture_id = plane->GetCaptureSourceId();
        if(sm.calibration)
        {
            sm.cal_rgba = GetCalibrationPatternBuffer(sm.cal_w, sm.cal_h);
        }
        else
        {
            if(capture_id.empty())
            {
                continue;
            }
            std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>::iterator cache_it =
                frame_cache_.find(capture_id);
            if(cache_it != frame_cache_.end())
            {
                sm.latest = cache_it->second;
            }
            sm.wave_speed = ResolveWaveSpeedMmPerMs(mon_settings.wave_time_to_edge_sec,
                                                     mon_settings.propagation_speed_mm_per_ms,
                                                     sm.max_distance_mm);
        }

        smooth_ms_ = std::max(smooth_ms_, mon_settings.smoothing_time_ms);
        sample_tick_.push_back(sm);
    }
}

RGBColor ScreenMirror::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)time;
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return ToRGBColor(0, 0, 0);
    }
    if(sample_tick_.empty())
    {
        return ToRGBColor(0, 0, 0);
    }

    const Vector3D led{x, y, z};
    float acc_r = 0.0f;
    float acc_g = 0.0f;
    float acc_b = 0.0f;
    float acc_w = 0.0f;

    for(size_t i = 0; i < sample_tick_.size(); ++i)
    {
        SampleMonitor& sm = sample_tick_[i];
        if(!sm.plane || !sm.settings)
        {
            continue;
        }
        const MonitorSettings& s = *sm.settings;
        int u_axis = 0;
        int v_axis = 1;
        ResolveSurfaceUvAxes(*sm.plane, tick_scale_mm_, grid.render_sequence, u_axis, v_axis);
        Geometry3D::PlaneProjection proj = Geometry3D::SpatialMapToScreen(led, *sm.plane, tick_scale_mm_,
                                                                          u_axis, v_axis);
        if(!proj.is_valid)
        {
            continue;
        }
        float u = proj.u;
        float v = proj.v;
        Geometry3D::ApplyUVRotationDegrees01(u, v, s.screen_map_roll_deg);
        Geometry3D::ApplyRadialCornerMapping01(u, v,
                                            RadialMapUiToInternal(s.radial_corner_expansion_ui),
                                            RadialMapUiToInternal(s.radial_corner_bias_tl_ui),
                                            RadialMapUiToInternal(s.radial_corner_bias_tr_ui),
                                            RadialMapUiToInternal(s.radial_corner_bias_bl_ui),
                                            RadialMapUiToInternal(s.radial_corner_bias_br_ui));
        if(u < sm.zone_u0 || u > sm.zone_u1 || v < sm.zone_v0 || v > sm.zone_v1)
        {
            continue;
        }

        const float lp = std::clamp(s.black_bar_letterbox_percent, 0.0f, 49.0f) / 100.0f;
        const float pp = std::clamp(s.black_bar_pillarbox_percent, 0.0f, 49.0f) / 100.0f;
        u = std::clamp(u, pp, 1.0f - pp);
        v = std::clamp(v, lp, 1.0f - lp);

        Vector3D d{
            led.x - sm.falloff_origin.x,
            led.y - sm.falloff_origin.y,
            led.z - sm.falloff_origin.z
        };
        float dist_mm = GridUnitsToMM(std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), tick_scale_mm_);
        float weight = FalloffWeight(dist_mm, sm.max_distance_mm, s.scale, s.edge_softness,
                                      s.falloff_curve_exponent, s.scale_inverted);
        if(weight <= 0.01f)
        {
            continue;
        }

        const float fb = s.front_back_balance;
        const float lr = s.left_right_balance;
        const float tb = s.top_bottom_balance;
        if(std::fabs(fb) > 0.5f || std::fabs(lr) > 0.5f || std::fabs(tb) > 0.5f)
        {
            const Vector3D local = Geometry3D::TransformDisplayPlaneWorldToLocal(led, sm.plane->GetTransform());
            const float inv_max = 1.0f / std::max(sm.max_distance_mm, 1.0f);
            const float lat = local.x * tick_scale_mm_;
            const float vert = local.y * tick_scale_mm_;
            const float depth = local.z * tick_scale_mm_;
            weight *= std::clamp(1.0f + (fb / 100.0f) * std::clamp(depth * inv_max, -1.0f, 1.0f), 0.0f, 2.0f);
            weight *= std::clamp(1.0f + (lr / 100.0f) * std::clamp(lat * inv_max, -1.0f, 1.0f), 0.0f, 2.0f);
            weight *= std::clamp(1.0f + (tb / 100.0f) * std::clamp(vert * inv_max, -1.0f, 1.0f), 0.0f, 2.0f);
        }
        if(weight <= 0.01f)
        {
            continue;
        }

        const uint8_t* rgba = nullptr;
        int fw = 0;
        int fh = 0;
        if(sm.calibration && sm.cal_rgba)
        {
            rgba = sm.cal_rgba;
            fw = sm.cal_w;
            fh = sm.cal_h;
        }
        else
        {
            std::shared_ptr<CapturedFrame> fr = sm.latest;
            if(sm.wave_speed >= 0.1f)
            {
                const std::string capture_id = sm.plane->GetCaptureSourceId();
                std::unordered_map<std::string, FrameHistory>::iterator hist_it = capture_history.find(capture_id);
                if(hist_it != capture_history.end() && !hist_it->second.frames.empty())
                {
                    const float delay_ms = dist_mm / std::max(sm.wave_speed, 0.1f);
                    uint64_t newest = hist_it->second.frames.back()->timestamp_ms;
                    uint64_t target = newest;
                    if(delay_ms > 0.0f && newest > (uint64_t)delay_ms)
                    {
                        target = newest - (uint64_t)delay_ms;
                    }
                    fr = ClosestFrameAtOrBefore(hist_it->second.frames, target);
                }
            }
            if(fr && fr->valid && !fr->data.empty())
            {
                rgba = fr->data.data();
                fw = fr->width;
                fh = fr->height;
            }
        }
        if(!rgba || fw <= 0 || fh <= 0)
        {
            continue;
        }

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        SampleNearestRgba(rgba, fw, fh, u, v, r, g, b);
        GradeRgb(r, g, b,
                 s.brightness_multiplier,
                 s.brightness_threshold,
                 s.white_rolloff,
                 s.vibrance,
                 s.led_output_gain_r,
                 s.led_output_gain_g,
                 s.led_output_gain_b,
                 !sm.calibration);
        acc_r += weight * r;
        acc_g += weight * g;
        acc_b += weight * b;
        acc_w += weight;
    }

    if(acc_w <= 0.01f)
    {
        return ToRGBColor(0, 0, 0);
    }
    float total_r = acc_r / acc_w;
    float total_g = acc_g / acc_w;
    float total_b = acc_b / acc_w;

    if(smooth_ms_ > 0.1f)
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
            float tau = smooth_ms_;
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
        led_states.erase(MakeLEDKey(x, y, z));
    }

    return ToRGBColor((uint8_t)std::clamp((int)std::lround(total_r), 0, 255),
                      (uint8_t)std::clamp((int)std::lround(total_g), 0, 255),
                      (uint8_t)std::clamp((int)std::lround(total_b), 0, 255));
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
        float speed = ResolveWaveSpeedMmPerMs(mon_settings.wave_time_to_edge_sec,
                                             mon_settings.propagation_speed_mm_per_ms,
                                             5000.0f);
        if(speed >= 0.1f)
        {
            monitor_retention = std::max(monitor_retention,
                                         WaveHistorySpanMs(speed, 5000.0f, mon_settings.wave_decay_ms));
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
