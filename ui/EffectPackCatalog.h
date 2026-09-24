// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include <QColor>
#include <QIcon>
#include <QList>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QVariant>

/** Shared Basic / Pixel / Volume catalog for toolbar + right-click menus. */
namespace EffectPackCatalog
{

enum class Category
{
    Basic,
    Pixel,
    Volume
};

struct Entry
{
    const char* name = nullptr;
    const char* description = nullptr;
    Category category = Category::Basic;
    EffectPack::BlockType type = EffectPack::BlockType::Solid;
    QColor swatch = QColor(180, 180, 190);
};

struct ColorEntry
{
    const char* label = nullptr;
    QColor color;
};

struct GradientEntry
{
    const char* label = nullptr;
    const char* id = nullptr;
};

struct CurveEntry
{
    const char* label = nullptr;
    const char* id = nullptr;
};

inline const char* kEffectMimeType = "application/x-openrgb3d-effect-type";
inline const char* kColorMimeType = "application/x-openrgb3d-rgb-color";
inline const char* kGradientPresetMimeType = "application/x-openrgb3d-gradient-preset";
inline const char* kCurvePresetMimeType = "application/x-openrgb3d-curve-preset";

inline QList<Entry> AllEntries()
{
    return {
        {"Set Level", "Solid colour hold", Category::Basic, EffectPack::BlockType::Solid, QColor(220, 220, 230)},
        {"Fade", "Blend between two colours over time", Category::Basic, EffectPack::BlockType::Fade, QColor(120, 180, 255)},
        {"Pulse", "Breathe intensity up and down", Category::Basic, EffectPack::BlockType::Pulse, QColor(255, 120, 90)},
        {"Wipe", "Hard edge sweeps along an axis", Category::Basic, EffectPack::BlockType::Wipe, QColor(90, 220, 160)},
        {"Chase", "Moving highlight along an axis", Category::Basic, EffectPack::BlockType::Chase, QColor(90, 200, 90)},
        {"Twinkle", "Random sparkles", Category::Basic, EffectPack::BlockType::Twinkle, QColor(255, 220, 80)},
        {"Alternating", "Flip between two colours", Category::Basic, EffectPack::BlockType::Alternating, QColor(255, 160, 60)},
        {"Strobe", "On/off flash", Category::Basic, EffectPack::BlockType::Strobe, QColor(255, 255, 255)},
        {"Spin", "Rotating beam around an axis", Category::Basic, EffectPack::BlockType::Spin, QColor(80, 200, 255)},
        {"Candle Flicker", "Warm irregular flicker", Category::Basic, EffectPack::BlockType::Candle, QColor(255, 140, 40)},
        {"Dissolve", "Reveal LEDs by random threshold", Category::Basic, EffectPack::BlockType::Dissolve, QColor(160, 120, 255)},
        {"Wave", "Soft sine brightness along an axis", Category::Basic, EffectPack::BlockType::Wave, QColor(100, 180, 255)},
        {"Cycle", "Loop the gradient on every LED", Category::Basic, EffectPack::BlockType::Cycle, QColor(180, 120, 255)},
        {"Blink", "Whole set on and off", Category::Basic, EffectPack::BlockType::Blink, QColor(255, 255, 220)},

        {"ColorWash", "Scrolling gradient wash", Category::Pixel, EffectPack::BlockType::ColorWash, QColor(200, 90, 220)},
        {"Plasma", "Animated noise field", Category::Pixel, EffectPack::BlockType::Plasma, QColor(255, 80, 200)},
        {"Snow", "Falling flakes", Category::Pixel, EffectPack::BlockType::Snow, QColor(220, 230, 255)},
        {"Fire", "Rising heat from the base", Category::Pixel, EffectPack::BlockType::Fire, QColor(255, 100, 20)},
        {"Balls", "Bouncing bright blobs", Category::Pixel, EffectPack::BlockType::Balls, QColor(80, 255, 160)},
        {"Bars", "Stepped bars along an axis", Category::Pixel, EffectPack::BlockType::Bars, QColor(100, 160, 255)},
        {"Scanner", "Ping-pong chase head (Larson)", Category::Pixel, EffectPack::BlockType::Scanner, QColor(255, 60, 60)},
        {"Confetti", "Random LEDs pop in gradient colours", Category::Pixel, EffectPack::BlockType::Confetti, QColor(255, 80, 160)},
        {"Comet", "Bright head with a fading trail", Category::Pixel, EffectPack::BlockType::Comet, QColor(120, 220, 255)},

        {"Sphere Wipe", "Sphere expands through the volume", Category::Volume, EffectPack::BlockType::SphereWipe, QColor(120, 255, 200)},
        {"Orbit", "Comet orbiting around an axis", Category::Volume, EffectPack::BlockType::Orbit, QColor(80, 180, 255)},
        {"Ripple", "Rings expand from the center", Category::Volume, EffectPack::BlockType::Ripple, QColor(100, 200, 255)},
        {"Meteor", "Streaks along an axis", Category::Volume, EffectPack::BlockType::Meteor, QColor(255, 220, 120)},
        {"Noise 3D", "Volumetric noise field", Category::Volume, EffectPack::BlockType::Noise3D, QColor(180, 100, 255)},
        {"Burst", "Expanding flash from the center", Category::Volume, EffectPack::BlockType::Burst, QColor(255, 240, 160)},
        {"Helix", "Spiral band around an axis", Category::Volume, EffectPack::BlockType::Helix, QColor(80, 255, 200)},
        {"Fill", "Closes in from the outside", Category::Volume, EffectPack::BlockType::Fill, QColor(255, 180, 80)},
    };
}

inline QList<Entry> EntriesFor(Category cat)
{
    QList<Entry> out;
    for(const Entry& e : AllEntries())
    {
        if(e.category == cat)
        {
            out.push_back(e);
        }
    }
    return out;
}

inline QString CategoryLabel(Category cat)
{
    switch(cat)
    {
        case Category::Basic: return QStringLiteral("Basic Lighting");
        case Category::Pixel: return QStringLiteral("Pixel Lighting");
        case Category::Volume: return QStringLiteral("Volume (3D)");
    }
    return QStringLiteral("Effects");
}

inline QString EffectTooltip(const Entry& e)
{
    const QString name = QString::fromUtf8(e.name ? e.name : "Effect");
    const QString desc = QString::fromUtf8(e.description ? e.description : "");
    if(desc.isEmpty())
    {
        return name + QStringLiteral(" — drag onto a timeline row");
    }
    return name + QStringLiteral(" — ") + desc + QStringLiteral(" — drag onto a timeline row");
}

inline QList<ColorEntry> ColorEntries()
{
    return {
        {"White", QColor(255, 255, 255)},
        {"Warm White", QColor(255, 230, 180)},
        {"Red", QColor(255, 0, 0)},
        {"Green", QColor(0, 255, 0)},
        {"Blue", QColor(0, 0, 255)},
        {"Yellow", QColor(255, 255, 0)},
        {"Magenta", QColor(255, 0, 255)},
        {"Cyan", QColor(0, 255, 255)},
        {"Orange", QColor(255, 128, 0)},
        {"Purple", QColor(128, 0, 255)},
        {"Dim Gray", QColor(48, 48, 52)},
        {"Black", QColor(0, 0, 0)},
    };
}

inline QList<GradientEntry> GradientEntries()
{
    return {
        {"Rainbow", "rainbow"},
        {"Red→Blue", "red_blue"},
        {"White→Color", "white_color"},
        {"Fire", "fire"},
        {"Ice", "ice"},
        {"Forest", "forest"},
        {"Sunset", "sunset"},
        {"Cyber", "cyber"},
    };
}

inline QList<CurveEntry> CurveEntries()
{
    return {
        {"Flat", "flat"},
        {"Triangle", "triangle"},
        {"Ease In", "ease_in"},
        {"Ease Out", "ease_out"},
        {"Pulse", "pulse_curve"},
        {"Hold Peak", "hold_peak"},
        {"Snap", "snap"},
    };
}

inline QPixmap MakeGradientPreview(const char* id, int w = 34, int h = 16)
{
    QPixmap pm(w, h);
    QPainter p(&pm);
    QLinearGradient grad(0, 0, w, 0);
    const QString sid = QString::fromUtf8(id ? id : "");
    if(sid == QStringLiteral("rainbow"))
    {
        grad.setColorAt(0.0, QColor(255, 0, 0));
        grad.setColorAt(0.2, QColor(255, 128, 0));
        grad.setColorAt(0.4, QColor(255, 255, 0));
        grad.setColorAt(0.6, QColor(0, 255, 0));
        grad.setColorAt(0.8, QColor(0, 128, 255));
        grad.setColorAt(1.0, QColor(180, 0, 255));
    }
    else if(sid == QStringLiteral("red_blue"))
    {
        grad.setColorAt(0.0, QColor(255, 0, 0));
        grad.setColorAt(1.0, QColor(0, 80, 255));
    }
    else if(sid == QStringLiteral("white_color"))
    {
        grad.setColorAt(0.0, QColor(255, 255, 255));
        grad.setColorAt(1.0, QColor(255, 80, 40));
    }
    else if(sid == QStringLiteral("fire"))
    {
        grad.setColorAt(0.0, QColor(20, 0, 0));
        grad.setColorAt(0.35, QColor(255, 40, 0));
        grad.setColorAt(0.7, QColor(255, 160, 0));
        grad.setColorAt(1.0, QColor(255, 255, 180));
    }
    else if(sid == QStringLiteral("ice"))
    {
        grad.setColorAt(0.0, QColor(20, 40, 80));
        grad.setColorAt(0.5, QColor(120, 200, 255));
        grad.setColorAt(1.0, QColor(240, 250, 255));
    }
    else if(sid == QStringLiteral("forest"))
    {
        grad.setColorAt(0.0, QColor(10, 40, 10));
        grad.setColorAt(0.5, QColor(40, 160, 60));
        grad.setColorAt(1.0, QColor(180, 255, 120));
    }
    else if(sid == QStringLiteral("sunset"))
    {
        grad.setColorAt(0.0, QColor(40, 20, 80));
        grad.setColorAt(0.4, QColor(255, 80, 40));
        grad.setColorAt(0.75, QColor(255, 180, 60));
        grad.setColorAt(1.0, QColor(255, 240, 200));
    }
    else if(sid == QStringLiteral("cyber"))
    {
        grad.setColorAt(0.0, QColor(0, 255, 180));
        grad.setColorAt(0.5, QColor(0, 120, 255));
        grad.setColorAt(1.0, QColor(200, 0, 255));
    }
    else
    {
        grad.setColorAt(0.0, QColor(80, 80, 90));
        grad.setColorAt(1.0, QColor(200, 200, 210));
    }
    p.fillRect(pm.rect(), grad);
    return pm;
}

inline void PaintEffectGlyph(QPainter& p, const QRect& r, EffectPack::BlockType type, const QColor& ink)
{
    const QColor dim = ink.darker(165);
    const QColor hot = ink.lighter(145);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);

