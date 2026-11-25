// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionTimelineWidget.h"
#include "ActionCatalog.h"
#include "ActionApplier.h"
#include "ActionBuiltinGpu.h"
#include "ActionMedia.h"
#include "ActionScript.h"
#include "ActionParticleField.h"
#include "ActionRibbonField.h"
#include "ActionVolumeField.h"
#include "SpatialOffscreenGlPool.h"
#include "ZoneManager3D.h"
#include "PluginTheme.h"

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
#include <QPainterPath>
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

    Action::BindClipOrigin(req.clip, room_bounds_.has_world_bounds ? &room_bounds_ : nullptr);

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
        Action::BindClipOrigin(sample, room_bounds_.has_world_bounds ? &room_bounds_ : nullptr);
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

namespace
{
QColor MixColor(const QColor& a, const QColor& b, double t)
{
    return QColor((int)std::lround(a.red() + (b.red() - a.red()) * t),
                  (int)std::lround(a.green() + (b.green() - a.green()) * t),
                  (int)std::lround(a.blue() + (b.blue() - a.blue()) * t));
}

QString FormatRulerTime(int ms, int step_ms)
{
    const int decimals = step_ms >= 1000 ? 0 : (step_ms % 100 == 0 ? 1 : 2);
    QString text = QString::number(ms / 1000.0, 'f', decimals);
    if(text.contains(QLatin1Char('.')))
    {
        while(text.endsWith(QLatin1Char('0')))
        {
            text.chop(1);
        }
        if(text.endsWith(QLatin1Char('.')))
        {
            text.chop(1);
        }
    }
    return text + QStringLiteral("s");
}

const QColor kPlayheadColor(235, 80, 80);
} // namespace

void ActionTimelineWidget::paintClipVisual(QPainter& p, const QRect& br, const PaintClip& pb, bool selected) const
{
    if(!pb.clip || br.width() < 2 || br.height() < 2)
    {
        return;
    }
    Action::Clip sample = *pb.clip;
    Action::EnsureClipGradient(&sample);

    /* Rounded card: the LED preview is clipped to the card shape, the frame drawn on top. */
    const qreal radius = std::min({5.0, br.height() / 3.0, br.width() / 2.0});
    QPainterPath shape;
    shape.addRoundedRect(QRectF(br), radius, radius);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipPath(shape, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 8, 10));
    p.drawRect(br);

    paintClipSpatialRaster(p, br, pb, sample);

    const bool is_text = sample.effect_id == "media_text";
    if(is_text && sample.media_text.empty() && br.width() > 24)
    {
        p.setPen(QColor(160, 160, 170));
        p.drawText(br.adjusted(4, 0, -4, 0), Qt::AlignCenter, QStringLiteral("(no text)"));
    }
    else if(is_text && !sample.media_text.empty() && br.width() > 32)
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

    /* Name tag on a soft dark pill so it reads over bright effects. */
    if(!is_text && br.width() >= 46 && br.height() >= 14)
    {
        QFont font = p.font();
        font.setPointSizeF(std::max(6.5, font.pointSizeF() - 0.5));
        font.setBold(true);
        p.setFont(font);
        const QFontMetrics fm(font);
        const int max_w = br.width() - 14;
        const QString name = fm.elidedText(effectDisplayName(sample), Qt::ElideRight, max_w);
        if(!name.isEmpty())
        {
            const int text_w = fm.horizontalAdvance(name);
            const QRect pill(br.left() + 5, br.top() + (br.height() - fm.height()) / 2 - 1,
                             text_w + 8, fm.height() + 2);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 120));
            p.drawRoundedRect(QRectF(pill), pill.height() / 2.0, pill.height() / 2.0);
            p.setPen(QColor(255, 255, 255, 235));
            p.drawText(pill, Qt::AlignCenter, name);
        }
    }
    p.restore();

    PluginTheme::PaintCardFrame(p, QRectF(br), RgbToQColor(sample.color), selected, false, palette(), radius);
}

