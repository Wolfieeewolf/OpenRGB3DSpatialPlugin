// SPDX-License-Identifier: GPL-2.0-only

#include "FolderVolumeEffect.h"
#include "EffectStratumBlend.h"
#include "MediaTextureEffectUtils.h"
#include "SpatialShaderCatalog.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "EffectColorUtils.h"
#include "AudioReactiveCommon.h"
#include "AudioReactiveUi.h"
#include "AudioInputManager.h"

#include <QDateTime>
#include <QImage>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>


FolderVolumeEffect::FolderVolumeEffect(FolderVolumeSpec spec, QWidget* parent)
    : SpatialEffect3D(parent)
    , spec_(std::move(spec))
{
    pattern_index = spec_.pattern_index_default;
    combo_values.resize(spec_.combos.size());
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        combo_values[i] = spec_.combos[i].value;
    }
    slider_values.resize(spec_.sliders.size());
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        slider_values[i] = spec_.sliders[i].value;
    }
    if(GetColors().empty() && !spec_.colors.empty())
    {
        SetColors(spec_.colors);
    }
    SetRainbowMode(spec_.rainbow);
    if(spec_.effect_instances > 0)
    {
        effect_instance_count = spec_.effect_instances;
    }
    if(spec_.uses_media)
    {
        gif_frame_timer = new QTimer(this);
        gif_frame_timer->setTimerType(Qt::PreciseTimer);
        connect(gif_frame_timer, &QTimer::timeout, this, &FolderVolumeEffect::OnGifFrameTimerTimeout);
    }
    if(spec_.uses_audio)
    {
        const std::string& preset = spec_.audio_preset;
        if(preset == "level")
            audio_settings = MakeDefaultLevelAudioReactiveSettings3D();
        else if(preset == "beat")
            audio_settings = MakeDefaultBeatAudioReactiveSettings3D();
        else if(preset == "low_punch")
            audio_settings = MakeDefaultLowPunchAudioReactiveSettings3D();
        else if(preset == "high_sparkle")
            audio_settings = MakeDefaultHighSparkleAudioReactiveSettings3D();
        else // spectrum / paint / strip / default
            audio_settings = MakeDefaultSpectrumAudioReactiveSettings3D();
    }
    const QString shader_id = spec_.shader_override.empty()
        ? QString::fromStdString(spec_.shader_id)
        : QString::fromStdString(spec_.shader_override);
    volume_assist_.setFragmentBody(SpatialShaderCatalog::LoadEffectShader(shader_id));
    if(spec_.resolution > 0)
    {
        volume_assist_.setResolution(spec_.resolution);
    }
}

FolderVolumeEffect::~FolderVolumeEffect()
{
    ClearMovie();
}

EffectInfo3D FolderVolumeEffect::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = spec_.ui_name.c_str();
    info.effect_description = spec_.description.c_str();
    info.category = spec_.category.c_str();
    info.is_reversible = !(spec_.uses_media || spec_.uses_audio);
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = (unsigned int)std::max(0, spec_.user_colors);
    info.has_custom_settings = !spec_.patterns.empty() || !spec_.sliders.empty()
        || !spec_.combos.empty() || spec_.uses_media || spec_.uses_audio;
    info.needs_3d_origin = spec_.uses_media;
    info.effect_type = SPATIAL_EFFECT_FOLDER_VOLUME;
    info.needs_frequency = spec_.needs_frequency;
    info.use_size_parameter = spec_.show_size;
    info.show_speed_control = spec_.show_speed;
    info.show_brightness_control = spec_.show_brightness;
    info.show_frequency_control = spec_.show_frequency;
    info.show_detail_control = spec_.show_detail;
    info.show_size_control = spec_.show_size;
    info.show_scale_control = spec_.show_scale;
    info.show_fps_control = spec_.show_fps;
    info.show_color_controls = spec_.show_color;
    info.show_surface_control = spec_.show_surface;
    info.show_position_offset_control = spec_.show_position;
    info.show_path_axis_control = spec_.show_path;
    info.show_thickness_control = spec_.show_thickness;
    info.show_edge_fade_control = spec_.show_edge;
    info.show_count_control = spec_.show_count;
    info.show_plane_control = spec_.show_plane;
    info.supports_strip_colormap = spec_.strip_colormap;
    info.supports_height_bands = spec_.height_bands;
    return info;
}

void FolderVolumeEffect::SubmitVolumeGpu(std::uint64_t render_sequence,
                                         float time_sec,
                                         const float* params,
                                         int count)
{
    volume_assist_.prepareProgram(render_sequence, time_sec, params, count);
}

std::uint32_t FolderVolumeEffect::PackNorm01Key(float n1, float n2, float n3)
{
    const auto q = [](float v) -> std::uint32_t {
        return (std::uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 1023.0f) & 1023u;
    };
    return (q(n1) << 20) | (q(n2) << 10) | q(n3);
}

QVector3D FolderVolumeEffect::SampleField01(float n1, float n2, float n3)
{
    const std::uint32_t key = PackNorm01Key(n1, n2, n3);
    const auto it = true_sample_map_.find(key);
    if(it != true_sample_map_.end())
    {
        return it->second;
    }
    return volume_assist_.sampleTrue01(n1, n2, n3);
}

void FolderVolumeEffect::BakeTrueGpuVolumeSamples(const float* sample_xyz,
                                                  int count,
                                                  float /*time*/,
                                                  const GridContext3D& grid)
{
    true_sample_map_.clear();
    if(!sample_xyz || count <= 0 || !volume_assist_.isAvailable())
    {
        return;
    }

    std::unordered_map<std::uint32_t, int> unique_index;
    unique_index.reserve((size_t)std::max(16, count / 2));
    std::vector<float> n01;
    n01.reserve((size_t)count * 3u);
    for(int i = 0; i < count; ++i)
    {
        const float x = sample_xyz[(size_t)i * 3u + 0u];
        const float y = sample_xyz[(size_t)i * 3u + 1u];
        const float z = sample_xyz[(size_t)i * 3u + 2u];
        float n1 = 0.5f, n2 = 0.5f, n3 = 0.5f;
        bool ok = false;
        if(spec_.sample_room)
        {
            if(!EffectGridSampleOutsideVolume(x, y, z, grid))
            {
                SampleGpuRoomVolume01(x, y, z, grid, &n1, &n2, &n3);
                ok = true;
            }
        }
        else
        {
            ok = SampleVolumeLocalUv01(x, y, z, grid, &n1, &n2, &n3);
        }
        if(!ok)
        {
            continue;
        }
        const std::uint32_t key = PackNorm01Key(n1, n2, n3);
        if(unique_index.find(key) != unique_index.end())
        {
            continue;
        }
        const int idx = (int)unique_index.size();
        unique_index.emplace(key, idx);
        n01.push_back(n1);
        n01.push_back(n2);
        n01.push_back(n3);
    }
    const int unique = (int)unique_index.size();
    if(unique <= 0)
    {
        return;
    }

    std::vector<float> rgb((size_t)unique * 3u, 0.0f);
    if(!volume_assist_.evaluateAtPoints(n01.data(), unique, rgb.data()))
    {
        return;
    }
    true_sample_map_.reserve((size_t)unique);
    for(const auto& kv : unique_index)
    {
        const int i = kv.second;
        true_sample_map_[kv.first] = QVector3D(
            rgb[(size_t)i * 3u + 0u],
            rgb[(size_t)i * 3u + 1u],
            rgb[(size_t)i * 3u + 2u]);
    }
}

