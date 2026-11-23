// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderCatalog.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringList>
#include <QTextStream>
#include <algorithm>

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
    /* Fail closed: ids are bare stems under effects/, never path fragments. */
    if(id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\'))
       || id.contains(QLatin1String("..")))
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

namespace
{

bool IsEffectShaderHeaderKey(const QString& key)
{
    return key == QStringLiteral("name") || key == QStringLiteral("description") || key == QStringLiteral("section")
        || key == QStringLiteral("pattern") || key == QStringLiteral("index") || key == QStringLiteral("palette")
        || key == QStringLiteral("class") || key == QStringLiteral("category") || key == QStringLiteral("finish")
        || key == QStringLiteral("param") || key == QStringLiteral("pattern_key") || key == QStringLiteral("colors")
        || key == QStringLiteral("supports_strip_colormap") || key == QStringLiteral("supports_height_bands")
        || key == QStringLiteral("slider") || key == QStringLiteral("resolution") || key == QStringLiteral("rainbow")
        || key == QStringLiteral("needs_frequency")
        || key == QStringLiteral("user_colors") || key == QStringLiteral("pattern_label")
        || key == QStringLiteral("combo") || key == QStringLiteral("option") || key == QStringLiteral("pattern_index")
        || key == QStringLiteral("pattern_source") || key == QStringLiteral("sample")
        || key == QStringLiteral("flow") || key == QStringLiteral("global") || key == QStringLiteral("media")
        || key == QStringLiteral("drive") || key == QStringLiteral("audio_preset") || key == QStringLiteral("audio_media")
        || key == QStringLiteral("shader") || key == QStringLiteral("media_layout")
        || key == QStringLiteral("instances");
}

} // namespace

QString LoadShaderBodyFromPath(const QString& path)
{
    if(path.isEmpty())
    {
        return QString();
    }
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    constexpr qint64 kMaxShaderBytes = 512 * 1024;
    if(file.size() < 0 || file.size() > kMaxShaderBytes)
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
                if(IsEffectShaderHeaderKey(key))
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

QList<ExtraParamSlider> LoadExtraParamSliders(const QString& path)
{
    QList<ExtraParamSlider> out;
    QFile file(path);
    if(path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return out;
    }
    QStringList params;
    QHash<QString, ExtraParamSlider> sliders;
    QTextStream stream(&file);
    while(!stream.atEnd())
    {
        const QString trimmed = stream.readLine().trimmed();
        if(trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        const int colon = trimmed.indexOf(QLatin1Char(':'));
        if(colon <= 0 || !IsEffectShaderHeaderKey(trimmed.left(colon).trimmed()))
        {
            break; /* end of header */
        }
        const QString key = trimmed.left(colon).trimmed();
        const QStringList parts = trimmed.mid(colon + 1).split(QLatin1Char('|'));
        const QStringList tok = parts.value(0).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if(key == QStringLiteral("param") && !tok.isEmpty())
        {
            params << tok.last();
        }
        else if(key == QStringLiteral("slider") && tok.size() >= 4)
        {
            /* slider: name min max default [pct|unit] | Label | tooltip */
            ExtraParamSlider s;
            s.name = tok[0];
            s.min = tok[1].toInt();
            s.max = tok[2].toInt();
            s.def = std::clamp(tok[3].toInt(), std::min(s.min, s.max), std::max(s.min, s.max));
            s.pct = tok.contains(QStringLiteral("pct")) || tok.contains(QStringLiteral("unit"));
            s.label = parts.value(1).trimmed();
            if(s.label.isEmpty())
            {
                s.label = s.name;
            }
            if(!s.label.endsWith(QLatin1Char(':')))
            {
                s.label += QLatin1Char(':');
            }
            s.tip = parts.value(2).trimmed();
            sliders.insert(s.name, s);
        }
    }
    for(int i = 4; i < params.size() && out.size() < 4; ++i)
    {
        ExtraParamSlider s = sliders.value(params[i]);
        if(s.name.isEmpty())
        {
            s.name = params[i];
            s.label = params[i] + QLatin1Char(':');
        }
        out << s;
    }
    return out;
}

QList<float> LoadExtraParamDefaults(const QString& path)
{
    QList<float> out;
    QFile file(path);
    if(path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return out;
    }
    QStringList params;
    QHash<QString, float> slider_defaults;
    QTextStream stream(&file);
    while(!stream.atEnd())
    {
        const QString trimmed = stream.readLine().trimmed();
        if(trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        const int colon = trimmed.indexOf(QLatin1Char(':'));
        if(colon <= 0 || !IsEffectShaderHeaderKey(trimmed.left(colon).trimmed()))
        {
            break; /* end of header */
        }
        const QString key = trimmed.left(colon).trimmed();
        QString val = trimmed.mid(colon + 1).trimmed();
        const int bar = val.indexOf(QLatin1Char('|'));
        if(bar >= 0)
        {
            val = val.left(bar).trimmed();
        }
        const QStringList tok = val.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if(key == QStringLiteral("param") && !tok.isEmpty())
        {
            params << tok.last();
        }
        else if(key == QStringLiteral("slider") && tok.size() >= 4)
        {
            /* slider: name min max default [pct|unit] */
            bool ok = false;
            float def = tok[3].toFloat(&ok);
            if(ok)
            {
                if(tok.contains(QStringLiteral("pct")) || tok.contains(QStringLiteral("unit")))
                {
                    def /= 100.0f;
                }
                slider_defaults.insert(tok[0], def);
            }
        }
    }
    for(int i = 4; i < params.size() && out.size() < 4; ++i)
    {
        out << slider_defaults.value(params[i], 0.0f);
    }
    return out;
}

QString LoadEffectShader(const QString& id)
{
    return LoadShaderBodyFromPath(FindEffectShaderPath(id));
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
        if(IsEffectShaderHeaderKey(key))
        {
            continue;
        }
        break;
    }
    return names;
}

} // namespace SpatialShaderCatalog
