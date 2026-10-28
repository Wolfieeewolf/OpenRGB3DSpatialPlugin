// SPDX-License-Identifier: GPL-2.0-only

#include "ActionMedia.h"
#include "MediaTextureEffectUtils.h"

#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QString>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Action
{
namespace
{

struct MediaFrames
{
    std::vector<QImage> frames;
    std::vector<int> delays_ms;
    int total_ms = 0;
};

QMutex g_media_mutex;
std::unordered_map<std::string, MediaFrames> g_image_cache;
std::unordered_map<std::string, QImage> g_text_cache;

/* Once-per-frame prepared media (avoids QImage copy on every LED). */
QImage g_prepared_frame;
std::uint64_t g_prepared_seq = 0;
int g_prepared_ms = -1;
std::string g_prepared_key;
bool g_prepared_ok = false;

QString PathToQString(const std::string& path)
{
    return QString::fromStdString(path);
}

const MediaFrames* LoadImageOrGif(const std::string& path)
{
    if(path.empty())
    {
        return nullptr;
    }
    QMutexLocker lock(&g_media_mutex);
    auto it = g_image_cache.find(path);
    if(it != g_image_cache.end())
    {
        return &it->second;
    }

    MediaFrames packed;
    QImageReader reader(PathToQString(path));
    reader.setAutoTransform(true);
    if(reader.canRead() && reader.supportsAnimation() && reader.imageCount() > 1)
    {
        const int count = reader.imageCount();
        packed.frames.reserve((size_t)std::max(1, count));
        packed.delays_ms.reserve((size_t)std::max(1, count));
        for(int i = 0; i < count; ++i)
        {
            reader.jumpToImage(i);
            QImage frame = reader.read();
            if(frame.isNull())
            {
                continue;
            }
            packed.frames.push_back(frame.convertToFormat(QImage::Format_ARGB32));
            int delay = reader.nextImageDelay();
            if(delay <= 0)
            {
                delay = 100;
            }
            packed.delays_ms.push_back(delay);
            packed.total_ms += delay;
        }
    }
    else
    {
        QImage img(PathToQString(path));
        if(!img.isNull())
        {
            packed.frames.push_back(img.convertToFormat(QImage::Format_ARGB32));
            packed.delays_ms.push_back(1000);
            packed.total_ms = 1000;
        }
    }

    if(packed.frames.empty())
    {
        return nullptr;
    }
    auto inserted = g_image_cache.emplace(path, std::move(packed));
    return &inserted.first->second;
}

QFont LedMarqueeFont()
{
    QFont font(QStringLiteral("Consolas"));
    if(!QFontInfo(font).exactMatch())
    {
        font = QFont(QStringLiteral("Courier New"));
    }
    if(!QFontInfo(font).exactMatch())
    {
        font = QFont(QStringLiteral("monospace"));
    }
    font.setBold(true);
    font.setStyleStrategy(QFont::NoAntialias);
    font.setPixelSize(12);
    return font;
}

QImage RasterizeText(const Clip& clip)
{
    if(clip.media_text.empty())
    {
        return QImage();
    }

    const std::string key = clip.media_text + "|" + std::to_string(clip.color)
        + "|" + std::to_string((int)(clip.intensity * 100))
        + "|" + (clip.media_scroll ? "s" : "n");
    {
        QMutexLocker lock(&g_media_mutex);
        auto it = g_text_cache.find(key);
        if(it != g_text_cache.end())
        {
            return it->second;
        }
    }

    const QString text = QString::fromStdString(clip.media_text);
    const QFont font = LedMarqueeFont();
    const QFontMetrics fm(font);
    const int pad = 1;
    const int tw = std::max(4, fm.horizontalAdvance(text) + pad * 2);
    const int th = std::max(4, fm.height() + pad * 2);
    const int gap = clip.media_scroll ? std::max(th, fm.horizontalAdvance(QLatin1Char('M'))) : 0;
    const int canvas_w = tw + gap;
    QImage img(canvas_w, th, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0));
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setFont(font);
    p.setPen(QColor(RGBGetRValue(clip.color), RGBGetGValue(clip.color), RGBGetBValue(clip.color)));
    p.drawText(pad, pad + fm.ascent(), text);
    p.end();

    for(int y = 0; y < img.height(); ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for(int x = 0; x < img.width(); ++x)
        {
            const int a = qAlpha(line[x]);
            if(a >= 96)
            {
                line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), 255);
            }
            else
            {
                line[x] = qRgba(0, 0, 0, 0);
            }
        }
    }

    QMutexLocker lock(&g_media_mutex);
    g_text_cache[key] = img;
    return img;
}