    switch(type)
    {
        case EffectPack::BlockType::Solid:
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 2, 2);
            break;
        case EffectPack::BlockType::Fade:
        {
            QLinearGradient g(r.topLeft(), r.topRight());
            g.setColorAt(0.0, dim);
            g.setColorAt(1.0, hot);
            p.setBrush(g);
            p.drawRoundedRect(r.adjusted(1, 3, -1, -3), 2, 2);
            break;
        }
        case EffectPack::BlockType::Pulse:
        {
            QRadialGradient g(r.center(), r.width() * 0.55);
            g.setColorAt(0.0, hot);
            g.setColorAt(1.0, dim);
            p.setBrush(g);
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
            break;
        }
        case EffectPack::BlockType::Wipe:
            p.setBrush(dim);
            p.drawRect(r.adjusted(1, 3, -1, -3));
            p.setBrush(hot);
            p.drawRect(QRect(r.left() + 1, r.top() + 3, r.width() / 2, r.height() - 6));
            break;
        case EffectPack::BlockType::Chase:
            for(int i = 0; i < 4; ++i)
            {
                p.setBrush(i == 2 ? hot : dim);
                const int s = r.height() / 5;
                p.drawEllipse(r.left() + 2 + i * (s + 1), r.center().y() - s / 2, s, s);
            }
            break;
        case EffectPack::BlockType::Twinkle:
            p.setBrush(hot);
            p.drawEllipse(r.left() + 3, r.top() + 4, 3, 3);
            p.drawEllipse(r.right() - 7, r.top() + 6, 2, 2);
            p.setBrush(ink);
            p.drawEllipse(r.center().x() - 1, r.center().y(), 4, 4);
            p.drawEllipse(r.left() + 6, r.bottom() - 6, 2, 2);
            break;
        case EffectPack::BlockType::Alternating:
            p.setBrush(hot);
            p.drawRect(r.left() + 2, r.top() + 3, r.width() / 2 - 2, r.height() - 6);
            p.setBrush(dim);
            p.drawRect(r.center().x() + 1, r.top() + 3, r.width() / 2 - 3, r.height() - 6);
            break;
        case EffectPack::BlockType::Strobe:
            p.setBrush(QColor(24, 24, 28));
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 2, 2);
            p.setBrush(hot);
            p.drawEllipse(r.adjusted(5, 5, -5, -5));
            break;
        case EffectPack::BlockType::Spin:
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(dim, 2));
            p.drawEllipse(r.adjusted(3, 3, -3, -3));
            p.setPen(Qt::NoPen);
            p.setBrush(hot);
            p.drawPie(r.adjusted(3, 3, -3, -3), 40 * 16, 70 * 16);
            break;
        case EffectPack::BlockType::Candle:
        {
            QPainterPath flame;
            const QPointF c = r.center();
            flame.moveTo(c.x(), r.top() + 2);
            flame.quadTo(r.right() - 2, c.y(), c.x(), r.bottom() - 2);
            flame.quadTo(r.left() + 2, c.y(), c.x(), r.top() + 2);
            p.setBrush(ink);
            p.drawPath(flame);
            p.setBrush(hot);
            p.drawEllipse(QRectF(c.x() - 2, c.y() - 1, 4, 5));
            break;
        }
        case EffectPack::BlockType::Dissolve:
            for(int y = 0; y < 4; ++y)
            {
                for(int x = 0; x < 4; ++x)
                {
                    p.setBrush(((x + y) % 3 == 0) ? hot : dim);
                    p.drawRect(r.left() + 3 + x * 4, r.top() + 3 + y * 4, 3, 3);
                }
            }
            break;
        case EffectPack::BlockType::Wave:
        {
            QPainterPath wave;
            wave.moveTo(r.left() + 2, r.center().y());
            wave.cubicTo(r.left() + r.width() * 0.3, r.top() + 2,
                         r.left() + r.width() * 0.7, r.bottom() - 2,
                         r.right() - 2, r.center().y());
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(hot, 2));
            p.drawPath(wave);
            break;
        }
        case EffectPack::BlockType::ColorWash:
        {
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0.0, QColor(255, 80, 80));
            g.setColorAt(0.5, ink);
            g.setColorAt(1.0, QColor(80, 140, 255));
            p.setBrush(g);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 2, 2);
            break;
        }
        case EffectPack::BlockType::Plasma:
            p.setBrush(dim);
            p.drawEllipse(r.left() + 2, r.top() + 3, 9, 9);
            p.setBrush(hot);
            p.drawEllipse(r.right() - 11, r.top() + 2, 8, 8);
            p.setBrush(ink);
            p.drawEllipse(r.left() + 6, r.bottom() - 10, 8, 8);
            break;
        case EffectPack::BlockType::Snow:
            p.setPen(QPen(hot, 1));
            for(const QPoint c : {QPoint(r.left() + 6, r.top() + 5), QPoint(r.right() - 6, r.top() + 8), QPoint(r.center().x(), r.bottom() - 6)})
            {
                p.drawLine(c.x() - 2, c.y(), c.x() + 2, c.y());
                p.drawLine(c.x(), c.y() - 2, c.x(), c.y() + 2);
            }
            break;
        case EffectPack::BlockType::Fire:
        {
            QLinearGradient g(r.topLeft(), r.bottomLeft());
            g.setColorAt(0.0, hot);
            g.setColorAt(1.0, QColor(180, 30, 0));
            p.setBrush(g);
            p.drawRoundedRect(r.left() + 4, r.top() + 3, 4, r.height() - 5, 2, 2);
            p.drawRoundedRect(r.left() + 10, r.top() + 7, 4, r.height() - 9, 2, 2);
            break;
        }
        case EffectPack::BlockType::Balls:
            p.setBrush(hot);
            p.drawEllipse(r.left() + 2, r.top() + 3, 8, 8);
            p.setBrush(ink);
            p.drawEllipse(r.right() - 10, r.bottom() - 10, 8, 8);
            break;
        case EffectPack::BlockType::Bars:
            for(int i = 0; i < 4; ++i)
            {
                const int h = 4 + (i % 3) * 4;
                p.setBrush(i == 2 ? hot : ink);
                p.drawRect(r.left() + 3 + i * 4, r.bottom() - 2 - h, 3, h);
            }
            break;
        case EffectPack::BlockType::Scanner:
            p.setBrush(dim);
            p.drawRect(r.left() + 2, r.center().y() - 2, r.width() - 4, 4);
            p.setBrush(hot);
            p.drawRect(r.center().x() - 2, r.top() + 4, 5, r.height() - 8);
            break;
        case EffectPack::BlockType::SphereWipe:
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(dim, 1));
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
            p.setBrush(hot);
            p.setPen(Qt::NoPen);
            p.drawEllipse(r.adjusted(6, 6, -6, -6));
            break;
        case EffectPack::BlockType::Orbit:
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(dim, 1));
            p.drawEllipse(r.adjusted(3, 3, -3, -3));
            p.setPen(Qt::NoPen);
            p.setBrush(hot);
            p.drawEllipse(r.right() - 7, r.center().y() - 2, 4, 4);
            break;
        case EffectPack::BlockType::Ripple:
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(ink, 1));
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
            p.setPen(QPen(hot, 1));
            p.drawEllipse(r.adjusted(6, 6, -6, -6));
            break;
        case EffectPack::BlockType::Meteor:
        {
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0.0, QColor(0, 0, 0, 0));
            g.setColorAt(1.0, hot);
            p.setBrush(g);
            p.drawRoundedRect(r.left() + 3, r.center().y() - 2, r.width() - 6, 4, 2, 2);
            p.setBrush(hot);
            p.drawEllipse(r.right() - 7, r.center().y() - 3, 5, 5);
            break;
        }
        case EffectPack::BlockType::Noise3D:
            for(int i = 0; i < 9; ++i)
            {
                p.setBrush((i % 2) ? hot : dim);
                p.drawEllipse(r.left() + 3 + (i % 3) * 5, r.top() + 3 + (i / 3) * 5, 3, 3);
            }
            break;
        case EffectPack::BlockType::Burst:
            p.setPen(QPen(hot, 1));
            p.drawLine(r.center(), QPoint(r.center().x(), r.top() + 2));
            p.drawLine(r.center(), QPoint(r.right() - 2, r.center().y()));
            p.drawLine(r.center(), QPoint(r.center().x(), r.bottom() - 2));
            p.drawLine(r.center(), QPoint(r.left() + 2, r.center().y()));
            p.setPen(Qt::NoPen);
            p.setBrush(ink);
            p.drawEllipse(r.center().x() - 2, r.center().y() - 2, 5, 5);
            break;
        case EffectPack::BlockType::Cycle:
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(hot, 2));
            p.drawArc(r.adjusted(3, 3, -3, -3), 30 * 16, 280 * 16);
            break;
        case EffectPack::BlockType::Blink:
            p.setBrush(hot);
            p.drawRoundedRect(r.adjusted(3, 4, -3, -4), 2, 2);
            break;
        case EffectPack::BlockType::Confetti:
            p.setBrush(hot);
            p.drawRect(r.left() + 4, r.top() + 4, 3, 3);
            p.setBrush(ink);
            p.drawRect(r.right() - 8, r.top() + 7, 3, 3);
            p.setBrush(dim);
            p.drawRect(r.left() + 8, r.bottom() - 7, 3, 3);
            break;
        case EffectPack::BlockType::Comet:
        {
            QLinearGradient g(r.topLeft(), r.topRight());
            g.setColorAt(0.0, QColor(0, 0, 0, 0));
            g.setColorAt(1.0, hot);
            p.setBrush(g);
            p.drawRoundedRect(r.left() + 2, r.center().y() - 2, r.width() - 6, 4, 2, 2);
            break;
        }
        case EffectPack::BlockType::Helix:
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(hot, 2));
            p.drawArc(r.adjusted(3, 2, -3, -10), 0, 180 * 16);
            p.drawArc(r.adjusted(3, 8, -3, -2), 180 * 16, 180 * 16);
            break;
        case EffectPack::BlockType::Fill:
            p.setBrush(dim);
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
            p.setBrush(hot);
            p.drawEllipse(r.adjusted(6, 6, -6, -6));
            break;
    }
}

