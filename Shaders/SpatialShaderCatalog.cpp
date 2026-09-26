// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderCatalog.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace SpatialShaderCatalog
{

QString UserShadersFolderPath()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
        return QString();
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::APIPointer);
    return QString::fromStdString(
        PluginSettingsPaths::ShaderFieldDir(OpenRGB3DSpatialPlugin::APIPointer).string());
}

bool EnsureUserShadersFolder()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
        return false;
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::APIPointer);
    return true;
}

QString FindEffectShaderPath(const QString& id)
{
    if(!OpenRGB3DSpatialPlugin::APIPointer || id.isEmpty())
    {
        return QString();
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    const QString root = QString::fromStdString(
        PluginSettingsPaths::EffectsDir(OpenRGB3DSpatialPlugin::APIPointer).string());
    const QString file_name = id + QStringLiteral(".fs");
    QDirIterator it(root, QStringList() << file_name, QDir::Files, QDirIterator::Subdirectories);
    if(it.hasNext())
    {
        return it.next();
    }
    return QString();
}

QString LoadEffectShader(const QString& id)
{
    const QString path = FindEffectShaderPath(id);
    if(path.isEmpty())
    {
        return QString();
    }
    QFile file(path);
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
                    || key == QStringLiteral("pattern") || key == QStringLiteral("index") || key == QStringLiteral("palette")
                    || key == QStringLiteral("class") || key == QStringLiteral("category") || key == QStringLiteral("finish")
                    || key == QStringLiteral("param") || key == QStringLiteral("pattern_key") || key == QStringLiteral("colors")
                    || key == QStringLiteral("supports_strip_colormap") || key == QStringLiteral("supports_height_bands")
                    || key == QStringLiteral("slider") || key == QStringLiteral("resolution") || key == QStringLiteral("rainbow")
                    || key == QStringLiteral("needs_frequency") || key == QStringLiteral("needs_arms") || key == QStringLiteral("show_axis")
                    || key == QStringLiteral("user_colors") || key == QStringLiteral("pattern_label")
                    || key == QStringLiteral("combo") || key == QStringLiteral("option") || key == QStringLiteral("pattern_index")
                    || key == QStringLiteral("pattern_source") || key == QStringLiteral("sample")
                    || key == QStringLiteral("flow") || key == QStringLiteral("global") || key == QStringLiteral("media")
                    || key == QStringLiteral("drive") || key == QStringLiteral("audio_preset") || key == QStringLiteral("shader"))
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
    const QString path = FindEffectShaderPath(id);
    if(path.isEmpty())
    {
        return names;
    }
    QFile file(path);
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
            || key == QStringLiteral("index") || key == QStringLiteral("palette")
            || key == QStringLiteral("class") || key == QStringLiteral("category") || key == QStringLiteral("finish")
            || key == QStringLiteral("param") || key == QStringLiteral("pattern_key") || key == QStringLiteral("colors")
            || key == QStringLiteral("supports_strip_colormap") || key == QStringLiteral("supports_height_bands")
            || key == QStringLiteral("slider") || key == QStringLiteral("resolution") || key == QStringLiteral("rainbow")
            || key == QStringLiteral("needs_frequency") || key == QStringLiteral("needs_arms") || key == QStringLiteral("show_axis")
            || key == QStringLiteral("user_colors") || key == QStringLiteral("pattern_label")
            || key == QStringLiteral("combo") || key == QStringLiteral("option") || key == QStringLiteral("pattern_index")
            || key == QStringLiteral("pattern_source") || key == QStringLiteral("sample")
            || key == QStringLiteral("flow") || key == QStringLiteral("global") || key == QStringLiteral("media"))
        {
            continue;
        }
        break;
    }
    return names;
}

} // namespace SpatialShaderCatalog
