// SPDX-License-Identifier: GPL-2.0-only

#include "TextureProjection.h"

#include "Geometry3DUtils.h"
#include "MediaTextureEffectUtils.h"
#include "SpatialLayerCore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QMovie>
#include <QMutexLocker>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TextureProjection::TextureProjection(QWidget* parent)
    : SpatialEffect3D(parent),
      browse_button(nullptr),
      path_label(nullptr),
      projection_combo(nullptr),
      ambience_dist_slider(nullptr),
      ambience_dist_label(nullptr),
      ambience_curve_slider(nullptr),
      ambience_curve_label(nullptr),
      ambience_edge_slider(nullptr),
      ambience_edge_label(nullptr),
      ambience_prop_slider(nullptr),
      ambience_prop_label(nullptr),
      motion_scroll_slider(nullptr),
      motion_scroll_label(nullptr),
      motion_warp_slider(nullptr),
      motion_warp_label(nullptr),
      motion_phase_slider(nullptr),
      motion_phase_label(nullptr),
      media_resolution_slider(nullptr),
      media_resolution_label(nullptr),
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
}

TextureProjection::~TextureProjection()
{
    ClearMovie();
}

EffectInfo3D TextureProjection::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Texture projection";
    info.effect_description =
        "Map an image or GIF onto your 3D layout using planar or spherical UVs. For GIFs, Speed is frames per second: "
        "0 = frozen, 1 = 1 FPS, 2 = 2 FPS, … up to 200 FPS. "
        "Size zooms the texture (larger Size = bigger features, fewer repeats). Scale still adds overall repeat. "
        "Detail warps UV; Frequency drives scroll and warp phase. Motion tuning adds Scroll / Warp / Phase plus media Resolution "
        "(combined with global Output shaping → Sampling); use Smoothing there for GIF frame blending. "
        "Ambience sliders: distance dim, falloff curve, edge fade (room bounds), wave delay (phase vs distance). "
        "Stratum bands (Y across the room after rotation) blend speed, tightness, and phase for motion and UV warp.";
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
    QWidget* outer = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(outer);
    vbox->setContentsMargins(0, 0, 0, 0);
    QWidget* w = new QWidget();
    vbox->addWidget(w);
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    int row = 0;
    browse_button = new QPushButton(tr("Choose image / GIF…"));
    layout->addWidget(browse_button, row, 0, 1, 1);
    path_label = new QLabel(tr("(no file)"));
    path_label->setWordWrap(true);
    path_label->setMinimumWidth(120);
    layout->addWidget(path_label, row, 1, 1, 2);
    row++;

    layout->addWidget(new QLabel(tr("Projection:")), row, 0);
    projection_combo = new QComboBox();
    projection_combo->addItem(tr("Planar: floor (X × Z)"));
    projection_combo->addItem(tr("Planar: wall X–Y"));
    projection_combo->addItem(tr("Planar: wall Y–Z"));
    projection_combo->addItem(tr("Sphere around effect origin"));
    projection_combo->setCurrentIndex(projection_mode);
    projection_combo->setToolTip(
        tr("UV mapping: planar modes use the active grid bounds on two axes; sphere uses direction from the effect origin."));
    layout->addWidget(projection_combo, row, 1, 1, 2);
    row++;

    QGroupBox* amb = new QGroupBox(tr("Ambience (Screen Mirror–style)"));
    QGridLayout* ag = new QGridLayout(amb);
    ag->setContentsMargins(4, 8, 4, 4);
    int ar = 0;
    std::function<void(const QString&, QSlider*&, QLabel*&, int, const QString&)> add_amb_row =
        [&](const QString& cap, QSlider*& sl, QLabel*& lab, int val, const QString& tip) {
        ag->addWidget(new QLabel(cap), ar, 0);
        sl = new QSlider(Qt::Horizontal);
        sl->setRange(0, 100);
        sl->setValue(val);
        sl->setToolTip(tip);
        ag->addWidget(sl, ar, 1);
        lab = new QLabel(QString::number(val));
        lab->setMinimumWidth(26);
        ag->addWidget(lab, ar, 2);
        connect(sl, &QSlider::valueChanged, this, &TextureProjection::OnAmbienceChanged);
        ar++;
    };
    add_amb_row(tr("Distance dim:"), ambience_dist_slider, ambience_dist_label, (int)ambience_dist_falloff,
                tr("Darken LEDs farther from the effect origin (radial falloff)."));
    add_amb_row(tr("Falloff curve:"), ambience_curve_slider, ambience_curve_label, (int)ambience_falloff_curve,
                tr("Shapes radial falloff (exponent). Still affects the map when Distance dim is 0; raise both for a strong vignette."));
    add_amb_row(tr("Edge fade:"), ambience_edge_slider, ambience_edge_label, (int)ambience_edge_soft,
                tr("Strong fade toward the active grid bounds (reaches further in at high values)."));
    add_amb_row(tr("Wave delay:"), ambience_prop_slider, ambience_prop_label, (int)ambience_propagation,
                tr("Strong propagation: UV scroll / warp phase lags more with distance from the origin."));
    layout->addWidget(amb, row, 0, 1, 3);
    row++;

    QGroupBox* motion = new QGroupBox(tr("Motion tuning"));
    QGridLayout* mg = new QGridLayout(motion);
    mg->setContentsMargins(4, 8, 4, 4);
    int mr = 0;
    std::function<void(const QString&, QSlider*&, QLabel*&, int, const QString&)> add_motion_row =
        [&](const QString& cap, QSlider*& sl, QLabel*& lab, int val, const QString& tip) {
        mg->addWidget(new QLabel(cap), mr, 0);
        sl = new QSlider(Qt::Horizontal);
        sl->setRange(0, 200);
        sl->setValue(val);
        sl->setToolTip(tip);
        mg->addWidget(sl, mr, 1);
        lab = new QLabel(QString::number(val));
        lab->setMinimumWidth(26);
        mg->addWidget(lab, mr, 2);
        connect(sl, &QSlider::valueChanged, this, &TextureProjection::OnMotionTuningChanged);
        mr++;
    };
    add_motion_row(tr("Scroll:"), motion_scroll_slider, motion_scroll_label, (int)motion_scroll,
                   tr("UV scroll intensity. 0 = off."));
    add_motion_row(tr("Warp:"), motion_warp_slider, motion_warp_label, (int)motion_warp,
                   tr("UV distortion amount. 0 = off."));
    add_motion_row(tr("Phase:"), motion_phase_slider, motion_phase_label, (int)motion_phase,
                   tr("Temporal phase rate for warp oscillation. 0 = off."));
    layout->addWidget(motion, row, 0, 1, 3);
    row++;

    QGroupBox* media_tuning = new QGroupBox(tr("Media sampling"));
    QGridLayout* tg = new QGridLayout(media_tuning);
    tg->setContentsMargins(4, 8, 4, 4);
    tile_repeat_check = new QCheckBox(tr("Tile / repeat outside image bounds"));
    tile_repeat_check->setChecked(tile_repeat_enabled);
    tile_repeat_check->setToolTip(tr("Off = single projected image (edge clamp). On = wrap/tile like a texture."));
    tg->addWidget(tile_repeat_check, 0, 0, 1, 3);
    tg->addWidget(new QLabel(tr("Resolution:")), 1, 0);
    media_resolution_slider = new QSlider(Qt::Horizontal);
    media_resolution_slider->setRange(0, 100);
    media_resolution_slider->setValue((int)media_resolution);
    media_resolution_slider->setToolTip(
        tr("Per-layer sampling (0 = blocky, 100 = full). Multiplied with global Sampling under Output shaping."));
    tg->addWidget(media_resolution_slider, 1, 1);
    media_resolution_label = new QLabel(QString::number((int)media_resolution));
    media_resolution_label->setMinimumWidth(26);
    tg->addWidget(media_resolution_label, 1, 2);
    connect(media_resolution_slider, &QSlider::valueChanged, this, &TextureProjection::OnMediaTuningChanged);
    connect(tile_repeat_check, &QCheckBox::toggled, this, &TextureProjection::OnMediaTuningChanged);
    layout->addWidget(media_tuning, row, 0, 1, 3);
    row++;

    AttachRoomMappingPanel(parent);

    AddWidgetToParent(outer, parent);

    connect(browse_button, &QPushButton::clicked, this, &TextureProjection::OnBrowseMedia);
    connect(projection_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TextureProjection::OnProjectionModeChanged);
}

