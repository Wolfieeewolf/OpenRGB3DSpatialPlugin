// SPDX-License-Identifier: GPL-2.0-only

#include "OmniShapeTexture.h"

#include "Geometry3DUtils.h"
#include "MediaTextureEffectUtils.h"
#include "SpatialLayerCore.h"
#include "StratumBandPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMovie>
#include <QMutexLocker>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QDateTime>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

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

} // namespace

OmniShapeTexture::OmniShapeTexture(QWidget* parent)
    : SpatialEffect3D(parent),
      browse_button(nullptr),
      path_label(nullptr),
      shape_combo(nullptr),
      morph_slider(nullptr),
      morph_label(nullptr),
      spin_slider(nullptr),
      spin_label(nullptr),
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

EffectInfo3D OmniShapeTexture::GetEffectInfo()
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
    return info;
}

void OmniShapeTexture::SetupCustomUI(QWidget* parent)
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

    layout->addWidget(new QLabel(tr("Shape UV:")), row, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem(tr("Sphere (lat/long)"));
    shape_combo->addItem(tr("Cube (6-face)"));
    shape_combo->addItem(tr("Octahedron (8 triangles)"));
    shape_combo->setCurrentIndex(base_shape);
    shape_combo->setToolTip(tr("Base mapping from direction to texture coordinates."));
    layout->addWidget(shape_combo, row, 1, 1, 2);
    row++;

    layout->addWidget(new QLabel(tr("Morph:")), row, 0);
    morph_slider = new QSlider(Qt::Horizontal);
    morph_slider->setRange(0, 100);
    morph_slider->setValue((int)morph_percent);
    morph_slider->setToolTip(tr("Blend toward the next shape in the list: Sphere → Cube → Octahedron → Sphere."));
    layout->addWidget(morph_slider, row, 1);
    morph_label = new QLabel(QString::number(morph_percent));
    morph_label->setMinimumWidth(28);
    layout->addWidget(morph_label, row, 2);
    row++;

    layout->addWidget(new QLabel(tr("Spin:")), row, 0);
    spin_slider = new QSlider(Qt::Horizontal);
    spin_slider->setRange(0, 100);
    spin_slider->setValue((int)spin_percent);
    spin_slider->setToolTip(tr("How fast the virtual shape mapping rotates (yaw + pitch)."));
    layout->addWidget(spin_slider, row, 1);
    spin_label = new QLabel(QString::number(spin_percent));
    spin_label->setMinimumWidth(28);
    layout->addWidget(spin_label, row, 2);
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
        connect(sl, &QSlider::valueChanged, this, &OmniShapeTexture::OnAmbienceChanged);
        ar++;
    };
    add_amb_row(tr("Distance dim:"), ambience_dist_slider, ambience_dist_label, (int)ambience_dist_falloff,
                tr("Extra dimming by distance from the effect origin (multiplied with the shape rim falloff)."));
    add_amb_row(tr("Falloff curve:"), ambience_curve_slider, ambience_curve_label, (int)ambience_falloff_curve,
                tr("Shapes radial falloff; still has effect when Distance dim is 0. Combine with dim for a strong vignette."));
    add_amb_row(tr("Edge fade:"), ambience_edge_slider, ambience_edge_label, (int)ambience_edge_soft,
                tr("Strong fade toward the active grid bounds (reaches further in at high values)."));
    add_amb_row(tr("Wave delay:"), ambience_prop_slider, ambience_prop_label, (int)ambience_propagation,
                tr("Strong propagation: spin / warp phase lags more with distance from the origin."));
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
        connect(sl, &QSlider::valueChanged, this, &OmniShapeTexture::OnMotionTuningChanged);
        mr++;
    };
    add_motion_row(tr("Scroll:"), motion_scroll_slider, motion_scroll_label, (int)motion_scroll,
                   tr("Shape spin/scroll intensity. 0 = off."));
    add_motion_row(tr("Warp:"), motion_warp_slider, motion_warp_label, (int)motion_warp,
                   tr("UV distortion amount. 0 = off."));
    add_motion_row(tr("Phase:"), motion_phase_slider, motion_phase_label, (int)motion_phase,
                   tr("Temporal phase rate for spin + warp oscillation. 0 = off."));
    layout->addWidget(motion, row, 0, 1, 3);
    row++;

    QGroupBox* media_tuning = new QGroupBox(tr("Media sampling"));
    QGridLayout* tg = new QGridLayout(media_tuning);
    tg->setContentsMargins(4, 8, 4, 4);
    tile_repeat_check = new QCheckBox(tr("Tile / repeat outside image bounds"));
    tile_repeat_check->setChecked(tile_repeat_enabled);
    tile_repeat_check->setToolTip(tr("Off = single mapped image; On = wrap/tile texture coordinates."));
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
    connect(media_resolution_slider, &QSlider::valueChanged, this, &OmniShapeTexture::OnMediaTuningChanged);
    connect(tile_repeat_check, &QCheckBox::toggled, this, &OmniShapeTexture::OnMediaTuningChanged);
    layout->addWidget(media_tuning, row, 0, 1, 3);
    row++;

    stratum_panel = new StratumBandPanel(outer);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &OmniShapeTexture::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(outer, parent);

    connect(browse_button, &QPushButton::clicked, this, &OmniShapeTexture::OnBrowseMedia);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OmniShapeTexture::OnShapeChanged);
    connect(morph_slider, &QSlider::valueChanged, this, &OmniShapeTexture::OnMorphChanged);
    connect(spin_slider, &QSlider::valueChanged, this, &OmniShapeTexture::OnSpinChanged);
    morph_label->setText(QString::number((int)morph_percent));
    spin_label->setText(QString::number((int)spin_percent));
}

