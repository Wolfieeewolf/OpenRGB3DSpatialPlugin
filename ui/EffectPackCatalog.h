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
    Category category = Category::Basic;
    EffectPack::BlockType type = EffectPack::BlockType::Solid;
    QColor swatch = QColor(180, 180, 190);
};

inline const char* kEffectMimeType = "application/x-openrgb3d-effect-type";
inline const char* kColorMimeType = "application/x-openrgb3d-rgb-color";
inline const char* kGradientPresetMimeType = "application/x-openrgb3d-gradient-preset";
inline const char* kCurvePresetMimeType = "application/x-openrgb3d-curve-preset";

inline QList<Entry> AllEntries()
{
    return {
        {"Set Level", Category::Basic, EffectPack::BlockType::Solid, QColor(220, 220, 230)},
        {"Fade", Category::Basic, EffectPack::BlockType::Fade, QColor(120, 180, 255)},
        {"Pulse", Category::Basic, EffectPack::BlockType::Pulse, QColor(255, 120, 90)},
        {"Wipe", Category::Basic, EffectPack::BlockType::Wipe, QColor(90, 220, 160)},
        {"Chase", Category::Basic, EffectPack::BlockType::Chase, QColor(90, 200, 90)},
        {"Twinkle", Category::Basic, EffectPack::BlockType::Twinkle, QColor(255, 220, 80)},
        {"Alternating", Category::Basic, EffectPack::BlockType::Alternating, QColor(255, 160, 60)},
        {"Strobe", Category::Basic, EffectPack::BlockType::Strobe, QColor(255, 255, 255)},
        {"Spin", Category::Basic, EffectPack::BlockType::Spin, QColor(80, 200, 255)},
        {"Candle Flicker", Category::Basic, EffectPack::BlockType::Candle, QColor(255, 140, 40)},
        {"Dissolve", Category::Basic, EffectPack::BlockType::Dissolve, QColor(160, 120, 255)},

        {"ColorWash", Category::Pixel, EffectPack::BlockType::ColorWash, QColor(200, 90, 220)},
        {"Plasma", Category::Pixel, EffectPack::BlockType::Plasma, QColor(255, 80, 200)},
        {"Snow", Category::Pixel, EffectPack::BlockType::Snow, QColor(220, 230, 255)},
        {"Fire", Category::Pixel, EffectPack::BlockType::Fire, QColor(255, 100, 20)},
        {"Balls", Category::Pixel, EffectPack::BlockType::Balls, QColor(80, 255, 160)},
        {"Bars", Category::Pixel, EffectPack::BlockType::Bars, QColor(100, 160, 255)},

        {"Sphere Wipe", Category::Volume, EffectPack::BlockType::SphereWipe, QColor(120, 255, 200)},
        {"Orbit", Category::Volume, EffectPack::BlockType::Orbit, QColor(80, 180, 255)},
        {"Ripple", Category::Volume, EffectPack::BlockType::Ripple, QColor(100, 200, 255)},
        {"Meteor", Category::Volume, EffectPack::BlockType::Meteor, QColor(255, 220, 120)},
        {"Noise 3D", Category::Volume, EffectPack::BlockType::Noise3D, QColor(180, 100, 255)},
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

inline QIcon MakeEffectIcon(const Entry& e, int size = 22)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(20, 20, 24));
    p.setBrush(e.swatch);
    p.drawRoundedRect(1, 1, size - 2, size - 2, 3, 3);
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
