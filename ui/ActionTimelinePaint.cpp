// SPDX-License-Identifier: GPL-2.0-only

#include "ActionTimelineWidget.h"
#include "ActionCatalog.h"
#include "Actions/ActionApplier.h"
#include "Actions/ActionBuiltinGpu.h"
#include "Actions/ActionMedia.h"
#include "Actions/ActionScript.h"
#include "Actions/ActionParticleField.h"
#include "Actions/ActionRibbonField.h"
#include "Actions/ActionVolumeField.h"
#include "SpatialOffscreenGlPool.h"
#include "ZoneManager3D.h"

#include <cstdint>

#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QSet>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

quint32 MixHash(quint32 h, quint32 v)
{
    h ^= v;
    h *= 16777619u;
    return h;
}

quint32 HashFloatBits(quint32 h, float v)
{
    quint32 bits = 0;
    static_assert(sizeof(bits) == sizeof(v), "float size");
    std::memcpy(&bits, &v, sizeof(bits));
    return MixHash(h, bits);
}

quint32 HashClipLook(const Action::Clip& sample)
{
    quint32 h = 2166136261u;
    h = HashFloatBits(h, sample.speed);
    h = HashFloatBits(h, sample.intensity);
    h = HashFloatBits(h, sample.min_intensity);
    h = HashFloatBits(h, sample.max_intensity);
    h = HashFloatBits(h, sample.pulse_length);
    h = HashFloatBits(h, sample.origin_offset_x);
    h = HashFloatBits(h, sample.origin_offset_y);
    h = HashFloatBits(h, sample.origin_offset_z);
    h = HashFloatBits(h, sample.effect_size);
    h = MixHash(h, (quint32)sample.start_ms);
    h = MixHash(h, (quint32)sample.end_ms);
    h = MixHash(h, (quint32)sample.period_ms);
    h = MixHash(h, (quint32)sample.color);
    h = MixHash(h, (quint32)sample.color_from);
    h = MixHash(h, (quint32)sample.color_to);
    h = MixHash(h, (quint32)sample.direction);
    h = MixHash(h, (quint32)sample.reverse);
    h = MixHash(h, (quint32)sample.flip_h);
    h = MixHash(h, (quint32)sample.flip_v);
    h = MixHash(h, (quint32)sample.rotate_quarters);
    h = MixHash(h, (quint32)sample.play_bounds);
    for(char c : sample.origin_ref)
    {
        h = MixHash(h, (quint32)(unsigned char)c);
    }
    for(const Action::GradientStop& s : sample.gradient)
    {
        h = HashFloatBits(h, s.pos);
        h = MixHash(h, (quint32)s.color);
    }
    for(const Action::CurvePoint& s : sample.intensity_curve)
    {
        h = HashFloatBits(h, s.pos);
        h = HashFloatBits(h, s.value);
    }
    for(const Action::CurvePoint& s : sample.period_curve)
    {
        h = HashFloatBits(h, s.pos);
        h = HashFloatBits(h, s.value);
    }
    return h;
}

} // namespace

QString ActionTimelineWidget::makeGpuThumbCacheKey(const Action::Clip& sample,
                                                    const Action::Target& target,
                                                    int sample_w,
                                                    int rows,
                                                    int led_n,
                                                    const std::vector<float>& xyz) const
{
    quint32 xyz_hash = 2166136261u;
    for(float v : xyz)
    {
        xyz_hash = HashFloatBits(xyz_hash, v);
    }
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(QString::fromStdString(Action::ClipFileId(sample)))
        .arg(targetKey(target))
        .arg(sample_w)
        .arg(rows)
        .arg(led_n)
        .arg(HashClipLook(sample), 0, 16)
        .arg(xyz_hash, 0, 16);
}

void ActionTimelineWidget::enqueueGpuThumb(GpuThumbRequest req) const
{
    if(req.key.isEmpty() || gpu_thumb_queued_keys_.contains(req.key))
    {
        return;
    }
    gpu_thumb_queued_keys_.insert(req.key);
    // Newest clip first — add/remove should light the clip you just touched ASAP.
    gpu_thumb_queue_.prepend(std::move(req));
    if(gpu_thumb_pump_scheduled_)
    {
        return;
    }
    gpu_thumb_pump_scheduled_ = true;
    QTimer::singleShot(0, const_cast<ActionTimelineWidget*>(this), &ActionTimelineWidget::pumpGpuThumbQueue);
}

