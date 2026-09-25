// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackUserCurves.h"
#include "EffectPackCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <system_error>

namespace EffectPackUserCurves
{
namespace
{

QString PathToQString(const filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::vector<EffectPack::CurvePoint> BuiltinPoints(const char* id)
{
    EffectPack::Block block;
    EffectPack::ApplyBuiltinIntensityCurve(&block, id);
    return block.intensity_curve;
}

std::vector<Entry> Defaults()
{
    std::vector<Entry> entries;
    for(const EffectPackCatalog::CurveEntry& curve : EffectPackCatalog::CurveEntries())
    {
        Entry entry;
        entry.id = QString::fromUtf8(curve.id ? curve.id : "");
        entry.label = QString::fromUtf8(curve.label ? curve.label : curve.id);
        entry.points = BuiltinPoints(curve.id);
        entries.push_back(std::move(entry));
    }
    return entries;
}

QJsonArray PointsToJson(const std::vector<EffectPack::CurvePoint>& points)
{
    QJsonArray arr;
    for(const EffectPack::CurvePoint& point : points)
    {
        arr.push_back(QJsonObject{
            {QStringLiteral("pos"), point.pos},
            {QStringLiteral("value"), point.value},
        });
    }
    return arr;
}

std::vector<EffectPack::CurvePoint> PointsFromJson(const QJsonArray& arr)
{
    std::vector<EffectPack::CurvePoint> points;
    for(const QJsonValue& item : arr)
    {
        const QJsonObject obj = item.toObject();
        if(!obj.contains(QStringLiteral("pos")) || !obj.contains(QStringLiteral("value")))
        {
            continue;
        }
        points.push_back({(float)obj.value(QStringLiteral("pos")).toDouble(),
                          (float)obj.value(QStringLiteral("value")).toDouble()});
    }
    return points;
}

bool Write(const filesystem::path& path, const std::vector<Entry>& entries)
{
    std::error_code ec;
    filesystem::create_directories(path.parent_path(), ec);
    QJsonArray arr;
    for(const Entry& entry : entries)
    {
        arr.push_back(QJsonObject{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("label"), entry.label},
            {QStringLiteral("points"), PointsToJson(entry.points)},
        });
    }
    QFile file(PathToQString(path));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("curves"), arr}}).toJson());
    return true;
}

} // namespace

std::vector<Entry> Load(const filesystem::path& path)
{
    QFile file(PathToQString(path));
    if(!file.open(QIODevice::ReadOnly))
    {
        const std::vector<Entry> defaults = Defaults();
        Write(path, defaults);
        return defaults;
    }
    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("curves")).toArray();
    std::vector<Entry> entries;
    for(const QJsonValue& item : arr)
    {
        const QJsonObject obj = item.toObject();
        Entry entry;
        entry.id = obj.value(QStringLiteral("id")).toString();
        entry.label = obj.value(QStringLiteral("label")).toString(entry.id);
        entry.points = PointsFromJson(obj.value(QStringLiteral("points")).toArray());
        if(entry.id.isEmpty())
        {
            continue;
        }
        entries.push_back(std::move(entry));
    }
    if(entries.empty())
    {
        return Defaults();
    }
    return entries;
}

bool Apply(EffectPack::Block* block, const QString& preset_id, const filesystem::path& path)
{
    if(!block || preset_id.isEmpty())
    {
        return false;
    }
    if(preset_id == QStringLiteral("flat"))
    {
        block->intensity_curve.clear();
        return true;
    }
    for(const Entry& entry : Load(path))
    {
        if(entry.id != preset_id)
        {
            continue;
        }
        block->intensity_curve = entry.points;
        return true;
    }
    return false;
}

} // namespace EffectPackUserCurves