void ActionTimelineWidget::paintEvent(QPaintEvent* event)
{
    if(content_dirty_ || content_cache_.size() != size())
    {
        content_cache_ = QPixmap(size());
        content_cache_.fill(palette().color(QPalette::Base));
        QPainter cache_painter(&content_cache_);
        paintTimelineContent(cache_painter);
        content_dirty_ = false;
    }

    QPainter p(this);
    const QRect damage = event ? event->rect() : rect();
    p.drawPixmap(damage, content_cache_, damage);

    /* Place-mode ghost: drawn over the cached timeline so moving it stays cheap. */
    if(!placing_effect_id_.isEmpty() && place_row_ >= 0 && place_row_ < visible_rows_.size())
    {
        Action::Clip ghost;
        ghost.effect_id = placing_effect_id_.toStdString();
        ghost.start_ms = place_ms_;
        ghost.end_ms = std::min(duration_ms_, place_ms_ + 1000);
        const QRectF r(clipRect(place_row_, ghost));
        const QColor accent = PluginTheme::Accent(palette());
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(PluginTheme::WithAlpha(accent, 60));
        p.drawRoundedRect(r, 5, 5);
        PluginTheme::PaintCardFrame(p, r, accent, false, true, palette());
        p.setPen(PluginTheme::WithAlpha(palette().color(QPalette::Text), 230));
        p.drawText(r.adjusted(8, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(effectDisplayName(ghost), Qt::ElideRight, std::max(0, (int)r.width() - 14)));
        PluginTheme::PaintGuideLine(p, r.left(), header_height_, height(), PluginTheme::WithAlpha(accent, 160));
    }

    /* Playhead: a line through the rows and a handle in the ruler. */
    const int px = timeToX(playhead_ms_);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(kPlayheadColor, 2));
    p.drawLine(px, header_height_ - 6, px, height());
    p.setPen(Qt::NoPen);
    p.setBrush(kPlayheadColor);
    QPainterPath handle;
    handle.addRoundedRect(QRectF(px - 5, 2, 10, header_height_ - 10), 2, 2);
    handle.moveTo(px - 5, header_height_ - 9);
    handle.lineTo(px, header_height_ - 3);
    handle.lineTo(px + 5, header_height_ - 9);
    handle.closeSubpath();
    p.drawPath(handle.simplified());
}

namespace
{
/** Eye glyph for the live device view button. */
void PaintEyeButton(QPainter& p, const QRect& r, const QColor& color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPointF c = QRectF(r).center();
    const double w = r.width() * 0.42;
    const double h = r.height() * 0.32;
    QPainterPath eye;
    eye.moveTo(c.x() - w, c.y());
    eye.quadTo(c.x(), c.y() - h * 2.0, c.x() + w, c.y());
    eye.quadTo(c.x(), c.y() + h * 2.0, c.x() - w, c.y());
    p.setPen(QPen(color, 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawPath(eye);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(c, h * 0.95, h * 0.95);
    p.restore();
}

/** Motion keys as small diamonds along the bottom of a clip. */
void PaintMotionKeys(QPainter& p, const QRect& r, const Action::Clip& clip)
{
    if(r.width() < 6 || r.height() < 6)
    {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(20, 20, 20, 200), 1));
    p.setBrush(QColor(255, 255, 255, 235));
    const double s = std::clamp(r.height() / 6.0, 2.5, 4.0);
    const double y = r.bottom() - s - 1.0;
    for(const Action::MotionKey& k : clip.motion)
    {
        const double x = r.left() + std::clamp((double)k.at, 0.0, 1.0) * (r.width() - 1);
        p.drawPolygon(QPolygonF({QPointF(x, y - s), QPointF(x + s, y), QPointF(x, y + s), QPointF(x - s, y)}));
    }
    p.restore();
}

/** Small outlined gutter button (layer fold / mute / solo); filled when on. */
void PaintLaneButton(QPainter& p, const QRect& r, const QString& label, bool on, const QColor& on_color,
                     const QPalette& pal)
{
    if(r.isEmpty())
    {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(on ? on_color : PluginTheme::Line(pal, 120, 120), 1));
    p.setBrush(on ? QBrush(PluginTheme::WithAlpha(on_color, 170)) : QBrush(Qt::NoBrush));
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    QFont f = p.font();
    f.setPointSize(7);
    f.setBold(on);
    p.setFont(f);
    p.setPen(on ? QColor(255, 255, 255) : PluginTheme::WithAlpha(pal.color(QPalette::Text), 210));
    p.drawText(r, Qt::AlignCenter, label);
    p.restore();
}
} // namespace

void ActionTimelineWidget::paintTimelineContent(QPainter& p) const
{
    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor window = pal.color(QPalette::Window);
    const QColor text = pal.color(QPalette::Text);
    const QColor accent = PluginTheme::Accent(pal);
    /* Rows alternate base and a slightly darker shade (no light-grey stripes in dark themes). */
    const bool dark = PluginTheme::IsDark(pal);
    const QColor row_a = base;
    const QColor row_b = dark ? MixColor(base, QColor(0, 0, 0), 0.28) : MixColor(base, text, 0.045);
    /* Past the pack's end the timeline stops: a flat backdrop, no rows or grid. */
    const QColor outside = dark ? MixColor(window, QColor(0, 0, 0), 0.35) : MixColor(window, text, 0.06);
    const int end_x = std::min(width(), timeToX(duration_ms_));
    const QColor grid_major = PluginTheme::Line(pal, 34, 30);
    const QColor grid_minor = PluginTheme::Line(pal, 16, 14);
    const QColor separator = PluginTheme::Line(pal, 40, 46);

    p.fillRect(rect(), outside);
    p.fillRect(0, 0, gutter_width_, height(), window);
    p.fillRect(gutter_width_, 0, end_x - gutter_width_, header_height_, window);

    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);

