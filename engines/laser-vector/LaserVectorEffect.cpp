// SPDX-License-Identifier: GPL-2.0-only

#include "LaserVectorEffect.h"

#include "HebReader.h"
#include "ShowFolderReader.h"
#include "LaserVectorVolumeBody.h"
#include "EffectListManager3D.h"
#include "EffectDoubleSliderRow.h"
#include "EffectSliderRow.h"
#include "Geometry3DUtils.h"
#include "GpuRoomGridOverlay.h"
#include "PlayerEngines.h"
#include "SpatialEffectTypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

namespace
{
constexpr SpatialEffectType kLaserVectorEffectType = static_cast<SpatialEffectType>(40);
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
    volume_assist_.setFragmentBody(QString::fromUtf8(LaserVectorVolumeBody()));
    volume_assist_.setResolution(SpatialVolumeFieldEngine::kMaxResolution);
}

EffectInfo3D LaserVectorEffect::GetEffectInfo() const
{
    EffectInfo3D info = {};
    info.effect_name = "Laser Vector";
    info.effect_description =
        "Play ILDA (.ild), ShowEditor figures (.heb), or a locked show folder (.shw timeline) as projector beams. "
        "Show folders use Key.ord + .seq + .heb; multi-Spur shows throw from separate room origins.";
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

void LaserVectorEffect::OnBrowseIld()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open laser file or show"),
        path_.isEmpty() ? QString() : QFileInfo(path_).absolutePath(),
        tr("Laser / ShowEditor (*.ild *.heb *.shw);;ILDA (*.ild);;ShowEditor HEB (*.heb);;"
           "ShowEditor show (*.shw);;All files (*.*)"));
    if(path.isEmpty())
    {
        return;
    }
    LoadLaserPath(path);
    emit ParametersChanged();
}