void FolderVolumeEffect::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    this->time_sec = time_sec;
    true_sample_map_.clear();
    true_sample_seq_ = render_sequence;
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    speed_mul = bb.speed_mul;
    tight_mul = bb.tight_mul;
    phase_shift = EffectStratumBlend::PhaseShift01(bb);
    progress = CalculateProgress(time_sec) * speed_mul;

    const Vector3D origin = GetEffectOriginGrid(grid);
    /* Same half-extents / local frame as SampleVolumeLocalUv01 + the gizmo OBB. */
    float hx = 1.0f, hy = 1.0f, hz = 1.0f;
    GetVolumeHalfExtents(grid, hx, hy, hz);
    const EffectGridAxisHalfExtents he{hx, hy, hz};
    float med = EffectGridMedianOfHalfExtents(he);
    if(med < 1e-4f)
        med = 1.0f;
    atlas_sx = std::max(0.25f, 2.0f * he.hw / med);
    atlas_sy = std::max(0.25f, 2.0f * he.hh / med);
    atlas_sz = std::max(0.25f, 2.0f * he.hd / med);
    float aspect_med = std::max(he.hw, he.hd);
    if(aspect_med < 1e-4f)
        aspect_med = 1.0f;
    atlas_ax = std::clamp(he.hw / aspect_med, 0.15f, 1.0f);
    atlas_az = std::clamp(he.hd / aspect_med, 0.15f, 1.0f);

    auto world_to_occ = [&](float x, float y, float z) {
        const Vector3D local = VolumeWorldDeltaToLocal(x - origin.x, y - origin.y, z - origin.z);
        const float hw = std::max(he.hw, 1e-5f);
        const float hh = std::max(he.hh, 1e-5f);
        const float hd = std::max(he.hd, 1e-5f);
        return Vector3D{
            0.5f + 0.5f * local.x / hw,
            0.5f + 0.5f * local.y / hh,
            0.5f + 0.5f * local.z / hd
        };
    };
    const Vector3D pmin = world_to_occ(grid.min_x, grid.min_y, grid.min_z);
    const Vector3D pmax = world_to_occ(grid.max_x, grid.max_y, grid.max_z);
    room_ymin = pmin.y;
    room_ymax = pmax.y;
    room_xmin = pmin.x;
    room_xmax = pmax.x;
    room_zmin = pmin.z;
    room_zmax = pmax.z;

    const float detail = std::max(0.05f, GetScaledDetail()) * bb.tight_mul;
    const float size = GetNormalizedSize();

    if(spec_.uses_media)
    {
        std::shared_ptr<QImage> snap;
        std::shared_ptr<QImage> prev_snap;
        qint64 step_ms = 0;
        int step_interval_ms = 0;
        {
            QMutexLocker lock(&display_mutex);
            snap = display_frame;
            prev_snap = previous_display_frame;
            step_ms = last_gif_step_ms;
            step_interval_ms = gif_step_interval_ms;
        }
        if(!snap || snap->isNull())
        {
            volume_assist_.clearMediaTexture();
            float zp[13] = {};
            SubmitVolumeGpu(render_sequence, time_sec, zp, 13);
            return;
        }

        QImage media = *snap;
        constexpr int kGpuMediaEdge = SpatialVolumeFieldEngine::kMaxMediaEdge;
        if(media.width() > kGpuMediaEdge || media.height() > kGpuMediaEdge)
        {
            media = media.scaled(kGpuMediaEdge, kGpuMediaEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        const bool is_omni = (spec_.media_layout == "omni");
        const unsigned int eff_res = CombineMediaSampling((unsigned int)std::clamp(SliderRaw("media_resolution"), 0, 100));
        if(is_omni && eff_res < 100u)
        {
            const float q = eff_res / 100.0f;
            const int tw = std::max(8, (int)std::lround(4.0f + q * q * (float)(std::max(2, media.width()) - 4)));
            const int th = std::max(8, (int)std::lround(4.0f + q * q * (float)(std::max(2, media.height()) - 4)));
            media = media.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        const float smoothing = GetSmoothing() / 100.0f;
        if(media_is_gif && prev_snap && !prev_snap->isNull() && smoothing > 0.0f && step_interval_ms > 0 && step_ms > 0)
        {
            const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
            const float elapsed_ms = (float)std::max<qint64>(0, now_ms - step_ms);
            const float blend_window_ms = std::max(1.0f, (float)step_interval_ms * smoothing);
            const float a = std::clamp(elapsed_ms / blend_window_ms, 0.0f, 1.0f);
            if(a < 0.999f)
            {
                QImage cur = media.convertToFormat(QImage::Format_ARGB32);
                QImage prev = prev_snap->scaled(cur.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                  .convertToFormat(QImage::Format_ARGB32);
                for(int y = 0; y < cur.height(); ++y)
                {
                    QRgb* dst = reinterpret_cast<QRgb*>(cur.scanLine(y));
                    const QRgb* src = reinterpret_cast<const QRgb*>(prev.constScanLine(y));
                    for(int x = 0; x < cur.width(); ++x)
                    {
                        const RGBColor c = MediaTextureEffect::LerpRGB(
                            ToRGBColor(qRed(src[x]), qGreen(src[x]), qBlue(src[x])),
                            ToRGBColor(qRed(dst[x]), qGreen(dst[x]), qBlue(dst[x])), a);
                        dst[x] = qRgba(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c), 255);
                    }
                }
                media = std::move(cur);
            }
        }

        volume_assist_.setMediaTexture(media, tile_repeat_enabled);

        const float tm = std::max(0.25f, bb.tight_mul);
        const bool freeze_motion = GetSpeed() == 0;
        const float scroll_mul = SliderRaw("motion_scroll") / 100.0f;
        const float warp_mul = SliderRaw("motion_warp") / 100.0f;
        const float phase_mul = SliderRaw("motion_phase") / 100.0f;
        const float speed_lin = std::clamp(GetSpeed() / 100.0f, 0.0f, 1.0f);
        const float size_m = std::max(0.08f, size);
        const float prop01 = SliderRaw("ambience_propagation") / 100.0f;

        if(spec_.media_layout == "projection")
        {
            const float detail_s = std::max(0.05f, GetScaledDetail()) * tm;
            const float freq_n = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);
            const float scroll_rate =
                freeze_motion
                    ? 0.0f
                    : scroll_mul * (0.22f + 0.48f * speed_lin + 0.18f * freq_n) * bb.speed_mul;
            const float repeat_from_freq = 0.55f + 1.65f * freq_n;
            const float size_zoom_div = std::clamp(0.40f + 0.36f * size_m, 0.32f, 2.2f);
            const float tile = std::clamp(repeat_from_freq / size_zoom_div, 0.12f, 6.5f);
            const float amp =
                freeze_motion ? 0.0f
                              : warp_mul * (0.045f + 0.20f * std::min(1.0f, GetScaledDetail() * 0.12f)) / tm;
            const float q = eff_res / 100.0f;
            const float steps_u = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, media.width()) - 4));
            const float steps_v = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, media.height()) - 4));
            const float packed_v = steps_v + ((eff_res < 100u) ? 1000.0f : 0.0f);
            float vp[13] = {
                (float)std::clamp(ComboIndex("projection_mode"), 0, 3),
                tile,
                scroll_rate,
                phase_mul,
                amp,
                detail_s,
                SliderRaw("ambience_dist_falloff") / 100.0f,
                SliderRaw("ambience_falloff_curve") / 100.0f,
                SliderRaw("ambience_edge_soft") / 100.0f,
                prop01,
                steps_u,
                packed_v,
                tile_repeat_enabled ? 1.0f : 0.0f
            };
            SubmitVolumeGpu(render_sequence, time_sec, vp, 13);
            return;
        }

        if(is_omni)
        {
            constexpr int kOmniShapeCount = 6;
            const float spin_percent = (float)SliderRaw("spin_percent");
            const float morph_percent = (float)SliderRaw("morph_percent");
            const float spin_rate =
                freeze_motion
                    ? 0.0f
                    : (0.25f + 2.6f * (spin_percent / 100.0f)) * (0.35f + 0.75f * speed_lin)
                          * (0.55f + 1.35f * scroll_mul) * bb.speed_mul;
            const float yaw_rate = spin_rate;
            const float pitch_rate = spin_rate * 0.71f;
            const float phase_drive =
                freeze_motion ? 0.0f : (phase_mul * 1.55f + scroll_mul * 0.95f);
            const float repeat_from_freq = 0.55f + 1.65f * GetNormalizedFrequency();
            const float size_zoom_div = std::clamp(0.55f + 0.28f * size_m, 0.40f, 2.0f);
            const float tile = std::clamp(repeat_from_freq / size_zoom_div, 0.12f, 6.5f);
            const float detail_raw = std::max(0.05f, GetScaledDetail());
            const float amp =
                freeze_motion ? 0.0f
                              : warp_mul * (0.05f + 0.22f * std::min(1.0f, detail_raw * 0.12f)) / tm;
            const float R_local = std::clamp(0.16f + 1.15f * size_m, 0.12f, 1.65f) / tm;
            const float packed_wrap = (tile_repeat_enabled ? 2.0f : 0.0f) + prop01;
            float vp[13] = {
                (float)std::clamp(ComboIndex("base_shape"), 0, kOmniShapeCount - 1),
                morph_percent / 100.0f,
                tile,
                yaw_rate,
                pitch_rate,
                phase_drive,
                amp,
                detail_raw,
                SliderRaw("ambience_dist_falloff") / 100.0f,
                SliderRaw("ambience_falloff_curve") / 100.0f,
                SliderRaw("ambience_edge_soft") / 100.0f,
                R_local,
                packed_wrap
            };
            SubmitVolumeGpu(render_sequence, time_sec, vp, 13);
            return;
        }

        float zp[13] = {};
        SubmitVolumeGpu(render_sequence, time_sec, zp, 13);
        return;
    }

    if(spec_.uses_audio)
    {
        const float amplitude = SampleAudioVisualLevel(audio_settings);
        const float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
        if(std::fabs(time_sec - audio_last_intensity_time) > 1e-4f)
        {
            audio_smoothed = alpha * audio_smoothed + (1.0f - alpha) * amplitude;
            audio_last_intensity_time = time_sec;

            AudioInputManager* aud = AudioInputManager::instance();
            float bass_t = 0.0f, mid_t = 0.0f, high_t = 0.0f;
            if(aud)
            {
                bass_t = std::clamp(aud->getBandSlowEnergyHz(40.0f, 180.0f), 0.0f, 1.0f);
                mid_t  = std::clamp(aud->getBandSlowEnergyHz(200.0f, 2000.0f), 0.0f, 1.0f);
                high_t = std::clamp(aud->getBandSlowEnergyHz(2500.0f, 12000.0f), 0.0f, 1.0f);
            }
            const float band_alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.95f);
            audio_band_lo  = band_alpha * audio_band_lo  + (1.0f - band_alpha) * bass_t;
            audio_band_mid = band_alpha * audio_band_mid + (1.0f - band_alpha) * mid_t;
            audio_band_hi  = band_alpha * audio_band_hi  + (1.0f - band_alpha) * high_t;

            const float drive = ApplyAudioVisualIntensity(SampleAudioVisualLevel(audio_settings), audio_settings);
            audio_time_boost += (0.35f + 1.8f * audio_band_lo + 0.4f * drive)
                                * std::max(0.0f, GetNormalizedSpeed()) * speed_mul * 0.016f;
            audio_time_boost = std::fmod(audio_time_boost + 1000.0f, 1000.0f);
        }
        else if(alpha <= 0.0f)
        {
            audio_smoothed = amplitude;
        }
        audio_fill_cached = ApplyAudioIntensity(audio_smoothed, audio_settings);
    }

    if(spec_.audio_media == "bands")
    {
        AudioInputManager* aud = AudioInputManager::instance();
        int total_bands = aud ? aud->getBandsCount() : 16;
        if(total_bands <= 0) total_bands = 16;
        float f_min = 20.0f, f_max = 20000.0f;
        if(aud)
        {
            float sr = (float)aud->getSampleRate();
            int fft  = aud->getFFTSize();
            if(sr > 0.0f && fft > 0)
            {
                f_min = std::max(1.0f, sr / (float)fft);
                f_max = sr * 0.5f;
                if(f_max <= f_min) f_max = f_min + 1.0f;
            }
        }
        auto mapHz = [&](float hz) -> int {
            float denom = std::log(f_max / f_min);
            if(std::abs(denom) < 1e-6f) return 0;
            float t = std::log(std::clamp(hz, f_min, f_max) / f_min) / denom;
            return std::clamp((int)(t * total_bands), 0, total_bands - 1);
        };
        audio_spec_band_start = mapHz((float)audio_settings.low_hz);
        audio_spec_band_end   = mapHz((float)audio_settings.high_hz);
        if(audio_spec_band_end < audio_spec_band_start) std::swap(audio_spec_band_end, audio_spec_band_start);
        audio_spec_band_start = std::clamp(audio_spec_band_start, 0, total_bands - 1);
        audio_spec_band_end   = std::clamp(audio_spec_band_end,   audio_spec_band_start, total_bands - 1);

        const int band_count = std::max(1, audio_spec_band_end - audio_spec_band_start + 1);
        if((int)audio_smoothed_bands.size() != band_count)
            audio_smoothed_bands.assign(band_count, 0.0f);

        std::vector<float> raw_bands;
        if(aud) aud->getBands(raw_bands);
        const float b_smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
        const float boost = std::clamp(audio_settings.peak_boost, 0.25f, 4.0f);
        for(int i = 0; i < band_count; ++i)
        {
            const int idx = audio_spec_band_start + i;
            float s = (idx >= 0 && idx < (int)raw_bands.size()) ? raw_bands[idx] : 0.0f;
            audio_smoothed_bands[i] = b_smooth * audio_smoothed_bands[i] + (1.0f - b_smooth) * s;
        }

        // Gate + boost in host; shader gets a clean gated texture with no second multiply
        QImage img(band_count, 1, QImage::Format_RGBA8888);
        for(int i = 0; i < band_count; ++i)
        {
            float v = AudioVisualNoiseGate(audio_smoothed_bands[i]);
            v = std::clamp(v * boost, 0.0f, 1.0f);
            const int g = (int)std::lround(v * 255.0f);
            img.setPixel(i, 0, qRgba(g, g, g, 255));
        }
        volume_assist_.setMediaTexture(img, false);

        audio_band_count_cached = (float)band_count;
        const float roll_sp = SliderRaw("roll_speed") / 100.0f;
        audio_roll_phase_cached = (roll_sp > 1e-6f)
            ? std::fmod(time_sec * roll_sp * speed_mul + 1000.0f, 1.0f) : 0.0f;
    }

    if(spec_.audio_media == "spectrogram")
    {
        if((int)audio_spectrogram_history.size() != kFvAudioSpectrogramRows)
        {
            audio_spectrogram_history.assign(kFvAudioSpectrogramRows,
                std::vector<float>(kFvAudioSpectrogramCols, 0.0f));
            audio_column_smoothed.assign(kFvAudioSpectrogramCols, 0.0f);
            audio_spectrogram_write_index = 0;
        }

        constexpr float kPushInterval = 1.0f / 60.0f;
        if(audio_last_spectrogram_push == std::numeric_limits<float>::lowest()
           || (time_sec - audio_last_spectrogram_push) >= kPushInterval)
        {
            AudioInputManager* aud = AudioInputManager::instance();
            if(aud && aud->isRunning())
            {
                AudioInputManager::SpectrumSnapshot snap = aud->getSpectrumSnapshot(kFvAudioSpectrogramCols);
                if(!snap.bins.empty())
                {
                    const float fm = snap.min_frequency_hz > 0.0f ? snap.min_frequency_hz : 20.0f;
                    const float fx = snap.max_frequency_hz > fm   ? snap.max_frequency_hz : 20000.0f;
                    auto colFor = [&](float hz) -> int {
                        float denom = std::log(fx / fm);
                        if(std::abs(denom) < 1e-6f) return 0;
                        float t = std::log(std::clamp(hz, fm, fx) / fm) / denom;
                        return std::clamp((int)(t * kFvAudioSpectrogramCols), 0, kFvAudioSpectrogramCols - 1);
                    };
                    int i0 = colFor((float)audio_settings.low_hz);
                    int i1 = colFor((float)audio_settings.high_hz);
                    if(i1 < i0) std::swap(i0, i1);
                    const int row = audio_spectrogram_write_index % kFvAudioSpectrogramRows;
                    auto& hist_row = audio_spectrogram_history[row];
                    hist_row.resize(kFvAudioSpectrogramCols, 0.0f);
                    const float sc_smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
                    const int eq_bands = std::max(1, aud->getEqBandCount());
                    for(int c = 0; c < kFvAudioSpectrogramCols; ++c)
                    {
                        float v = 0.0f;
                        if(c >= i0 && c <= i1 && c < (int)snap.bins.size())
                        {
                            const int eq_b = std::min((c * eq_bands) / kFvAudioSpectrogramCols, eq_bands - 1);
                            v = std::clamp(snap.bins[c] * aud->getEqGain(eq_b), 0.0f, 1.0f);
                        }
                        hist_row[c] = v;
                        audio_column_smoothed[c] = sc_smooth * audio_column_smoothed[c] + (1.0f - sc_smooth) * v;
                    }
                    audio_spectrogram_write_index++;
                }
            }
            audio_last_spectrogram_push = time_sec;
        }

        const float scroll_sp = SliderRaw("scroll_speed") / 100.0f;
        audio_strip_scroll_cached = std::fmod(time_sec * scroll_sp, 1.0f);
        if(audio_strip_scroll_cached < 0.0f) audio_strip_scroll_cached += 1.0f;
        audio_bar_edge_cached = std::max(0.02f, audio_settings.falloff * 0.0015f);

        const float boost = std::clamp(audio_settings.peak_boost, 0.25f, 4.0f);
        const int disp_mode = ComboIndex("display_mode");
        if(disp_mode == 1)
        {
            QImage img(kFvAudioSpectrogramCols, kFvAudioSpectrogramRows, QImage::Format_RGBA8888);
            const int rows = kFvAudioSpectrogramRows;
            const int newest = (audio_spectrogram_write_index > 0)
                ? ((audio_spectrogram_write_index - 1) % rows) : 0;
            for(int age = 0; age < rows; ++age)
            {
                const int src_row = (newest - age + rows) % rows;
                const auto& rd = (src_row < (int)audio_spectrogram_history.size())
                    ? audio_spectrogram_history[src_row] : audio_column_smoothed;
                for(int c = 0; c < kFvAudioSpectrogramCols; ++c)
                {
                    float v = (c < (int)rd.size()) ? rd[c] : 0.0f;
                    v = AudioVisualNoiseGate(v) * boost;
                    const int g = (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
                    img.setPixel(c, age, qRgba(g, g, g, 255));
                }
            }
            volume_assist_.setMediaTexture(img, false);
        }
        else
        {
            QImage img(kFvAudioSpectrogramCols, 1, QImage::Format_RGBA8888);
            for(int c = 0; c < kFvAudioSpectrogramCols; ++c)
            {
                float v = (c < (int)audio_column_smoothed.size()) ? audio_column_smoothed[c] : 0.0f;
                v = AudioVisualNoiseGate(v) * boost;
                const int g = (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
                img.setPixel(c, 0, qRgba(g, g, g, 255));
            }
            volume_assist_.setMediaTexture(img, false);
        }
    }

    if(spec_.audio_preset == "beat" || spec_.audio_preset == "low_punch")
    {
        // Tick pulse queue
        if(std::fabs(time_sec - audio_last_pulse_tick_time) > 1e-5f)
        {
            const float dt = (audio_last_pulse_tick_time == std::numeric_limits<float>::lowest())
                ? 0.0f : std::clamp(time_sec - audio_last_pulse_tick_time, 0.0f, 0.1f);
            audio_last_pulse_tick_time = time_sec;

            AudioInputManager* aud = AudioInputManager::instance();
            const float onset_thr = std::clamp(SliderRaw("onset_trigger") / 100.0f, 0.05f, 0.95f);
            float strength = 0.0f;
            if(aud && aud->isRunning() && TryTriggerAudioPulse(dt, audio_settings, audio_pulse_trigger,
                onset_thr, AudioReactiveOnsetSmoothAlpha(audio_settings),
                AudioReactiveBeatPulseHoldSec(), strength))
            {
                AudioPulseEntry p;
                p.birth_time  = time_sec;
                p.strength    = strength;
                p.color_slot  = audio_next_pulse_color_slot++;
                audio_pulses.push_back(p);
            }

            const float exp_decay = BeatWaveShellDecay(audio_settings, 2.0f);
            audio_pulses.erase(std::remove_if(audio_pulses.begin(), audio_pulses.end(),
                [&](const AudioPulseEntry& p) {
                    const float age = time_sec - p.birth_time;
                    return age > 2.6f || AudioReactivePulseFade(p.strength, age, exp_decay) < 0.003f;
                }), audio_pulses.end());
        }

        // Compute grid-derived geometry (match gizmo / SampleVolumeLocalUv01 extents)
        Vector3D origin = GetEffectOriginGrid(grid);
        float vh_x = 1.0f, vh_y = 1.0f, vh_z = 1.0f;
        GetVolumeHalfExtents(grid, vh_x, vh_y, vh_z);
        const EffectGridAxisHalfExtents extents{vh_x, vh_y, vh_z};
        constexpr float kExplosionFill = 3.0f;
        float radius_basis = EffectGridMedianOfHalfExtents(extents)
            * 1.7320508f * kExplosionFill;
        radius_basis = std::max(radius_basis, 1e-3f);

        const AudioBeatWaveMode wave_mode = static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode);
        const bool classic = (wave_mode == AudioBeatWaveMode::ClassicWave);
        const float pulse_speed = BeatWaveScaledSpeed(GetNormalizedSpeed() * speed_mul, audio_settings);
        const float decay_v     = classic ? BeatWaveShellDecay(audio_settings, 2.0f)
                                          : BeatWaveShellDecay(audio_settings, 2.35f);
        const float half_w      = AudioRingHalfWidthFromFalloff(radius_basis, audio_settings.falloff, size, tight_mul, detail);
        const float max_travel  = classic ? BeatWaveBurstPhaseCap(audio_settings)
                                          : radius_basis * (0.88f + 0.12f * size);

        audio_radius_basis_cached = radius_basis;
        audio_pulse_half_w_cached = half_w;
        audio_max_travel_cached   = max_travel;
        audio_pulse_speed_cached  = pulse_speed;
        audio_pulse_hw_cached     = std::max(1e-5f, extents.hw);
        audio_pulse_hh_cached     = std::max(1e-5f, extents.hh);
        audio_pulse_hd_cached     = std::max(1e-5f, extents.hd);
        audio_pulse_decay_cached  = decay_v;

        float vp[24] = {};
        const int p_count = std::min(14, (int)spec_.params.size());
        for(int i = 0; i < p_count; ++i)
            vp[i] = ParamValue(spec_.params[(size_t)i], detail, size);

        for(int i = 0; i < 5; ++i) { vp[14 + i*2] = -1.0f; vp[15 + i*2] = 0.0f; }
        std::fill(std::begin(audio_packed_color_slots), std::end(audio_packed_color_slots), 0u);
        audio_packed_pulse_count = 0;
        const int start = std::max(0, (int)audio_pulses.size() - 5);
        for(int pi = start; pi < (int)audio_pulses.size() && audio_packed_pulse_count < 5; ++pi)
        {
            const AudioPulseEntry& p = audio_pulses[pi];
            const float age = time_sec - p.birth_time;
            if(age < 0.0f || p.strength <= 0.0f) continue;
            const int slot = audio_packed_pulse_count++;
            vp[14 + slot*2] = age;
            vp[15 + slot*2] = p.strength;
            audio_packed_color_slots[slot] = p.color_slot;
        }

        SubmitVolumeGpu(render_sequence, time_sec, vp, 24);
        return;
    }

    if(spec_.audio_preset == "high_sparkle")
    {
        AudioInputManager* aud = AudioInputManager::instance();
        audio_note_count = 0;
        std::fill(std::begin(audio_note_hues), std::end(audio_note_hues), 0.0f);
        std::fill(std::begin(audio_note_amps), std::end(audio_note_amps), 0.0f);
        audio_hi_drive_cached = 0.0f;

        if(aud && aud->isRunning())
        {
            audio_hi_drive_cached = std::clamp(
                std::max(aud->getNoteDrive01(), aud->getBandSlowEnergyHz(2800.0f, 14000.0f)),
                0.0f, 1.0f);
            const auto notes = aud->getActiveNotes();
            for(const auto& n : notes)
            {
                if(audio_note_count >= 4) break;
                if(n.amp < 0.10f) continue;
                float mean = n.mean;
                if(mean < 0.0f)   mean += 12.0f;
                if(mean >= 12.0f) mean -= 12.0f;
                audio_note_hues[audio_note_count] = std::clamp(mean / 12.0f, 0.0f, 1.0f);
                audio_note_amps[audio_note_count] = std::clamp(n.amp, 0.0f, 1.0f);
                ++audio_note_count;
            }
            if(audio_note_count == 0 && aud->getNoteDrive01() > 0.05f)
            {
                audio_note_hues[0] = aud->getDominantNoteHue01();
                audio_note_amps[0] = aud->getNoteDrive01();
                audio_note_count = 1;
            }
        }

        float vp[24] = {};
        const int p_count = std::min(12, (int)spec_.params.size());
        for(int i = 0; i < p_count; ++i)
            vp[i] = ParamValue(spec_.params[(size_t)i], detail, size);
        vp[6] = audio_hi_drive_cached;

        for(int i = 0; i < 4; ++i)
        {
            vp[12 + i*2] = audio_note_hues[i];
            vp[13 + i*2] = audio_note_amps[i];
        }
        vp[20] = (float)audio_note_count;
        vp[21] = time_sec * speed_mul * GetNormalizedSpeed();

        SubmitVolumeGpu(render_sequence, time_sec, vp, 22);
        return;
    }

    float vp[24] = {};
    const int count = std::min(24, (int)spec_.params.size());
    for(int i = 0; i < count; ++i)
    {
        vp[i] = ParamValue(spec_.params[(size_t)i], detail, size);
    }
    if(count > 0)
    {
        SubmitVolumeGpu(render_sequence, time_sec, vp, count);
    }
}

