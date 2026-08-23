// SPDX-License-Identifier: GPL-2.0-only

#include "TextureProjection.h"

#include "Geometry3DUtils.h"
#include "MediaTextureEffectUtils.h"
#include "SpatialLayerCore.h"
#include "TextureProjectionVolumeFieldGlsl.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QMovie>
#include <QMutexLocker>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "MediaTextureAmbienceBlock.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TextureProjection::TextureProjection(QWidget* parent)
    : SpatialEffect3D(parent),
      browse_button(nullptr),
      path_label(nullptr),
      projection_combo(nullptr),
      ambience_dist_slider(nullptr),
      ambience_curve_slider(nullptr),
      ambience_edge_slider(nullptr),
      ambience_prop_slider(nullptr),
      motion_scroll_slider(nullptr),
      motion_warp_slider(nullptr),
      motion_phase_slider(nullptr),
      media_resolution_slider(nullptr),
      tile_repeat_check(nullptr),
      movie(nullptr),
      gif_frame_timer(nullptr),
      media_is_gif(false),
      projection_mode(0)
{
    gif_frame_timer = new QTimer(this);
    gif_frame_timer->setTimerType(Qt::PreciseTimer);
    connect(gif_frame_timer, &QTimer::timeout, this, &TextureProjection::OnGifFrameTimerTimeout);
    SetRainbowMode(false);
    SetSpeed(30);
    volume_assist_.setFragmentBody(QString::fromUtf8(TextureProjectionVolumeFieldGlsl()));
    volume_assist_.setResolution(18);
}

TextureProjection::~TextureProjection()
{
    ClearMovie();
}

EffectInfo3D TextureProjection::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Texture projection";
    info.effect_description =
        "Map an image or GIF onto your 3D layout (planar or spherical). "
        "Scroll pans the texture in a continuous loop; Warp distorts UVs; Phase steers scroll direction and warp tempo. "
        "Tile repeats the image across the room; motion still loops when Tile is off. "
        "Ambience: distance dim, falloff curve, edge fade, and wave delay (motion lags with distance). "
        "For GIFs, Speed is frames per second (0 = frozen). Size zooms; Scale adds repeats.";
    info.category = "Media";
    info.effect_type = SPATIAL_EFFECT_TEXTURE_PROJECTION;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 10.0f;
    info.use_size_parameter = true;
    info.default_frequency_scale = 14.0f;
    info.default_detail_scale = 12.0f;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = false;
    info.supports_height_bands = true;

    return info;
}

