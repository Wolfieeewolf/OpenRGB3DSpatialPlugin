// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "ScreenCaptureDownscale.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "PluginLog.h"
#include "ScreenMirror/ScreenMirrorCalibrationPattern.h"
#include "ScreenMirror/ScreenMirror_Internal.h"
#include "SpatialVolumeFieldEngine.h"

#include <QImage>
#include <QByteArray>
#include <QVector3D>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
    constexpr int kGpuMaxMonitors = 2;
    constexpr int kGpuMaxHistory = kScreenMirrorGpuMaxHistory;
    constexpr int kGpuMonParamCount = 24;
    constexpr int kGpuSharedCount = 6;

    float Pack01(float a, float b)
    {
        a = std::clamp(a, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        const float ai = std::floor(a * 4095.0f + 0.5f);
        const float bi = std::floor(b * 4095.0f + 0.5f);
        return ai * 4096.0f + bi;
    }

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

    QImage ImageFromRgba(const uint8_t* data, int w, int h)
    {
        if(!data || w <= 0 || h <= 0)
        {
            return QImage();
        }
        QImage img(data, w, h, w * 4, QImage::Format_RGBA8888);
        return img.copy();
    }

    QImage GradeImage(const QImage& src,
                      const ScreenMirror::MonitorSettings& mon,
                      bool apply_threshold)
    {
        QImage img = src.convertToFormat(QImage::Format_RGBA8888);
        for(int y = 0; y < img.height(); ++y)
        {
            unsigned char* row = img.scanLine(y);
            for(int x = 0; x < img.width(); ++x)
            {
                unsigned char* px = row + (size_t)x * 4u;
                float r = (float)px[0];
                float g = (float)px[1];
                float b = (float)px[2];
                GradeRgb(r, g, b,
                         mon.brightness_multiplier,
                         mon.brightness_threshold,
                         mon.white_rolloff,
                         mon.vibrance,
                         mon.led_output_gain_r,
                         mon.led_output_gain_g,
                         mon.led_output_gain_b,
                         apply_threshold);
                px[0] = (unsigned char)std::clamp((int)std::lround(r), 0, 255);
                px[1] = (unsigned char)std::clamp((int)std::lround(g), 0, 255);
                px[2] = (unsigned char)std::clamp((int)std::lround(b), 0, 255);
                px[3] = 255;
            }
        }
        return img;
    }

    struct GpuMonitor
    {
        ScreenMirror::MonitorSettings* settings = nullptr;
        Vector3D map_uv{};
        Vector3D falloff_uv{};
        Vector3D right{};
        Vector3D up{};
        bool flip_v = true;
        bool calibration = false;
        float wave_speed = 0.0f;
        float wave_span_ms = 0.0f;
        float max_distance_mm = 1.0f;
        float zone_u0 = 0.0f;
        float zone_u1 = 1.0f;
        float zone_v0 = 0.0f;
        float zone_v1 = 1.0f;
        QImage calibration_image;
        std::vector<std::shared_ptr<CapturedFrame>> live_newest_first;
    };

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

    void FillHistoryByAge(const std::deque<std::shared_ptr<CapturedFrame>>& dq,
                          float span_ms,
                          int nhist,
                          std::vector<std::shared_ptr<CapturedFrame>>& out)
    {
        out.clear();
        if(dq.empty() || nhist <= 0)
        {
            return;
        }
        uint64_t newest = dq.back()->timestamp_ms;
        const int rows = std::max(1, nhist);
        for(int h = 0; h < rows; ++h)
        {
            float age = (rows == 1) ? 0.0f : ((float)h / (float)(rows - 1)) * std::max(span_ms, 0.0f);
            uint64_t target = newest;
            if(age > 0.0f && newest > (uint64_t)age)
            {
                target = newest - (uint64_t)age;
            }
            else if(age > 0.0f)
            {
                target = dq.front()->timestamp_ms;
            }
            std::shared_ptr<CapturedFrame> fr = ClosestFrameAtOrBefore(dq, target);
            if(fr && fr->valid && !fr->data.empty())
            {
                out.push_back(fr);
            }
        }
    }

    Vector3D RoomUvUnclamped(const Vector3D& world, const GridContext3D& grid,
                             float span_x, float span_y, float span_z)
    {
        Vector3D uv;
        uv.x = (world.x - grid.min_x) / span_x;
        uv.y = (world.y - grid.min_y) / span_y;
        uv.z = (world.z - grid.min_z) / span_z;
        return uv;
    }

    void FillMonitorParams(float* dst, const GpuMonitor& mon)
    {
        dst[0] = mon.map_uv.x;
        dst[1] = mon.map_uv.y;
        dst[2] = mon.map_uv.z;
        dst[3] = mon.right.x;
        dst[4] = mon.right.y;
        dst[5] = mon.right.z;
        dst[6] = mon.up.x;
        dst[7] = mon.up.y;
        dst[8] = mon.up.z;
        dst[9] = mon.falloff_uv.x;
        dst[10] = mon.falloff_uv.y;
        dst[11] = mon.falloff_uv.z;
        const ScreenMirror::MonitorSettings& s = *mon.settings;
        dst[12] = Pack01(std::clamp(s.scale, 0.0f, 3.0f) / 3.0f, s.scale_inverted ? 1.0f : 0.0f);
        dst[13] = Pack01(std::clamp(s.edge_softness, 0.0f, 100.0f) / 100.0f,
                         std::clamp((std::clamp(s.falloff_curve_exponent, 0.25f, 4.0f) - 0.25f) / 3.75f, 0.0f, 1.0f));
        dst[14] = Pack01((std::clamp(s.screen_map_roll_deg, -180.0f, 180.0f) + 180.0f) / 360.0f,
                         (RadialMapUiToInternal(s.radial_corner_expansion_ui) + 50.0f) / 100.0f);
        dst[15] = Pack01((RadialMapUiToInternal(s.radial_corner_bias_tl_ui) + 50.0f) / 100.0f,
                         (RadialMapUiToInternal(s.radial_corner_bias_tr_ui) + 50.0f) / 100.0f);
        dst[16] = Pack01((RadialMapUiToInternal(s.radial_corner_bias_bl_ui) + 50.0f) / 100.0f,
                         (RadialMapUiToInternal(s.radial_corner_bias_br_ui) + 50.0f) / 100.0f);
        dst[17] = Pack01(std::clamp(s.black_bar_letterbox_percent, 0.0f, 49.0f) / 49.0f,
                         std::clamp(s.black_bar_pillarbox_percent, 0.0f, 49.0f) / 49.0f);
        dst[18] = Pack01(std::clamp(s.corner_blend_strength_pct / 100.0f, 0.0f, 1.0f),
                         std::clamp(s.corner_blend_zone_pct / 100.0f, 0.0f, 0.32f) / 0.32f);
        dst[19] = Pack01(std::clamp(mon.zone_u0, 0.0f, 1.0f), std::clamp(mon.zone_u1, 0.0f, 1.0f));
        dst[20] = Pack01(std::clamp(mon.zone_v0, 0.0f, 1.0f), std::clamp(mon.zone_v1, 0.0f, 1.0f));
        dst[21] = Pack01(PackWaveSpeed01(mon.wave_speed),
                         std::clamp(s.wave_decay_ms / 10000.0f, 0.0f, 1.0f));
        dst[22] = Pack01((std::clamp(s.front_back_balance, -100.0f, 100.0f) + 100.0f) / 200.0f,
                         (std::clamp(s.left_right_balance, -100.0f, 100.0f) + 100.0f) / 200.0f);
        dst[23] = Pack01((std::clamp(s.top_bottom_balance, -100.0f, 100.0f) + 100.0f) / 200.0f,
                         std::clamp(s.blend / 100.0f, 0.0f, 1.0f));
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

void ScreenMirror::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    RefreshFrameCacheForRenderSequence(grid);
    gpu_smooth_ms_ = 0.0f;

    const float scale_mm = SafeGridScaleMm(grid.grid_scale_mm);
    const float span_x = std::max(grid.max_x - grid.min_x, 1e-4f);
    const float span_y = std::max(grid.max_y - grid.min_y, 1e-4f);
    const float span_z = std::max(grid.max_z - grid.min_z, 1e-4f);
    const Vector3D grid_anchor_ref = GetEffectOriginGrid(grid);

    std::vector<DisplayPlane3D*> planes = frame_cache_planes_;
    if(planes.empty())
    {
        planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    }

    std::vector<GpuMonitor> gpus;
    gpus.reserve(kGpuMaxMonitors);

    for(size_t plane_index = 0; plane_index < planes.size() && (int)gpus.size() < kGpuMaxMonitors; ++plane_index)
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

        const std::string capture_id = plane->GetCaptureSourceId();

        GpuMonitor gm;
        gm.settings = &mon_settings;
        gm.calibration = mon_settings.show_calibration_pattern;
        gm.flip_v = !gm.calibration;
        ZoneUnion(mon_settings, gm.zone_u0, gm.zone_u1, gm.zone_v0, gm.zone_v1);

        gm.map_uv = RoomUvUnclamped(plane->GetTransform().position, grid, span_x, span_y, span_z);

        Vector3D falloff_ref = grid_anchor_ref;
        Vector3D custom_ref;
        if(mon_settings.reference_point_id > 0 && ResolveReferencePointById(mon_settings.reference_point_id, custom_ref))
        {
            falloff_ref = custom_ref;
            falloff_ref.x += (effect_offset_x / 100.0f) * (grid.width * 0.5f);
            falloff_ref.y += (effect_offset_y / 100.0f) * (grid.height * 0.5f);
            falloff_ref.z += (effect_offset_z / 100.0f) * (grid.depth * 0.5f);
            gm.map_uv = RoomUvUnclamped(falloff_ref, grid, span_x, span_y, span_z);
        }
        gm.falloff_uv = RoomUvUnclamped(falloff_ref, grid, span_x, span_y, span_z);

        float rot[9];
        Geometry3D::ComputeRotationMatrix(plane->GetTransform().rotation, rot);
        gm.right = {rot[0], rot[3], rot[6]};
        gm.up = {rot[1], rot[4], rot[7]};

        const float span_mm_x = span_x * scale_mm;
        const float span_mm_y = span_y * scale_mm;
        const float span_mm_z = span_z * scale_mm;
        gm.max_distance_mm = RoomCornerMaxDistanceMm(span_mm_x, span_mm_y, span_mm_z,
                                                     gm.falloff_uv.x, gm.falloff_uv.y, gm.falloff_uv.z);

        bool use_wave = !gm.calibration && !capture_id.empty();
        if(use_wave)
        {
            gm.wave_speed = ResolveWaveSpeedMmPerMs(mon_settings.wave_time_to_edge_sec,
                                                    mon_settings.propagation_speed_mm_per_ms,
                                                    gm.max_distance_mm);
            gm.wave_span_ms = WaveHistorySpanMs(gm.wave_speed,
                                                gm.max_distance_mm,
                                                mon_settings.wave_decay_ms);
        }

        if(gm.calibration)
        {
            int cw = 0, ch = 0;
            const uint8_t* cal = GetCalibrationPatternBuffer(cw, ch);
            QImage cal_img = ImageFromRgba(cal, cw, ch);
            if(!cal_img.isNull())
            {
                gm.calibration_image = std::move(cal_img);
            }
        }
        else
        {
            if(capture_id.empty())
            {
                continue;
            }
            std::vector<std::shared_ptr<CapturedFrame>> frames;
            std::unordered_map<std::string, FrameHistory>::iterator hist_it = capture_history.find(capture_id);
            if(hist_it != capture_history.end() && !hist_it->second.frames.empty())
            {
                const int rows = (gm.wave_speed >= 0.1f) ? kGpuMaxHistory : 1;
                FillHistoryByAge(hist_it->second.frames, gm.wave_span_ms, rows, frames);
            }
            else
            {
                std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>::iterator cache_it =
                    frame_cache_.find(capture_id);
                if(cache_it != frame_cache_.end())
                {
                    frames.push_back(cache_it->second);
                }
            }
            for(size_t fi = 0; fi < frames.size(); ++fi)
            {
                const std::shared_ptr<CapturedFrame>& fr = frames[fi];
                if(!fr || !fr->valid || fr->data.empty())
                {
                    continue;
                }
                gm.live_newest_first.push_back(fr);
            }
        }

        if((gm.calibration && gm.calibration_image.isNull()) ||
           (!gm.calibration && gm.live_newest_first.empty()))
        {
            continue;
        }

        gpu_smooth_ms_ = std::max(gpu_smooth_ms_, mon_settings.smoothing_time_ms);
        gpus.push_back(std::move(gm));
    }

    if(gpus.empty())
    {
        volume_assist_.clearMediaTexture();
        float zp[kGpuSharedCount] = {};
        volume_assist_.prepare(render_sequence, time_sec, zp, kGpuSharedCount);
        return;
    }

    int nmon = (int)gpus.size();
    int nhist = 1;
    for(size_t i = 0; i < gpus.size(); ++i)
    {
        if(gpus[i].wave_speed >= 0.1f)
        {
            nhist = std::max(nhist, std::max(1, (int)gpus[i].live_newest_first.size()));
        }
    }
    nhist = std::clamp(nhist, 1, kGpuMaxHistory);

    const int tile_w = std::max(1, SpatialVolumeFieldEngine::kMaxMediaEdge / nmon);
    const int tile_h = std::max(1, SpatialVolumeFieldEngine::kMaxMediaEdge / nhist);
    QImage media(tile_w * nmon, tile_h * nhist, QImage::Format_RGBA8888);
    media.fill(qRgba(0, 0, 0, 255));
    for(int m = 0; m < nmon; ++m)
    {
        GpuMonitor& gm = gpus[(size_t)m];
        for(int h = 0; h < nhist; ++h)
        {
            QImage src;
            if(gm.calibration)
            {
                src = gm.calibration_image;
            }
            else if(!gm.live_newest_first.empty())
            {
                const int idx = std::min(h, (int)gm.live_newest_first.size() - 1);
                const std::shared_ptr<CapturedFrame>& fr = gm.live_newest_first[(size_t)idx];
                if(fr && fr->valid && !fr->data.empty())
                {
                    src = QImage(fr->data.data(), fr->width, fr->height, fr->width * 4, QImage::Format_RGBA8888);
                }
            }
            if(src.isNull())
            {
                continue;
            }
            QImage scaled(tile_w, tile_h, QImage::Format_RGBA8888);
            if(scaled.isNull())
            {
                continue;
            }
            QImage rgba = src.convertToFormat(QImage::Format_RGBA8888);
            uint8_t* dest_bits = scaled.bits();
            if(scaled.bytesPerLine() == tile_w * 4)
            {
                BoxDownscaleToRgba(rgba.constBits(),
                                   rgba.width(),
                                   rgba.height(),
                                   rgba.bytesPerLine(),
                                   dest_bits,
                                   tile_w,
                                   tile_h,
                                   0,
                                   1,
                                   2,
                                   3);
            }
            else
            {
                std::vector<uint8_t> packed((size_t)tile_w * (size_t)tile_h * 4u);
                BoxDownscaleToRgba(rgba.constBits(),
                                   rgba.width(),
                                   rgba.height(),
                                   rgba.bytesPerLine(),
                                   packed.data(),
                                   tile_w,
                                   tile_h,
                                   0,
                                   1,
                                   2,
                                   3);
                scaled = QImage(packed.data(), tile_w, tile_h, tile_w * 4, QImage::Format_RGBA8888).copy();
            }
            scaled = GradeImage(scaled, *gm.settings, !gm.calibration);
            const int ox = m * tile_w;
            const int oy = h * tile_h;
            for(int y = 0; y < tile_h; ++y)
            {
                const unsigned char* srow = scaled.constScanLine(y);
                unsigned char* drow = media.scanLine(oy + y);
                memcpy(drow + (size_t)ox * 4u, srow, (size_t)tile_w * 4u);
            }
        }
    }
    volume_assist_.setMediaTexture(media, false);

    float avg_frame_ms = 16.67f;
    for(size_t plane_index = 0; plane_index < planes.size(); ++plane_index)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane)
        {
            continue;
        }
        std::unordered_map<std::string, FrameHistory>::iterator hist_it =
            capture_history.find(plane->GetCaptureSourceId());
        if(hist_it != capture_history.end() && hist_it->second.cached_avg_frame_time_ms > 0.0f)
        {
            avg_frame_ms = hist_it->second.cached_avg_frame_time_ms;
            break;
        }
    }

    const unsigned int samp = GetSamplingResolution();
    const bool use_q = samp < 100u;
    float steps_u = 2.0f;
    float steps_v = 2.0f;
    int src_w = tile_w;
    int src_h = tile_h;
    if(gpus[0].calibration && !gpus[0].calibration_image.isNull())
    {
        src_w = gpus[0].calibration_image.width();
        src_h = gpus[0].calibration_image.height();
    }
    else if(!gpus[0].live_newest_first.empty() && gpus[0].live_newest_first[0])
    {
        src_w = gpus[0].live_newest_first[0]->width;
        src_h = gpus[0].live_newest_first[0]->height;
    }
    {
        const float q = samp / 100.0f;
        steps_u = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, src_w) - 4));
        steps_v = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, src_h) - 4));
    }

    float layout = (float)nmon + 10.0f * (float)nhist + (use_q ? 100.0f : 0.0f);
    if(gpus[0].flip_v)
    {
        layout += 1000.0f;
    }
    if(nmon > 1 && gpus[1].flip_v)
    {
        layout += 2000.0f;
    }

    float vp[SpatialVolumeFieldEngine::kMaxParams] = {};
    vp[0] = span_x * scale_mm;
    vp[1] = span_y * scale_mm;
    vp[2] = span_z * scale_mm;
    vp[3] = layout;
    vp[4] = Pack01(std::clamp(steps_u / 2048.0f, 0.0f, 1.0f), std::clamp(steps_v / 2048.0f, 0.0f, 1.0f));
    vp[5] = avg_frame_ms;
    FillMonitorParams(vp + kGpuSharedCount, gpus[0]);
    if(nmon > 1)
    {
        FillMonitorParams(vp + kGpuSharedCount + kGpuMonParamCount, gpus[1]);
    }
    volume_assist_.prepare(render_sequence, time_sec, vp, SpatialVolumeFieldEngine::kMaxParams);
}

