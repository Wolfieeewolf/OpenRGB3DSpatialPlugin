// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackMedia.h"
#include "MediaTextureEffectUtils.h"

#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>

#include <cmath>
#include <unordered_map>
#include <vector>

namespace EffectPack
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

QImage RasterizeText(const Block& block)
{
    const std::string key = block.media_text + "|" + std::to_string(block.color)
        + "|" + std::to_string((int)(block.intensity * 100))
        + "|" + (block.media_scroll ? "s" : "n");
    {
        QMutexLocker lock(&g_media_mutex);
        auto it = g_text_cache.find(key);
        if(it != g_text_cache.end())
        {
            return it->second;
        }
    }

    const QString text = QString::fromStdString(
        block.media_text.empty() ? std::string("TEXT") : block.media_text);
    // Crisp bitmap glyphs for sparse LED matrices — no AA, bold, large pixels.
    QFont font(QStringLiteral("Arial"));
    font.setBold(true);
    font.setStyleStrategy(QFont::NoAntialias);
    font.setPixelSize(48);

    QFontMetrics fm(font);
    const int pad = 2;
    const int tw = std::max(8, fm.horizontalAdvance(text) + pad * 2);
    const int th = std::max(8, fm.height() + pad * 2);
    // Tight canvas; marquee only needs a short gap, not a double-wide empty field.
    const int gap = block.media_scroll ? std::max(th, tw / 4) : 0;
    const int canvas_w = tw + gap;
    QImage img(canvas_w, th, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0));
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setFont(font);
    p.setPen(QColor(RGBGetRValue(block.color), RGBGetGValue(block.color), RGBGetBValue(block.color)));
    p.drawText(pad, pad + fm.ascent(), text);
    p.end();

    // Binary threshold so LED samples hit full-on or off (readable words).
    for(int y = 0; y < img.height(); ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for(int x = 0; x < img.width(); ++x)
        {
            const int a = qAlpha(line[x]);
            if(a >= 128)
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

void ScrollUv(const Block& block, int local_ms, float* u, float* v)
{
    if(!u || !v)
    {
        return;
    }
    const int elapsed = std::max(0, local_ms - block.start_ms);
    const float speed = std::max(0.05f, block.speed);
    const float period = std::max(50.0f, (float)std::max(1, block.period_ms) / speed);
    const float phase = (float)elapsed / period;
    float du = 0.0f;
    float dv = 0.0f;
    switch(block.direction)
    {
        case Direction::Left:
        case Direction::NegX:
            du = phase;
            break;
        case Direction::Right:
        case Direction::PosX:
            du = -phase;
            break;
        case Direction::Up:
        case Direction::PosY:
            dv = -phase;
            break;
        case Direction::Down:
        case Direction::NegY:
            dv = phase;
            break;
        case Direction::Forward:
        case Direction::Back:
        case Direction::PosZ:
        case Direction::NegZ:
            du = phase;
            break;
    }
    if(block.reverse)
    {
        du = -du;
        dv = -dv;
    }
    *u = MediaTextureEffect::Frac01(*u + du);
    *v = MediaTextureEffect::Frac01(*v + dv);
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

bool IsMediaBlock(const Block& block)
{
    return IsMediaEffect(block.effect_id);
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

bool EvaluateMediaAtUv(const Block& block,
                       int local_ms,
                       float nx,
                       float ny,
                       RGBColor* out_color,
                       float* out_intensity,
                       bool collapse_v)
{
    if(local_ms < block.start_ms || local_ms >= block.end_ms || block.end_ms <= block.start_ms)
    {
        return false;
    }

    float u = std::clamp(nx, 0.0f, 1.0f);
    float v = std::clamp(ny, 0.0f, 1.0f);
    ApplyBlockUvTransform(block, &u, &v);
    if(block.media_scroll)
    {
        ScrollUv(block, local_ms, &u, &v);
    }

    RGBColor color = 0;
    bool lit = false;
    if(block.effect_id == "media_text")
    {
        const QImage text = RasterizeText(block);
        color = collapse_v ? SampleColumnReduce(text, u, &lit)
                           : SampleNearestAlpha(text, u, v, &lit);
        if(!lit)
        {
            return false;
        }
    }
    else
    {
        QImage frame;
        if(!CopyCachedFrame(block.media_path, std::max(0, local_ms - block.start_ms), &frame))
        {
            return false;
        }
        // Nearest + alpha (and column-reduce on strips) stays sharper on sparse LEDs
        // than bilinear, which muddies edges and ignores transparency.
        color = collapse_v ? SampleColumnReduce(frame, u, &lit)
                           : SampleNearestAlpha(frame, u, v, &lit);
        if(!lit)
        {
            return false;
        }
    }

    float intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    ModulateBlockIntensity(block, BlockProgress(block, local_ms), BlockPeriodProgress(block, local_ms), &intensity);

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

} // namespace EffectPack