QImage ActionTimelineWidget::renderGpuEngineThumb(const GpuThumbRequest& req) const
{
    QImage img(req.sample_w, req.rows, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    if(req.xyz.empty() || req.sample_w <= 0 || req.rows <= 0)
    {
        return img;
    }

    const int n = (int)req.xyz.size() / 3;
    if(n <= 0 || n != req.rows)
    {
        return img;
    }

    Action::ClipOrigin01 origin;
    if(room_bounds_.has_world_bounds)
    {
        origin = Action::ResolveClipOrigin01(req.clip,
                                            room_bounds_.min_x, room_bounds_.max_x,
                                            room_bounds_.min_y, room_bounds_.max_y,
                                            room_bounds_.min_z, room_bounds_.max_z,
                                            room_bounds_.reference_points);
    }
    else
    {
        origin = Action::ResolveClipOrigin01(req.clip, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, {});
    }
    Action::SetActiveClipOrigin01(origin);
    Action::volume_field::SetSampleOrigin01(origin.ox, origin.oy, origin.oz, origin.size);
    Action::particle_field::SetSampleOrigin01(origin.ox, origin.oy, origin.oz, origin.size);
    Action::ribbon_field::SetSampleOrigin01(origin.ox, origin.oy, origin.oz, origin.size);

    const int dur = std::max(1, req.clip.end_ms - req.clip.start_ms);
    const int time_keys = std::max(4, std::min(req.sample_w, 8));
    static std::uint64_t s_timeline_gpu_seq = 0;
    std::vector<float> rgb((size_t)n * 3u, 0.0f);
    std::vector<float> xyz = req.xyz;

    for(int k = 0; k < time_keys; ++k)
    {
        const float t = (time_keys <= 1) ? 0.0f : (float)k / (float)(time_keys - 1);
        const int ms = req.clip.start_ms + (int)std::lround(t * (float)dur);
        const int sample_ms = std::min(std::max(ms, req.clip.start_ms), req.clip.end_ms - 1);
        const std::uint64_t seq = ++s_timeline_gpu_seq;
        bool prepared = false;
        if(req.field)
        {
            prepared = Action::volume_field::PrepareForClip(req.clip, sample_ms, seq);
        }
        else if(req.particle)
        {
            prepared = Action::particle_field::PrepareForClip(req.clip, sample_ms, seq);
        }
        else if(req.ribbon)
        {
            prepared = Action::ribbon_field::PrepareForClip(req.clip, sample_ms, seq);
        }
        else if(req.builtin)
        {
            prepared = Action::builtin_gpu::PrepareForClip(req.clip, sample_ms, seq);
        }
        if(!prepared)
        {
            break;
        }

        bool ok = false;
        if(req.field)
        {
            ok = Action::volume_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        else if(req.particle)
        {
            ok = Action::particle_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        else if(req.ribbon)
        {
            ok = Action::ribbon_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        else if(req.builtin)
        {
            ok = Action::builtin_gpu::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        if(!ok)
        {
            break;
        }

        float intensity = std::clamp(req.clip.intensity, 0.0f, 1.0f);
        Action::ModulateClipIntensity(req.clip, Action::ClipSpanProgress(req.clip, sample_ms),
                                      Action::ClipPeriodProgress(req.clip, sample_ms), &intensity);
        const int x0 = (k * req.sample_w) / time_keys;
        const int x1 = ((k + 1) * req.sample_w) / time_keys;
        for(int r = 0; r < n; ++r)
        {
            const float rr = rgb[(size_t)r * 3u + 0u];
            const float gg = rgb[(size_t)r * 3u + 1u];
            const float bb = rgb[(size_t)r * 3u + 2u];
            const float peak = std::max(rr, std::max(gg, bb));
            QColor pix(0, 0, 0);
            if(peak * intensity >= 0.02f)
            {
                pix = QColor(
                    (int)std::lround(std::clamp(rr * intensity, 0.0f, 1.0f) * 255.0f),
                    (int)std::lround(std::clamp(gg * intensity, 0.0f, 1.0f) * 255.0f),
                    (int)std::lround(std::clamp(bb * intensity, 0.0f, 1.0f) * 255.0f));
            }
            for(int x = x0; x < x1; ++x)
            {
                img.setPixelColor(x, r, pix);
            }
        }
    }
    return img;
}

void ActionTimelineWidget::pumpGpuThumbQueue()
{
    gpu_thumb_pump_scheduled_ = false;
    if(gpu_thumb_queue_.isEmpty())
    {
        return;
    }

    // Host GL not live yet (editor opened before viewport init) — retry shortly.
    if(!SpatialOffscreenGlPool::hostContextReady())
    {
        gpu_thumb_pump_scheduled_ = true;
        QTimer::singleShot(32, this, &ActionTimelineWidget::pumpGpuThumbQueue);
        return;
    }

    // Spend a short budget filling thumbs without freezing the UI thread.
    // Still uses the single shared offscreen pool (no second context).
    QElapsedTimer budget;
    budget.start();
    constexpr qint64 kBudgetMs = 12;
    bool any = false;
    while(!gpu_thumb_queue_.isEmpty() && budget.elapsed() < kBudgetMs)
    {
        GpuThumbRequest req = std::move(gpu_thumb_queue_.front());
        gpu_thumb_queue_.pop_front();
        gpu_thumb_queued_keys_.remove(req.key);

        if(!gpu_thumb_cache_.contains(req.key))
        {
            if(gpu_thumb_cache_.size() >= 64)
            {
                gpu_thumb_cache_.clear();
            }
            gpu_thumb_cache_.insert(req.key, renderGpuEngineThumb(req));
            any = true;
        }
    }

    if(any)
    {
        requestContentRepaint();
    }
    if(!gpu_thumb_queue_.isEmpty())
    {
        gpu_thumb_pump_scheduled_ = true;
        QTimer::singleShot(0, this, &ActionTimelineWidget::pumpGpuThumbQueue);
    }
}

void ActionTimelineWidget::paintClipSpatialRaster(QPainter& p, const QRect& br, const PaintClip& pb,
                                                       const Action::Clip& sample) const
{
    // True black base so off / zero-intensity LEDs read as black (not tinted effect colour).
    p.fillRect(br, QColor(0, 0, 0));

    std::vector<float> axes;
    std::vector<int> seeds;
    std::vector<float> nxs, nys, nzs;
    if(pack_ && transforms_ && !pb.single_led_row)
    {
        Action::BuildSpatialAxesForTarget(*pack_, pb.view_target, sample,
                                              transforms_, &axes, &seeds, &nxs, &nys, &nzs,
                                              zone_manager_,
                                              room_bounds_.has_world_bounds ? &room_bounds_ : nullptr);
    }

    std::vector<int> order;
    if(!axes.empty() && axes.size() == seeds.size())
    {
        order.resize((int)axes.size());
        for(int i = 0; i < (int)order.size(); ++i)
        {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return axes[(size_t)a] < axes[(size_t)b];
        });
    }

    const int led_n = pb.single_led_row
        ? 1
        : (!order.empty() ? (int)order.size() : std::max(1, pb.led_count > 1 ? pb.led_count : 24));

    const int dur = std::max(1, sample.end_ms - sample.start_ms);
    const std::string effect_id = Action::ClipFileId(sample);
    const bool world_eval = Action::ActionScriptUsesWorld(effect_id)
        || Action::IsMediaClip(sample);
    const bool field_eval = Action::script::UsesField(effect_id);
    const bool particle_eval = Action::script::UsesParticles(effect_id);
    const bool ribbon_eval = Action::script::UsesRibbons(effect_id);
    const bool media_eval = Action::IsMediaClip(sample);
    const bool builtin_eval = Action::builtin_gpu::Uses(effect_id)
        && Action::script::FieldShader(effect_id).empty();
    const bool gpu_engine = field_eval || particle_eval || ribbon_eval || builtin_eval;

    // GPU engines — best path under the single shared offscreen GL pool:
    // batched EvaluateAtPoints, low-res, cached, deferred off paintEvent.
    // Fail closed to black (no gradient / CPU fake).
    if(gpu_engine)
    {
        constexpr int kGpuMaxW = 32;
        constexpr int kGpuMaxRows = 8;
        const int max_rows = pb.single_led_row ? 1 : std::max(2, std::min(br.height(), kGpuMaxRows));
        int skip = (led_n > max_rows) ? std::max(1, (led_n + max_rows - 1) / max_rows) : 1;
        if(skip > 1 && (skip % 2) == 0)
        {
            ++skip;
        }
        const int rows = pb.single_led_row ? 1 : std::max(1, (led_n + skip - 1) / skip);
        const int sample_w = std::max(1, std::min(br.width(), kGpuMaxW));

        std::vector<int> row_led;
        row_led.reserve((size_t)rows);
        for(int led = 0; led < led_n && (int)row_led.size() < rows; led += skip)
        {
            row_led.push_back(led);
        }
        const int n = (int)row_led.size();
        std::vector<float> xyz((size_t)n * 3u);
        for(int r = 0; r < n; ++r)
        {
            const int led_slot = row_led[(size_t)r];
            float nx = 0.5f;
            float ny = 0.5f;
            float nz = 0.5f;
            if(!order.empty() && !nxs.empty())
            {
                const int idx = order[(size_t)std::clamp(led_slot, 0, (int)order.size() - 1)];
                if(idx < (int)nxs.size())
                {
                    nx = nxs[(size_t)idx];
                    ny = nys[(size_t)idx];
                    nz = nzs[(size_t)idx];
                }
            }
            else if(pb.single_led_row)
            {
                const float u = (pb.led_count <= 1)
                    ? 0.5f
                    : (float)pb.led_index / (float)(pb.led_count - 1);
                nx = u;
                ny = 0.45f + 0.1f * u;
                nz = 0.5f;
            }
            else
            {
                const float u = (led_n <= 1) ? 0.5f : (float)led_slot / (float)(led_n - 1);
                nx = u;
                ny = 0.45f + 0.1f * u;
                nz = 0.5f;
            }
            Action::ApplyClipUvTransform(sample, &nx, &ny);
            xyz[(size_t)r * 3u + 0u] = nx;
            xyz[(size_t)r * 3u + 1u] = ny;
            xyz[(size_t)r * 3u + 2u] = nz;
        }

        const QString cache_key = makeGpuThumbCacheKey(sample, pb.view_target, sample_w, n, led_n, xyz);
        const auto cached = gpu_thumb_cache_.constFind(cache_key);
        if(cached != gpu_thumb_cache_.cend())
        {
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawImage(br, cached.value());
            return;
        }

        GpuThumbRequest req;
        req.key = cache_key;
        req.clip = sample;
        req.xyz = std::move(xyz);
        req.sample_w = sample_w;
        req.rows = n;
        req.field = field_eval;
        req.particle = particle_eval;
        req.ribbon = ribbon_eval;
        req.builtin = builtin_eval;
        enqueueGpuThumb(std::move(req));

        p.fillRect(br, QColor(0, 0, 0));
        return;
    }

    // Cap LED preview rows — full clip height × many LEDs stalls the UI thread.
    constexpr int kMaxPreviewRows = 16;
    const int max_rows = pb.single_led_row ? 1 : std::max(2, std::min(br.height(), kMaxPreviewRows));
    int skip = (led_n > max_rows) ? std::max(1, (led_n + max_rows - 1) / max_rows) : 1;
    if(skip > 1 && (skip % 2) == 0)
    {
        ++skip;
    }
    const int rows = pb.single_led_row ? 1 : std::max(1, (led_n + skip - 1) / skip);

    // Sample fewer time columns then scale up — clip thumbnails are illustrative.
    constexpr int kMaxSampleWidth = 96;
    const int sample_w = std::max(1, std::min(br.width(), kMaxSampleWidth));
    QImage img(sample_w, rows, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    // GPU engines returned above; only media still prepares on this CPU path.
    int last_prepare_ms = -1;
    const int prepare_step_ms = std::max(24, dur / std::max(1, img.width() / 6));
    static std::uint64_t s_timeline_media_seq = 0;
    auto prepareMediaAt = [&](int sample_ms) {
        if(!media_eval)
        {
            return;
        }
        if(last_prepare_ms >= 0 && std::abs(sample_ms - last_prepare_ms) < prepare_step_ms)
        {
            return;
        }
        Action::ClipOrigin01 origin;
        if(room_bounds_.has_world_bounds)
        {
            origin = Action::ResolveClipOrigin01(sample,
                                                room_bounds_.min_x, room_bounds_.max_x,
                                                room_bounds_.min_y, room_bounds_.max_y,
                                                room_bounds_.min_z, room_bounds_.max_z,
                                                room_bounds_.reference_points);
        }
        else
        {
            origin = Action::ResolveClipOrigin01(sample, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, {});
        }
        Action::SetActiveClipOrigin01(origin);
        Action::PrepareMediaForClip(sample, sample_ms, ++s_timeline_media_seq);
        last_prepare_ms = sample_ms;
    };

    auto sampleLed = [&](int led_slot, int ms, RGBColor* c, float* intens) -> bool {
        constexpr float k0 = 0.0f;
        constexpr float k1 = 1.0f;
        if(pb.single_led_row)
        {
            // Expanded LED child under a parent track: evaluate this LED's slot in its
            // sibling strip so wipe/alternating match the hardware row.
            return Action::EvaluateClipAtLed(sample, ms, pb.led_index,
                                                  std::max(1, pb.led_count), c, intens);
        }
        if(!order.empty())
        {
            const int idx = order[(size_t)std::clamp(led_slot, 0, (int)order.size() - 1)];
            if(world_eval && idx < (int)nxs.size())
            {
                return Action::EvaluateClipAtWorld(sample, ms,
                                                        nxs[(size_t)idx], nys[(size_t)idx], nzs[(size_t)idx],
                                                        k0, k1, k0, k1, k0, k1,
                                                        seeds[(size_t)idx], c, intens);
            }
            return Action::EvaluateClipAtAxis(sample, ms,
                                                   axes[(size_t)idx], seeds[(size_t)idx],
                                                   c, intens);
        }
        if(world_eval || sample.play_bounds == Action::PlayBounds::RoomBox)
        {
            const float t = (led_n <= 1) ? 0.5f : (float)led_slot / (float)(led_n - 1);
            return Action::EvaluateClipAtAxis(sample, ms, t, led_slot, c, intens);
        }
        return Action::EvaluateClipAtLed(sample, ms, led_slot, led_n, c, intens);
    };

    auto pixelFromSample = [&](RGBColor c, float intens) -> QColor {
        intens = std::clamp(intens, 0.0f, 1.0f);
        if(intens < 0.02f || c == 0)
        {
            return QColor(0, 0, 0);
        }
        QColor qc = RgbToQColor(c);
        qc.setAlpha(255);
        return qc;
    };

    for(int x = 0; x < img.width(); ++x)
    {
        const float t = (img.width() <= 1) ? 0.0f : (float)x / (float)(img.width() - 1);
        const int ms = sample.start_ms + (int)std::lround(t * (float)dur);
        const int sample_ms = std::min(std::max(ms, sample.start_ms), sample.end_ms - 1);
        prepareMediaAt(sample_ms);

        int row = 0;
        for(int led = 0; led < led_n; led += skip, ++row)
        {
            if(row >= rows)
            {
                break;
            }
            RGBColor c0 = 0;
            float i0 = 0.0f;
            if(!sampleLed(led, sample_ms, &c0, &i0))
            {
                c0 = 0;
                i0 = 0.0f;
            }
            QColor a = pixelFromSample(c0, i0);

            // When several LEDs collapse into one preview row, also sample the neighbour
            // so alternating on/off does not bake into a single solid colour.
            if(skip > 1 && led + 1 < led_n)
            {
                RGBColor c1 = 0;
                float i1 = 0.0f;
                if(!sampleLed(led + 1, sample_ms, &c1, &i1))
                {
                    c1 = 0;
                    i1 = 0.0f;
                }
                const QColor b = pixelFromSample(c1, i1);
                if(a.rgb() != b.rgb())
                {
                    img.setPixelColor(x, row, ((x + row) & 1) ? a : b);
                    continue;
                }
            }
            img.setPixelColor(x, row, a);
        }
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(br, img);
}

void ActionTimelineWidget::paintClipGradientBar(QPainter& p, const QRect& br,
                                                         const Action::Clip& clip, int alpha) const
{
    Action::Clip sample = clip;
    Action::EnsureClipGradient(&sample);
    QLinearGradient grad(QPointF(br.left(), br.top()), QPointF(br.right() + 1, br.top()));
    if(sample.gradient.empty())
    {
        QColor c = RgbToQColor(sample.color);
        c.setAlpha(alpha);
        grad.setColorAt(0.0, c);
        grad.setColorAt(1.0, c);
    }
    else if(sample.gradient.size() <= 2
            && sample.gradient.front().color == sample.gradient.back().color)
    {
        QColor c = RgbToQColor(sample.gradient.front().color);
        c.setAlpha(alpha);
        grad.setColorAt(0.0, c);
        grad.setColorAt(1.0, c);
    }
    else
    {
        for(const Action::GradientStop& s : sample.gradient)
        {
            QColor c = RgbToQColor(s.color);
            c.setAlpha(alpha);
            grad.setColorAt(std::clamp(s.pos, 0.0f, 1.0f), c);
        }
        if(sample.gradient.empty())
        {
            QColor c = RgbToQColor(sample.color);
            c.setAlpha(alpha);
            grad.setColorAt(0.0, c);
            grad.setColorAt(1.0, c);
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRect(br);

    if(Action::EffectPreviewPulse(Action::ClipFileId(sample)) && br.width() > 8)
    {
        const float speed = std::max(0.05f, sample.speed);
        const int period = std::max(1, (int)std::lround((float)std::max(1, sample.period_ms) / speed));
        const int cycles = std::max(1, (sample.end_ms - sample.start_ms) / period);
        for(int i = 0; i < cycles; ++i)
        {
            const float t0 = (float)i / (float)cycles;
            const float t1 = (float)(i + 1) / (float)cycles;
            const int x0 = br.left() + (int)std::lround(t0 * (float)br.width());
            const int x1 = br.left() + (int)std::lround(t1 * (float)br.width());
            const int mid = (x0 + x1) / 2;
            QLinearGradient pulse(mid, br.top(), mid, br.bottom());
            pulse.setColorAt(0.0, QColor(0, 0, 0, 0));
            pulse.setColorAt(0.5, QColor(255, 255, 255, 36));
            pulse.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.fillRect(QRect(x0, br.top(), std::max(1, x1 - x0), br.height()), pulse);
        }
    }
}

void ActionTimelineWidget::paintClipIntensityCurve(QPainter& p, const QRect& br,
                                                        const Action::Clip& clip) const
{
    if(clip.intensity_curve.empty() || br.width() < 4)
    {
        return;
    }
    // Guide line only — colour/intensity are already baked by the temporal raster.
    const int steps = std::max(8, br.width());
    QPolygon line;
    line.reserve(steps);
    for(int i = 0; i < steps; ++i)
    {
        const float t = ((float)i + 0.5f) / (float)steps;
        const float level = std::clamp(Action::SampleCurve(clip.intensity_curve, t), 0.0f, 1.0f);
        const int x0 = br.left() + (i * br.width()) / steps;
        const int y = br.bottom() - (int)std::lround(level * (float)(br.height() - 1));
        line << QPoint(x0, y);
    }
    p.setPen(QPen(QColor(255, 255, 255, 180), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);
}

void ActionTimelineWidget::paintClipVisual(QPainter& p, const QRect& br, const PaintClip& pb, bool selected) const
{
    if(!pb.clip || br.width() < 2 || br.height() < 2)
    {
        return;
    }
    Action::Clip sample = *pb.clip;
    Action::EnsureClipGradient(&sample);

    p.save();
    p.setClipRect(br, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 8, 10));
    p.drawRect(br);

    paintClipSpatialRaster(p, br, pb, sample);

    if(sample.effect_id == "media_text" && sample.media_text.empty() && br.width() > 24)
    {
        p.setPen(QColor(160, 160, 170));
        p.drawText(br.adjusted(4, 0, -4, 0), Qt::AlignCenter, QStringLiteral("(no text)"));
    }
    else if(sample.effect_id == "media_text" && !sample.media_text.empty() && br.width() > 32)
    {
        // Caption helps when the spatial raster is sparse / dark mid-scroll.
        const QString label = QString::fromStdString(sample.media_text);
        p.setPen(QColor(20, 20, 24, 180));
        p.drawText(br.adjusted(5, 1, -3, 1), Qt::AlignLeft | Qt::AlignVCenter, label);
        p.setPen(QColor(240, 230, 160, 220));
        p.drawText(br.adjusted(4, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    {
        const int grip = std::min(3, std::max(2, br.width() / 10));
        p.fillRect(br.left(), br.top(), grip, br.height(), QColor(255, 255, 255, 28));
        p.fillRect(br.right() - grip + 1, br.top(), grip, br.height(), QColor(255, 255, 255, 28));
    }

    paintClipIntensityCurve(p, br, sample);

    p.setBrush(Qt::NoBrush);
    p.setPen(selected ? QColor(255, 220, 80) : QColor(70, 70, 78));
    p.drawRect(br.adjusted(0, 0, -1, -1));
    p.restore();
}

void ActionTimelineWidget::paintEvent(QPaintEvent* event)
{
    if(content_dirty_ || content_cache_.size() != size())
    {
        content_cache_ = QPixmap(size());
        content_cache_.fill(QColor(32, 32, 36));
        QPainter cache_painter(&content_cache_);
        paintTimelineContent(cache_painter);
        content_dirty_ = false;
    }

    QPainter p(this);
    const QRect damage = event ? event->rect() : rect();
    p.drawPixmap(damage, content_cache_, damage);

    const int px = timeToX(playhead_ms_);
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(px, 0, px, height());
}

void ActionTimelineWidget::paintTimelineContent(QPainter& p) const
{
    p.fillRect(rect(), QColor(32, 32, 36));

    p.fillRect(0, 0, gutter_width_, height(), QColor(40, 40, 44));
    p.fillRect(gutter_width_, 0, width() - gutter_width_, header_height_, QColor(45, 45, 50));

    p.setPen(QColor(160, 160, 170));
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    for(int sec = 0; sec * 1000 <= duration_ms_; ++sec)
    {
        const int x = timeToX(sec * 1000);
        p.drawLine(x, header_height_ - 8, x, header_height_);
        p.drawText(x + 2, header_height_ - 10, QString::number(sec) + QStringLiteral("s"));
    }
    p.drawText(8, header_height_ - 8, QStringLiteral("Pack scope"));

    for(int row = 0; row < visible_rows_.size(); ++row)
    {
        const Row& r = visible_rows_[row];
        const int y = header_height_ + row * row_height_;
        const QColor bg = (row % 2 == 0) ? QColor(38, 38, 42) : QColor(34, 34, 38);
        const QColor gutter_bg = (selected_row_ == row) ? QColor(55, 55, 70) : bg;
        p.fillRect(0, y, gutter_width_, row_height_, gutter_bg);
        p.fillRect(gutter_width_, y, width() - gutter_width_, row_height_, bg);
        const int second_px = timeToX(1000) - timeToX(0);
        p.setPen(QColor(255, 255, 255, 28));
        for(int sec = 1; sec * 1000 <= duration_ms_; ++sec)
        {
            const int x = timeToX(sec * 1000);
            p.drawLine(x, y, x, y + row_height_);
        }
        if(second_px >= 48)
        {
            p.setPen(QColor(255, 255, 255, 14));
            for(int half = 1; half * 500 <= duration_ms_; half += 2)
            {
                const int x = timeToX(half * 500);
                p.drawLine(x, y, x, y + row_height_);
            }
        }
        p.setPen(QColor(55, 55, 60));
        p.drawLine(0, y + row_height_ - 1, width(), y + row_height_ - 1);

        if(r.depth == 0 || r.target.kind == Action::TargetKind::SceneZone)
        {
            QColor accent = QColor(90, 140, 220);
            if(r.target.kind == Action::TargetKind::All)
            {
                accent = QColor(220, 160, 60);
            }
            else if(r.target.kind == Action::TargetKind::SceneZone)
            {
                accent = QColor(120, 200, 140);
            }
            p.fillRect(0, y, 3, row_height_, accent);
        }
        if(row_reorder_from_ >= 0 && row == row_reorder_hover_ && row != row_reorder_from_)
        {
            p.fillRect(0, y, gutter_width_, 2, QColor(255, 220, 80));
        }
        if(drag_op_ == DragOp::Move && drag_moved_ && drag_dest_row_ == row)
        {
            p.fillRect(gutter_width_, y, width() - gutter_width_, row_height_, QColor(120, 180, 255, 28));
        }

        const int indent = 6 + r.depth * 14;
        if(r.expandable)
        {
            const QRect plus(indent, y + (row_height_ - expand_hit_) / 2, expand_hit_, expand_hit_);
            p.setPen(QColor(200, 200, 210));
            p.setBrush(QColor(55, 55, 62));
            p.drawRect(plus);
            p.drawText(plus, Qt::AlignCenter, r.expanded ? QStringLiteral("−") : QStringLiteral("+"));
        }

        p.setPen(r.depth == 0 ? QColor(230, 230, 235) : QColor(180, 180, 190));
        const int text_x = indent + (r.expandable ? expand_hit_ + 4 : 4);
        p.drawText(text_x, y, gutter_width_ - text_x - 4, row_height_,
                   Qt::AlignVCenter | Qt::AlignLeft, r.label);

        const QVector<PaintClip> clips = paintClipsForRow(row);
        for(const PaintClip& pb : clips)
        {
            if(!pb.clip)
            {
                continue;
            }
            // Hide the live clip while floating — it is drawn under the pointer instead.
            if(drag_op_ == DragOp::Move && drag_moved_
               && pb.track == drag_track_ && pb.clip_index == drag_clip_)
            {
                continue;
            }
            // Hide while vertical-resize paints one continuous span preview.
            if(drag_op_ == DragOp::ResizeVertical
               && pb.track == drag_track_ && pb.clip_index == drag_clip_)
            {
                continue;
            }
            // Paint on every covered row (parent + LED children). A single tall
            // draw on the parent is covered by later opaque row backgrounds.
            const bool selected = isClipSelected(pb.track, pb.clip_index);
            paintClipVisual(p, clipRect(row, *pb.clip), pb, selected);
        }
    }

    // Origin ghost + floating clip during Move.
    if(drag_op_ == DragOp::Move && drag_moved_
       && drag_track_ >= 0 && drag_clip_ >= 0 && pack_
       && drag_track_ < (int)pack_->tracks.size()
       && drag_clip_ < (int)pack_->tracks[(size_t)drag_track_].clips.size())
    {
        const Action::Clip& b = pack_->tracks[(size_t)drag_track_].clips[(size_t)drag_clip_];
        if(drag_home_row_ >= 0 && drag_home_row_ < visible_rows_.size())
        {
            Action::Clip origin = b;
            origin.start_ms = drag_origin_start_;
            origin.end_ms = drag_origin_end_;
            const QRect home = clipRect(drag_home_row_, origin);
            p.fillRect(home, QColor(255, 255, 255, 18));
            p.setPen(QPen(QColor(200, 200, 210, 140), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(home.adjusted(0, 0, -1, -1));
        }

        PaintClip pb;
        pb.clip = &b;
        pb.track = drag_track_;
        pb.clip_index = drag_clip_;
        const QRect live = clipRect(0, b);
        int float_top = drag_float_y_ - drag_grab_offset_y_;
        const int min_top = header_height_ + 3;
        const int row_count = (int)visible_rows_.size();
        const int max_top = header_height_ + std::max(0, row_count) * row_height_ - (row_height_ - 3);
        float_top = std::clamp(float_top, min_top, std::max(min_top, max_top));
        const QRect floating(live.x(), float_top, live.width(), row_height_ - 6);
        p.setOpacity(0.92);
        paintClipVisual(p, floating, pb, true);
        p.setOpacity(1.0);
        p.setPen(QPen(QColor(140, 200, 255, 220), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(floating.adjusted(0, 0, -1, -1));
    }

    // Vertical span preview: one continuous clip covering the track rows.
    if(drag_op_ == DragOp::ResizeVertical
       && drag_span_anchor_row_ >= 0 && drag_span_hover_row_ >= 0
       && drag_track_ >= 0 && drag_clip_ >= 0 && pack_
       && drag_track_ < (int)pack_->tracks.size()
       && drag_clip_ < (int)pack_->tracks[(size_t)drag_track_].clips.size())
    {
        const Action::Clip& b = pack_->tracks[(size_t)drag_track_].clips[(size_t)drag_clip_];
        const int lo = std::min(drag_span_anchor_row_, drag_span_hover_row_);
        const int hi = std::max(drag_span_anchor_row_, drag_span_hover_row_);
        const int x0 = timeToX(b.start_ms);
        const int x1 = std::max(x0 + 4, timeToX(b.end_ms));
        const int y0 = header_height_ + lo * row_height_ + 3;
        const int y1 = header_height_ + (hi + 1) * row_height_ - 3;
        PaintClip pb;
        pb.clip = &b;
        pb.track = drag_track_;
        pb.clip_index = drag_clip_;
        pb.single_led_row = false;
        pb.led_index = 0;
        pb.led_count = std::max(1, hi - lo + 1);
        pb.view_target = pack_->tracks[(size_t)drag_track_].target;
        const QRect span(x0, y0, x1 - x0, y1 - y0);
        p.setOpacity(0.88);
        paintClipVisual(p, span, pb, true);
        p.setOpacity(1.0);
        p.setPen(QPen(QColor(140, 200, 255, 220), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(span.adjusted(0, 0, -1, -1));
    }

    // Marquee rubber-band while dragging a selection box.
    if(drag_op_ == DragOp::Marquee)
    {
        const QRect box = QRect(marquee_origin_, marquee_current_).normalized();
        p.fillRect(box, QColor(120, 180, 255, 40));
        p.setPen(QPen(QColor(140, 200, 255, 220), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(0, 0, -1, -1));
    }

    p.setPen(QPen(QColor(168, 168, 176), 2));
    p.drawLine(gutter_width_, 0, gutter_width_, height());
}

