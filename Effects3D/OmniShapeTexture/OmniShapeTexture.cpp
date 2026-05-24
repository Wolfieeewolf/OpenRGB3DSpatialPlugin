// SPDX-License-Identifier: GPL-2.0-only

#include "OmniShapeTexture.h"

#include "Geometry3DUtils.h"
#include "MediaTextureEffectUtils.h"
#include "SpatialLayerCore.h"
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

namespace
{

void RotateDir(float& dx, float& dy, float& dz, float yaw, float pitch)
{
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float x1 = cy * dx + sy * dz;
    const float z1 = -sy * dx + cy * dz;
    const float y1 = dy;

    const float cx = std::cos(pitch);
    const float sx = std::sin(pitch);
    dx = x1;
    dy = cx * y1 - sx * z1;
    dz = sx * y1 + cx * z1;
}

void DirToSphereUV(float dx, float dy, float dz, float& u, float& v)
{
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if(len < 1e-6f)
    {
        u = v = 0.5f;
        return;
    }
    dx /= len;
    dy /= len;
    dz /= len;
    u = std::atan2(dz, dx) / (float)(2.0 * M_PI) + 0.5f;
    v = std::asin(std::clamp(dy, -1.0f, 1.0f)) / (float)M_PI + 0.5f;
}

void DirToCubeUV(float dx, float dy, float dz, float& u, float& v)
{
    const float ax = std::fabs(dx);
    const float ay = std::fabs(dy);
    const float az = std::fabs(dz);
    const float m = std::max(ax, std::max(ay, az));
    if(m < 1e-8f)
    {
        u = v = 0.5f;
        return;
    }
    const float inv = 1.0f / m;
    const float px = dx * inv;
    const float py = dy * inv;
    const float pz = dz * inv;

    if(ax >= ay && ax >= az)
    {
        if(dx > 0.0f)
        {
            u = (-pz + 1.0f) * 0.5f;
            v = (py + 1.0f) * 0.5f;
        }
        else
        {
            u = (pz + 1.0f) * 0.5f;
            v = (py + 1.0f) * 0.5f;
        }
    }
    else if(ay >= az)
    {
        if(dy > 0.0f)
        {
            u = (px + 1.0f) * 0.5f;
            v = (-pz + 1.0f) * 0.5f;
        }
        else
        {
            u = (px + 1.0f) * 0.5f;
            v = (pz + 1.0f) * 0.5f;
        }
    }
    else
    {
        if(dz > 0.0f)
        {
            u = (px + 1.0f) * 0.5f;
            v = (py + 1.0f) * 0.5f;
        }
        else
        {
            u = (-px + 1.0f) * 0.5f;
            v = (py + 1.0f) * 0.5f;
        }
    }
}

void DirToOctaUV(float dx, float dy, float dz, float& u, float& v)
{
    const float l = std::fabs(dx) + std::fabs(dy) + std::fabs(dz);
    if(l < 1e-8f)
    {
        u = v = 0.5f;
        return;
    }
    float nx = dx / l;
    float ny = dy / l;
    const float nz = dz / l;
    if(nz < 0.0f)
    {
        const float wx = (1.0f - std::fabs(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
        const float wy = (1.0f - std::fabs(nx)) * (ny >= 0.0f ? 1.0f : -1.0f);
        nx = wx;
        ny = wy;
    }
    u = nx * 0.5f + 0.5f;
    v = ny * 0.5f + 0.5f;
}

void ShapeToUV(int shape, float dx, float dy, float dz, float& u, float& v)
{
    switch(shape % 3)
    {
    case 0:
        DirToSphereUV(dx, dy, dz, u, v);
        break;
    case 1:
        DirToCubeUV(dx, dy, dz, u, v);
        break;
    default:
        DirToOctaUV(dx, dy, dz, u, v);
        break;
    }
}

}

OmniShapeTexture::OmniShapeTexture(QWidget* parent)
    : SpatialEffect3D(parent),
      browse_button(nullptr),
      path_label(nullptr),
      shape_combo(nullptr),
      morph_slider(nullptr),
      spin_slider(nullptr),
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
      base_shape(0),
      morph_percent(0),
      spin_percent(40)
{
    gif_frame_timer = new QTimer(this);
    gif_frame_timer->setTimerType(Qt::PreciseTimer);
    connect(gif_frame_timer, &QTimer::timeout, this, &OmniShapeTexture::OnGifFrameTimerTimeout);
    SetRainbowMode(false);
    SetSpeed(30);
}

OmniShapeTexture::~OmniShapeTexture()
{
    ClearMovie();
}

EffectInfo3D OmniShapeTexture::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Omni shape texture";
    info.effect_description =
        "Image or GIF mapped onto a virtual shape at the effect origin; LEDs sample by outward direction (360°). "
        "For GIFs, Speed is frames per second: 0 = frozen, 1 = 1 FPS, … up to 200 FPS. "
        "Size zooms the texture on the shape; Scale still adds repeat. "
        "Detail warps UV; Frequency drives warp phase. Motion tuning adds Scroll / Warp / Phase plus media Resolution "
        "(combined with global Output shaping → Sampling); use Smoothing there for GIF frame blending. Morph / Spin as before. "
        "Ambience: distance dim, falloff curve, edge fade, wave delay vs distance (multiplies the built-in radial rim). "
        "Stratum bands (Y across the room after rotation) blend speed, tightness, and phase for spin, warp, and radial falloff.";
    info.category = "Media";
    info.effect_type = SPATIAL_EFFECT_OMNI_SHAPE_TEXTURE;
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

    info.default_speed_scale = 12.0f;
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

void OmniShapeTexture::SetupCustomUI(QWidget* parent)
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

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, tr("Shape UV:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    shape_combo = shape_row->combo();
    shape_combo->addItem(tr("Sphere (lat/long)"));
    shape_combo->addItem(tr("Cube (6-face)"));
    shape_combo->addItem(tr("Octahedron (8 triangles)"));
    shape_combo->setCurrentIndex(base_shape);
    shape_combo->setToolTip(tr("Base mapping from direction to texture coordinates."));
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OmniShapeTexture::OnShapeChanged);

    EffectSliderRow* morph_row = EffectUiRows::AppendSliderRow(
        layout,
        tr("Morph:"),
        0,
        100,
        (int)morph_percent,
        tr("Blend toward the next shape in the list: Sphere → Cube → Octahedron → Sphere."));
    morph_row->setObjectName(QStringLiteral("morphRow"));
    morph_slider = morph_row->slider();
    morph_row->bindValueChanged(
        this,
        [this](int v) { morph_percent = (unsigned int)std::clamp(v, 0, 100); },
        int_format,
        on_changed);

    EffectSliderRow* spin_row = EffectUiRows::AppendSliderRow(
        layout, tr("Spin:"), 0, 100, (int)spin_percent,
        tr("How fast the virtual shape mapping rotates (yaw + pitch)."));
    spin_row->setObjectName(QStringLiteral("spinRow"));
    spin_slider = spin_row->slider();
    spin_row->bindValueChanged(
        this,
        [this](int v) { spin_percent = (unsigned int)std::clamp(v, 0, 100); },
        int_format,
        on_changed);

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
              tr("Extra dimming by distance from the effect origin (multiplied with the shape rim falloff)."),
              ambience_dist_falloff, ambience_dist_slider);
    bind_u100(media->ambienceCurveRow(), tr("Falloff curve:"),
              tr("Shapes radial falloff; still has effect when Distance dim is 0. Combine with dim for a strong vignette."),
              ambience_falloff_curve, ambience_curve_slider);
    bind_u100(media->ambienceEdgeRow(), tr("Edge fade:"),
              tr("Strong fade toward the active grid bounds (reaches further in at high values)."),
              ambience_edge_soft, ambience_edge_slider);
    bind_u100(media->ambiencePropRow(), tr("Wave delay:"),
              tr("Strong propagation: spin / warp phase lags more with distance from the origin."),
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
    bind_motion(media->motionScrollRow(), tr("Scroll:"), tr("Shape spin/scroll intensity. 0 = off."),
                motion_scroll, motion_scroll_slider);
    bind_motion(media->motionWarpRow(), tr("Warp:"), tr("UV distortion amount. 0 = off."), motion_warp,
                motion_warp_slider);
    bind_motion(media->motionPhaseRow(), tr("Phase:"), tr("Temporal phase rate for spin + warp oscillation. 0 = off."),
                motion_phase, motion_phase_slider);

    tile_repeat_check = media->tileRepeatCheck();
    tile_repeat_check->setChecked(tile_repeat_enabled);
    tile_repeat_check->setToolTip(tr("Off = single mapped image; On = wrap/tile texture coordinates."));
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
    connect(browse_button, &QPushButton::clicked, this, &OmniShapeTexture::OnBrowseMedia);
}

void OmniShapeTexture::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_OMNI_SHAPE_TEXTURE;
}