RGBColor ScreenMirror::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)time;
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return ToRGBColor(0, 0, 0);
    }
    if(!volume_assist_.isAvailable())
    {
        if(!gpu_logged_unavail_)
        {
            gpu_logged_unavail_ = true;
            const QByteArray err = volume_assist_.lastError().toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] ScreenMirror volume assist unavailable: %s",
                        err.isEmpty() ? "ensureReady failed" : err.constData());
        }
        return ToRGBColor(0, 0, 0);
    }

    float c1 = 0.0f, c2 = 0.0f, c3 = 0.0f;
    if(!TrySampleGpuRoomVolume01(x, y, z, grid, &c1, &c2, &c3))
    {
        return ToRGBColor(0, 0, 0);
    }
    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
    float total_r = std::clamp(samp.x(), 0.0f, 1.0f) * 255.0f;
    float total_g = std::clamp(samp.y(), 0.0f, 1.0f) * 255.0f;
    float total_b = std::clamp(samp.z(), 0.0f, 1.0f) * 255.0f;

    if(gpu_smooth_ms_ > 0.1f)
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
            float tau = gpu_smooth_ms_;
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

    if(history.frames.size() >= 2)
    {
        size_t check_frames = std::min(history.frames.size() - 1, (size_t)10);
        uint64_t total_time = 0;
        size_t valid_pairs = 0;
        const uint64_t min_delta_ms = 4;
        const uint64_t max_delta_ms = 80;
        for(size_t i = history.frames.size() - check_frames; i < history.frames.size(); ++i)
        {
            if(i == 0)
            {
                continue;
            }
            uint64_t delta = (history.frames[i]->timestamp_ms > history.frames[i - 1]->timestamp_ms)
                ? (history.frames[i]->timestamp_ms - history.frames[i - 1]->timestamp_ms)
                : 0;
            if(delta >= min_delta_ms && delta <= max_delta_ms)
            {
                total_time += delta;
                valid_pairs++;
            }
        }
        if(valid_pairs > 0 && total_time > 0)
        {
            float measured_ms = std::clamp((float)total_time / (float)valid_pairs, 4.0f, 50.0f);
            if(history.cached_avg_frame_time_ms > 0.0f)
            {
                history.cached_avg_frame_time_ms = 0.75f * history.cached_avg_frame_time_ms + 0.25f * measured_ms;
            }
            else
            {
                history.cached_avg_frame_time_ms = measured_ms;
            }
        }
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
