// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackUserGradients.h"

#include <QLinearGradient>
#include <QPainter>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace EffectPackUserGradients
{

namespace
{

nlohmann::json StopsToJson(const std::vector<EffectPack::GradientStop>& stops)
{
    nlohmann::json out = nlohmann::json::array();
    for(const EffectPack::GradientStop& stop : stops)
    {
        out.push_back({
            {"pos", stop.pos},
            {"r", RGBGetRValue(stop.color)},
            {"g", RGBGetGValue(stop.color)},
            {"b", RGBGetBValue(stop.color)},
        });
    }
    return out;
}

std::vector<EffectPack::GradientStop> StopsFromJson(const nlohmann::json& in)
{
    std::vector<EffectPack::GradientStop> stops;
    if(!in.is_array())
    {
        return stops;
    }
    for(const nlohmann::json& item : in)
    {
        if(!item.is_object() || !item.contains("pos") || !item.contains("r") || !item.contains("g") || !item.contains("b"))
        {
            continue;
        }
        EffectPack::GradientStop stop;
        stop.pos = item["pos"].get<float>();
        stop.color = ToRGBColor(item["r"].get<int>(), item["g"].get<int>(), item["b"].get<int>());
        stops.push_back(stop);
    }
    return stops;
}

} // namespace

QString MakeId(const QString& label)
{
    QString id = QStringLiteral("user_");
    for(const QChar ch : label.trimmed().toLower())
    {
        if(ch.isLetterOrNumber())
        {
            id.append(ch);
        }
        else if(ch.isSpace() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
        {
            id.append(QLatin1Char('_'));
        }
    }
    while(id.endsWith(QLatin1Char('_')))
    {
        id.chop(1);
    }
    if(id == QStringLiteral("user"))
    {
        id = QStringLiteral("user_gradient");
    }
    return id;
}

std::vector<Entry> Load(const filesystem::path& path)
{
    std::vector<Entry> entries;
    std::ifstream in(path);
    if(!in)
    {
        return entries;
    }
    try
    {
        nlohmann::json root;
        in >> root;
        if(!root.contains("gradients") || !root["gradients"].is_array())
        {
            return entries;
        }
        for(const nlohmann::json& item : root["gradients"])
        {
            if(!item.is_object() || !item.contains("id") || !item.contains("label"))
            {
                continue;
            }
            Entry entry;
            entry.id = QString::fromStdString(item["id"].get<std::string>());
            entry.label = QString::fromStdString(item["label"].get<std::string>());
            entry.stops = StopsFromJson(item.value("stops", nlohmann::json::array()));
            if(entry.id.isEmpty() || entry.stops.size() < 2)
            {
                continue;
            }
            entries.push_back(std::move(entry));
        }
    }
    catch(const std::exception&)
    {
        return {};
    }
    return entries;
}

bool Save(const filesystem::path& path, const std::vector<Entry>& entries)
{
    std::error_code ec;
    filesystem::create_directories(path.parent_path(), ec);
    nlohmann::json root;
    root["gradients"] = nlohmann::json::array();
    for(const Entry& entry : entries)
    {
        root["gradients"].push_back({
            {"id", entry.id.toStdString()},
            {"label", entry.label.toStdString()},
            {"stops", StopsToJson(entry.stops)},
        });
    }
    std::ofstream out(path);
    if(!out)
    {
        return false;
    }
    out << root.dump(2);
    return true;
}

bool Apply(EffectPack::Block* block, const QString& preset_id, const filesystem::path& path)
{
    if(!block || !preset_id.startsWith(QStringLiteral("user_")))
    {
        return false;
    }
    for(const Entry& entry : Load(path))
    {
        if(entry.id != preset_id)
        {
            continue;
        }
        block->gradient = entry.stops;
        block->color = entry.stops.front().color;
        block->color_from = entry.stops.front().color;
        block->color_to = entry.stops.back().color;
        return true;
    }
    return false;
}

QPixmap Preview(const std::vector<EffectPack::GradientStop>& stops, int w, int h)
{
    QPixmap pm(w, h);
    QPainter painter(&pm);
    QLinearGradient grad(0, 0, w, 0);
    if(stops.empty())
    {
        grad.setColorAt(0.0, QColor(80, 80, 90));
        grad.setColorAt(1.0, QColor(200, 200, 210));
    }
    else
    {
        for(const EffectPack::GradientStop& stop : stops)
        {
            const float pos = std::clamp(stop.pos, 0.0f, 1.0f);
            grad.setColorAt(pos, QColor(RGBGetRValue(stop.color), RGBGetGValue(stop.color), RGBGetBValue(stop.color)));
        }
    }
    painter.fillRect(pm.rect(), grad);
    return pm;
}

} // namespace EffectPackUserGradients