void LaserVectorEffect::OnClearIld()
{
    if(path_.isEmpty() && show_.frames.empty() && !use_show_folder_)
    {
        return;
    }
    path_.clear();
    show_ = LaserShow{};
    show_folder_ = ShowFolder{};
    use_show_folder_ = false;
    show_frame_ = LaserFrame{};
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

void LaserVectorEffect::LoadLaserPath(const QString& path)
{
    path_ = path;
    use_show_folder_ = false;
    show_folder_ = ShowFolder{};
    show_ = LaserShow{};
    const QString ext = QFileInfo(path).suffix().toLower();
    if(ext == QLatin1String("shw"))
    {
        show_folder_ = ShowFolderReader::LoadPath(path.toStdString());
        use_show_folder_ = true;
        if(!show_folder_.error.empty())
        {
            show_.error = show_folder_.error;
        }
    }
    else if(ext == QLatin1String("heb"))
    {
        show_ = HebReader::LoadFile(path.toStdString());
    }
    else
    {
        show_ = IldaReader::LoadFile(path.toStdString());
    }
    RebuildShowGeometry();
    if(scanner_combo_)
    {
        const QSignalBlocker block(scanner_combo_);
        scanner_combo_->clear();
        if(use_show_folder_)
        {
            scanner_combo_->addItem(tr("All Spur / tracks"), -1);
            for(int t : show_folder_.tracks)
            {
                scanner_combo_->addItem(tr("Spur / track %1").arg(t), t);
            }
            scanner_combo_->setToolTip(
                tr("ShowEditor Spur (output track). Encoded in .seq as 1+track×19.\n"
                   "Multi projector origins place each Spur across the room; filter solos one head."));
        }
        else
        {
            scanner_combo_->addItem(tr("All scanners"), -1);
            for(int s : show_.scanners)
            {
                scanner_combo_->addItem(tr("Scanner %1").arg(s), s);
            }
            scanner_combo_->setToolTip(
                tr("ILDA header “scanner head” (byte 31): which scanner pair a frame is for.\n"
                   "Multi projector origins spread heads left→right when All is selected."));
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
    if(use_show_folder_)
    {
        for(auto& kv : show_folder_.figures)
        {
            for(LaserFrame& frame : kv.second.frames)
            {
                frame.RebuildSegments(join_lit_path_);
            }
        }
        return;
    }
    for(LaserFrame& frame : show_.frames)
    {
        frame.RebuildSegments(join_lit_path_);
    }
}

void LaserVectorEffect::RebuildPlayIndices()
{
    play_indices_.clear();
    if(use_show_folder_)
    {
        return;
    }
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
    if(use_show_folder_)
    {
        if(!show_folder_.error.empty())
        {
            status_label_->setText(tr("Error: %1").arg(QString::fromStdString(show_folder_.error)));
            return;
        }
        QString tracks;
        for(int t : show_folder_.tracks)
        {
            if(!tracks.isEmpty())
            {
                tracks += QLatin1Char(',');
            }
            tracks += QString::number(t);
        }
        QString line =
            tr("ShowEditor “%1”: %2 cues, %3 figures, Spur %4, %5 s")
                .arg(QString::fromStdString(show_folder_.show_name))
                .arg(show_folder_.events.size())
                .arg(show_folder_.figures.size())
                .arg(tracks.isEmpty() ? QStringLiteral("0") : tracks)
                .arg(show_folder_.duration_ms / 1000.0, 0, 'f', 1);
        if(show_folder_.tracks.size() > 1)
        {
            line += multi_projectors_
                        ? tr(" — multi projector origins on")
                        : tr(" — multi projector origins off (stacked)");
        }
        if(!show_folder_.info_notes.empty())
        {
            QString note = QString::fromStdString(show_folder_.info_notes);
            note.replace(QLatin1Char('\n'), QLatin1Char(' '));
            if(note.size() > 120)
            {
                note = note.left(117) + QStringLiteral("...");
            }
            line += QLatin1Char('\n') + note;
        }
        status_label_->setText(line);
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
    if(use_show_folder_)
    {
        if(!show_folder_.ok())
        {
            return nullptr;
        }
        const float speed_mul = (GetSpeed() == 0) ? 0.0f : (static_cast<float>(GetSpeed()) / 100.0f);
        double t_ms = static_cast<double>(std::max(0.0f, time)) * 1000.0 * static_cast<double>(speed_mul);
        if(loop_ && show_folder_.duration_ms > 1.0)
        {
            t_ms = std::fmod(t_ms, show_folder_.duration_ms);
            if(t_ms < 0.0)
            {
                t_ms += show_folder_.duration_ms;
            }
        }
        show_frame_ = show_folder_.FrameAt(t_ms, static_cast<float>(fps_), scanner_filter_);
        if(show_frame_.segments.empty() && show_frame_.dots.empty()
           && show_frame_.points.empty())
        {
            return nullptr;
        }
        return &show_frame_;
    }

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

std::uint32_t LaserVectorEffect::PackNorm01Key(float n1, float n2, float n3)
{
    const auto q = [](float v) -> std::uint32_t {
        return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 1023.0f)) & 1023u;
    };
    return (q(n1) << 20) | (q(n2) << 10) | q(n3);
}

std::vector<int> LaserVectorEffect::ActiveProjectorTracks() const
{
    std::vector<int> tracks;
    if(use_show_folder_ && !show_folder_.tracks.empty())
    {
        tracks = show_folder_.tracks;
    }
    else if(!show_.scanners.empty())
    {
        tracks = show_.scanners;
    }
    else
    {
        tracks.push_back(0);
    }
    /* Keep only slots the GPU origin table can address. */
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                                [](int t) { return t < 0 || t >= kMaxProjectorTracks; }),
                 tracks.end());
    if(tracks.empty())
    {
        tracks.push_back(0);
    }
    std::sort(tracks.begin(), tracks.end());
    tracks.erase(std::unique(tracks.begin(), tracks.end()), tracks.end());
    return tracks;
}

void LaserVectorEffect::FillGpuParams(float* params, int stroke_count) const
{
    const float depth = std::clamp(projector_depth_, 0.0f, 1.0f);
    const float spread = std::clamp(projector_spread_, 0.0f, 1.0f);
    params[0] = 0.5f;
    params[1] = 0.5f;
    params[2] = depth;
    params[3] = beam_thickness_;
    params[4] = width_dynamics_;
    params[5] = show_beams_ ? 1.0f : 0.0f;
    params[6] = crisp_beams_ ? 1.0f : 0.0f;
    params[7] = intensity_widens_ ? 1.0f : 0.0f;
    params[8] = dwell_widens_ ? 1.0f : 0.0f;
    params[9] = static_cast<float>(stroke_count);
    params[10] = static_cast<float>(static_cast<int>(machine_profile_));
    params[11] = static_cast<float>(effect_brightness) / 100.0f;
    params[12] = (max_beams_ > 0) ? static_cast<float>(max_beams_) : 0.0f;
    params[13] = flatten_z_ ? 1.0f : 0.0f;
    params[14] = show_graphic_ ? 1.0f : 0.0f;
    params[15] = std::clamp(fog_, 0.0f, 1.0f);
    params[16] = std::clamp(transparency_, 0.0f, 1.0f);
    params[17] = std::clamp(beam_gain_, 0.0f, 1.0f);
    params[18] = std::clamp(screen_, 0.0f, 1.0f);

    const std::vector<int> tracks = ActiveProjectorTracks();
    const bool multi = multi_projectors_ && tracks.size() > 1;
    params[19] = multi ? 1.0f : 0.0f;

    /* Default every Spur slot to the shared centre; overwrite active tracks evenly. */
    for(int t = 0; t < kMaxProjectorTracks; ++t)
    {
        const int b = 20 + t * 3;
        params[b] = 0.5f;
        params[b + 1] = 0.5f;
        params[b + 2] = depth;
    }
    const int n = static_cast<int>(tracks.size());
    for(int i = 0; i < n; ++i)
    {
        const int t = tracks[static_cast<size_t>(i)];
        const float u = (n <= 1) ? 0.5f : static_cast<float>(i) / static_cast<float>(n - 1);
        const float x_full = 0.08f + 0.84f * u;
        const float x = 0.5f + (x_full - 0.5f) * spread;
        const int b = 20 + t * 3;
        params[b] = x;
        params[b + 1] = 0.5f;
        params[b + 2] = depth;
    }
}

int LaserVectorEffect::UploadGpuStrokeMedia(const LaserFrame& frame)
{
    const size_t seg_n = frame.segments.size();
    const size_t dot_n = frame.dots.size();
    size_t total = seg_n + dot_n;
    if(total == 0)
    {
        volume_assist_.clearMediaTexture();
        gpu_stroke_count_ = 0;
        return 0;
    }
    if(total > static_cast<size_t>(kGpuMaxStrokes))
    {
        total = static_cast<size_t>(kGpuMaxStrokes);
    }

    /* width=3: xy ends | z ends | rgb+dwell — texelFetch ignores filter. */
    QImage img(3, static_cast<int>(total), QImage::Format_RGBA8888);
    img.fill(Qt::black);

    auto encLaser = [](float v) -> unsigned char {
        return static_cast<unsigned char>(
            std::lround(std::clamp(0.5f + 0.5f * v, 0.0f, 1.0f) * 255.0f));
    };
    auto enc01 = [](float v) -> unsigned char {
        return static_cast<unsigned char>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    auto writeStroke = [&](int row,
                           float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           float r, float g, float b, float dwell,
                           int track) {
        unsigned char* base = img.scanLine(row);
        unsigned char* xy = base;
        xy[0] = encLaser(x0);
        xy[1] = encLaser(y0);
        xy[2] = encLaser(x1);
        xy[3] = encLaser(y1);
        unsigned char* zz = base + 4;
        zz[0] = encLaser(z0);
        zz[1] = encLaser(z1);
        /* Origin table covers Spur 0..11; clamp so texelFetch track matches params. */
        zz[2] = static_cast<unsigned char>(std::clamp(track, 0, kMaxProjectorTracks - 1));
        zz[3] = 0;
        unsigned char* rgb = base + 8;
        rgb[0] = enc01(r);
        rgb[1] = enc01(g);
        rgb[2] = enc01(b);
        rgb[3] = enc01(std::clamp((dwell - 1.0f) / 15.0f, 0.0f, 1.0f));
    };

    auto strokeTrack = [&](int tagged) -> int {
        /* ShowFolder tags Spur on each stroke; ILDA uses frame scanner head. */
        if(use_show_folder_)
        {
            return tagged;
        }
        return frame.scanner;
    };

    size_t row = 0;
    const size_t seg_limit = std::min(seg_n, total);
    for(size_t i = 0; i < seg_limit; ++i)
    {
        const LaserSegment& s = frame.segments[i];
        writeStroke(static_cast<int>(row),
                    s.x0, s.y0, s.z0, s.x1, s.y1, s.z1,
                    s.r, s.g, s.b, s.dwell,
                    strokeTrack(s.track));
        ++row;
    }
    for(size_t i = 0; i < dot_n && row < total; ++i)
    {
        const LaserDot& d = frame.dots[i];
        writeStroke(static_cast<int>(row),
                    d.x, d.y, d.z, d.x, d.y, d.z,
                    d.r, d.g, d.b, d.dwell,
                    strokeTrack(d.track));
        ++row;
    }

    volume_assist_.setMediaTexture(img, false);
    gpu_stroke_count_ = static_cast<int>(row);
    return gpu_stroke_count_;
}

QVector3D LaserVectorEffect::SampleGpuField01(float n1, float n2, float n3)
{
    const std::uint32_t key = PackNorm01Key(n1, n2, n3);
    const auto it = true_sample_map_.find(key);
    if(it != true_sample_map_.end())
    {
        return it->second;
    }
    return volume_assist_.sampleTrue01(n1, n2, n3);
}

void LaserVectorEffect::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    true_sample_map_.clear();

    const LaserFrame* frame = CurrentFrame(time_sec);
    if(!frame || (frame->segments.empty() && frame->dots.empty()))
    {
        volume_assist_.clearMediaTexture();
        gpu_stroke_count_ = 0;
        return;
    }

    const int strokes = UploadGpuStrokeMedia(*frame);
    float params[SpatialVolumeFieldEngine::kMaxParams] = {};
    FillGpuParams(params, strokes);
    volume_assist_.prepareProgram(render_sequence, time_sec, params, kGpuParamCount);
}

void LaserVectorEffect::BakeTrueGpuVolumeSamples(const float* sample_xyz,
                                                 int count,
                                                 float /*time*/,
                                                 const GridContext3D& grid)
{
    true_sample_map_.clear();
    if(!sample_xyz || count <= 0 || !volume_assist_.isAvailable() || gpu_stroke_count_ <= 0)
    {
        return;
    }

    std::unordered_map<std::uint32_t, int> unique_index;
    unique_index.reserve(static_cast<size_t>(std::max(16, count / 2)));
    std::vector<float> n01;
    n01.reserve(static_cast<size_t>(count) * 3u);
    for(int i = 0; i < count; ++i)
    {
        const float x = sample_xyz[static_cast<size_t>(i) * 3u + 0u];
        const float y = sample_xyz[static_cast<size_t>(i) * 3u + 1u];
        const float z = sample_xyz[static_cast<size_t>(i) * 3u + 2u];
        float n1 = 0.5f, n2 = 0.5f, n3 = 0.5f;
        if(!SampleVolumeLocalUv01(x, y, z, grid, &n1, &n2, &n3))
        {
            continue;
        }
        const std::uint32_t key = PackNorm01Key(n1, n2, n3);
        if(unique_index.find(key) != unique_index.end())
        {
            continue;
        }
        const int idx = static_cast<int>(unique_index.size());
        unique_index.emplace(key, idx);
        n01.push_back(n1);
        n01.push_back(n2);
        n01.push_back(n3);
    }
    const int unique = static_cast<int>(unique_index.size());
    if(unique <= 0)
    {
        return;
    }

    std::vector<float> rgb(static_cast<size_t>(unique) * 3u, 0.0f);
    if(!volume_assist_.evaluateAtPoints(n01.data(), unique, rgb.data()))
    {
        return;
    }
    true_sample_map_.reserve(static_cast<size_t>(unique));
    for(const auto& kv : unique_index)
    {
        const int i = kv.second;
        /* volumeMain packs B,G,R — convert to display R,G,B for LED ToRGBColor. */
        true_sample_map_[kv.first] = QVector3D(
            rgb[static_cast<size_t>(i) * 3u + 2u],
            rgb[static_cast<size_t>(i) * 3u + 1u],
            rgb[static_cast<size_t>(i) * 3u + 0u]);
    }
}

RGBColor LaserVectorEffect::CalculateColorGrid(float x, float y, float z, float /*time*/, const GridContext3D& grid)
{
    if(gpu_stroke_count_ <= 0)
    {
        return ToRGBColor(0, 0, 0);
    }

    float u = 0.5f, v = 0.5f, w = 0.5f;
    if(!SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
    {
        return ToRGBColor(0, 0, 0);
    }

    const QVector3D samp = SampleGpuField01(u, v, w);
    return ToRGBColor(
        static_cast<int>(std::clamp(samp.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
        static_cast<int>(std::clamp(samp.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
        static_cast<int>(std::clamp(samp.z(), 0.0f, 1.0f) * 255.0f + 0.5f));
}

bool LaserVectorEffect::TryFillGpuRoomGridOverlayLayer(GpuRoomGridOverlayLayer& out,
                                                      const GridContext3D& grid,
                                                      float /*time_sec*/)
{
    if(gpu_stroke_count_ <= 0)
    {
        return false;
    }

    std::vector<unsigned char> rgba;
    int n = 0;
    if(!volume_assist_.exportAtlasRgba8(rgba, n)
       || n < 2
       || rgba.size() < static_cast<size_t>(n) * static_cast<size_t>(n) * static_cast<size_t>(n) * 4u)
    {
        return false;
    }

    out = GpuRoomGridOverlayLayer{};
    out.atlas_tex = 0;
    out.atlas_rgba = std::move(rgba);
    out.atlas_n = n;
    out.atlas_nearest = true; /* no LINEAR smear — spokes must stay saturated */
    out.brightness = 1.0f;
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

    float hx = 1.0f, hy = 1.0f, hz = 1.0f;
    GetVolumeHalfExtents(grid, hx, hy, hz);
    out.half_ext[0] = hx;
    out.half_ext[1] = hy;
    out.half_ext[2] = hz;
    out.room_min[0] = grid.min_x;
    out.room_min[1] = grid.min_y;
    out.room_min[2] = grid.min_z;
    out.room_max[0] = grid.max_x;
    out.room_max[1] = grid.max_y;
    out.room_max[2] = grid.max_z;

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
    j["show_beams"] = show_beams_;
    j["show_graphic"] = show_graphic_;
    j["projector_depth"] = projector_depth_;
    j["multi_projectors"] = multi_projectors_;
    j["projector_spread"] = projector_spread_;
    j["fog"] = fog_;
    j["transparency"] = transparency_;
    j["beam_gain"] = beam_gain_;
    j["screen"] = screen_;
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
    if(settings.contains("show_beams") && settings["show_beams"].is_boolean())
    {
        show_beams_ = settings["show_beams"].get<bool>();
    }
    if(settings.contains("show_graphic") && settings["show_graphic"].is_boolean())
    {
        show_graphic_ = settings["show_graphic"].get<bool>();
    }
    if(settings.contains("projector_depth") && settings["projector_depth"].is_number())
    {
        projector_depth_ = std::clamp(settings["projector_depth"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("multi_projectors") && settings["multi_projectors"].is_boolean())
    {
        multi_projectors_ = settings["multi_projectors"].get<bool>();
    }
    if(settings.contains("projector_spread") && settings["projector_spread"].is_number())
    {
        projector_spread_ = std::clamp(settings["projector_spread"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("fog") && settings["fog"].is_number())
    {
        fog_ = std::clamp(settings["fog"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("transparency") && settings["transparency"].is_number())
    {
        transparency_ = std::clamp(settings["transparency"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("beam_gain") && settings["beam_gain"].is_number())
    {
        beam_gain_ = std::clamp(settings["beam_gain"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("screen") && settings["screen"].is_number())
    {
        screen_ = std::clamp(settings["screen"].get<float>(), 0.0f, 1.0f);
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
            LoadLaserPath(QString::fromStdString(raw));
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
    if(projector_spread_row_)
    {
        projector_spread_row_->syncSpinValue(projector_spread_);
    }
    if(fog_row_)
    {
        fog_row_->syncSpinValue(fog_);
    }
    if(transparency_row_)
    {
        transparency_row_->syncSpinValue(transparency_);
    }
    if(beam_gain_row_)
    {
        beam_gain_row_->syncSpinValue(beam_gain_);
    }
    if(screen_row_)
    {
        screen_row_->syncSpinValue(screen_);
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
    if(beam_check_)
    {
        beam_check_->setChecked(show_beams_);
    }
    if(graphic_check_)
    {
        graphic_check_->setChecked(show_graphic_);
    }
    if(multi_projectors_check_)
    {
        multi_projectors_check_->setChecked(multi_projectors_);
    }
    if(projector_spread_row_)
    {
        projector_spread_row_->setEnabled(multi_projectors_);
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