    /* Ruler: major ticks with labels at a readable step, minor ticks between. */
    const int step = rulerStepMs();
    const int minor = (step % 5 == 0 && step >= 50) ? step / 5 : step / 2;
    const bool show_minor = minor > 0 && minor * pixels_per_second_ / 1000.0 >= 8.0;
    if(show_minor)
    {
        p.setPen(PluginTheme::Line(pal, 90, 90));
        for(int ms = minor; ms <= duration_ms_; ms += minor)
        {
            if(ms % step != 0)
            {
                const int x = timeToX(ms);
                p.drawLine(x, header_height_ - 4, x, header_height_);
            }
        }
    }
    for(int ms = 0; ms <= duration_ms_; ms += step)
    {
        const int x = timeToX(ms);
        p.setPen(PluginTheme::Line(pal, 150, 150));
        p.drawLine(x, header_height_ - 9, x, header_height_);
        p.setPen(PluginTheme::WithAlpha(text, 200));
        p.drawText(x + 3, header_height_ - 10, FormatRulerTime(ms, step));
    }
    p.setPen(PluginTheme::WithAlpha(text, 170));
    p.drawText(8, header_height_ - 8, QStringLiteral("Pack scope"));

    /* Markers: a flag in the ruler (with its name when there is room) and a line through the rows. */
    const QColor marker_color(250, 190, 60);
    if(pack_)
    {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        for(size_t i = 0; i < pack_->markers.size(); ++i)
        {
            const Action::Marker& m = pack_->markers[i];
            const int x = timeToX(m.ms);
            p.setPen(Qt::NoPen);
            p.setBrush(marker_color);
            p.drawPolygon(QPolygonF({QPointF(x, header_height_ - 1), QPointF(x - 4.5, header_height_ - 8),
                                     QPointF(x + 4.5, header_height_ - 8)}));
            if(!m.label.empty())
            {
                const int next_x = (i + 1 < pack_->markers.size()) ? timeToX(pack_->markers[i + 1].ms) : width();
                const int room = next_x - x - 8;
                if(room > 24)
                {
                    p.setPen(marker_color);
                    p.drawText(QRect(x + 6, 0, room, header_height_ - 10), Qt::AlignLeft | Qt::AlignBottom,
                               p.fontMetrics().elidedText(QString::fromStdString(m.label), Qt::ElideRight, room));
                }
            }
        }
        p.restore();
    }
    p.setPen(separator);
    p.drawLine(0, header_height_ - 1, end_x, header_height_ - 1);