void OmniShapeTexture::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_OMNI_SHAPE_TEXTURE;
}

void OmniShapeTexture::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void OmniShapeTexture::OnShapeChanged(int index)
{
    base_shape = std::max(0, std::min(2, index));
    emit ParametersChanged();
}

void OmniShapeTexture::OnMorphChanged(int value)
{
    morph_percent = (unsigned int)std::clamp(value, 0, 100);
    if(morph_label)
    {
        morph_label->setText(QString::number((int)morph_percent));
    }
    emit ParametersChanged();
}

void OmniShapeTexture::OnSpinChanged(int value)
{
    spin_percent = (unsigned int)std::clamp(value, 0, 100);
    if(spin_label)
    {
        spin_label->setText(QString::number((int)spin_percent));
    }
    emit ParametersChanged();
}

void OmniShapeTexture::OnAmbienceChanged()
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

void OmniShapeTexture::OnMotionTuningChanged()
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

void OmniShapeTexture::OnMediaTuningChanged()
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
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw_bb, stratum_tuning_);
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
    const float phase = anim_time * freq * 0.12f * phase_mul + bb.phase_deg * (float)(M_PI / 180.0f) * 0.12f;
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
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "omnishapetexture_stratum_layout_mode",
                                           sm,
                                           st,
                                           "omnishapetexture_stratum_band_speed_pct",
                                           "omnishapetexture_stratum_band_tight_pct",
                                           "omnishapetexture_stratum_band_phase_deg");
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
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "omnishapetexture_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "omnishapetexture_stratum_band_speed_pct",
                                            "omnishapetexture_stratum_band_tight_pct",
                                            "omnishapetexture_stratum_band_phase_deg");
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
        {
            morph_slider->setValue((int)morph_percent);
        }
        if(morph_label)
        {
            morph_label->setText(QString::number((int)morph_percent));
        }
    }
    if(settings.contains("spin_percent") && settings["spin_percent"].is_number_integer())
    {
        spin_percent = (unsigned int)std::clamp(settings["spin_percent"].get<int>(), 0, 100);
        if(spin_slider)
        {
            spin_slider->setValue((int)spin_percent);
        }
        if(spin_label)
        {
            spin_label->setText(QString::number((int)spin_percent));
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
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(OmniShapeTexture);