inline QIcon MakeEffectIcon(const Entry& e, int size = 22)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(32, 32, 36));
    p.drawRoundedRect(0, 0, size, size, 3, 3);
    PaintEffectGlyph(p, QRect(1, 1, size - 2, size - 2), e.type, e.swatch);
    return QIcon(pm);
}

inline QMimeData* MakeEffectMime(EffectPack::BlockType type)
{
    auto* mime = new QMimeData();
    QByteArray bytes;
    bytes.append((char)(unsigned char)(int)type);
    mime->setData(QString::fromUtf8(kEffectMimeType), bytes);
    mime->setText(QString::fromUtf8(EffectPack::BlockTypeDisplayName(type)));
    return mime;
}

inline bool EffectTypeFromMime(const QMimeData* mime, EffectPack::BlockType* out)
{
    if(!mime || !out || !mime->hasFormat(QString::fromUtf8(kEffectMimeType)))
    {
        return false;
    }
    const QByteArray bytes = mime->data(QString::fromUtf8(kEffectMimeType));
    if(bytes.isEmpty())
    {
        return false;
    }
    *out = (EffectPack::BlockType)(unsigned char)bytes.at(0);
    return true;
}

inline QMimeData* MakeColorMime(RGBColor color)
{
    auto* mime = new QMimeData();
    const int r = RGBGetRValue(color);
    const int g = RGBGetGValue(color);
    const int b = RGBGetBValue(color);
    QByteArray bytes;
    bytes.append((char)r);
    bytes.append((char)g);
    bytes.append((char)b);
    mime->setData(QString::fromUtf8(kColorMimeType), bytes);
    mime->setColorData(QColor(r, g, b));
    return mime;
}