void TextureProjection::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto int_format = [](int v) { return QString::number(v); };

    auto* pick_row = new QHBoxLayout();
    pick_row->setContentsMargins(0, 0, 0, 0);
    browse_button = new QPushButton(tr("Choose image / GIF…"), w);
    browse_button->setObjectName(QStringLiteral("browseButton"));
    path_label = new QLabel(tr("(no file)"), w);
    path_label->setObjectName(QStringLiteral("pathLabel"));
    path_label->setWordWrap(true);
    path_label->setMinimumWidth(120);
    pick_row->addWidget(browse_button);
    pick_row->addWidget(path_label);
    layout->addLayout(pick_row);

    EffectLabeledComboRow* projection_row = EffectUiRows::AppendComboRow(layout, tr("Projection:"));
    projection_row->setObjectName(QStringLiteral("projectionRow"));
    projection_combo = projection_row->combo();
    projection_combo->addItem(tr("Planar: floor (X × Z)"));
    projection_combo->addItem(tr("Planar: wall X–Y"));
    projection_combo->addItem(tr("Planar: wall Y–Z"));
    projection_combo->addItem(tr("Sphere around effect origin"));
    projection_combo->setCurrentIndex(projection_mode);
    projection_combo->setToolTip(tr(
        "UV mapping: planar modes use the active grid bounds on two axes; sphere uses direction from the effect origin."));
    connect(projection_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TextureProjection::OnProjectionModeChanged);

    const auto bind_u100 = [&](EffectSliderRow* row, const QString& caption, const QString& tip,
                               unsigned int& field, QSlider*& slider_out) {
        row->setCaptionText(caption);
        row->configure(0, 100, (int)field, tip);
        slider_out = row->slider();
        row->bindValueChanged(
            this,
            [&field](int v) { field = (unsigned int)std::clamp(v, 0, 100); },
            int_format,
            on_changed);
    };
    auto* media = new MediaTextureAmbienceBlock(w);
    media->setObjectName(QStringLiteral("mediaBlock"));
    layout->addWidget(media);
    bind_u100(media->ambienceDistRow(), tr("Distance dim:"),
              tr("Darkens LEDs farther from the effect origin. Mid values already show a clear vignette."),
              ambience_dist_falloff, ambience_dist_slider);
    bind_u100(media->ambienceCurveRow(), tr("Falloff curve:"),
              tr("Shapes how fast the vignette drops (soft → hard). Also adds dimming on its own."),
              ambience_falloff_curve, ambience_curve_slider);
    bind_u100(media->ambienceEdgeRow(), tr("Edge fade:"),
              tr("Fades toward room walls/floor/ceiling. Higher = stronger border fade."),
              ambience_edge_soft, ambience_edge_slider);
    bind_u100(media->ambiencePropRow(), tr("Wave delay:"),
              tr("Motion lags farther from the origin — scroll/warp travel as a wave across the room."),
              ambience_propagation, ambience_prop_slider);

    const auto bind_motion = [&](EffectSliderRow* row, const QString& caption, const QString& tip,
                                 unsigned int& field, QSlider*& slider_out) {
        row->setCaptionText(caption);
        row->configure(0, 200, (int)field, tip);
        slider_out = row->slider();
        row->bindValueChanged(
            this,
            [&field](int v) { field = (unsigned int)std::clamp(v, 0, 200); },
            int_format,
            on_changed);
    };
    bind_motion(media->motionScrollRow(), tr("Scroll:"),
                tr("Pans the texture continuously (loops like a conveyor). Works without Tile. 0 = still."),
                motion_scroll, motion_scroll_slider);
    bind_motion(media->motionWarpRow(), tr("Warp:"),
                tr("Waves the UV — visible distortion at mid values. 0 = off."),
                motion_warp, motion_warp_slider);
    bind_motion(media->motionPhaseRow(), tr("Phase:"),
                tr("Steers scroll into V and speeds warp pulsing. Raise this to see diagonal drift + living warp."),
                motion_phase, motion_phase_slider);

    tile_repeat_check = media->tileRepeatCheck();
    tile_repeat_check->setChecked(tile_repeat_enabled);
    tile_repeat_check->setToolTip(tr(
        "On = tile/repeat the image across the projection. Off = one copy (still loops while Scroll/Warp moves)."));
    connect(tile_repeat_check, &QCheckBox::toggled, this, [this](bool on) {
        tile_repeat_enabled = on;
        emit ParametersChanged();
    });

    media->mediaResolutionRow()->setCaptionText(tr("Resolution:"));
    media->mediaResolutionRow()->configure(
        0, 100, (int)media_resolution,
        tr("Per-layer sampling (0 = blocky, 100 = full). Multiplied with global Sampling under Output shaping."));
    media_resolution_slider = media->mediaResolutionRow()->slider();
    media->mediaResolutionRow()->bindValueChanged(
        this,
        [this](int v) { media_resolution = (unsigned int)std::clamp(v, 0, 100); },
        int_format,
        on_changed);

    AddWidgetToParent(w, parent);
    connect(browse_button, &QPushButton::clicked, this, &TextureProjection::OnBrowseMedia);
}

void TextureProjection::OnProjectionModeChanged(int index)
{
    projection_mode = std::max(0, std::min(3, index));
    emit ParametersChanged();
}

void TextureProjection::OnMediaFrameChanged(int /*frameNumber*/)
{
    RefreshFrameFromMovie();
}

void TextureProjection::OnGifFrameTimerTimeout()
{
    if(!movie || !media_is_gif)
    {
        return;
    }
    const int fc = movie->frameCount();
    if(fc <= 0)
    {
        if(gif_frame_timer)
        {
            gif_frame_timer->stop();
        }
        return;
    }
    const int cur = movie->currentFrameNumber();
    const int next = (cur + 1) % fc;
    {
        QMutexLocker lock(&display_mutex);
        previous_display_frame = display_frame;
        last_gif_step_ms = QDateTime::currentMSecsSinceEpoch();
    }
    (void)movie->jumpToFrame(next);
    PublishDisplayFrame(movie->currentImage());
}

void TextureProjection::ClearMovie()
{
    if(gif_frame_timer)
    {
        gif_frame_timer->stop();
    }
    if(movie)
    {
        movie->stop();
        disconnect(movie, nullptr, this, nullptr);
        delete movie;
        movie = nullptr;
    }
    media_is_gif = false;
    last_gif_step_ms = 0;
    gif_step_interval_ms = 0;
    QMutexLocker lock(&display_mutex);
    previous_display_frame.reset();
}

