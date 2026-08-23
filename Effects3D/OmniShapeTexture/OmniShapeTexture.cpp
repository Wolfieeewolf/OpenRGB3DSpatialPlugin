// SPDX-License-Identifier: GPL-2.0-only

#include "OmniShapeTexture.h"

#include "Geometry3DUtils.h"
#include "MediaTextureEffectUtils.h"
#include "OmniShapeTextureVolumeFieldGlsl.h"
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
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{

constexpr int kOmniShapeCount = 6;

float Smstep(float e0, float e1, float x)
{
    float t = (x - e0) / std::max(e1 - e0, 1e-5f);
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float HexMetric(float px, float pz)
{
    const float ax = std::fabs(px);
    const float az = std::fabs(pz);
    return std::max(ax * 0.5f + az * 0.8660254f, ax) / 0.8660254f;
}

float PolyRadial(float px, float pz, float n)
{
    const float an = (float)(2.0 * M_PI) / std::max(n, 3.0f);
    const float a = std::atan2(pz, px);
    const float r = std::sqrt(px * px + pz * pz);
    return std::cos(std::floor(0.5f + a / an) * an - a) * r / std::cos((float)M_PI / n);
}

/** Iso-metric matching OmniShapeTextureVolumeFieldGlsl — cube stays a cube. */
float ShapeMetric(float lx, float ly, float lz, int shape)
{
    switch(shape)
    {
    case 1:
        return std::max(std::max(std::fabs(lx), std::fabs(ly)), std::fabs(lz));
    case 2:
        return (std::fabs(lx) + std::fabs(ly) + std::fabs(lz)) * 0.57735027f;
    case 3:
        return std::max(std::sqrt(lx * lx + lz * lz), std::fabs(ly));
    case 4:
        return std::max(HexMetric(lx, lz), std::fabs(ly));
    case 5:
        return std::max(PolyRadial(lx, lz, 3.0f), std::fabs(ly));
    default:
        return std::sqrt(lx * lx + ly * ly + lz * lz);
    }
}

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

void DirToCylUV(float dx, float dy, float dz, float& u, float& v)
{
    u = std::atan2(dz, dx) / (float)(2.0 * M_PI) + 0.5f;
    v = std::clamp(dy * 0.5f + 0.5f, 0.0f, 1.0f);
}

void DirToPrismUV(float dx, float dy, float dz, float n, float& u, float& v)
{
    u = std::atan2(dz, dx) / (float)(2.0 * M_PI) + 0.5f;
    v = std::clamp(dy * 0.5f + 0.5f, 0.0f, 1.0f);
    const float an = 1.0f / std::max(n, 3.0f);
    u = std::floor(u / an) * an + an * 0.5f;
}

void ShapeToUV(int shape, float dx, float dy, float dz, float& u, float& v)
{
    switch(shape)
    {
    case 1:
        DirToCubeUV(dx, dy, dz, u, v);
        break;
    case 2:
        DirToOctaUV(dx, dy, dz, u, v);
        break;
    case 3:
        DirToCylUV(dx, dy, dz, u, v);
        break;
    case 4:
        DirToPrismUV(dx, dy, dz, 6.0f, u, v);
        break;
    case 5:
        DirToPrismUV(dx, dy, dz, 3.0f, u, v);
        break;
    default:
        DirToSphereUV(dx, dy, dz, u, v);
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
    volume_assist_.setFragmentBody(QString::fromUtf8(OmniShapeTextureVolumeFieldGlsl()));
    volume_assist_.setResolution(28);
}

OmniShapeTexture::~OmniShapeTexture()
{
    ClearMovie();
}

EffectInfo3D OmniShapeTexture::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Omni shape texture";
    info.effect_description =
        "Image or GIF mapped onto a crisp 3D shape envelope at the effect origin (sphere / cube / octahedron / "
        "cylinder / hex / triangle). Size grows that silhouette; Scale tiles the texture. "
        "Morph blends to the next shape. Spin rotates the mapping; Scroll / Warp / Phase drive strong UV motion. "
        "For GIFs, Speed is frames per second: 0 = frozen, 1 = 1 FPS, … up to 200 FPS. "
        "Ambience: distance dim, falloff curve, edge fade, wave delay. "
        "Stratum bands blend speed, tightness, and phase.";
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

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, tr("Shape:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    shape_combo = shape_row->combo();
    shape_combo->addItem(tr("Sphere"));
    shape_combo->addItem(tr("Cube"));
    shape_combo->addItem(tr("Octahedron"));
    shape_combo->addItem(tr("Cylinder"));
    shape_combo->addItem(tr("Hex prism"));
    shape_combo->addItem(tr("Triangle prism"));
    shape_combo->setCurrentIndex(base_shape);
    shape_combo->setToolTip(tr("Crisp 3D envelope the texture lives on. Cube stays a cube — not a soft sphere blob."));
    shape_combo->setItemData(0, tr("Round ball shell."), Qt::ToolTipRole);
    shape_combo->setItemData(1, tr("Axis-aligned cube with hard faces."), Qt::ToolTipRole);
    shape_combo->setItemData(2, tr("Diamond / octahedron."), Qt::ToolTipRole);
    shape_combo->setItemData(3, tr("Vertical cylinder (flat top/bottom)."), Qt::ToolTipRole);
    shape_combo->setItemData(4, tr("Hexagonal prism."), Qt::ToolTipRole);
    shape_combo->setItemData(5, tr("Triangular prism."), Qt::ToolTipRole);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OmniShapeTexture::OnShapeChanged);

    EffectSliderRow* morph_row = EffectUiRows::AppendSliderRow(
        layout,
        tr("Morph:"),
        0,
        100,
        (int)morph_percent,
        tr("Blend toward the next shape: Sphere → Cube → Octa → Cylinder → Hex → Triangle → Sphere."));
    morph_row->setObjectName(QStringLiteral("morphProp"));
    morph_slider = morph_row->slider();
    morph_row->bindValueChanged(
        this,
        [this](int v) { morph_percent = (unsigned int)std::clamp(v, 0, 100); },
        int_format,
        on_changed);

    EffectSliderRow* spin_row = EffectUiRows::AppendSliderRow(
        layout, tr("Spin:"), 0, 100, (int)spin_percent,
        tr("Yaw/pitch rotation rate of the whole shape mapping. Works without Scroll."));
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
              tr("Strong dimming by distance from the effect origin."),
              ambience_dist_falloff, ambience_dist_slider);
    bind_u100(media->ambienceCurveRow(), tr("Falloff curve:"),
              tr("Power curve for distance dim — still bites when Distance dim is low."),
              ambience_falloff_curve, ambience_curve_slider);
    bind_u100(media->ambienceEdgeRow(), tr("Edge fade:"),
              tr("Fade toward room bounds and sharpen the shape silhouette."),
              ambience_edge_soft, ambience_edge_slider);
    bind_u100(media->ambiencePropRow(), tr("Wave delay:"),
              tr("Spin / UV motion lags with distance from the origin."),
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
                tr("Texture scroll across the shape surface + spin boost. 0 = off."),
                motion_scroll, motion_scroll_slider);
    bind_motion(media->motionWarpRow(), tr("Warp:"), tr("Strong UV distortion amount. 0 = off."), motion_warp,
                motion_warp_slider);
    bind_motion(media->motionPhaseRow(), tr("Phase:"),
                tr("Scroll/warp tempo. High values visibly race the texture. 0 = off."),
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

void OmniShapeTexture::OnShapeChanged(int index)
{
    base_shape = std::max(0, std::min(kOmniShapeCount - 1, index));
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

void OmniShapeTexture::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    (void)grid;
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
        /* Rebuild atlas so GPU samples go black instead of a stale frame. */
        float zp[13] = {};
        volume_assist_.prepare(render_sequence, time_sec, zp, 13);
        return;
    }

    QImage media = *snap;
    constexpr int kGpuMediaEdge = SpatialVolumeFieldEngine::kMaxMediaEdge;
    if(media.width() > kGpuMediaEdge || media.height() > kGpuMediaEdge)
    {
        media = media.scaled(kGpuMediaEdge, kGpuMediaEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    const unsigned int eff_res = CombineMediaSampling(media_resolution);
    if(eff_res < 100u)
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
    /* Spin alone rotates; Scroll boosts spin and drives UV scroll via phase_drive. */
    const float spin_rate =
        freeze_gif_motion
            ? 0.0f
            : (0.25f + 2.6f * (spin_percent / 100.0f)) * (0.35f + 0.75f * speed_lin)
                  * (0.55f + 1.35f * scroll_mul) * bb.speed_mul;
    const float yaw_rate = spin_rate;
    const float pitch_rate = spin_rate * 0.71f;
    const float phase_drive =
        freeze_gif_motion ? 0.0f : (phase_mul * 1.55f + scroll_mul * 0.95f);
    const float size_m = std::max(0.08f, GetNormalizedSize());
    const float repeat_from_scale = 0.35f + 1.75f * GetNormalizedScale();
    const float size_zoom_div = std::clamp(0.55f + 0.28f * size_m, 0.40f, 2.0f);
    const float tile = std::clamp(repeat_from_scale / size_zoom_div, 0.12f, 6.5f);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float amp =
        freeze_gif_motion ? 0.0f
                          : warp_mul * (0.05f + 0.22f * std::min(1.0f, detail * 0.12f)) / tm;
    /* Local half-extent coords: Size expands the crisp shape envelope (not a soft sphere). */
    const float R_local = std::clamp(0.16f + 1.15f * size_m, 0.12f, 1.65f) / tm;
    const float prop01 = ambience_propagation / 100.0f;
    /* wrap*2+prop keeps prop∈[0,1] from colliding with the wrap flag. */
    const float packed_wrap = (tile_repeat_enabled ? 2.0f : 0.0f) + prop01;

    float vp[13] = {
        (float)std::clamp(base_shape, 0, kOmniShapeCount - 1),
        morph_percent / 100.0f,
        tile,
        yaw_rate,
        pitch_rate,
        phase_drive,
        amp,
        detail,
        ambience_dist_falloff / 100.0f,
        ambience_falloff_curve / 100.0f,
        ambience_edge_soft / 100.0f,
        R_local,
        packed_wrap
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 13);
}

RGBColor OmniShapeTexture::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)time;
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    if(volume_assist_.isAvailable())
    {
        const Vector3D origin = GetEffectOriginGrid(grid);
        float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
        SampleGpuVolumeOriginLocal01(x, y, z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        return ToRGBColor((int)(std::clamp(samp.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.z(), 0.0f, 1.0f) * 255.0f + 0.5f));
    }
    return 0x00000000;
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
        base_shape = std::max(0, std::min(kOmniShapeCount - 1, settings["base_shape"].get<int>()));
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