const QImage* FrameForTime(const MediaFrames& media, int elapsed_ms)
{
    if(media.frames.empty())
    {
        return nullptr;
    }
    if(media.frames.size() == 1 || media.total_ms <= 0)
    {
        return &media.frames.front();
    }
    int t = elapsed_ms % media.total_ms;
    if(t < 0)
    {
        t += media.total_ms;
    }
    int acc = 0;
    for(size_t i = 0; i < media.frames.size(); ++i)
    {
        acc += media.delays_ms[i];
        if(t < acc)
        {
            return &media.frames[i];
        }
    }
    return &media.frames.back();
}

void ScrollPhase(const Clip& clip, int local_ms, float* du, float* dv)
{
    if(!du || !dv)
    {
        return;
    }
    *du = 0.0f;
    *dv = 0.0f;
    // Continuous anim clock when playing forever/while_active (same as .fx time_ms).
    const int anim_ms = GpuAnimMsOr(local_ms);
    const int elapsed = std::max(0, anim_ms - clip.start_ms);
    const float speed = std::max(0.05f, clip.speed);
    const float period = std::max(50.0f, (float)std::max(1, clip.period_ms) / speed);
    const float phase = (float)elapsed / period;
    switch(clip.direction)
    {
        case Direction::Left:
        case Direction::NegX:
            *du = phase;
            break;
        case Direction::Right:
        case Direction::PosX:
            *du = -phase;
            break;
        case Direction::Up:
        case Direction::PosY:
            *dv = -phase;
            break;
        case Direction::Down:
        case Direction::NegY:
            *dv = phase;
            break;
        case Direction::Forward:
        case Direction::Back:
        case Direction::PosZ:
        case Direction::NegZ:
            *du = phase;
            break;
    }
    if(clip.reverse)
    {
        *du = -*du;
        *dv = -*dv;
    }
}

/** Map device UV → texture UV (marquee uses an aspect-correct scroll window). */
void MapSampleUv(const Clip& clip,
                 const QImage& img,
                 float u,
                 float v,
                 float uv_aspect,
                 int local_ms,
                 float* out_u,
                 float* out_v)
{
    float su = std::clamp(u, 0.0f, 1.0f);
    float sv = std::clamp(v, 0.0f, 1.0f);

    const bool text_marquee = (clip.effect_id == "media_text" && clip.media_scroll
                               && !img.isNull() && img.width() > 0 && img.height() > 0);
    if(text_marquee)
    {
        const float img_aspect = (float)img.width() / (float)img.height();
        const float device_aspect = std::max(0.25f, uv_aspect);
        float window = device_aspect / std::max(0.25f, img_aspect);
        window = std::clamp(window, 0.06f, 1.0f);
        su *= window;
    }

    if(clip.media_scroll)
    {
        float du = 0.0f;
        float dv = 0.0f;
        ScrollPhase(clip, local_ms, &du, &dv);
        su = MediaTextureEffect::Frac01(su + du);
        sv = MediaTextureEffect::Frac01(sv + dv);
    }

    *out_u = su;
    *out_v = sv;
}

int UvToX(const QImage& img, float u)
{
    const int w = img.width();
    return std::clamp((int)std::lround(std::clamp(u, 0.0f, 1.0f) * (float)(w - 1)), 0, w - 1);
}

int UvToY(const QImage& img, float v)
{
    const int h = img.height();
    return std::clamp((int)std::lround((1.0f - std::clamp(v, 0.0f, 1.0f)) * (float)(h - 1)), 0, h - 1);
}

RGBColor SampleNearestAlpha(const QImage& img, float u, float v, bool* out_lit)
{
    if(out_lit)
    {
        *out_lit = false;
    }
    if(img.isNull() || img.width() < 1 || img.height() < 1)
    {
        return 0;
    }
    const QRgb px = img.pixel(UvToX(img, u), UvToY(img, v));
    if(qAlpha(px) < 16)
    {
        return 0;
    }
    if(out_lit)
    {
        *out_lit = true;
    }
    return ToRGBColor(qRed(px), qGreen(px), qBlue(px));
}

/** Project a whole image column onto one LED (1D strips) — keeps shapes/words readable. */
RGBColor SampleColumnReduce(const QImage& img, float u, bool* out_lit)
{
    if(out_lit)
    {
        *out_lit = false;
    }
    if(img.isNull() || img.width() < 1 || img.height() < 1)
    {
        return 0;
    }
    const int x = UvToX(img, u);
    int best_score = -1;
    QRgb best = 0;
    for(int y = 0; y < img.height(); ++y)
    {
        const QRgb px = img.pixel(x, y);
        const int a = qAlpha(px);
        if(a < 16)
        {
            continue;
        }
        const int score = a * 3 + qRed(px) + qGreen(px) + qBlue(px);
        if(score > best_score)
        {
            best_score = score;
            best = px;
        }
    }
    if(best_score < 0)
    {
        return 0;
    }
    if(out_lit)
    {
        *out_lit = true;
    }
    return ToRGBColor(qRed(best), qGreen(best), qBlue(best));
}

} // namespace

bool IsMediaEffect(const std::string& effect_id)
{
    return effect_id == "media_image"
        || effect_id == "media_gif"
        || effect_id == "media_text";
}