void TextureProjection::PublishDisplayFrame(const QImage& src)
{
    if(src.isNull())
    {
        QMutexLocker lock(&display_mutex);
        previous_display_frame.reset();
        display_frame.reset();
        return;
    }
    QImage conv = src.convertToFormat(QImage::Format_ARGB32);
    constexpr int kMaxSampleEdge = 1536;
    if(conv.width() > kMaxSampleEdge || conv.height() > kMaxSampleEdge)
    {
        conv = conv.scaled(kMaxSampleEdge, kMaxSampleEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        conv = conv.convertToFormat(QImage::Format_ARGB32);
    }
    std::shared_ptr<QImage> shot = std::make_shared<QImage>(std::move(conv));
    QMutexLocker lock(&display_mutex);
    display_frame = std::move(shot);
}

void TextureProjection::RefreshFrameFromMovie()
{
    if(!movie)
    {
        return;
    }
    PublishDisplayFrame(movie->currentImage());
}

void TextureProjection::LoadMediaFile(const QString& path)
{
    media_path = path;
    if(path_label)
    {
        path_label->setText(path.isEmpty() ? tr("(no file)") : path);
    }

    ClearMovie();
    {
        QMutexLocker lock(&display_mutex);
        previous_display_frame.reset();
        display_frame.reset();
    }

    if(path.isEmpty())
    {
        emit ParametersChanged();
        return;
    }

    const bool is_gif = path.endsWith(QLatin1String(".gif"), Qt::CaseInsensitive);
    if(is_gif)
    {
        movie = new QMovie(path, QByteArray(), this);
        if(!movie->isValid())
        {
            delete movie;
            movie = nullptr;
            if(path_label)
            {
                path_label->setText(tr("Invalid or unsupported GIF"));
            }
            emit ParametersChanged();
            return;
        }
        media_is_gif = true;
        movie->start();
        movie->setPaused(true);
        (void)movie->jumpToFrame(0);
        PublishDisplayFrame(movie->currentImage());
        {
            QMutexLocker lock(&display_mutex);
            previous_display_frame.reset();
        }
        last_gif_step_ms = 0;
        ApplyGifPlaybackSpeed();
    }
    else
    {
        QImage img(path);
        if(img.isNull())
        {
            emit ParametersChanged();
            return;
        }
        PublishDisplayFrame(img);
    }

    emit ParametersChanged();
}

void TextureProjection::SetSpeed(unsigned int speed)
{
    SpatialEffect3D::SetSpeed(speed);
    ApplyGifPlaybackSpeed();
}

void TextureProjection::ApplyGifPlaybackSpeed()
{
    if(!movie || !media_is_gif || !gif_frame_timer)
    {
        if(gif_frame_timer)
        {
            gif_frame_timer->stop();
        }
        return;
    }
    const unsigned int fps = GetSpeed();
    if(fps == 0)
    {
        gif_frame_timer->stop();
        movie->setPaused(true);
        gif_step_interval_ms = 0;
        return;
    }
    movie->setPaused(true);
    const int interval_ms = std::max(1, (int)std::lround(1000.0 / (double)fps));
    gif_frame_timer->stop();
    gif_frame_timer->setInterval(interval_ms);
    gif_step_interval_ms = interval_ms;
    gif_frame_timer->start();
}

void TextureProjection::OnBrowseMedia()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open image or GIF"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp);;GIF (*.gif);;All files (*.*)"));
    if(path.isEmpty())
    {
        return;
    }
    LoadMediaFile(path);
}

