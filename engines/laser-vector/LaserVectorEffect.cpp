// SPDX-License-Identifier: GPL-2.0-only

#include "LaserVectorEffect.h"

#include "EffectListManager3D.h"
#include "EffectSliderPromote.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "Geometry3DUtils.h"
#include "GpuRoomGridOverlay.h"
#include "PlayerEngines.h"
#include "SpatialEffectTypes.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <nlohmann/json.hpp>
#include <vector>

namespace
{
constexpr SpatialEffectType kLaserVectorEffectType = static_cast<SpatialEffectType>(40);

float DistPointSegment2D(float px, float py,
                         float x0, float y0, float x1, float y1)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len2 = dx * dx + dy * dy;
    float t = 0.0f;
    if(len2 > 1.0e-12f)
    {
        t = ((px - x0) * dx + (py - y0) * dy) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    const float cx = x0 + t * dx;
    const float cy = y0 + t * dy;
    const float ex = px - cx;
    const float ey = py - cy;
    return std::sqrt(ex * ex + ey * ey);
}

float DistPointSegment3D(float px, float py, float pz,
                         float x0, float y0, float z0,
                         float x1, float y1, float z1)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float dz = z1 - z0;
    const float len2 = dx * dx + dy * dy + dz * dz;
    float t = 0.0f;
    if(len2 > 1.0e-12f)
    {
        t = ((px - x0) * dx + (py - y0) * dy + (pz - z0) * dz) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    const float cx = x0 + t * dx;
    const float cy = y0 + t * dy;
    const float cz = z0 + t * dz;
    const float ex = px - cx;
    const float ey = py - cy;
    const float ez = pz - cz;
    return std::sqrt(ex * ex + ey * ey + ez * ez);
}

/** Distance to finite ray segment from origin O to far point F (t clamped to [0,1]). */
float DistPointRay01(float px, float py, float pz,
                     float ox, float oy, float oz,
                     float fx, float fy, float fz)
{
    return DistPointSegment3D(px, py, pz, ox, oy, oz, fx, fy, fz);
}

void LaserToFarUv(float lx, float ly, float& fu, float& fv, float& fw)
{
    fu = std::clamp(0.5f + 0.5f * lx, 0.0f, 1.0f);
    fv = std::clamp(0.5f + 0.5f * ly, 0.0f, 1.0f);
    fw = 1.0f; // throw plane = far face of the effect volume
}
} // namespace

void RegisterLaserVectorEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "LaserVector",
        "Laser Vector",
        "Laser",
        "",
        "",
        []() { return new LaserVectorEffect; },
        SpatialEffectSettingsLayout::FullWithTransport);
}

LaserVectorEffect::LaserVectorEffect(QWidget* parent)
    : SpatialEffect3D(parent)
{
    // Plugin-wide Speed 0 freezes motion; ILDA should play at configured FPS out of the box.
    SetSpeed(100);
}

EffectInfo3D LaserVectorEffect::GetEffectInfo() const
{
    EffectInfo3D info = {};
    info.effect_name = "Laser Vector";
    info.effect_description =
        "Play an ILDA (.ild) show as projector beams through the effect volume. "
        "One .ild = one scanner (time-multiplexed); use Max beams to thin sparkles, or stack layers for more projectors.";
    info.category = "Laser";
    info.effect_type = kLaserVectorEffectType;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_frequency = false;
    info.use_size_parameter = true;
    info.show_color_controls = false;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_detail_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = false;
    info.show_surface_control = true;
    info.show_position_offset_control = true;
    info.show_room_output_control = true;
    return info;
}