bool IsMediaClip(const Clip& clip)
{
    return IsMediaEffect(clip.effect_id);
}

void InvalidateMediaCache(const std::string& path)
{
    QMutexLocker lock(&g_media_mutex);
    if(path.empty())
    {
        g_image_cache.clear();
        g_text_cache.clear();
        return;
    }
    g_image_cache.erase(path);
}

void InvalidateMediaTextCache()
{
    QMutexLocker lock(&g_media_mutex);
    g_text_cache.clear();
}

void ResetMediaGpu()
{
    QMutexLocker lock(&g_media_mutex);
    g_prepared_frame = QImage();
    g_prepared_ok = false;
    g_prepared_seq = 0;
    g_prepared_ms = -1;
    g_prepared_key.clear();
}

bool CopyCachedFrame(const std::string& path, int elapsed_ms, QImage* out_frame);

bool PrepareMediaForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence)
{
    if(!IsMediaClip(clip))
    {
        return false;
    }
    if(clip.effect_id == "media_text" && clip.media_text.empty())
    {
        return false;
    }

    const int anim_ms = GpuAnimMsOr(local_ms);
    const std::string key = (clip.effect_id == "media_text")
        ? ("text|" + clip.media_text + "|" + std::to_string(clip.color))
        : ("img|" + clip.media_path);
    const bool same = g_prepared_ok
        && g_prepared_seq == render_sequence
        && g_prepared_ms == anim_ms
        && g_prepared_key == key;
    if(same)
    {
        return true;
    }

    QImage frame;
    if(clip.effect_id == "media_text")
    {
        frame = RasterizeText(clip);
    }
    else if(!CopyCachedFrame(clip.media_path, std::max(0, anim_ms - clip.start_ms), &frame))
    {
        g_prepared_ok = false;
        return false;
    }
    if(frame.isNull())
    {
        g_prepared_ok = false;
        return false;
    }

    g_prepared_frame = frame;
    g_prepared_ok = true;
    g_prepared_seq = render_sequence;
    g_prepared_ms = anim_ms;
    g_prepared_key = key;

    // Full-resolution frame for true UV sampling at each LED (not a low-res atlas bake).
    return true;
}

bool CopyCachedFrame(const std::string& path, int elapsed_ms, QImage* out_frame)
{
    if(!out_frame || path.empty())
    {
        return false;
    }
    if(!LoadImageOrGif(path))
    {
        return false;
    }
    QMutexLocker lock(&g_media_mutex);
    auto it = g_image_cache.find(path);
    if(it == g_image_cache.end())
    {
        return false;
    }
    const QImage* frame = FrameForTime(it->second, elapsed_ms);
    if(!frame || frame->isNull())
    {
        return false;
    }
    *out_frame = *frame;
    return true;
}

bool EvaluateMediaAtUv(const Clip& clip,
                       int local_ms,
                       float nx,
                       float ny,
                       RGBColor* out_color,
                       float* out_intensity,
                       bool collapse_v,
                       float uv_aspect)
{
    if(local_ms < clip.start_ms || local_ms >= clip.end_ms || clip.end_ms <= clip.start_ms)
    {
        return false;
    }

    if(clip.effect_id == "media_text" && clip.media_text.empty())
    {
        return false;
    }

    float u = std::clamp(nx, 0.0f, 1.0f);
    float v = std::clamp(ny, 0.0f, 1.0f);
    ApplyClipUvTransform(clip, &u, &v);

    RGBColor color = 0;
    bool lit = false;

    const int anim_ms = GpuAnimMsOr(local_ms);
    // Prefer the frame pinned by PrepareMediaForClip; otherwise load once here.
    if(!g_prepared_ok || g_prepared_ms != anim_ms)
    {
        PrepareMediaForClip(clip, local_ms, 0);
    }
    if(!g_prepared_ok || g_prepared_frame.isNull())
    {
        return false;
    }

    float su = u;
    float sv = v;
    MapSampleUv(clip, g_prepared_frame, u, v, uv_aspect, anim_ms, &su, &sv);

    color = collapse_v ? SampleColumnReduce(g_prepared_frame, su, &lit)
                       : SampleNearestAlpha(g_prepared_frame, su, sv, &lit);
    if(!lit)
    {
        return false;
    }

    float intensity = std::clamp(clip.intensity, 0.0f, 1.0f);
    ModulateClipIntensity(clip, ClipSpanProgress(clip, local_ms), ClipPeriodProgress(clip, local_ms), &intensity);

    if(out_color)
    {
        const int r = (int)std::lround(RGBGetRValue(color) * intensity);
        const int g = (int)std::lround(RGBGetGValue(color) * intensity);
        const int b = (int)std::lround(RGBGetBValue(color) * intensity);
        *out_color = ToRGBColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
    }
    if(out_intensity)
    {
        *out_intensity = intensity;
    }
    return true;
}

} // namespace Action