void TextureProjection::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
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
        float zp[16] = {};
        volume_assist_.prepare(render_sequence, time_sec, zp, 16);
        return;
    }

    QImage media = *snap;
    // Cap before optional GIF blend so per-frame work stays cheap.
    constexpr int kGpuMediaEdge = SpatialVolumeFieldEngine::kMaxMediaEdge;
    if(media.width() > kGpuMediaEdge || media.height() > kGpuMediaEdge)
    {
        media = media.scaled(kGpuMediaEdge, kGpuMediaEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

    const Vector3D origin = GetEffectOriginGrid(grid);
    float ox, oy, oz;
    PackEffectOrigin01(grid, origin, &ox, &oy, &oz);

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float tm = std::max(0.25f, bb.tight_mul);

    const bool freeze_gif_motion = media_is_gif && GetSpeed() == 0;
    const float scroll_mul = motion_scroll / 100.0f;
    const float warp_mul = motion_warp / 100.0f;
    const float phase_mul = motion_phase / 100.0f;
    const float speed_lin = std::clamp(GetSpeed() / 100.0f, 0.0f, 1.0f);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float detail_s = detail * tm;
    const float freq_n = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);
    /* Scroll rate in UV/sec — Scroll slider dominates; Speed/Frequency boost. */
    const float scroll_rate =
        freeze_gif_motion
            ? 0.0f
            : scroll_mul * (0.22f + 0.48f * speed_lin + 0.18f * freq_n) * bb.speed_mul;
    const float size_m = std::max(0.08f, GetNormalizedSize());
    const float repeat_from_scale = 0.35f + 1.75f * GetNormalizedScale();
    const float size_zoom_div = std::clamp(0.40f + 0.36f * size_m, 0.32f, 2.2f);
    const float tile = std::clamp(repeat_from_scale / size_zoom_div, 0.12f, 6.5f);
    const float amp =
        freeze_gif_motion ? 0.0f
                          : warp_mul * (0.045f + 0.20f * std::min(1.0f, detail * 0.12f)) / tm;
    const float prop01 = ambience_propagation / 100.0f;

    const unsigned int eff_res = CombineMediaSampling(media_resolution);
    const float q = eff_res / 100.0f;
    const float steps_u = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, media.width()) - 4));
    const float steps_v = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, media.height()) - 4));
    const float packed_v = steps_v + ((eff_res < 100u) ? 1000.0f : 0.0f);

    float vp[16] = {
        (float)std::clamp(projection_mode, 0, 3),
        tile,
        scroll_rate,
        phase_mul,
        amp,
        detail_s,
        ox,
        oy,
        oz,
        ambience_dist_falloff / 100.0f,
        ambience_falloff_curve / 100.0f,
        ambience_edge_soft / 100.0f,
        prop01,
        steps_u,
        packed_v,
        tile_repeat_enabled ? 1.0f : 0.0f
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 16);
}

RGBColor TextureProjection::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)time;
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    if(volume_assist_.isAvailable())
    {
        const float nx = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
        const float ny = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
        const float nz = NormalizeGridAxis01(z, grid.min_z, grid.max_z);
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        return ToRGBColor((int)(std::clamp(samp.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.z(), 0.0f, 1.0f) * 255.0f + 0.5f));
    }
    return 0x00000000;
}

nlohmann::json TextureProjection::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["media_path"] = media_path.toStdString();
    j["projection_mode"] = projection_mode;
    j["ambience_dist_falloff"] = ambience_dist_falloff;
    j["ambience_falloff_curve"] = ambience_falloff_curve;
    j["ambience_edge_soft"] = ambience_edge_soft;
    j["ambience_propagation"] = ambience_propagation;
    j["motion_scroll"] = motion_scroll;
    j["motion_warp"] = motion_warp;
    j["motion_phase"] = motion_phase;
    j["media_resolution"] = media_resolution;
    j["tile_repeat_enabled"] = tile_repeat_enabled;
    return j;
}

void TextureProjection::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("projection_mode") && settings["projection_mode"].is_number_integer())
    {
        projection_mode = std::max(0, std::min(3, settings["projection_mode"].get<int>()));
        if(projection_combo)
        {
            projection_combo->setCurrentIndex(projection_mode);
        }
    }
    if(settings.contains("media_path") && settings["media_path"].is_string())
    {
        const QString p = QString::fromStdString(settings["media_path"].get<std::string>());
        LoadMediaFile(p);
    }
    auto load_u = [&](const char* key, unsigned int& out, QSlider* sl) {
        if(settings.contains(key) && settings[key].is_number_integer())
        {
            out = (unsigned int)std::clamp(settings[key].get<int>(), 0, 100);
            if(sl)
                sl->setValue((int)out);
        }
    };
    load_u("ambience_dist_falloff", ambience_dist_falloff, ambience_dist_slider);
    load_u("ambience_falloff_curve", ambience_falloff_curve, ambience_curve_slider);
    load_u("ambience_edge_soft", ambience_edge_soft, ambience_edge_slider);
    load_u("ambience_propagation", ambience_propagation, ambience_prop_slider);
    auto load_motion = [&](const char* key, unsigned int& out, QSlider* sl) {
        if(settings.contains(key) && settings[key].is_number_integer())
        {
            out = (unsigned int)std::clamp(settings[key].get<int>(), 0, 200);
            if(sl)
                sl->setValue((int)out);
        }
    };
    load_motion("motion_scroll", motion_scroll, motion_scroll_slider);
    load_motion("motion_warp", motion_warp, motion_warp_slider);
    load_motion("motion_phase", motion_phase, motion_phase_slider);
    if(settings.contains("media_resolution") && settings["media_resolution"].is_number_integer())
    {
        media_resolution = (unsigned int)std::clamp(settings["media_resolution"].get<int>(), 0, 100);
        if(media_resolution_slider)
            media_resolution_slider->setValue((int)media_resolution);
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

REGISTER_EFFECT_3D(TextureProjection);