void OmniShapeTexture::OnShapeChanged(int index)
{
    base_shape = std::max(0, std::min(2, index));
    emit ParametersChanged();
}

void OmniShapeTexture::OnMediaFrameChanged(int /*frameNumber*/)
{
    RefreshFrameFromMovie();
}

void OmniShapeTexture::OnGifFrameTimerTimeout()
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

void OmniShapeTexture::ClearMovie()
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

void OmniShapeTexture::PublishDisplayFrame(const QImage& src)
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

void OmniShapeTexture::RefreshFrameFromMovie()
{
    if(!movie)
    {
        return;
    }
    PublishDisplayFrame(movie->currentImage());
}

void OmniShapeTexture::LoadMediaFile(const QString& path)
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

void OmniShapeTexture::SetSpeed(unsigned int speed)
{
    SpatialEffect3D::SetSpeed(speed);
    ApplyGifPlaybackSpeed();
}

void OmniShapeTexture::ApplyGifPlaybackSpeed()
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

void OmniShapeTexture::OnBrowseMedia()
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

RGBColor OmniShapeTexture::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    const Vector3D o_strat = GetEffectOriginGrid(grid);
    Vector3D rp_bb = TransformPointByRotation(x, y, z, o_strat);
    float coord2_bb = NormalizeGridAxis01(rp_bb.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st_bb;
    EffectStratumBlend::InitStratumBreaks(strat_st_bb);
    float sw_bb[3];
    EffectStratumBlend::WeightsForYNorm(coord2_bb, strat_st_bb, sw_bb);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw_bb, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw_bb, grid, x, y, z, o_strat, time);

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
    const float d_face = std::min(
        { (x - min_x) * inv_w, (max_x - x) * inv_w, (y - min_y) * inv_h, (max_y - y) * inv_h,
          (z - min_z) * inv_d, (max_z - z) * inv_d });
    const float max_r = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;

    const Vector3D o = o_strat;
    float dx = x - o.x;
    float dy = y - o.y;
    float dz = z - o.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float prop01 = ambience_propagation / 100.0f;
    const float t_anim =
        time * bb.speed_mul - prop01 * std::min(1.0f, dist / std::max(max_r, 1e-4f)) * 12.0f;
    const bool freeze_gif_motion = media_is_gif && GetSpeed() == 0;
    const float anim_time = freeze_gif_motion ? 0.0f : t_anim;
    const float scroll_mul = motion_scroll / 100.0f;
    const float warp_mul = motion_warp / 100.0f;
    const float phase_mul = motion_phase / 100.0f;

    std::function<RGBColor(RGBColor, float)> apply_ambience = [&](RGBColor base, float dist_for_gain) -> RGBColor {
        const float ag =
            MediaTextureEffect::AmbienceGain(dist_for_gain, max_r, d_face, ambience_dist_falloff, ambience_falloff_curve,
                                             ambience_edge_soft);
        const float basis = EffectGridMedianHalfExtent(grid, GetNormalizedScale());
        const float r_core = std::max(1e-3f, basis * (0.12f + 0.55f * GetNormalizedSize())) / tm;
        const float r_fade = r_core * 2.25f;
        const float fall = 1.0f - MediaTextureEffect::Smoothstep(r_core * 0.35f, r_fade, dist_for_gain);
        const float g = std::clamp(fall * ag, 0.0f, 1.0f);
        return ToRGBColor((int)(RGBGetRValue(base) * g + 0.5f), (int)(RGBGetGValue(base) * g + 0.5f),
                          (int)(RGBGetBValue(base) * g + 0.5f));
    };

    if(dist < 1e-5f)
    {
        return apply_ambience(MediaTextureEffect::SampleImageBilinear(frame, 0.5f, 0.5f), dist);
    }
    dx /= dist;
    dy /= dist;
    dz /= dist;

    const float spin_w = (0.07f + 0.65f * (spin_percent / 100.0f)) * (0.35f + GetScaledSpeed() * 0.045f) * scroll_mul * bb.speed_mul;
    const float yaw = anim_time * spin_w;
    const float pitch = anim_time * spin_w * 0.71f;
    RotateDir(dx, dy, dz, yaw, pitch);

    const int shape_a = std::clamp(base_shape, 0, 2);
    const int shape_b = (shape_a + 1) % 3;
    const float mt = morph_percent / 100.0f;

    float ua, va, ub, vb;
    ShapeToUV(shape_a, dx, dy, dz, ua, va);
    ShapeToUV(shape_b, dx, dy, dz, ub, vb);

    const float size_m = std::max(0.08f, GetNormalizedSize());
    const float repeat_from_scale = 0.4f + 1.5f * GetNormalizedScale();
    const float size_zoom_div = std::clamp(0.40f + 0.36f * size_m, 0.32f, 2.2f);
    const float tile = std::clamp(repeat_from_scale / size_zoom_div, 0.12f, 6.5f);
    ua = (ua - 0.5f) * tile + 0.5f;
    va = (va - 0.5f) * tile + 0.5f;
    ub = (ub - 0.5f) * tile + 0.5f;
    vb = (vb - 0.5f) * tile + 0.5f;

    const float detail = std::max(0.05f, GetScaledDetail());
    const float freq = std::min(6.0f, std::max(0.02f, GetScaledFrequency() * 0.065f));
    const float phase = anim_time * freq * 0.12f * phase_mul + EffectStratumBlend::ApplyMotionToAngleRad(EffectStratumBlend::PhaseShiftRad(bb), stratum_mot01) * 0.12f;
    const float amp = 0.017f * std::min(3.2f, detail * 0.2f) * warp_mul / tm;
    ua += std::sin(phase + ua * 10.5f * detail * 0.07f + va * 6.5f * detail * 0.06f) * amp;
    va += std::cos(phase * 0.89f + ua * 7.8f * detail * 0.06f - va * 9.0f * detail * 0.07f) * amp;
    ub += std::sin(phase * 1.03f + ub * 10.0f * detail * 0.07f + vb * 6.8f * detail * 0.06f) * amp;
    vb += std::cos(phase * 0.93f + ub * 8.0f * detail * 0.06f - vb * 8.5f * detail * 0.07f) * amp;

    if(tile_repeat_enabled)
    {
        ua -= std::floor(ua);
        va -= std::floor(va);
        ub -= std::floor(ub);
        vb -= std::floor(vb);
    }
    else
    {
        ua = std::clamp(ua, 0.0f, 1.0f);
        va = std::clamp(va, 0.0f, 1.0f);
        ub = std::clamp(ub, 0.0f, 1.0f);
        vb = std::clamp(vb, 0.0f, 1.0f);
    }

    float sua = ua;
    float sva = va;
    float sub = ub;
    float svb = vb;
    const unsigned int eff_res = CombineMediaSampling(media_resolution);
    if(eff_res < 100u)
    {
        Geometry3D::QuantizeMediaUV01(sua, sva, frame.width(), frame.height(), eff_res);
        Geometry3D::QuantizeMediaUV01(sub, svb, frame.width(), frame.height(), eff_res);
    }

    RGBColor ca = MediaTextureEffect::SampleImageBilinear(frame, sua, sva);
    RGBColor cb = MediaTextureEffect::SampleImageBilinear(frame, sub, svb);
    RGBColor out = MediaTextureEffect::LerpRGB(ca, cb, mt);
    const float smoothing = GetSmoothing() / 100.0f;
    if(media_is_gif && prev_snap && !prev_snap->isNull() && smoothing > 0.0f && step_interval_ms > 0 && step_ms > 0)
    {
        const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        const float elapsed_ms = (float)std::max<qint64>(0, now_ms - step_ms);
        const float blend_window_ms = std::max(1.0f, (float)step_interval_ms * smoothing);
        const float a = std::clamp(elapsed_ms / blend_window_ms, 0.0f, 1.0f);
        const RGBColor pca = MediaTextureEffect::SampleImageBilinear(*prev_snap, sua, sva);
        const RGBColor pcb = MediaTextureEffect::SampleImageBilinear(*prev_snap, sub, svb);
        const RGBColor prev_out = MediaTextureEffect::LerpRGB(pca, pcb, mt);
        out = MediaTextureEffect::LerpRGB(prev_out, out, a);
    }

    return apply_ambience(out, dist);
}

nlohmann::json OmniShapeTexture::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["media_path"] = media_path.toStdString();
    j["base_shape"] = base_shape;
    j["morph_percent"] = morph_percent;
    j["spin_percent"] = spin_percent;
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

void OmniShapeTexture::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("base_shape") && settings["base_shape"].is_number_integer())
    {
        base_shape = std::max(0, std::min(2, settings["base_shape"].get<int>()));
        if(shape_combo)
        {
            shape_combo->setCurrentIndex(base_shape);
        }
    }
    if(settings.contains("morph_percent") && settings["morph_percent"].is_number_integer())
    {
        morph_percent = (unsigned int)std::clamp(settings["morph_percent"].get<int>(), 0, 100);
        if(morph_slider)
            morph_slider->setValue((int)morph_percent);
    }
    if(settings.contains("spin_percent") && settings["spin_percent"].is_number_integer())
    {
        spin_percent = (unsigned int)std::clamp(settings["spin_percent"].get<int>(), 0, 100);
        if(spin_slider)
            spin_slider->setValue((int)spin_percent);
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

REGISTER_EFFECT_3D(OmniShapeTexture);