    for(int row = 0; row < visible_rows_.size(); ++row)
    {
        const Row& r = visible_rows_[row];
        const int y = rowTop(row);
        const int h = rowHeightAt(row);
        const QColor bg = (row % 2 == 0) ? row_a : row_b;
        const QColor gutter_bg = (selected_row_ == row) ? MixColor(window, accent, 0.28) : window;
        p.fillRect(0, y, gutter_width_, h, gutter_bg);
        p.fillRect(gutter_width_, y, end_x - gutter_width_, h, bg);
        if(show_minor)
        {
            p.setPen(grid_minor);
            for(int ms = minor; ms <= duration_ms_; ms += minor)
            {
                if(ms % step != 0)
                {
                    const int x = timeToX(ms);
                    p.drawLine(x, y, x, y + h);
                }
            }
        }
        p.setPen(grid_major);
        for(int ms = step; ms <= duration_ms_; ms += step)
        {
            const int x = timeToX(ms);
            p.drawLine(x, y, x, y + h);
        }
        p.setPen(separator);
        p.drawLine(0, y + h - 1, end_x, y + h - 1);
        if(pack_ && !pack_->markers.empty())
        {
            p.setPen(PluginTheme::WithAlpha(marker_color, 70));
            for(const Action::Marker& m : pack_->markers)
            {
                const int x = timeToX(m.ms);
                p.drawLine(x, y, x, y + h);
            }
        }

        if(r.depth == 0 || r.target.kind == Action::TargetKind::SceneZone)
        {
            QColor tag = QColor(90, 140, 220);
            if(r.target.kind == Action::TargetKind::All)
            {
                tag = QColor(220, 160, 60);
            }
            else if(r.target.kind == Action::TargetKind::SceneZone)
            {
                tag = QColor(120, 200, 140);
            }
            p.fillRect(0, y + 2, 3, h - 4, tag);
        }
        if(row_reorder_from_ >= 0 && row == row_reorder_hover_ && row != row_reorder_from_)
        {
            p.fillRect(0, y, gutter_width_, 2, accent);
        }
        if(drag_op_ == DragOp::Move && drag_moved_ && drag_dest_row_ == row)
        {
            const QRect dest(gutter_width_, y, end_x - gutter_width_, h);
            const int dest_lanes = laneCountForTrack(trackIndexForRow(row));
            p.fillRect(drag_dest_lane_ >= 0 && dest_lanes > 1
                           ? LaneBand(dest.adjusted(0, 3, 0, -3), dest_lanes, drag_dest_lane_)
                           : dest,
                       PluginTheme::WithAlpha(accent, 34));
        }

        const int indent = 6 + r.depth * 14;
        if(r.expandable)
        {
            /* Disclosure chevron: right when collapsed, down when open. */
            const QRectF box(indent, y + (row_height_ - expand_hit_) / 2.0, expand_hit_, expand_hit_);
            const QPointF c = box.center();
            QPainterPath chevron;
            if(r.expanded)
            {
                chevron.moveTo(c.x() - 4, c.y() - 2);
                chevron.lineTo(c.x(), c.y() + 2.5);
                chevron.lineTo(c.x() + 4, c.y() - 2);
            }
            else
            {
                chevron.moveTo(c.x() - 2, c.y() - 4);
                chevron.lineTo(c.x() + 2.5, c.y());
                chevron.lineTo(c.x() - 2, c.y() + 4);
            }
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(PluginTheme::WithAlpha(text, 200), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(chevron);
            p.restore();
        }

        p.setPen(r.depth == 0 ? text : PluginTheme::WithAlpha(text, 185));
        const int text_x = indent + (r.expandable ? expand_hit_ + 4 : 4);
        /* Layer lanes: the row's own track may hold several lanes (folded = thin
           stripes, shown = one full-height lane each with mute / solo). */
        const int own_track = trackIndexForRow(row);
        const int own_lanes = laneCountForTrack(own_track);
        const bool own_open = own_lanes > 1 && pack_->tracks[(size_t)own_track].layers_open;
        int label_right = own_lanes > 1 ? (own_open ? gutter_width_ - 104 : gutter_width_ - 62) : gutter_width_ - 4;
        const QRect eye = eyeRect(row);
        if(!eye.isEmpty())
        {
            label_right = eye.left() - 2;
            PaintEyeButton(p, eye, PluginTheme::WithAlpha(text, 170));
        }
        p.drawText(text_x, y, std::max(10, label_right - text_x), row_height_,
                   Qt::AlignVCenter | Qt::AlignLeft, r.label);
        if(own_lanes > 1)
        {
            const Action::Track& ot = pack_->tracks[(size_t)own_track];
            const QRect lane_area(gutter_width_, y + 3, end_x - gutter_width_, h - 6);
            for(int lane = 0; lane < own_lanes; ++lane)
            {
                const QRect band = LaneBand(lane_area, own_lanes, lane);
                if(!Action::TrackLayerAudible(ot, lane))
                {
                    p.fillRect(band, PluginTheme::WithAlpha(QColor(0, 0, 0), dark ? 80 : 28));
                }
                if(lane > 0)
                {
                    p.setPen(QPen(PluginTheme::Line(pal, 60, 70), 1, Qt::DotLine));
                    p.drawLine(gutter_width_, band.top() - 1, end_x, band.top() - 1);
                }
            }
            PaintLaneButton(p, laneFoldRect(row),
                            own_open ? QStringLiteral("Fold") : QStringLiteral("%1 layers").arg(own_lanes),
                            false, accent, pal);
            if(own_open)
            {
                for(int lane = 0; lane < own_lanes; ++lane)
                {
                    const Action::TrackLayer* info = lane < (int)ot.layers.size() ? &ot.layers[(size_t)lane] : nullptr;
                    const QString name = (info && !info->name.empty()) ? QString::fromStdString(info->name)
                                                                        : QStringLiteral("Layer %1").arg(lane + 1);
                    const QRect nr = laneNameRect(row, lane);
                    p.save();
                    QFont lf = p.font();
                    lf.setPointSize(7);
                    p.setFont(lf);
                    p.setPen(PluginTheme::WithAlpha(text, Action::TrackLayerAudible(ot, lane) ? 200 : 110));
                    p.drawText(nr, Qt::AlignVCenter | Qt::AlignRight,
                               p.fontMetrics().elidedText(name, Qt::ElideRight, nr.width() - 4));
                    p.restore();
                    PaintLaneButton(p, laneMuteRect(row, lane), QStringLiteral("M"), info && info->muted,
                                    QColor(220, 70, 60), pal);
                    PaintLaneButton(p, laneSoloRect(row, lane), QStringLiteral("S"), info && info->solo,
                                    QColor(230, 170, 30), pal);
                }
            }
        }

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
            /* Muted layers (or layers outside a solo) stay visible but faded. */
            const bool audible = Action::TrackLayerAudible(pack_->tracks[(size_t)pb.track], pb.clip->layer);
            if(!audible)
            {
                p.setOpacity(0.35);
            }
            const QRect clip_rect = clipRectOnTrack(row, pb.track, *pb.clip);
            paintClipVisual(p, clip_rect, pb, selected);
            if(!pb.clip->motion.empty())
            {
                PaintMotionKeys(p, clip_rect, *pb.clip);
            }
            if(!audible)
            {
                p.setOpacity(1.0);
            }
        }
    }

