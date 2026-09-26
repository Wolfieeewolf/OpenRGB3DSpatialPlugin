// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackCatalog.h"

#include <QFile>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <system_error>

namespace EffectPackCatalog
{
namespace
{

bool ReadEntry(const filesystem::path& file, const QString& section, Entry* out)
{
#ifdef _WIN32
    QFile handle(QString::fromStdWString(file.wstring()));
#else
    QFile handle(QString::fromStdString(file.string()));
#endif
    if(!handle.open(QIODevice::ReadOnly))
    {
        return false;
    }
    if(file.extension() == ".fx")
    {
        out->id = QString::fromStdString(file.stem().string());
        out->name = out->id;
        out->section = section;
        QTextStream stream(&handle);
        while(!stream.atEnd())
        {
            const QString line = stream.readLine().trimmed();
            if(line.isEmpty() || line.startsWith(QLatin1Char('#')))
            {
                continue;
            }
            const int colon = line.indexOf(':');
            if(colon < 0)
            {
                break;
            }
            const QString key = line.left(colon).trimmed();
            const QString val = line.mid(colon + 1).trimmed();
            if(key == QStringLiteral("name")) out->name = val;
            else if(key == QStringLiteral("description")) out->description = val;
            else if(key == QStringLiteral("knobs"))
            {
                const QStringList parts = val.split(' ', Qt::SkipEmptyParts);
                for(const QString& knob : parts)
                {
                    if(knob == QStringLiteral("color_to")) out->knob_color_to = true;
                    else if(knob == QStringLiteral("direction")) out->knob_direction = true;
                    else if(knob == QStringLiteral("speed")) out->knob_speed = true;
                    else if(knob == QStringLiteral("period")) out->knob_period = true;
                    else if(knob == QStringLiteral("pulse")) out->knob_pulse = true;
                    else if(knob == QStringLiteral("min_intensity")) out->knob_min_intensity = true;
                }
            }
            else if(key == QStringLiteral("icon"))
            {
                out->icon.push_back(val);
            }
            else if(key == QStringLiteral("swatch"))
            {
                const QStringList parts = val.split(' ', Qt::SkipEmptyParts);
                if(parts.size() >= 3)
                {
                    out->swatch = QColor(parts.at(0).toInt(), parts.at(1).toInt(), parts.at(2).toInt());
                }
            }
        }
        return !out->name.isEmpty();
    }
    const QJsonObject obj = QJsonDocument::fromJson(handle.readAll()).object();
    const QJsonArray swatch = obj.value(QStringLiteral("swatch")).toArray();
    out->id = QString::fromStdString(file.stem().string());
    out->name = obj.value(QStringLiteral("name")).toString();
    if(out->name.isEmpty())
    {
        out->name = QString::fromStdString(file.stem().string());
    }
    out->description = obj.value(QStringLiteral("description")).toString();
    out->section = section;
    if(swatch.size() >= 3)
    {
        out->swatch = QColor(swatch.at(0).toInt(180), swatch.at(1).toInt(180), swatch.at(2).toInt(190));
    }
    return true;
}

void ReadJsonFiles(const filesystem::path& dir, const QString& section, QList<Entry>* out)
{
    std::error_code ec;
    if(!filesystem::is_directory(dir, ec))
    {
        return;
    }
    std::vector<filesystem::path> files;
    for(const filesystem::directory_entry& item : filesystem::directory_iterator(dir, ec))
    {
        if(ec || !item.is_regular_file())
        {
            continue;
        }
        const filesystem::path ext = item.path().extension();
        if(ext == ".fx" || ext == ".json")
        {
            files.push_back(item.path());
        }
    }
    std::sort(files.begin(), files.end());
    for(const filesystem::path& file : files)
    {
        Entry entry;
        if(ReadEntry(file, section, &entry))
        {
            out->push_back(entry);
        }
    }
}

} // namespace

QList<Entry> LoadEntries(const filesystem::path& dir)
{
    QList<Entry> entries;
    std::error_code ec;
    if(dir.empty() || !filesystem::is_directory(dir, ec))
    {
        return entries;
    }
    std::vector<filesystem::path> sections;
    for(const filesystem::directory_entry& item : filesystem::directory_iterator(dir, ec))
    {
        if(!ec && item.is_directory())
        {
            sections.push_back(item.path());
        }
    }
    std::sort(sections.begin(), sections.end());
    for(const filesystem::path& section_dir : sections)
    {
        const QString section = QString::fromStdString(section_dir.filename().string());
        ReadJsonFiles(section_dir, section, &entries);
        std::vector<filesystem::path> groups;
        for(const filesystem::directory_entry& item : filesystem::directory_iterator(section_dir, ec))
        {
            if(!ec && item.is_directory())
            {
                groups.push_back(item.path());
            }
        }
        std::sort(groups.begin(), groups.end());
        for(const filesystem::path& group : groups)
        {
            ReadJsonFiles(group, section, &entries);
        }
    }
    return entries;
}

} // namespace EffectPackCatalog
