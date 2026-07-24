// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include <QColor>
#include <QIcon>
#include <QList>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QVariant>
#include <algorithm>

/**
 * Shared Basic / Pixel effect catalog for toolbars and right-click menus.
 * Keep both surfaces on this list so they never drift (Vixen/xLights dual UX).
 */
namespace EffectPackCatalog
{

enum class Category
{
    Basic,
    Pixel
};

struct Entry
{
    const char* name = nullptr;
    Category category = Category::Basic;
    EffectPack::BlockType type = EffectPack::BlockType::Solid;
    QColor swatch = QColor(180, 180, 190);
};

inline const char* kEffectMimeType = "application/x-openrgb3d-effect-type";
inline const char* kColorMimeType = "application/x-openrgb3d-rgb-color";
inline const char* kGradientPresetMimeType = "application/x-openrgb3d-gradient-preset";

inline QList<Entry> AllEntries()
{
    // Basic Lighting (Vixen-style) — pack types we ship today.
    // Pixel Lighting starts with ColorWash; more types arrive in later phases.
    return {
        {"Set Level", Category::Basic, EffectPack::BlockType::Solid, QColor(220, 220, 230)},
        {"Fade", Category::Basic, EffectPack::BlockType::Fade, QColor(120, 180, 255)},
        {"Pulse", Category::Basic, EffectPack::BlockType::Pulse, QColor(255, 120, 90)},
        {"Wipe", Category::Basic, EffectPack::BlockType::Wipe, QColor(90, 220, 160)},
        {"Chase", Category::Basic, EffectPack::BlockType::Chase, QColor(90, 200, 90)},
        {"Twinkle", Category::Basic, EffectPack::BlockType::Twinkle, QColor(255, 220, 80)},
        {"ColorWash", Category::Pixel, EffectPack::BlockType::ColorWash, QColor(200, 90, 220)},
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
    }
    return QStringLiteral("Effects");
}

inline QIcon MakeEffectIcon(const Entry& e, int size = 22)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(20, 20, 24));
    p.setBrush(e.swatch);
    p.drawRoundedRect(1, 1, size - 2, size - 2, 3, 3);
    // Tiny type cue so icons aren't only color blobs.
    p.setPen(QColor(15, 15, 18));
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(std::max(8, size / 2 - 1));
    p.setFont(f);
    const QChar ch = e.name && e.name[0] ? QChar(e.name[0]) : QChar('?');
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(ch));
    return QIcon(pm);
}

inline QMimeData* MakeEffectMime(EffectPack::BlockType type)
{
    auto* mime = new QMimeData();
    QByteArray bytes;
    bytes.append((char)(int)type);
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

} // namespace EffectPackCatalog