void TextureProjection::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_TEXTURE_PROJECTION;
}

void TextureProjection::OnProjectionModeChanged(int index)
{
    projection_mode = std::max(0, std::min(3, index));
    emit ParametersChanged();
}

void TextureProjection::OnAmbienceChanged()
{
    if(ambience_dist_slider)
    {
        ambience_dist_falloff = (unsigned int)std::clamp(ambience_dist_slider->value(), 0, 100);
        if(ambience_dist_label) ambience_dist_label->setText(QString::number((int)ambience_dist_falloff));
    }
    if(ambience_curve_slider)
    {
        ambience_falloff_curve = (unsigned int)std::clamp(ambience_curve_slider->value(), 0, 100);
        if(ambience_curve_label) ambience_curve_label->setText(QString::number((int)ambience_falloff_curve));
    }
    if(ambience_edge_slider)
    {
        ambience_edge_soft = (unsigned int)std::clamp(ambience_edge_slider->value(), 0, 100);
        if(ambience_edge_label) ambience_edge_label->setText(QString::number((int)ambience_edge_soft));
    }
    if(ambience_prop_slider)
    {
        ambience_propagation = (unsigned int)std::clamp(ambience_prop_slider->value(), 0, 100);
        if(ambience_prop_label) ambience_prop_label->setText(QString::number((int)ambience_propagation));
    }
    emit ParametersChanged();
}