    // End of the pack: a solid line through the ruler and rows.
    {
        const int rows_bottom = rowTop((int)visible_rows_.size());
        p.setPen(QPen(PluginTheme::Line(pal, 150, 170), 2));
        p.drawLine(end_x - 1, 0, end_x - 1, rows_bottom);
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
            const QRect home = clipRectOnTrack(drag_home_row_, drag_track_, origin);
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(PluginTheme::WithAlpha(text, 18));
            p.drawRoundedRect(QRectF(home), 5, 5);
            p.restore();
            PluginTheme::PaintCardFrame(p, QRectF(home), RgbToQColor(b.color), false, true, pal);
        }

        PaintClip pb;
        pb.clip = &b;
        pb.track = drag_track_;
        pb.clip_index = drag_clip_;
        const QRect live = clipRect(0, b);
        int float_top = drag_float_y_ - drag_grab_offset_y_;
        const int min_top = header_height_ + 3;
        const int row_count = (int)visible_rows_.size();
        const int max_top = rowTop(std::max(0, row_count)) - (row_height_ - 3);
        float_top = std::clamp(float_top, min_top, std::max(min_top, max_top));
        const QRect floating(live.x(), float_top, live.width(), row_height_ - 6);
        p.setOpacity(0.92);
        paintClipVisual(p, floating, pb, true);
        p.setOpacity(1.0);
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
        const int y0 = rowTop(lo) + 3;
        const int y1 = rowTop(hi + 1) - 3;
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
    }

    // Snap guide: shows what the dragged edge or playhead stuck to.
    if(snap_guide_ms_ >= 0 && drag_op_ != DragOp::None && drag_op_ != DragOp::Marquee)
    {
        PluginTheme::PaintGuideLine(p, timeToX(snap_guide_ms_), 0, height(), PluginTheme::WithAlpha(accent, 230));
    }

    // Marquee rubber-band while dragging a selection box.
    if(drag_op_ == DragOp::Marquee)
    {
        const QRect box = QRect(marquee_origin_, marquee_current_).normalized();
        p.fillRect(box, PluginTheme::WithAlpha(accent, 40));
        p.setPen(QPen(PluginTheme::WithAlpha(accent, 220), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(0, 0, -1, -1));
    }

    p.setPen(QPen(PluginTheme::Line(pal, 90, 110), 1));
    p.drawLine(gutter_width_, 0, gutter_width_, height());
}