void LaserVectorEffect::SetupCustomUI(QWidget* parent)
{
    if(!parent)
    {
        return;
    }

    settings_panel_ = EffectUiRows::NewEffectPanel("LaserVectorEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(settings_panel_);
    if(!layout)
    {
        return;
    }

    EffectUiRows::AppendInfoLabel(
        layout,
        tr(".ild is geometry + colour in abstract RGB — not the projector model. "
           "Pick Machine profile for RGB / RGY / mono / beam-style mapping (like Beyond colour balance). "
           "Scanner head = multi-projector channel in the file; stack layers for more heads."));

    QHBoxLayout* pick_row = new QHBoxLayout();
    pick_row->setContentsMargins(0, 0, 0, 0);
    browse_button_ = new QPushButton(tr("Open ILDA…"), settings_panel_);
    browse_button_->setObjectName(QStringLiteral("browseIldaButton"));
    browse_button_->setToolTip(
        tr("Browse for an .ild file.\nMouse: click button.\nKeyboard: Tab here, Space/Enter, or Ctrl+O.\nMenu: right-click this panel → Open ILDA…"));
    browse_button_->setAccessibleName(tr("Open ILDA file"));
    browse_button_->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    clear_button_ = new QPushButton(tr("Clear"), settings_panel_);
    clear_button_->setObjectName(QStringLiteral("clearIldaButton"));
    clear_button_->setToolTip(
        tr("Unload the current .ild.\nMouse: click Clear.\nKeyboard: Tab to Clear, Space/Enter, or Ctrl+W.\nMenu: right-click this panel → Clear file."));
    clear_button_->setAccessibleName(tr("Clear ILDA file"));
    clear_button_->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    clear_button_->setEnabled(!path_.isEmpty());
    path_label_ = new QLabel(settings_panel_);
    path_label_->setObjectName(QStringLiteral("ildaPathLabel"));
    path_label_->setWordWrap(true);
    path_label_->setMinimumWidth(120);
    UpdatePathLabel();
    pick_row->addWidget(browse_button_);
    pick_row->addWidget(clear_button_);
    pick_row->addWidget(path_label_, 1);
    layout->addLayout(pick_row);

    status_label_ = new QLabel(settings_panel_);
    status_label_->setObjectName(QStringLiteral("ildaStatusLabel"));
    status_label_->setWordWrap(true);
    layout->addWidget(status_label_);

    QHBoxLayout* machine_row = new QHBoxLayout();
    machine_row->setContentsMargins(0, 0, 0, 0);
    QLabel* machine_cap = new QLabel(tr("Machine profile"), settings_panel_);
    machine_cap->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    machine_combo_ = new QComboBox(settings_panel_);
    machine_combo_->setToolTip(
        tr("Not stored in .ild — mirrors ShowEditor/Beyond projector colour setup.\n"
           "RGB: full colour. RGY: no blue. RGBY: yellow diode bias. "
           "Mono: single laser. Beam punch: atmospheric beam look."));
    machine_combo_->setAccessibleName(tr("Laser machine colour profile"));
    machine_combo_->addItem(tr("RGB (full colour)"), static_cast<int>(MachineProfile::Rgb));
    machine_combo_->addItem(tr("RGY (no blue)"), static_cast<int>(MachineProfile::Rgy));
    machine_combo_->addItem(tr("RGBY (yellow diode)"), static_cast<int>(MachineProfile::Rgby));
    machine_combo_->addItem(tr("Mono green"), static_cast<int>(MachineProfile::MonoGreen));
    machine_combo_->addItem(tr("Mono white"), static_cast<int>(MachineProfile::MonoWhite));
    machine_combo_->addItem(tr("Beam punch"), static_cast<int>(MachineProfile::BeamPunch));
    machine_combo_->setCurrentIndex(static_cast<int>(machine_profile_));
    machine_row->addWidget(machine_cap);
    machine_row->addWidget(machine_combo_, 1);
    layout->addLayout(machine_row);

    QHBoxLayout* scanner_row = new QHBoxLayout();
    scanner_row->setContentsMargins(0, 0, 0, 0);
    QLabel* scanner_cap = new QLabel(tr("Scanner / projector"), settings_panel_);
    scanner_cap->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    scanner_combo_ = new QComboBox(settings_panel_);
    scanner_combo_->setToolTip(
        tr("ILDA header “scanner head” (byte 31): which scanner pair a frame is for.\n"
           "Satellites / multi-head shows use different heads in one file. All = every frame."));
    scanner_combo_->setAccessibleName(tr("Scanner or projector channel"));
    scanner_combo_->addItem(tr("All scanners"), -1);
    scanner_row->addWidget(scanner_cap);
    scanner_row->addWidget(scanner_combo_, 1);
    layout->addLayout(scanner_row);

    projector_check_ = new QCheckBox(tr("Projector beams (from volume near face)"), settings_panel_);
    projector_check_->setChecked(projector_beams_);
    projector_check_->setToolTip(
        tr("On: rays from the projector face through each lit figure point/stroke (laser look).\n"
           "Off: flat figure inside the volume (old blob-style sampling)."));
    layout->addWidget(projector_check_);

    join_path_check_ = new QCheckBox(tr("Join lit path (ring/outline)"), settings_panel_);
    join_path_check_->setChecked(join_lit_path_);
    join_path_check_->setToolTip(
        tr("Connect successive lit dwell points with strokes (what many editors draw as guide lines).\n"
           "Needed for RING.ild-style shows that only light corners and blank while moving.\n"
           "Mouse / checkbox; keyboard: Tab + Space."));
    layout->addWidget(join_path_check_);

    intensity_widen_check_ = new QCheckBox(tr("Intensity widens beam"), settings_panel_);
    intensity_widen_check_->setChecked(intensity_widens_);
    intensity_widen_check_->setToolTip(
        tr(".ild has no beam-width field (ILDA Beam Brush was never adopted).\n"
           "Brighter RGB ≈ fatter shaft in haze — use this to approximate thin/fat mixes."));
    layout->addWidget(intensity_widen_check_);

    dwell_widen_check_ = new QCheckBox(tr("Dwell widens beam"), settings_panel_);
    dwell_widen_check_->setChecked(dwell_widens_);
    dwell_widen_check_->setToolTip(
        tr("Repeated lit points at one spot (dwell) look brighter/fatter on real projectors.\n"
           "RING corners often have long dwells — those can read thicker than quick strokes."));
    layout->addWidget(dwell_widen_check_);

    crisp_beams_check_ = new QCheckBox(tr("Crisp beams (no soft wash)"), settings_panel_);
    crisp_beams_check_->setChecked(crisp_beams_);
    crisp_beams_check_->setToolTip(
        tr("Keeps the full .ild colour range. Soft falloff greys edges and washes toward white.\n"
           "On: hard shaft — file colour or black. Nearest beam wins (no stacking into white)."));
    layout->addWidget(crisp_beams_check_);

    thickness_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Beam thickness"),
        0.005, 0.40, beam_thickness_,
        3, 0.005,
        EffectSpinOverSliderTip(tr("Base beam radius. Dynamics (intensity/dwell) scale up from this.")));
    width_dyn_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Width dynamics"),
        1.0, 5.0, width_dynamics_,
        2, 0.1,
        EffectSpinOverSliderTip(
            tr("How much brighter/longer dwells fatten that beam’s colour into empty space "
               "(1 = flat, 3–4 = strong). Does not mix neighbours into white.")));
    projector_depth_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Projector depth"),
        0.0, 1.0, projector_depth_,
        2, 0.01,
        EffectSpinOverSliderTip(tr("Where the projector sits along volume depth (0 = near throw face, 1 = far).")));
    fps_row_ = EffectUiRows::AppendSliderRow(
        layout,
        tr("Frame rate"),
        1, 120, fps_,
        EffectSpinOverSliderTip(tr("ILDA frames per second before Speed scales playback.")));
    fps_row_->setSuffix(tr(" fps"));
    max_beams_row_ = EffectUiRows::AppendSliderRow(
        layout,
        tr("Max beams"),
        0, 128, max_beams_,
        EffectSpinOverSliderTip(
            tr("Cap how many sparkle points / strokes draw per frame (thins busy frames). 0 = all. "
               "Does not invent extra scanners — ILDA is still one beam path.")));
    max_beams_row_->setSuffix(tr(" (0=all)"));

    flatten_check_ = new QCheckBox(tr("Flatten Z (2D projection)"), settings_panel_);
    flatten_check_->setChecked(flatten_z_);
    flatten_check_->setToolTip(tr("Treat the figure as 2D on the throw plane."));
    layout->addWidget(flatten_check_);

    loop_check_ = new QCheckBox(tr("Loop show"), settings_panel_);
    loop_check_->setChecked(loop_);
    layout->addWidget(loop_check_);

    AlignHBoxRowCaptions({machine_row, scanner_row}, 110);
    AlignCaptionLabels({
        thickness_row_ ? thickness_row_->captionLabel() : nullptr,
        width_dyn_row_ ? width_dyn_row_->captionLabel() : nullptr,
        projector_depth_row_ ? projector_depth_row_->captionLabel() : nullptr,
        fps_row_ ? fps_row_->captionLabel() : nullptr,
        max_beams_row_ ? max_beams_row_->captionLabel() : nullptr,
    });

    settings_panel_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(settings_panel_, &QWidget::customContextMenuRequested,
            this, &LaserVectorEffect::OnPanelContextMenu);
    connect(browse_button_, &QPushButton::clicked, this, &LaserVectorEffect::OnBrowseIld);
    connect(clear_button_, &QPushButton::clicked, this, &LaserVectorEffect::OnClearIld);
    if(thickness_row_)
    {
        thickness_row_->bindValueChanged(
            this,
            [this](double v) { beam_thickness_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(width_dyn_row_)
    {
        width_dyn_row_->bindValueChanged(
            this,
            [this](double v) { width_dynamics_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(projector_depth_row_)
    {
        projector_depth_row_->bindValueChanged(
            this,
            [this](double v) { projector_depth_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(fps_row_)
    {
        fps_row_->bindValueChanged(
            this,
            [this](int v) { fps_ = v; },
            nullptr,
            [this]() { emit ParametersChanged(); });
    }
    if(max_beams_row_)
    {
        max_beams_row_->bindValueChanged(
            this,
            [this](int v) { max_beams_ = v; },
            nullptr,
            [this]() { emit ParametersChanged(); });
    }
    connect(machine_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if(!machine_combo_)
        {
            return;
        }
        machine_profile_ = static_cast<MachineProfile>(machine_combo_->currentData().toInt());
        emit ParametersChanged();
    });
    connect(scanner_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if(!scanner_combo_)
        {
            return;
        }
        scanner_filter_ = scanner_combo_->currentData().toInt();
        RebuildPlayIndices();
        emit ParametersChanged();
    });
    connect(projector_check_, &QCheckBox::toggled, this, [this](bool on) {
        projector_beams_ = on;
        emit ParametersChanged();
    });
    connect(join_path_check_, &QCheckBox::toggled, this, [this](bool on) {
        join_lit_path_ = on;
        RebuildShowGeometry();
        UpdateStatusLabel();
        emit ParametersChanged();
    });
    connect(intensity_widen_check_, &QCheckBox::toggled, this, [this](bool on) {
        intensity_widens_ = on;
        emit ParametersChanged();
    });
    connect(dwell_widen_check_, &QCheckBox::toggled, this, [this](bool on) {
        dwell_widens_ = on;
        emit ParametersChanged();
    });
    connect(crisp_beams_check_, &QCheckBox::toggled, this, [this](bool on) {
        crisp_beams_ = on;
        emit ParametersChanged();
    });
    connect(flatten_check_, &QCheckBox::toggled, this, [this](bool on) {
        flatten_z_ = on;
        emit ParametersChanged();
    });
    connect(loop_check_, &QCheckBox::toggled, this, [this](bool on) {
        loop_ = on;
        emit ParametersChanged();
    });

    UpdateStatusLabel();
    AddWidgetToParent(settings_panel_, parent);
}

void LaserVectorEffect::OnBrowseIld()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open ILDA laser file"),
        path_.isEmpty() ? QString() : QFileInfo(path_).absolutePath(),
        tr("ILDA (*.ild);;All files (*.*)"));
    if(path.isEmpty())
    {
        return;
    }
    LoadIldPath(path);
    emit ParametersChanged();
}

void LaserVectorEffect::OnClearIld()
{
    if(path_.isEmpty() && show_.frames.empty())
    {
        return;
    }
    path_.clear();
    show_ = LaserShow{};
    play_indices_.clear();
    scanner_filter_ = -1;
    if(scanner_combo_)
    {
        const QSignalBlocker block(scanner_combo_);
        scanner_combo_->clear();
        scanner_combo_->addItem(tr("All scanners"), -1);
        scanner_combo_->setCurrentIndex(0);
    }
    UpdatePathLabel();
    UpdateStatusLabel();
    emit ParametersChanged();
}

void LaserVectorEffect::OnPanelContextMenu(const QPoint& pos)
{
    if(!settings_panel_)
    {
        return;
    }
    QMenu menu(settings_panel_);
    QAction* open_act = menu.addAction(tr("Open ILDA…"));
    open_act->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    QAction* clear_act = menu.addAction(tr("Clear file"));
    clear_act->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    clear_act->setEnabled(!path_.isEmpty());
    QAction* chosen = menu.exec(settings_panel_->mapToGlobal(pos));
    if(chosen == open_act)
    {
        OnBrowseIld();
    }
    else if(chosen == clear_act)
    {
        OnClearIld();
    }
}

void LaserVectorEffect::LoadIldPath(const QString& path)
{
    path_ = path;
    show_ = IldaReader::LoadFile(path.toStdString());
    RebuildShowGeometry();
    if(scanner_combo_)
    {
        const QSignalBlocker block(scanner_combo_);
        scanner_combo_->clear();
        scanner_combo_->addItem(tr("All scanners"), -1);
        for(int s : show_.scanners)
        {
            scanner_combo_->addItem(tr("Scanner %1").arg(s), s);
        }
        int idx = 0;
        for(int i = 0; i < scanner_combo_->count(); ++i)
        {
            if(scanner_combo_->itemData(i).toInt() == scanner_filter_)
            {
                idx = i;
                break;
            }
        }
        scanner_combo_->setCurrentIndex(idx);
        scanner_filter_ = scanner_combo_->currentData().toInt();
    }
    RebuildPlayIndices();
    UpdatePathLabel();
    UpdateStatusLabel();
}

void LaserVectorEffect::RebuildShowGeometry()
{
    for(LaserFrame& frame : show_.frames)
    {
        frame.RebuildSegments(join_lit_path_);
    }
}

void LaserVectorEffect::RebuildPlayIndices()
{
    play_indices_.clear();
    play_indices_.reserve(show_.frames.size());
    for(size_t i = 0; i < show_.frames.size(); ++i)
    {
        if(scanner_filter_ < 0 || show_.frames[i].scanner == scanner_filter_)
        {
            play_indices_.push_back(i);
        }
    }
}

void LaserVectorEffect::UpdatePathLabel()
{
    if(clear_button_)
    {
        clear_button_->setEnabled(!path_.isEmpty());
    }
    if(!path_label_)
    {
        return;
    }
    if(path_.isEmpty())
    {
        path_label_->setText(tr("(no file)"));
        path_label_->setToolTip(QString());
        return;
    }
    path_label_->setText(QFileInfo(path_).fileName());
    path_label_->setToolTip(path_);
}

void LaserVectorEffect::UpdateStatusLabel()
{
    if(!status_label_)
    {
        return;
    }
    if(path_.isEmpty())
    {
        status_label_->setText(tr("No file loaded."));
        return;
    }
    if(!show_.error.empty())
    {
        status_label_->setText(tr("Error: %1").arg(QString::fromStdString(show_.error)));
        return;
    }
    size_t segs = 0;
    size_t dots = 0;
    float min_i = 1.0f, max_i = 0.0f;
    float min_d = 1.0e9f, max_d = 0.0f;
    bool any_prim = false;
    for(const LaserFrame& f : show_.frames)
    {
        segs += f.segments.size();
        dots += f.dots.size();
        for(const LaserSegment& s : f.segments)
        {
            const float inten = std::max({s.r, s.g, s.b});
            min_i = std::min(min_i, inten);
            max_i = std::max(max_i, inten);
            min_d = std::min(min_d, s.dwell);
            max_d = std::max(max_d, s.dwell);
            any_prim = true;
        }
        for(const LaserDot& d : f.dots)
        {
            const float inten = std::max({d.r, d.g, d.b});
            min_i = std::min(min_i, inten);
            max_i = std::max(max_i, inten);
            min_d = std::min(min_d, d.dwell);
            max_d = std::max(max_d, d.dwell);
            any_prim = true;
        }
    }
    QString heads = tr("head 0");
    if(!show_.scanners.empty())
    {
        QStringList parts;
        for(int s : show_.scanners)
        {
            parts << QString::number(s);
        }
        heads = tr("ILDA scanner heads: %1").arg(parts.join(QLatin1Char(',')));
    }
    QString width_note = tr("no beam-width in .ild");
    if(any_prim)
    {
        const bool mixed_i = (max_i - min_i) > 0.15f;
        const bool mixed_d = max_d > (min_d + 1.5f);
        if(mixed_i || mixed_d)
        {
            width_note = tr("mixed thin/fat proxies — intensity %1–%2, dwell %3–%4")
                             .arg(min_i, 0, 'f', 2)
                             .arg(max_i, 0, 'f', 2)
                             .arg(min_d, 0, 'f', 0)
                             .arg(max_d, 0, 'f', 0);
        }
        else
        {
            width_note = tr("even width proxies — intensity ~%1, dwell ~%2")
                             .arg(max_i, 0, 'f', 2)
                             .arg(max_d, 0, 'f', 0);
        }
    }
    status_label_->setText(
        tr("%1 frames, %2 strokes, %3 sparkles — %4. %5. Speed 100 = realtime.")
            .arg(show_.frames.size())
            .arg(segs)
            .arg(dots)
            .arg(heads)
            .arg(width_note));
}

const LaserFrame* LaserVectorEffect::CurrentFrame(float time) const
{
    if(play_indices_.empty() || show_.frames.empty())
    {
        return nullptr;
    }

    const size_t n = play_indices_.size();
    // Speed 0 freezes (same as other effects). Speed 100 = 1× the Frame rate slider.
    if(GetSpeed() == 0)
    {
        return &show_.frames[play_indices_[0]];
    }
    const float speed_mul = static_cast<float>(GetSpeed()) / 100.0f;
    const float rate = std::max(1.0f, static_cast<float>(fps_)) * speed_mul;
    float frame_f = std::max(0.0f, time) * rate;
    if(loop_)
    {
        frame_f = std::fmod(frame_f, static_cast<float>(n));
        if(frame_f < 0.0f)
        {
            frame_f += static_cast<float>(n);
        }
    }
    else if(frame_f > static_cast<float>(n - 1))
    {
        frame_f = static_cast<float>(n - 1);
    }
    const size_t idx = std::min(static_cast<size_t>(frame_f), n - 1);
    return &show_.frames[play_indices_[idx]];
}

float LaserVectorEffect::EffectiveThickness(float intensity01, float dwell) const
{
    float mul = 1.0f;
    const float dyn = std::max(1.0f, width_dynamics_);
    if(intensity_widens_)
    {
        const float t = std::clamp(intensity01, 0.0f, 1.0f);
        mul *= (1.0f + (dyn - 1.0f) * t);
    }
    if(dwell_widens_ && dwell > 1.0f)
    {
        // ~8–20 dwell samples is common at RING corners; soft log scale.
        const float d = std::clamp(std::log2(1.0f + dwell) / 5.0f, 0.0f, 1.0f);
        mul *= (1.0f + (dyn - 1.0f) * 0.75f * d);
    }
    return std::max(0.001f, beam_thickness_ * mul);
}

void LaserVectorEffect::MapMachineColour(float& r, float& g, float& b) const
{
    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);

    switch(machine_profile_)
    {
        case MachineProfile::Rgb:
            break;
        case MachineProfile::Rgy:
            // Classic RGY: no blue; yellow comes from R+G overlap.
            b = 0.0f;
            break;
        case MachineProfile::Rgby:
        {
            // Dedicated yellow diode: pull yellow (high R+G, low B) off the mix toward pure yellow.
            const float yellow = std::min(r, g) * (1.0f - b);
            if(yellow > 0.05f)
            {
                r = std::min(1.0f, r + yellow * 0.35f);
                g = std::min(1.0f, g + yellow * 0.45f);
                b *= (1.0f - yellow * 0.6f);
            }
            break;
        }
        case MachineProfile::MonoGreen:
        {
            const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            r = 0.0f;
            g = lum;
            b = 0.0f;
            break;
        }
        case MachineProfile::MonoWhite:
        {
            const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            r = g = b = lum;
            break;
        }
        case MachineProfile::BeamPunch:
        {
            // Atmospheric beam look: crush mid greys, punch saturated primaries.
            auto punch = [](float c) {
                if(c < 0.12f)
                {
                    return 0.0f;
                }
                return std::pow(c, 0.65f);
            };
            r = punch(r);
            g = punch(g);
            b = punch(b);
            break;
        }
        default:
            break;
    }
}

float LaserVectorEffect::BeamPresence(float dist, float thickness) const
{
    const float t = std::max(0.001f, thickness);
    if(dist >= t)
    {
        return 0.0f;
    }
    const float linear = 1.0f - (dist / t);
    if(crisp_beams_)
    {
        // Hard core (~70% of radius), tiny edge — avoids grey halos that look like mud/white wash.
        if(linear >= 0.30f)
        {
            return 1.0f;
        }
        return std::clamp(linear / 0.30f, 0.0f, 1.0f);
    }
    return linear;
}

RGBColor LaserVectorEffect::SampleLaserAtLocal01(float u, float v, float w, const LaserFrame& frame) const
{
    if(frame.segments.empty() && frame.dots.empty())
    {
        return ToRGBColor(0, 0, 0);
    }

    // Nearest shaft wins. Additive diode stacking + width dynamics painted every overlap white.
    float best_d = 1.0e9f;
    float best_presence = 0.0f;
    float br = 0.0f, bg = 0.0f, bb = 0.0f;
    bool hit = false;

    const float ox = 0.5f;
    const float oy = 0.5f;
    const float oz = std::clamp(projector_depth_, 0.0f, 1.0f);

    auto consider = [&](float d, float r, float g, float b, float thick) {
        const float presence = BeamPresence(d, thick);
        if(presence <= 0.0f)
        {
            return;
        }
        // Geometric nearest center — fat beams expand into black, they don't bury closer strokes.
        if(d < best_d || (d == best_d && presence > best_presence))
        {
            MapMachineColour(r, g, b);
            best_d = d;
            best_presence = presence;
            br = r;
            bg = g;
            bb = b;
            hit = true;
        }
    };

    size_t seg_limit = frame.segments.size();
    size_t dot_limit = frame.dots.size();
    if(max_beams_ > 0)
    {
        const size_t cap = static_cast<size_t>(max_beams_);
        // Prefer dots (sparkles) when capped; otherwise trim both proportionally.
        if(!frame.dots.empty())
        {
            dot_limit = std::min(dot_limit, cap);
            seg_limit = (dot_limit >= cap) ? 0 : std::min(seg_limit, cap - dot_limit);
        }
        else
        {
            seg_limit = std::min(seg_limit, cap);
        }
    }

    if(projector_beams_)
    {
        for(size_t i = 0; i < seg_limit; ++i)
        {
            const LaserSegment& seg = frame.segments[i];
            const float inten = std::max({seg.r, seg.g, seg.b});
            const float thick = EffectiveThickness(inten, seg.dwell);
            float fu0, fv0, fw0, fu1, fv1, fw1;
            LaserToFarUv(seg.x0, seg.y0, fu0, fv0, fw0);
            LaserToFarUv(seg.x1, seg.y1, fu1, fv1, fw1);
            // One presence per stroke (closest of the fan rays) — don't add the same colour 3×.
            const float d0 = DistPointRay01(u, v, w, ox, oy, oz, fu0, fv0, fw0);
            const float d1 = DistPointRay01(u, v, w, ox, oy, oz, fu1, fv1, fw1);
            const float dm = DistPointRay01(u, v, w, ox, oy, oz,
                                            0.5f * (fu0 + fu1), 0.5f * (fv0 + fv1), 0.5f * (fw0 + fw1));
            consider(std::min({d0, d1, dm}), seg.r, seg.g, seg.b, thick);
        }
        for(size_t i = 0; i < dot_limit; ++i)
        {
            const LaserDot& dot = frame.dots[i];
            const float inten = std::max({dot.r, dot.g, dot.b});
            const float thick = EffectiveThickness(inten, dot.dwell);
            float fu, fv, fw;
            LaserToFarUv(dot.x, dot.y, fu, fv, fw);
            consider(DistPointRay01(u, v, w, ox, oy, oz, fu, fv, fw), dot.r, dot.g, dot.b, thick);
        }
    }
    else
    {
        const float lx = u * 2.0f - 1.0f;
        const float ly = v * 2.0f - 1.0f;
        const float lz = w * 2.0f - 1.0f;
        for(size_t i = 0; i < seg_limit; ++i)
        {
            const LaserSegment& seg = frame.segments[i];
            const float inten = std::max({seg.r, seg.g, seg.b});
            const float thick = EffectiveThickness(inten, seg.dwell);
            float d;
            if(flatten_z_)
            {
                d = DistPointSegment2D(lx, ly, seg.x0, seg.y0, seg.x1, seg.y1);
            }
            else
            {
                d = DistPointSegment3D(lx, ly, lz, seg.x0, seg.y0, seg.z0, seg.x1, seg.y1, seg.z1);
            }
            consider(d, seg.r, seg.g, seg.b, thick);
        }
        for(size_t i = 0; i < dot_limit; ++i)
        {
            const LaserDot& dot = frame.dots[i];
            const float inten = std::max({dot.r, dot.g, dot.b});
            const float thick = EffectiveThickness(inten, dot.dwell);
            float d;
            if(flatten_z_)
            {
                const float ex = lx - dot.x;
                const float ey = ly - dot.y;
                d = std::sqrt(ex * ex + ey * ey);
            }
            else
            {
                const float ex = lx - dot.x;
                const float ey = ly - dot.y;
                const float ez = lz - dot.z;
                d = std::sqrt(ex * ex + ey * ey + ez * ez);
            }
            consider(d, dot.r, dot.g, dot.b, thick);
        }
    }

    if(!hit)
    {
        return ToRGBColor(0, 0, 0);
    }

    const float bright = static_cast<float>(effect_brightness) / 100.0f;
    // Crisp: full shaft colour. Soft: edge presence dims toward black (not toward white/grey mix).
    const float gain = crisp_beams_ ? bright : (bright * best_presence);
    return ToRGBColor(
        static_cast<int>(std::clamp(br * gain, 0.0f, 1.0f) * 255.0f + 0.5f),
        static_cast<int>(std::clamp(bg * gain, 0.0f, 1.0f) * 255.0f + 0.5f),
        static_cast<int>(std::clamp(bb * gain, 0.0f, 1.0f) * 255.0f + 0.5f));
}

RGBColor LaserVectorEffect::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    const LaserFrame* frame = CurrentFrame(time);
    if(!frame)
    {
        return ToRGBColor(0, 0, 0);
    }

    float u = 0.5f, v = 0.5f, w = 0.5f;
    if(!SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
    {
        return ToRGBColor(0, 0, 0);
    }

    return SampleLaserAtLocal01(u, v, w, *frame);
}

bool LaserVectorEffect::TryFillGpuRoomGridOverlayLayer(GpuRoomGridOverlayLayer& out,
                                                      const GridContext3D& grid,
                                                      float time_sec)
{
    const LaserFrame* frame = CurrentFrame(time_sec);
    if(!frame || (frame->segments.empty() && frame->dots.empty()))
    {
        return false;
    }

    constexpr int kAtlasN = 24;
    const int n = kAtlasN;
    const int w = n;
    const int h = n * n;
    std::vector<unsigned char> rgba((size_t)w * (size_t)h * 4u, 0);

    for(int slice = 0; slice < n; ++slice)
    {
        const float wz = (static_cast<float>(slice) + 0.5f) / static_cast<float>(n);
        for(int y = 0; y < n; ++y)
        {
            const float wy = (static_cast<float>(y) + 0.5f) / static_cast<float>(n);
            const int row = slice * n + y;
            unsigned char* row_ptr = rgba.data() + ((size_t)row * (size_t)w) * 4u;
            for(int x = 0; x < n; ++x)
            {
                const float wx = (static_cast<float>(x) + 0.5f) / static_cast<float>(n);
                const RGBColor c = SampleLaserAtLocal01(wx, wy, wz, *frame);
                unsigned char* px = row_ptr + (size_t)x * 4u;
                /* FinishRgb swaps texel B↔R (volume atlases store OpenRGB 0x00BBGGRR as rgb).
                   Pack B,G,R here so the overlay matches CalculateColorGrid / controllers. */
                px[0] = static_cast<unsigned char>((c >> 16) & 0xFF); // B
                px[1] = static_cast<unsigned char>((c >> 8) & 0xFF);  // G
                px[2] = static_cast<unsigned char>(c & 0xFF);         // R
                px[3] = 255;
            }
        }
    }

    out = GpuRoomGridOverlayLayer{};
    out.atlas_tex = 0;
    out.atlas_rgba = std::move(rgba);
    out.atlas_n = n;
    out.brightness = 1.0f; /* already applied in SampleLaserAtLocal01 */
    out.finish_mode = GpuRoomGridOverlayLayer::FinishRgb;
    out.sample_room = false;
    out.volume_tools = effect_volume_tools_unlocked;
    out.volume_shape = effect_volume_tools_unlocked ? effect_volume_shape : (int)VOLUME_SHAPE_BOX;
    out.shear_xy = effect_shear_xy;
    out.shear_xz = effect_shear_xz;
    out.shear_yz = effect_shear_yz;

    const Vector3D origin = GetEffectOriginGrid(grid);
    out.origin[0] = origin.x;
    out.origin[1] = origin.y;
    out.origin[2] = origin.z;
    out.room_min[0] = grid.min_x;
    out.room_min[1] = grid.min_y;
    out.room_min[2] = grid.min_z;
    out.room_max[0] = grid.max_x;
    out.room_max[1] = grid.max_y;
    out.room_max[2] = grid.max_z;

    float hx = 1.0f, hy = 1.0f, hz = 1.0f;
    GetVolumeHalfExtents(grid, hx, hy, hz);
    out.half_ext[0] = hx;
    out.half_ext[1] = hy;
    out.half_ext[2] = hz;

    Rotation3D rot;
    rot.x = effect_rotation_pitch;
    rot.y = effect_rotation_yaw;
    rot.z = effect_rotation_roll;
    float m[9];
    Geometry3D::ComputeRotationMatrix(rot, m);
    std::memcpy(out.rot_inv, m, sizeof(m));
    return true;
}

nlohmann::json LaserVectorEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ild_path"] = path_.toStdString();
    j["beam_thickness"] = beam_thickness_;
    j["fps"] = fps_;
    j["flatten_z"] = flatten_z_;
    j["loop"] = loop_;
    j["projector_beams"] = projector_beams_;
    j["projector_depth"] = projector_depth_;
    j["max_beams"] = max_beams_;
    j["join_lit_path"] = join_lit_path_;
    j["scanner_filter"] = scanner_filter_;
    j["machine_profile"] = static_cast<int>(machine_profile_);
    j["intensity_widens"] = intensity_widens_;
    j["dwell_widens"] = dwell_widens_;
    j["width_dynamics"] = width_dynamics_;
    j["crisp_beams"] = crisp_beams_;
    return j;
}

void LaserVectorEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("beam_thickness") && settings["beam_thickness"].is_number())
    {
        beam_thickness_ = std::clamp(settings["beam_thickness"].get<float>(), 0.005f, 0.40f);
    }
    if(settings.contains("fps") && settings["fps"].is_number_integer())
    {
        fps_ = std::clamp(settings["fps"].get<int>(), 1, 120);
    }
    if(settings.contains("flatten_z") && settings["flatten_z"].is_boolean())
    {
        flatten_z_ = settings["flatten_z"].get<bool>();
    }
    if(settings.contains("loop") && settings["loop"].is_boolean())
    {
        loop_ = settings["loop"].get<bool>();
    }
    if(settings.contains("projector_beams") && settings["projector_beams"].is_boolean())
    {
        projector_beams_ = settings["projector_beams"].get<bool>();
    }
    if(settings.contains("projector_depth") && settings["projector_depth"].is_number())
    {
        projector_depth_ = std::clamp(settings["projector_depth"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("max_beams") && settings["max_beams"].is_number_integer())
    {
        max_beams_ = std::clamp(settings["max_beams"].get<int>(), 0, 128);
    }
    if(settings.contains("join_lit_path") && settings["join_lit_path"].is_boolean())
    {
        join_lit_path_ = settings["join_lit_path"].get<bool>();
    }
    if(settings.contains("scanner_filter") && settings["scanner_filter"].is_number_integer())
    {
        const int s = settings["scanner_filter"].get<int>();
        scanner_filter_ = (s < 0) ? -1 : std::clamp(s, 0, 255);
    }
    if(settings.contains("machine_profile") && settings["machine_profile"].is_number_integer())
    {
        const int m = settings["machine_profile"].get<int>();
        if(m >= static_cast<int>(MachineProfile::Rgb) && m <= static_cast<int>(MachineProfile::BeamPunch))
        {
            machine_profile_ = static_cast<MachineProfile>(m);
        }
    }
    if(settings.contains("intensity_widens") && settings["intensity_widens"].is_boolean())
    {
        intensity_widens_ = settings["intensity_widens"].get<bool>();
    }
    if(settings.contains("dwell_widens") && settings["dwell_widens"].is_boolean())
    {
        dwell_widens_ = settings["dwell_widens"].get<bool>();
    }
    if(settings.contains("width_dynamics") && settings["width_dynamics"].is_number())
    {
        width_dynamics_ = std::clamp(settings["width_dynamics"].get<float>(), 1.0f, 5.0f);
    }
    if(settings.contains("crisp_beams") && settings["crisp_beams"].is_boolean())
    {
        crisp_beams_ = settings["crisp_beams"].get<bool>();
    }
    if(settings.contains("ild_path") && settings["ild_path"].is_string())
    {
        const std::string raw = settings["ild_path"].get<std::string>();
        // Fail closed on absurd path strings from settings JSON.
        if(!raw.empty() && raw.size() <= 4096u)
        {
            LoadIldPath(QString::fromStdString(raw));
        }
    }
    else
    {
        RebuildShowGeometry();
    }
    if(thickness_row_)
    {
        thickness_row_->syncSpinValue(beam_thickness_);
    }
    if(width_dyn_row_)
    {
        width_dyn_row_->syncSpinValue(width_dynamics_);
    }
    if(projector_depth_row_)
    {
        projector_depth_row_->syncSpinValue(projector_depth_);
    }
    if(fps_row_)
    {
        fps_row_->syncSliderValue(fps_);
    }
    if(max_beams_row_)
    {
        max_beams_row_->syncSliderValue(max_beams_);
    }
    if(machine_combo_)
    {
        const QSignalBlocker block(machine_combo_);
        const int want = static_cast<int>(machine_profile_);
        int idx = 0;
        for(int i = 0; i < machine_combo_->count(); ++i)
        {
            if(machine_combo_->itemData(i).toInt() == want)
            {
                idx = i;
                break;
            }
        }
        machine_combo_->setCurrentIndex(idx);
    }
    if(projector_check_)
    {
        projector_check_->setChecked(projector_beams_);
    }
    if(join_path_check_)
    {
        join_path_check_->setChecked(join_lit_path_);
    }
    if(intensity_widen_check_)
    {
        intensity_widen_check_->setChecked(intensity_widens_);
    }
    if(dwell_widen_check_)
    {
        dwell_widen_check_->setChecked(dwell_widens_);
    }
    if(crisp_beams_check_)
    {
        crisp_beams_check_->setChecked(crisp_beams_);
    }
    if(flatten_check_)
    {
        flatten_check_->setChecked(flatten_z_);
    }
    if(loop_check_)
    {
        loop_check_->setChecked(loop_);
    }
}
