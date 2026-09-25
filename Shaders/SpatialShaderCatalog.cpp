// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderCatalog.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"

#include <QFile>
#include <QTextStream>

namespace SpatialShaderCatalog
{

QString UserShadersFolderPath()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
        return QString();
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::APIPointer);
    return QString::fromStdString(
        PluginSettingsPaths::SpatialShadersDir(OpenRGB3DSpatialPlugin::APIPointer).string());
}

bool EnsureUserShadersFolder()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
        return false;
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::APIPointer);
    return true;
}

QString LoadEffectShader(const QString& id)
{
    if(!OpenRGB3DSpatialPlugin::APIPointer || id.isEmpty())
    {
        return QString();
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    const filesystem::path path = PluginSettingsPaths::SpatialEffectsDir(OpenRGB3DSpatialPlugin::APIPointer) / (id.toStdString() + ".fs");
    QFile file(QString::fromStdString(path.string()));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    QString body;
    QTextStream stream(&file);
    bool header = true;
    while(!stream.atEnd())
    {
        const QString line = stream.readLine();
        const QString trimmed = line.trimmed();
        if(header)
        {
            if(trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            {
                continue;
            }
            const int colon = trimmed.indexOf(QLatin1Char(':'));
            if(colon > 0)
            {
                const QString key = trimmed.left(colon).trimmed();
                if(key == QStringLiteral("name") || key == QStringLiteral("description") || key == QStringLiteral("section")
                    || key == QStringLiteral("pattern") || key == QStringLiteral("index") || key == QStringLiteral("palette"))
                {
                    continue;
                }
            }
            header = false;
        }
        body += line;
        body += QLatin1Char('\n');
    }
    return body;
}

QList<EffectShaderPattern> EffectShaderPatterns(const QString& id)
{
    QList<EffectShaderPattern> names;
    if(!OpenRGB3DSpatialPlugin::APIPointer || id.isEmpty())
    {
        return names;
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    const filesystem::path path = PluginSettingsPaths::SpatialEffectsDir(OpenRGB3DSpatialPlugin::APIPointer) / (id.toStdString() + ".fs");
    QFile file(QString::fromStdString(path.string()));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return names;
    }
    QTextStream stream(&file);
    while(!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        if(line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if(colon <= 0)
        {
            break;
        }
        const QString key = line.left(colon).trimmed();
        if(key == QStringLiteral("pattern"))
        {
            QString val = line.mid(colon + 1).trimmed();
            QString tip;
            const int bar = val.indexOf(QLatin1Char('|'));
            if(bar >= 0)
            {
                tip = val.mid(bar + 1).trimmed();
                val = val.left(bar).trimmed();
            }
            if(!val.isEmpty())
            {
                names.push_back({val, tip});
            }
            continue;
        }
        if(key == QStringLiteral("name") || key == QStringLiteral("description") || key == QStringLiteral("section")
            || key == QStringLiteral("index") || key == QStringLiteral("palette"))
        {
            continue;
        }
        break;
    }
    return names;
}

} // namespace SpatialShaderCatalog