void TextureProjection::OnMotionTuningChanged()
{
    if(motion_scroll_slider)
    {
        motion_scroll = (unsigned int)std::clamp(motion_scroll_slider->value(), 0, 200);
        if(motion_scroll_label) motion_scroll_label->setText(QString::number((int)motion_scroll));
    }
    if(motion_warp_slider)
    {
        motion_warp = (unsigned int)std::clamp(motion_warp_slider->value(), 0, 200);
        if(motion_warp_label) motion_warp_label->setText(QString::number((int)motion_warp));
    }
    if(motion_phase_slider)
    {
        motion_phase = (unsigned int)std::clamp(motion_phase_slider->value(), 0, 200);
        if(motion_phase_label) motion_phase_label->setText(QString::number((int)motion_phase));
    }
    emit ParametersChanged();
}

void TextureProjection::OnMediaTuningChanged()
{
    if(media_resolution_slider)
    {
        media_resolution = (unsigned int)std::clamp(media_resolution_slider->value(), 0, 100);
        if(media_resolution_label)
        {
            media_resolution_label->setText(QString::number((int)media_resolution));
        }
    }
    if(tile_repeat_check)
    {
        tile_repeat_enabled = tile_repeat_check->isChecked();
    }
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

RGBColor TextureProjection::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    const Vector3D origin_bb = GetEffectOriginGrid(grid);
    Vector3D rp_bb = TransformPointByRotation(x, y, z, origin_bb);
    float coord2_bb = NormalizeGridAxis01(rp_bb.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st_bb;
    EffectStratumBlend::InitStratumBreaks(strat_st_bb);
    float sw_bb[3];
    EffectStratumBlend::WeightsForYNorm(coord2_bb, strat_st_bb, sw_bb);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw_bb, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw_bb, grid, x, y, z, origin_bb, time);

    const float tm = std::max(0.25f, bb.tight_mul);

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
        return 0x00000000;
    }
    const QImage& frame = *snap;

    const float min_x = grid.min_x;
    const float max_x = grid.max_x;
    const float min_y = grid.min_y;
    const float max_y = grid.max_y;
    const float min_z = grid.min_z;
    const float max_z = grid.max_z;
    const float inv_w = 1.0f / std::max(1e-4f, max_x - min_x);
    const float inv_h = 1.0f / std::max(1e-4f, max_y - min_y);
    const float inv_d = 1.0f / std::max(1e-4f, max_z - min_z);

    const Vector3D o = origin_bb;
    const float ox = x - o.x;
    const float oy = y - o.y;
    const float oz = z - o.z;
    const float dist_o = std::sqrt(ox * ox + oy * oy + oz * oz);
    const float d_face = std::min(
        { (x - min_x) * inv_w, (max_x - x) * inv_w, (y - min_y) * inv_h, (max_y - y) * inv_h,
          (z - min_z) * inv_d, (max_z - z) * inv_d });
    const float max_r = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    const float prop01 = ambience_propagation / 100.0f;
    const float t_anim =
        time * bb.speed_mul
        - prop01 * std::min(1.0f, dist_o / std::max(max_r, 1e-4f)) * 12.0f;
    const bool freeze_gif_motion = media_is_gif && GetSpeed() == 0;
    const float anim_time = freeze_gif_motion ? 0.0f : t_anim;
    const float scroll_mul = motion_scroll / 100.0f;
    const float warp_mul = motion_warp / 100.0f;
    const float phase_mul = motion_phase / 100.0f;

    const float speed_lin = std::clamp(GetSpeed() / 100.0f, 0.0f, 1.0f);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float detail_s = detail * tm;
    const float freq = std::min(6.0f, std::max(0.02f, GetScaledFrequency() * 0.065f));
    const float scroll = anim_time * (0.022f + 0.07f * speed_lin + 0.09f * freq) * scroll_mul;
    
    const float size_m = std::max(0.08f, GetNormalizedSize());
    const float repeat_from_scale = 0.35f + 1.75f * GetNormalizedScale();
    const float size_zoom_div = std::clamp(0.40f + 0.36f * size_m, 0.32f, 2.2f);
    const float tile = std::clamp(repeat_from_scale / size_zoom_div, 0.12f, 6.5f);

    float u = 0.5f;
    float v = 0.5f;

    switch(projection_mode)
    {
    case 0:
        u = (x - min_x) * inv_w;
        v = (z - min_z) * inv_d;
        break;
    case 1:
        u = (x - min_x) * inv_w;
        v = (y - min_y) * inv_h;
        break;
    case 2:
        u = (y - min_y) * inv_h;
        v = (z - min_z) * inv_d;
        break;
    default:
    {
        float rx = x - o.x;
        float ry = y - o.y;
        float rz = z - o.z;
        const float len = std::sqrt(rx * rx + ry * ry + rz * rz);
        if(len < 1e-4f)
        {
            u = 0.5f;
            v = 0.5f;
        }
        else
        {
            rx /= len;
            ry /= len;
            rz /= len;
            u = std::atan2(rz, rx) / (float)(2.0 * M_PI) + 0.5f;
            v = std::asin(std::clamp(ry, -1.0f, 1.0f)) / (float)M_PI + 0.5f;
        }
        break;
    }
    }

    u = (u - 0.5f) * tile + 0.5f;
    v = (v - 0.5f) * tile + 0.5f;
    u += scroll;
    v += scroll * 0.37f;

    const float phase =
        anim_time * freq * 0.14f * phase_mul + EffectStratumBlend::ApplyMotionToAngleRad(EffectStratumBlend::PhaseShiftRad(bb), stratum_mot01);
    const float amp = 0.022f * std::min(3.2f, detail * 0.22f) * warp_mul / tm;
    u += std::sin(phase + u * 11.5f * detail_s * 0.08f + v * 7.0f * detail_s * 0.07f) * amp;
    v += std::cos(phase * 0.91f + u * 8.5f * detail_s * 0.07f - v * 10.0f * detail_s * 0.08f) * amp;

    if(tile_repeat_enabled)
    {
        u = MediaTextureEffect::Frac01(u);
        v = MediaTextureEffect::Frac01(v);
    }
    else
    {
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
    }

    float su = u;
    float sv = v;
    const unsigned int eff_res = CombineMediaSampling(media_resolution);
    if(eff_res < 100u)
    {
        Geometry3D::QuantizeMediaUV01(su, sv, frame.width(), frame.height(), eff_res);
    }

    RGBColor col = MediaTextureEffect::SampleImageBilinear(frame, su, sv);
    const float smoothing = GetSmoothing() / 100.0f;
    if(media_is_gif && prev_snap && !prev_snap->isNull() && smoothing > 0.0f && step_interval_ms > 0 && step_ms > 0)
    {
        const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        const float elapsed_ms = (float)std::max<qint64>(0, now_ms - step_ms);
        const float blend_window_ms = std::max(1.0f, (float)step_interval_ms * smoothing);
        const float a = std::clamp(elapsed_ms / blend_window_ms, 0.0f, 1.0f);
        const RGBColor prev_col = MediaTextureEffect::SampleImageBilinear(*prev_snap, su, sv);
        col = MediaTextureEffect::LerpRGB(prev_col, col, a);
    }
    col = RemapSaturatedRgbWithRoomMapping(col, x, y, z, time, grid);
    const float ag = MediaTextureEffect::AmbienceGain(dist_o, max_r, d_face, ambience_dist_falloff, ambience_falloff_curve,
                                  ambience_edge_soft);
    if(ag >= 0.999f)
    {
        return col;
    }
    return ToRGBColor((int)(RGBGetRValue(col) * ag + 0.5f), (int)(RGBGetGValue(col) * ag + 0.5f),
                     (int)(RGBGetBValue(col) * ag + 0.5f));
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
    std::function<void(const char*, unsigned int&, QSlider*, QLabel*)> load_u =
        [&](const char* key, unsigned int& out, QSlider* sl, QLabel* lab) {
        if(settings.contains(key) && settings[key].is_number_integer())
        {
            out = (unsigned int)std::clamp(settings[key].get<int>(), 0, 100);
            if(sl)
            {
                sl->setValue((int)out);
            }
            if(lab)
            {
                lab->setText(QString::number((int)out));
            }
        }
    };
    load_u("ambience_dist_falloff", ambience_dist_falloff, ambience_dist_slider, ambience_dist_label);
    load_u("ambience_falloff_curve", ambience_falloff_curve, ambience_curve_slider, ambience_curve_label);
    load_u("ambience_edge_soft", ambience_edge_soft, ambience_edge_slider, ambience_edge_label);
    load_u("ambience_propagation", ambience_propagation, ambience_prop_slider, ambience_prop_label);
    std::function<void(const char*, unsigned int&, QSlider*, QLabel*)> load_motion =
        [&](const char* key, unsigned int& out, QSlider* sl, QLabel* lab) {
        if(settings.contains(key) && settings[key].is_number_integer())
        {
            out = (unsigned int)std::clamp(settings[key].get<int>(), 0, 200);
            if(sl)
            {
                sl->setValue((int)out);
            }
            if(lab)
            {
                lab->setText(QString::number((int)out));
            }
        }
    };
    load_motion("motion_scroll", motion_scroll, motion_scroll_slider, motion_scroll_label);
    load_motion("motion_warp", motion_warp, motion_warp_slider, motion_warp_label);
    load_motion("motion_phase", motion_phase, motion_phase_slider, motion_phase_label);
    if(settings.contains("media_resolution") && settings["media_resolution"].is_number_integer())
    {
        media_resolution = (unsigned int)std::clamp(settings["media_resolution"].get<int>(), 0, 100);
        if(media_resolution_slider)
        {
            media_resolution_slider->setValue((int)media_resolution);
        }
        if(media_resolution_label)
        {
            media_resolution_label->setText(QString::number((int)media_resolution));
        }
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