inline bool ColorFromMime(const QMimeData* mime, RGBColor* out)
{
    if(!mime || !out)
    {
        return false;
    }
    if(mime->hasFormat(QString::fromUtf8(kColorMimeType)))
    {
        const QByteArray bytes = mime->data(QString::fromUtf8(kColorMimeType));
        if(bytes.size() >= 3)
        {
            *out = ToRGBColor((unsigned char)bytes[0], (unsigned char)bytes[1], (unsigned char)bytes[2]);
            return true;
        }
    }
    if(mime->hasColor())
    {
        const QColor c = qvariant_cast<QColor>(mime->colorData());
        if(c.isValid())
        {
            *out = ToRGBColor(c.red(), c.green(), c.blue());
            return true;
        }
    }
    return false;
}

inline QMimeData* MakeGradientPresetMime(const QString& preset_id)
{
    auto* mime = new QMimeData();
    mime->setData(QString::fromUtf8(kGradientPresetMimeType), preset_id.toUtf8());
    mime->setText(preset_id);
    return mime;
}

inline bool GradientPresetFromMime(const QMimeData* mime, QString* out)
{
    if(!mime || !out || !mime->hasFormat(QString::fromUtf8(kGradientPresetMimeType)))
    {
        return false;
    }
    *out = QString::fromUtf8(mime->data(QString::fromUtf8(kGradientPresetMimeType)));
    return !out->isEmpty();
}

inline QMimeData* MakeCurvePresetMime(const QString& preset_id)
{
    auto* mime = new QMimeData();
    mime->setData(QString::fromUtf8(kCurvePresetMimeType), preset_id.toUtf8());
    mime->setText(preset_id);
    return mime;
}

inline bool CurvePresetFromMime(const QMimeData* mime, QString* out)
{
    if(!mime || !out || !mime->hasFormat(QString::fromUtf8(kCurvePresetMimeType)))
    {
        return false;
    }
    *out = QString::fromUtf8(mime->data(QString::fromUtf8(kCurvePresetMimeType)));
    return !out->isEmpty();
}

} // namespace EffectPackCatalog
