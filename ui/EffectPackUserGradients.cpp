// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackUserGradients.h"
#include "EffectPackCatalog.h"

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

std::vector<EffectPack::GradientStop> StopsFromJson(const nlohmann::json& in);

struct RawEntry
{
    QString id;
    QString label;
    std::vector<EffectPack::GradientStop> stops;
    bool hidden = false;
};

std::vector<RawEntry> ReadAll(const filesystem::path& path)
{
    std::vector<RawEntry> entries;
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
            if(!item.is_object() || !item.contains("id"))
            {
                continue;
            }
            RawEntry entry;
            entry.id = QString::fromStdString(item["id"].get<std::string>());
            if(item.contains("label") && item["label"].is_string())
            {
                entry.label = QString::fromStdString(item["label"].get<std::string>());
            }
            entry.hidden = item.value("hidden", false);
            entry.stops = StopsFromJson(item.value("stops", nlohmann::json::array()));
            if(entry.id.isEmpty())
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

bool WriteAll(const filesystem::path& path, const std::vector<RawEntry>& entries)
{
    std::error_code ec;
    filesystem::create_directories(path.parent_path(), ec);
    nlohmann::json root;
    root["gradients"] = nlohmann::json::array();
    for(const RawEntry& entry : entries)
    {
        nlohmann::json item = {
            {"id", entry.id.toStdString()},
            {"label", entry.label.toStdString()},
            {"stops", StopsToJson(entry.stops)},
        };
        if(entry.hidden)
        {
            item["hidden"] = true;
        }
        root["gradients"].push_back(std::move(item));
    }
    std::ofstream out(path);
    if(!out)
    {
        return false;
    }
    out << root.dump(2);
    return true;
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

bool IsBuiltin(const QString& id)
{
    for(const EffectPackCatalog::GradientEntry& entry : EffectPackCatalog::GradientEntries())
    {
        if(id == QString::fromUtf8(entry.id ? entry.id : ""))
        {
            return true;
        }
    }
    return false;
}

std::vector<Entry> BuiltinDefaults()
{
    std::vector<Entry> entries;
    for(const EffectPackCatalog::GradientEntry& preset : EffectPackCatalog::GradientEntries())
    {
        const char* id = preset.id ? preset.id : "";
        EffectPack::Block block;
        if(!EffectPack::ApplyGradientPresetId(&block, id))
        {
            continue;
        }
        Entry entry;
        entry.id = QString::fromUtf8(id);
        entry.label = QString::fromUtf8(preset.label ? preset.label : id);
        entry.stops = block.gradient;
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<Entry> Load(const filesystem::path& path)
{
    std::vector<Entry> entries;
    for(const RawEntry& raw : ReadAll(path))
    {
        if(raw.hidden || raw.stops.empty() || IsBuiltin(raw.id))
        {
            continue;
        }
        entries.push_back({raw.id, raw.label, raw.stops});
    }
    return entries;
}

std::vector<Entry> Visible(const filesystem::path& path)
{
    std::vector<Entry> entries = BuiltinDefaults();
    for(const RawEntry& raw : ReadAll(path))
    {
        if(raw.hidden)
        {
            entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Entry& entry) {
                return entry.id == raw.id;
            }), entries.end());
            continue;
        }
        if(raw.stops.empty())
        {
            continue;
        }
        auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) {
            return entry.id == raw.id;
        });
        if(found != entries.end())
        {
            if(!raw.label.isEmpty())
            {
                found->label = raw.label;
            }
            found->stops = raw.stops;
            continue;
        }
        entries.push_back({raw.id, raw.label.isEmpty() ? raw.id : raw.label, raw.stops});
    }
    return entries;
}

std::vector<Entry> Hidden(const filesystem::path& path)
{
    std::vector<Entry> entries;
    for(const RawEntry& raw : ReadAll(path))
    {
        if(!raw.hidden || !IsBuiltin(raw.id))
        {
            continue;
        }
        Entry entry;
        entry.id = raw.id;
        for(const Entry& def : BuiltinDefaults())
        {
            if(def.id == raw.id)
            {
                entry.label = def.label;
                break;
            }
        }
        if(entry.label.isEmpty())
        {
            entry.label = raw.id;
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

bool Save(const filesystem::path& path, const std::vector<Entry>& entries)
{
    std::vector<RawEntry> raw = ReadAll(path);
    raw.erase(std::remove_if(raw.begin(), raw.end(), [](const RawEntry& entry) {
        return !entry.hidden && !IsBuiltin(entry.id);
    }), raw.end());
    for(const Entry& entry : entries)
    {
        if(entry.id.isEmpty() || entry.stops.empty())
        {
            continue;
        }
        raw.push_back({entry.id, entry.label, entry.stops, false});
    }
    return WriteAll(path, raw);
}

bool Replace(const filesystem::path& path, const QString& id, const QString& label, const std::vector<EffectPack::GradientStop>& stops)
{
    if(id.isEmpty() || stops.empty())
    {
        return false;
    }
    std::vector<RawEntry> raw = ReadAll(path);
    bool found = false;
    for(RawEntry& entry : raw)
    {
        if(entry.id != id)
        {
            continue;
        }
        entry.label = label;
        entry.stops = stops;
        entry.hidden = false;
        found = true;
        break;
    }
    if(!found)
    {
        raw.push_back({id, label, stops, false});
    }
    return WriteAll(path, raw);
}

bool Remove(const filesystem::path& path, const QString& id)
{
    if(id.isEmpty())
    {
        return false;
    }
    std::vector<RawEntry> raw = ReadAll(path);
    if(IsBuiltin(id))
    {
        bool found = false;
        for(RawEntry& entry : raw)
        {
            if(entry.id != id)
            {
                continue;
            }
            entry.hidden = true;
            entry.stops.clear();
            found = true;
            break;
        }
        if(!found)
        {
            raw.push_back({id, QString(), {}, true});
        }
    }
    else
    {
        raw.erase(std::remove_if(raw.begin(), raw.end(), [&](const RawEntry& entry) {
            return entry.id == id;
        }), raw.end());
    }
    return WriteAll(path, raw);
}

bool Reset(const filesystem::path& path, const QString& id)
{
    if(!IsBuiltin(id))
    {
        return false;
    }
    std::vector<RawEntry> raw = ReadAll(path);
    raw.erase(std::remove_if(raw.begin(), raw.end(), [&](const RawEntry& entry) {
        return entry.id == id;
    }), raw.end());
    return WriteAll(path, raw);
}

bool IsCustomized(const filesystem::path& path, const QString& id)
{
    for(const RawEntry& entry : ReadAll(path))
    {
        if(entry.id == id && !entry.hidden && !entry.stops.empty())
        {
            return true;
        }
    }
    return false;
}

bool Apply(EffectPack::Block* block, const QString& preset_id, const filesystem::path& path)
{
    if(!block || preset_id.isEmpty())
    {
        return false;
    }
    if(IsBuiltin(preset_id) && !IsCustomized(path, preset_id))
    {
        return false;
    }
    for(const Entry& entry : Visible(path))
    {
        if(entry.id != preset_id || entry.stops.empty())
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