RGBColor FolderVolumeEffect::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /* World-space sample bound to the gizmo volume (rotation/move/scale/shear). */
    Vector3D origin = GetEffectOriginGrid(grid);
    float n1 = 0.5f, n2 = 0.5f, n3 = 0.5f;
    if(spec_.sample_room)
    {
        if(EffectGridSampleOutsideVolume(x, y, z, grid))
        {
            return 0x00000000;
        }
        SampleGpuRoomVolume01(x, y, z, grid, &n1, &n2, &n3);
    }
    else if(!SampleVolumeLocalUv01(x, y, z, grid, &n1, &n2, &n3))
    {
        return 0x00000000;
    }

    progress = CalculateProgress(time);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float size_multiplier = GetNormalizedSize();
    /* Local-frame stand-in for stratum / audio helpers that expect a grid point. */
    const Vector3D local = VolumeWorldDeltaToLocal(x - origin.x, y - origin.y, z - origin.z);
    Vector3D rotated_pos{origin.x + local.x, origin.y + local.y, origin.z + local.z};
    if(!volume_assist_.isAvailable())
    {
        return 0x00000000;
    }

    const float coord2 = SampleStratumYNorm01(rotated_pos.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 = ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
    const float prog = progress * bb.speed_mul;
    const float pshift = EffectStratumBlend::PhaseShift01(bb);
    if(spec_.finish_rgb)
    {
        const QVector3D samp = SampleField01(n1, n2, n3);
        return ToRGBColor((int)(std::clamp(samp.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.z(), 0.0f, 1.0f) * 255.0f + 0.5f));
    }
    if(spec_.finish_surface)
    {
        const QVector3D samp = SampleField01(n1, n2, n3);
        float intensity = samp.x();
        const float plasma = samp.y();
        const float hot = samp.z();
        if(GetStratumLayoutMode() == 1)
            intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.18f);
        if(intensity < 0.01f)
            return 0x00000000;

        const float phase01 = EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);
        const int style = ComboIndex("style");
        RGBColor c = 0;
        if(style > 0 && style < 8)
        {
            const float p = std::clamp(plasma, 0.0f, 1.0f);
            const float h = std::clamp(hot, 0.0f, 1.0f);
            const float spread = std::clamp(0.9f + GetNormalizedFrequency() * 0.9f, 0.9f, 2.6f);
            const float shimmer =
                std::sin(time * GetColorCycleHz() * 6.2831853f * bb.speed_mul + p * 6.28318f) *
                (5.0f + 9.0f * p);
            if(style == 7)
            {
                const float cool = 150.0f + p * 70.0f;
                const float warm = 190.0f + p * 50.0f;
                const float mix = std::clamp(0.35f + 0.5f * p + h * 0.25f, 0.0f, 1.0f);
                const unsigned char r = (unsigned char)std::clamp((int)(cool * (1.0f - mix) + warm * mix + h * 40.0f), 0, 255);
                const unsigned char g = (unsigned char)std::clamp((int)(cool * 0.95f + p * 40.0f + h * 35.0f), 0, 255);
                const unsigned char b = (unsigned char)std::clamp((int)(cool + (1.0f - p) * 35.0f + h * 25.0f), 0, 255);
                c = (RGBColor)((b << 16) | (g << 8) | r);
            }
            else
            {
                float hue0 = 0.0f;
                float span = 60.0f;
                switch(style)
                {
                case 1: hue0 = 6.0f;   span = 58.0f; break;
                case 2: hue0 = 165.0f; span = 75.0f; break;
                case 3: hue0 = 70.0f;  span = 70.0f; break;
                case 4: hue0 = 0.0f;   span = 68.0f; break;
                case 5: hue0 = 4.0f;   span = 52.0f; break;
                case 6: hue0 = 175.0f; span = 70.0f; break;
                default: hue0 = p * 360.0f; span = 0.0f; break;
                }
                float hue = hue0 + p * span * spread + shimmer + phase01 * 14.0f + h * 18.0f;
                hue = std::fmod(hue, 360.0f);
                if(hue < 0.0f)
                    hue += 360.0f;
                c = GetRainbowColor(hue);
                if(style == 1 || style == 4 || style == 5)
                {
                    const float whiten = std::clamp(h * h * 0.92f, 0.0f, 1.0f);
                    const int r = (int)((c & 0xFF) * (1.0f - whiten) + 255.0f * whiten);
                    const int g = (int)(((c >> 8) & 0xFF) * (1.0f - whiten) + 235.0f * whiten);
                    const int b = (int)(((c >> 16) & 0xFF) * (1.0f - whiten) + 160.0f * whiten);
                    c = (RGBColor)((std::clamp(b, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
                }
                else if(style == 2 || style == 6)
                {
                    const float glint = std::clamp(h * 0.75f, 0.0f, 1.0f);
                    const int r = (int)((c & 0xFF) * (1.0f - glint * 0.55f) + 220.0f * glint);
                    const int g = (int)(((c >> 8) & 0xFF) * (1.0f - glint * 0.35f) + 240.0f * glint);
                    const int b = (int)(((c >> 16) & 0xFF) * (1.0f - glint * 0.15f) + 255.0f * glint);
                    c = (RGBColor)((std::clamp(b, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
                }
                else if(style == 3)
                {
                    const float gloss = std::clamp(h * 0.85f, 0.0f, 1.0f);
                    const int r = (int)((c & 0xFF) * (1.0f - gloss * 0.4f) + 210.0f * gloss);
                    const int g = (int)(((c >> 8) & 0xFF) * (1.0f - gloss * 0.25f) + 255.0f * gloss);
                    const int b = (int)(((c >> 16) & 0xFF) * (1.0f - gloss * 0.45f) + 160.0f * gloss);
                    c = (RGBColor)((std::clamp(b, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
                }
            }
        }
        else if(UseEffectStripColormap())
        {
            const float ph01 = std::fmod(time * GetColorCycleHz() * bb.speed_mul +
                                             phase01 + plasma * 0.08f + 1.f, 1.f);
            const float pal = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                         GetEffectStripColormapUnfold(),
                                                         GetEffectStripColormapDirectionDeg(),
                                                         ph01,
                                                         time,
                                                         grid,
                                                         GetNormalizedSize(),
                                                         origin,
                                                         rotated_pos);
            c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(),
                                             std::clamp(pal, 0.0f, 1.0f),
                                             time);
        }
        else if(GetRainbowMode())
        {
            float hue = std::fmod(plasma * 360.0f + time * GetColorCycleHz() * 360.0f * bb.speed_mul
                                      + phase01 * 360.0f, 360.0f);
            if(hue < 0.0f)
                hue += 360.0f;
            c = GetRainbowColor(hue);
        }
        else
        {
            c = GetColorAtPosition(plasma);
        }
        const int r = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
        const int g = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
        const int b = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
        return (RGBColor)((b << 16) | (g << 8) | r);
    }
    if(spec_.finish_hsv)
    {
        const QVector3D samp = SampleField01(n1, n2, n3);
        const float sat = samp.x();
        float val = samp.y();
        float h = samp.z();
        if(val <= 1e-5f)
            return 0x00000000;
        h = std::fmod(h + time * GetColorCycleHz() * bb.speed_mul
                      + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
        if(UseEffectStripColormap())
        {
            const float ph01 = std::fmod(h + 1.0f, 1.0f);
            float pal01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                       GetEffectStripColormapUnfold(),
                                                       GetEffectStripColormapDirectionDeg(),
                                                       ph01,
                                                       time,
                                                       grid,
                                                       GetNormalizedScale(),
                                                       origin,
                                                       rotated_pos);
            RGBColor c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(),
                                                      std::clamp(pal01, 0.0f, 1.0f), time);
            const int cr = (int)(c & 0xFF);
            const int cg = (int)((c >> 8) & 0xFF);
            const int cb = (int)((c >> 16) & 0xFF);
            const float cv = std::max({cr, cg, cb}) / 255.0f;
            return EffectHsv01ToBgr(h, sat, std::clamp(val * cv, 0.0f, 1.0f));
        }
        if(GetRainbowMode())
        {
            SpatialLayerCore::Basis basis;
            SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
            SpatialLayerCore::MapperSettings map;
            EffectStratumBlend::InitStratumBreaks(map);
            SpatialLayerCore::SamplePoint sp{};
            sp.grid_x = x;
            sp.grid_y = y;
            sp.grid_z = z;
            sp.origin_x = origin.x;
            sp.origin_y = origin.y;
            sp.origin_z = origin.z;
            sp.y_norm = coord2;
            float hue_deg = h * 360.0f;
            hue_deg = ApplySpatialRainbowHue(hue_deg, h, basis, sp, map, time, &grid);
            h = std::fmod(hue_deg / 360.0f + 1.0f, 1.0f);
        }
        return EffectHsv01ToBgr(h, sat, val);
    }
    if(spec_.finish_hex)
    {
        const QVector3D samp = SampleField01(n1, n2, n3);
        float v = samp.x();
        float h01 = samp.y();
        if(spec_.finish_atlas)
        {
            const float hue_rad = std::atan2(samp.y() * 2.0f - 1.0f, samp.x() * 2.0f - 1.0f);
            h01 = std::fmod(hue_rad / 6.2831853f + 1.0f, 1.0f);
            v = samp.z() > 0.0f ? std::clamp(samp.z(), 0.0f, 1.0f) : 1.0f;
        }
        RGBColor c = 0;
        if(UseEffectStripColormap())
        {
            const float p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                           GetEffectStripColormapUnfold(),
                                                           GetEffectStripColormapDirectionDeg(),
                                                           h01,
                                                           time,
                                                           grid,
                                                           GetNormalizedSize(),
                                                           origin,
                                                           rotated_pos);
            c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), p01, time);
        }
        else if(GetRainbowMode())
        {
            SpatialLayerCore::Basis basis;
            SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
            SpatialLayerCore::MapperSettings map;
            SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
            SpatialLayerCore::SamplePoint sp{};
            sp.grid_x = x;
            sp.grid_y = y;
            sp.grid_z = z;
            sp.origin_x = origin.x;
            sp.origin_y = origin.y;
            sp.origin_z = origin.z;
            const float hue = ApplySpatialRainbowHue(h01 * 360.0f, h01, basis, sp, map, time, &grid);
            c = GetRainbowColor(std::fmod(hue + 720.0f, 360.0f));
        }
        else
        {
            c = GetColorAtPosition(h01);
        }
        if(GetEdgeFade() > 0)
        {
            const float fade = std::clamp(GetEdgeFade() / 100.0f, 0.0f, 1.0f);
            const float u = RoomXZEdgeProximity01(x, z, grid);
            const float t = std::clamp(u, 0.0f, 1.0f);
            v *= std::max(0.0f, 1.0f - fade * (t * t * (3.0f - 2.0f * t)));
        }
        const int r = std::clamp((int)((float)(c & 0xFF) * v), 0, 255);
        const int g = std::clamp((int)((float)((c >> 8) & 0xFF) * v), 0, 255);
        const int b = std::clamp((int)((float)((c >> 16) & 0xFF) * v), 0, 255);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }
    if(spec_.finish_audio)
    {
        const QVector3D samp = SampleField01(n1, n2, n3);
        float intensity = samp.x();
        if(GetStratumLayoutMode() == 1)
            intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.18f);
        if(intensity <= 0.001f)
            return 0x00000000;

        // Note sparkle: shader outputs (intensity, note_hue01, gradient)
        if(spec_.audio_preset == "high_sparkle")
        {
            const float note_hue01  = std::clamp(samp.y(), 0.0f, 1.0f);
            const float gradient_pos = std::clamp(samp.z(), 0.0f, 1.0f);
            const auto mode = static_cast<AudioPulseColorMode>(audio_settings.pulse_color_mode);
            RGBColor color;
            if(GetRainbowMode()
               && mode != AudioPulseColorMode::PerBeatCycle
               && mode != AudioPulseColorMode::Uniform)
            {
                color = GetRainbowColor(gradient_pos * 360.0f);
            }
            else if(mode == AudioPulseColorMode::FollowNotes)
            {
                color = GetRainbowColor(note_hue01 * 360.0f);
            }
            else
            {
                AudioReactiveColorParams cp{};
                cp.gradient_pos01 = gradient_pos;
                cp.intensity       = intensity;
                cp.beat_color_slot = (uint32_t)std::floor(note_hue01 * 12.0f);
                cp.time = time; cp.grid_x = x; cp.grid_y = y; cp.grid_z = z;
                cp.grid = &grid; cp.origin = origin; cp.rotated_pos = rotated_pos;
                cp.y_norm01 = coord2; cp.stratum_mot01 = stratum_mot01; cp.band_scalars = &bb;
                color = ResolveAudioReactiveColor(audio_settings, cp);
            }
            return BrightenAudioEffectColor(color, intensity);
        }

        // Pulse: shader outputs (energy, gradient, pulse_idx01)
        float gradient_pos = samp.y();
        uint32_t beat_slot = (uint32_t)std::floor(time * 2.5f);
        if((spec_.audio_preset == "beat" || spec_.audio_preset == "low_punch")
           && audio_packed_pulse_count > 0)
        {
            const int pulse_slot = std::clamp((int)std::floor(samp.z() * 5.0f), 0, 4);
            beat_slot = audio_packed_color_slots[pulse_slot];

            const bool classic = (static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode)
                                  == AudioBeatWaveMode::ClassicWave);
            intensity = std::min(1.0f, intensity * (classic ? 1.45f : 1.22f));
            if(intensity <= 0.006f) return 0x00000000;
        }

        AudioReactiveColorParams color_params{};
        color_params.gradient_pos01 = gradient_pos;
        color_params.intensity      = intensity;
        color_params.beat_color_slot = beat_slot;
        color_params.time = time; color_params.grid_x = x; color_params.grid_y = y; color_params.grid_z = z;
        color_params.grid = &grid; color_params.origin = origin; color_params.rotated_pos = rotated_pos;
        color_params.y_norm01 = coord2; color_params.stratum_mot01 = stratum_mot01; color_params.band_scalars = &bb;

        RGBColor color = ResolveAudioReactiveColor(audio_settings, color_params);
        return BrightenAudioEffectColor(color, intensity);
    }

    float value = SampleField01(n1, n2, n3).x();
    value = EffectStratumBlend::ApplyMotionToUnit01(value, stratum_mot01, 0.28f);

    float depth_factor = 1.0f;
    if(spec_.finish_depth)
    {
        const float rot_rel_x = rotated_pos.x - origin.x;
        const float rot_rel_y = rotated_pos.y - origin.y;
        const float rot_rel_z = rotated_pos.z - origin.z;
        const float radial_distance = sqrtf(rot_rel_x * rot_rel_x + rot_rel_y * rot_rel_y + rot_rel_z * rot_rel_z);
        const float max_radius = EffectGridGpuAtlasMedianHalfExtent(grid, origin, GetNormalizedScale()) * 1.7320508f;
        if(max_radius > 0.001f)
        {
            const float normalized_dist = fmin(1.0f, radial_distance / max_radius);
            depth_factor = 0.45f + 0.55f * (1.0f - normalized_dist * 0.6f);
        }
    }

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    RGBColor final_color = 0;
    const float phase01 = std::fmod(prog + pshift + 1.0f, 1.0f);
    const float arms = std::max(1, SliderRaw("num_arms"));
    const float coil01 = SliderRaw("coil_amount") / 100.0f;
    const float height01 = SliderRaw("height_coil_amount") / 100.0f;
    float spiral_angle = 0.0f;
    if(spec_.finish_spiral)
    {
        const float rot_rel_x = rotated_pos.x - origin.x;
        const float rot_rel_z = rotated_pos.z - origin.z;
        const float angle = atan2f(rot_rel_z, rot_rel_x);
        float ex_x = 1.0f, ex_y = 1.0f, ex_z = 1.0f;
        GetVolumeHalfExtents(grid, ex_x, ex_y, ex_z);
        float norm_radius = EffectGridHorizontalRadialNorm01(
            EffectGridHorizontalRadialNormXZ(rot_rel_x, rot_rel_z, ex_x, ex_z));
        norm_radius = fmaxf(0.0f, fminf(1.0f, norm_radius));
        const float detail_e = detail * bb.tight_mul;
        const float two_pi = 6.2831853f;
        const float radial_coil = norm_radius * coil01 * two_pi * (2.2f + 1.4f * std::clamp(detail_e / 20.0f, 0.0f, 1.5f));
        const float z_twist = coord2 * height01 * two_pi * (1.0f + 0.75f * std::clamp(detail_e / 20.0f, 0.0f, 1.5f));
        spiral_angle = angle * arms + radial_coil + z_twist - prog * 1.35f;
        spiral_angle += EffectStratumBlend::PhaseShiftRad(bb);
        spiral_angle += stratum_mot01 * two_pi * 0.55f;
    }

    if(UseEffectStripColormap())
    {
        const float strip_p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                             GetEffectStripColormapUnfold(),
                                                             GetEffectStripColormapDirectionDeg(),
                                                             phase01,
                                                             time,
                                                             grid,
                                                             size_multiplier,
                                                             origin,
                                                             rotated_pos);
        const float tinted = spec_.finish_spiral
            ? std::fmod(strip_p01 + value * 0.35f + 1.0f, 1.0f)
            : strip_p01;
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), tinted, time);
    }
    else if(spec_.finish_spiral && (pattern_index == 1 || pattern_index == 2 || pattern_index == 5 || pattern_index == 6) && !GetRainbowMode())
    {
        float arm_index = fmodf(spiral_angle / (6.28318f / arms), arms);
        if(arm_index < 0.0f)
        {
            arm_index += arms;
        }
        float pos = fmodf((arm_index / arms) + time * GetColorCycleHz() * bb.speed_mul, 1.0f);
        if(pos < 0.0f)
        {
            pos += 1.0f;
        }
        final_color = GetColorAtPosition(ApplySpatialPalette01(pos, basis, sp, map, time, &grid));
    }
    else if(GetRainbowMode() && spec_.finish_spiral)
    {
        float hue = spiral_angle * 57.2958f + value * 200.0f + coord2 * 40.0f + time * GetColorCycleHz() * bb.speed_mul * 360.0f;
        hue = ApplySpatialRainbowHue(hue, fmodf(value + 0.25f, 1.0f), basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else if(spec_.finish_spiral)
    {
        float pos = fmodf(value + time * GetColorCycleHz() * bb.speed_mul, 1.0f);
        if(pos < 0.0f)
        {
            pos += 1.0f;
        }
        final_color = GetColorAtPosition(ApplySpatialPalette01(pos, basis, sp, map, time, &grid));
    }
    else if(GetRainbowMode())
    {
        float hue = value * 360.0f + time * GetColorCycleHz() * 360.0f;
        hue = ApplySpatialRainbowHue(hue, value, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        final_color = GetColorAtPosition(ApplySpatialPalette01(value, basis, sp, map, time, &grid));
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    float mask = depth_factor;
    if(spec_.finish_spiral)
    {
        mask = std::pow(std::clamp(value, 0.0f, 1.0f), 0.85f);
    }
    if(GetEdgeFade() > 0)
    {
        const float fade = std::clamp(GetEdgeFade() / 100.0f, 0.0f, 1.0f);
        const float u = RoomXZEdgeProximity01(x, z, grid);
        const float t = std::clamp(u, 0.0f, 1.0f);
        mask *= std::max(0.0f, 1.0f - fade * (t * t * (3.0f - 2.0f * t)));
    }
    r = (unsigned char)std::clamp((int)std::lround((float)r * mask), 0, 255);
    g = (unsigned char)std::clamp((int)std::lround((float)g * mask), 0, 255);
    b = (unsigned char)std::clamp((int)std::lround((float)b * mask), 0, 255);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json FolderVolumeEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    if(!spec_.patterns.empty())
    {
        const int listed = (int)spec_.patterns.size();
        const int idx = std::clamp(pattern_index, 0, std::max(0, listed - 1));
        if(spec_.pattern_from_kernels)
        {
            const char* kernel_id = SpatialPatternKernelId(idx);
            if(kernel_id && kernel_id[0] != '\0')
            {
                j[spec_.pattern_key] = kernel_id;
            }
            else
            {
                j[spec_.pattern_key] = idx;
            }
        }
        else
        {
            j[spec_.pattern_key] = spec_.patterns[(size_t)idx].name.toStdString();
        }
    }
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        j[spec_.combos[i].key] = combo_values[i];
    }
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        if(spec_.sliders[i].save_unit)
        {
            j[spec_.sliders[i].key] = slider_values[i] / 100.0;
        }
        else
        {
            j[spec_.sliders[i].key] = slider_values[i];
        }
    }
    if(spec_.uses_audio)
    {
        AudioReactiveSaveToJson(j, audio_settings);
    }
    if(spec_.uses_media)
    {
        j["media_path"] = media_path.toStdString();
        j["tile_repeat_enabled"] = tile_repeat_enabled;
    }
    return j;
}

void FolderVolumeEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    SyncEffectStripColormapPanelFromModel();
    if(!spec_.pattern_key.empty() && settings.contains(spec_.pattern_key))
    {
        const auto& pattern_value = settings[spec_.pattern_key];
        const int listed = (int)spec_.patterns.size();
        const int pattern_count = std::max(1, pattern_combo ? pattern_combo->count() : listed);
        if(pattern_value.is_string())
        {
            const std::string id = pattern_value.get<std::string>();
            int found = -1;
            if(spec_.pattern_from_kernels)
            {
                found = SpatialPatternKernelFindById(id);
            }
            if(found < 0)
            {
                for(size_t i = 0; i < spec_.patterns.size(); ++i)
                {
                    if(spec_.patterns[i].name.toStdString() == id)
                    {
                        found = (int)i;
                        break;
                    }
                }
            }
            if(found >= 0)
            {
                pattern_index = std::clamp(found, 0, pattern_count - 1);
            }
        }
        else if(pattern_value.is_number_integer())
        {
            pattern_index = std::clamp(pattern_value.get<int>(), 0, pattern_count - 1);
        }
    }
    if(pattern_combo)
    {
        pattern_combo->setCurrentIndex(pattern_index);
    }
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        const std::string& key = spec_.combos[i].key;
        if(!settings.contains(key) || !settings[key].is_number_integer())
        {
            continue;
        }
        const int count = std::max(1, (int)spec_.combos[i].options.size());
        combo_values[i] = std::clamp(settings[key].get<int>(), 0, count - 1);
    }
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        const std::string& key = spec_.sliders[i].key;
        if(!settings.contains(key) || !settings[key].is_number())
        {
            continue;
        }
        int stored = 0;
        if(spec_.sliders[i].save_unit)
        {
            stored = (int)std::lround(settings[key].get<double>() * 100.0);
        }
        else
        {
            stored = (int)std::lround(settings[key].get<double>());
        }
        slider_values[i] = std::clamp(stored, spec_.sliders[i].min, spec_.sliders[i].max);
    }
    if(spec_.uses_audio)
    {
        AudioReactiveLoadFromJson(audio_settings, settings);
        audio_smoothed = 0.0f;
        audio_last_intensity_time = std::numeric_limits<float>::lowest();
        audio_band_lo = 0.0f;
        audio_band_mid = 0.0f;
        audio_band_hi = 0.0f;
        audio_time_boost = 0.0f;
        audio_band_count_cached = 0.0f;
        audio_smoothed_bands.clear();
        audio_spectrogram_history.clear();
        audio_column_smoothed.clear();
        audio_spectrogram_write_index = 0;
        audio_last_spectrogram_push = std::numeric_limits<float>::lowest();
        audio_pulses.clear();
        audio_pulse_trigger = {};
        audio_last_pulse_tick_time = std::numeric_limits<float>::lowest();
        audio_next_pulse_color_slot = 0u;
        audio_packed_pulse_count = 0;
        std::fill(std::begin(audio_packed_color_slots), std::end(audio_packed_color_slots), 0u);
        audio_note_count = 0;
        audio_hi_drive_cached = 0.0f;
        std::fill(std::begin(audio_note_hues), std::end(audio_note_hues), 0.0f);
        std::fill(std::begin(audio_note_amps), std::end(audio_note_amps), 0.0f);
        AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    }
    if(spec_.uses_media)
    {
        if(settings.contains("media_path") && settings["media_path"].is_string())
        {
            LoadMediaFile(QString::fromStdString(settings["media_path"].get<std::string>()));
        }
        if(settings.contains("tile_repeat_enabled") && settings["tile_repeat_enabled"].is_boolean())
        {
            tile_repeat_enabled = settings["tile_repeat_enabled"].get<bool>();
            if(tile_repeat_check)
            {
                tile_repeat_check->setChecked(tile_repeat_enabled);
            }
        }
    }
}
