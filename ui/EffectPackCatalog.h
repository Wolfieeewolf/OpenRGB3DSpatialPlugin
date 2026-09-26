// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "filesystem.h"
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QIcon>
#include <QList>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVariant>

/** Effect-pack toolbar catalog. Entries come from timelines/blocks/<section>/*.fx. */
namespace EffectPackCatalog
{

struct Entry
{
    QString id;
    QString name;
    QString description;
    QString section;
    QColor swatch = QColor(180, 180, 190);
    bool knob_color_to = false;
    bool knob_direction = false;
    bool knob_speed = false;
    bool knob_period = false;
    bool knob_pulse = false;
    bool knob_min_intensity = false;
    QStringList icon;
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

QList<Entry> LoadEntries(const filesystem::path& dir);

inline QList<Entry> EntriesFor(const QList<Entry>& entries, const QString& section)
{
    QList<Entry> out;
    for(const Entry& e : entries)
    {
        if(e.section.compare(section, Qt::CaseInsensitive) == 0)
        {
            out.push_back(e);
        }
    }
    return out;
}

inline QStringList SectionOrder(const QList<Entry>& entries)
{
    QStringList order;
    auto add = [&](const QString& section) {
        for(const QString& have : order)
        {
            if(have.compare(section, Qt::CaseInsensitive) == 0)
            {
                return;
            }
        }
        order.push_back(section);
    };
    for(const char* preferred : {"basic", "pixel", "volume"})
    {
        for(const Entry& e : entries)
        {
            if(e.section.compare(QString::fromUtf8(preferred), Qt::CaseInsensitive) == 0)
            {
                add(e.section);
                break;
            }
        }
    }
    QStringList rest;
    for(const Entry& e : entries)
    {
        bool known = false;
        for(const QString& have : order)
        {
            if(have.compare(e.section, Qt::CaseInsensitive) == 0)
            {
                known = true;
                break;
            }
        }
        if(!known)
        {
            rest.push_back(e.section);
        }
    }
    rest.sort(Qt::CaseInsensitive);
    for(const QString& section : rest)
    {
        add(section);
    }
    return order;
}

inline QString SectionLabel(const QString& section)
{
    const QString key = section.trimmed().toLower();
    if(key == QStringLiteral("basic")) return QStringLiteral("Basic");
    if(key == QStringLiteral("pixel")) return QStringLiteral("Pixel");
    if(key == QStringLiteral("volume")) return QStringLiteral("Volume");
    if(section.trimmed().isEmpty()) return QStringLiteral("Effects");
    return section.trimmed();
}

inline QString EffectTooltip(const Entry& e)
{
    const QString name = e.name.isEmpty() ? QStringLiteral("Effect") : e.name;
    const QString desc = e.description;
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
        {"Color", "solid"},
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
    if(sid == QStringLiteral("solid"))
    {
        grad.setColorAt(0.0, QColor(255, 80, 40));
        grad.setColorAt(1.0, QColor(255, 80, 40));
    }
    else if(sid == QStringLiteral("rainbow"))
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

inline QRectF IconBox(const QRect& r, float x, float y, float w, float h)
{
    return QRectF(r.left() + x * r.width(), r.top() + y * r.height(), w * r.width(), h * r.height());
}

inline QColor IconInk(const QString& name, const QColor& ink)
{
    if(name == QStringLiteral("hot")) return ink.lighter(145);
    if(name == QStringLiteral("dim")) return ink.darker(165);
    if(name == QStringLiteral("none")) return QColor(0, 0, 0, 0);
    if(name == QStringLiteral("dark")) return QColor(24, 24, 28);
    return ink;
}

inline void PaintEffectIcon(QPainter& p, const QRect& r, const QStringList& lines, const QColor& ink)
{
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    if(lines.isEmpty())
    {
        p.drawRoundedRect(IconBox(r, 0.08f, 0.08f, 0.84f, 0.84f), 2, 2);
        return;
    }
    auto num = [](const QStringList& parts, int i) {
        return i < parts.size() ? parts.at(i).toFloat() : 0.0f;
    };
    for(const QString& line : lines)
    {
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if(parts.isEmpty())
        {
            continue;
        }
        const QString cmd = parts.at(0);
        if(cmd == QStringLiteral("brush"))
        {
            p.setBrush(IconInk(parts.value(1), ink));
        }
        else if(cmd == QStringLiteral("pen"))
        {
            const float w = std::max(1.0f, num(parts, 2) * (float)std::min(r.width(), r.height()));
            const QColor c = IconInk(parts.value(1), ink);
            if(parts.value(1) == QStringLiteral("none"))
            {
                p.setPen(Qt::NoPen);
            }
            else
            {
                p.setPen(QPen(c, w));
            }
            p.setBrush(Qt::NoBrush);
        }
        else if(cmd == QStringLiteral("gradh") || cmd == QStringLiteral("gradv") || cmd == QStringLiteral("gradd"))
        {
            const QRectF box = IconBox(r, num(parts, 1), num(parts, 2), num(parts, 3), num(parts, 4));
            QLinearGradient g(box.topLeft(), cmd == QStringLiteral("gradv") ? box.bottomLeft()
                : (cmd == QStringLiteral("gradd") ? box.bottomRight() : box.topRight()));
            g.setColorAt(0.0, IconInk(parts.value(5, QStringLiteral("dim")), ink));
            g.setColorAt(1.0, IconInk(parts.value(6, QStringLiteral("hot")), ink));
            p.setBrush(g);
            p.setPen(Qt::NoPen);
        }
        else if(cmd == QStringLiteral("gradr"))
        {
            const QRectF box = IconBox(r, num(parts, 1), num(parts, 2), num(parts, 3), num(parts, 4));
            QRadialGradient g(box.center(), std::max(box.width(), box.height()) * 0.5);
            g.setColorAt(0.0, IconInk(parts.value(5, QStringLiteral("hot")), ink));
            g.setColorAt(1.0, IconInk(parts.value(6, QStringLiteral("dim")), ink));
            p.setBrush(g);
            p.setPen(Qt::NoPen);
        }
        else if(cmd == QStringLiteral("rect"))
        {
            p.drawRect(IconBox(r, num(parts, 1), num(parts, 2), num(parts, 3), num(parts, 4)));
        }
        else if(cmd == QStringLiteral("round"))
        {
            const QRectF box = IconBox(r, num(parts, 1), num(parts, 2), num(parts, 3), num(parts, 4));
            const float rx = num(parts, 5) * r.width();
            p.drawRoundedRect(box, rx, rx);
        }
        else if(cmd == QStringLiteral("ellipse"))
        {
            p.drawEllipse(IconBox(r, num(parts, 1), num(parts, 2), num(parts, 3), num(parts, 4)));
        }
        else if(cmd == QStringLiteral("line"))
        {
            const QPointF a(r.left() + num(parts, 1) * r.width(), r.top() + num(parts, 2) * r.height());
            const QPointF b(r.left() + num(parts, 3) * r.width(), r.top() + num(parts, 4) * r.height());
            p.drawLine(a, b);
        }
        else if(cmd == QStringLiteral("pie") || cmd == QStringLiteral("arc"))
        {
            const QRectF box = IconBox(r, num(parts, 1), num(parts, 2), num(parts, 3), num(parts, 4));
            const int start = (int)std::lround(num(parts, 5) * 16.0f);
            const int span = (int)std::lround(num(parts, 6) * 16.0f);
            if(cmd == QStringLiteral("pie"))
            {
                p.drawPie(box, start, span);
            }
            else
            {
                p.drawArc(box, start, span);
            }
        }
        else if(cmd == QStringLiteral("poly"))
        {
            QPainterPath path;
            bool moved = false;
            for(int i = 1; i + 1 < parts.size(); i += 2)
            {
                const QPointF pt(r.left() + parts.at(i).toFloat() * r.width(), r.top() + parts.at(i + 1).toFloat() * r.height());
                if(!moved)
                {
                    path.moveTo(pt);
                    moved = true;
                }
                else
                {
                    path.lineTo(pt);
                }
            }
            path.closeSubpath();
            p.drawPath(path);
        }
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
    PaintEffectIcon(p, QRect(1, 1, size - 2, size - 2), e.icon, e.swatch);
    return QIcon(pm);
}

inline QMimeData* MakeEffectMime(const QString& effect_id)
{
    auto* mime = new QMimeData();
    mime->setData(QString::fromUtf8(kEffectMimeType), effect_id.toUtf8());
    mime->setText(effect_id);
    return mime;
}

inline QString EffectIdFromMime(const QMimeData* mime)
{
    if(!mime || !mime->hasFormat(QString::fromUtf8(kEffectMimeType)))
    {
        return QString();
    }
    return QString::fromUtf8(mime->data(QString::fromUtf8(kEffectMimeType)));
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
