// SPDX-License-Identifier: GPL-2.0-only

#include "FolderVolumeEffect.h"
#include "EffectListManager3D.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"
#include "SpatialShaderCatalog.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "AudioReactiveCommon.h"

#include "filesystem.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QString>
#include <algorithm>

namespace
{
RGBColor ColorFromHex(const QString& text)
{
    bool ok = false;
    const unsigned int rgb = text.toUInt(&ok, 16);
    if(!ok)
    {
        return 0;
    }
    const unsigned int r = (rgb >> 16) & 0xFF;
    const unsigned int g = (rgb >> 8) & 0xFF;
    const unsigned int b = rgb & 0xFF;
    return (b << 16) | (g << 8) | r;
}


FolderVolumeSpec ReadSpec(const QString& path)
{
    FolderVolumeSpec spec;
    spec.shader_id = QFileInfo(path).completeBaseName().toStdString();
    spec.ui_name = spec.shader_id;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return spec;
    }
    constexpr qint64 kMaxSpecBytes = 512 * 1024;
    if(file.size() < 0 || file.size() > kMaxSpecBytes)
    {
        return spec;
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
        QString val = line.mid(colon + 1).trimmed();
        QString tip;
        const int bar = val.indexOf(QLatin1Char('|'));
        if(bar >= 0 && key == QStringLiteral("pattern"))
        {
            tip = val.mid(bar + 1).trimmed();
            val = val.left(bar).trimmed();
        }
        if(key == QStringLiteral("global"))
        {
            const QStringList names = val.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for(const QString& name : names)
            {
                if(name == QStringLiteral("speed")) spec.show_speed = true;
                else if(name == QStringLiteral("brightness")) spec.show_brightness = true;
                else if(name == QStringLiteral("frequency")) spec.show_frequency = true;
                else if(name == QStringLiteral("detail")) spec.show_detail = true;
                else if(name == QStringLiteral("size")) spec.show_size = true;
                else if(name == QStringLiteral("scale")) spec.show_scale = true;
                else if(name == QStringLiteral("fps")) spec.show_fps = true;
                else if(name == QStringLiteral("color")) spec.show_color = true;
                else if(name == QStringLiteral("surface")) spec.show_surface = true;
                else if(name == QStringLiteral("position")) spec.show_position = true;
                else if(name == QStringLiteral("axis") || name == QStringLiteral("path")) spec.show_path = true;
                else if(name == QStringLiteral("thickness")) spec.show_thickness = true;
                else if(name == QStringLiteral("edge")) spec.show_edge = true;
                else if(name == QStringLiteral("count")) spec.show_count = true;
                else if(name == QStringLiteral("plane")) spec.show_plane = true;
                else if(name == QStringLiteral("strip")) spec.strip_colormap = true;
                else if(name == QStringLiteral("bands")) spec.height_bands = true;
                else if(name == QStringLiteral("media")) spec.uses_media = true;
                else if(name == QStringLiteral("audio")) spec.uses_audio = true;
            }
        }
        else if(key == QStringLiteral("pattern_source") && val == QStringLiteral("kernels"))
        {
            spec.pattern_from_kernels = true;
        }
        else if(key == QStringLiteral("class"))
        {
            spec.class_name = val.toStdString();
        }
        else if(key == QStringLiteral("name"))
        {
            spec.ui_name = val.toStdString();
        }
        else if(key == QStringLiteral("category"))
        {
            spec.category = val.toStdString();
        }
        else if(key == QStringLiteral("description"))
        {
            spec.description = val.toStdString();
        }
        else if(key == QStringLiteral("pattern_key"))
        {
            spec.pattern_key = val.toStdString();
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("depth"))
        {
            spec.finish_depth = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("rgb"))
        {
            spec.finish_rgb = true;
        }
        else if(key == QStringLiteral("media")
                && (val == QStringLiteral("yes") || val == QStringLiteral("true")))
        {
            spec.uses_media = true;
        }
        else if(key == QStringLiteral("supports_strip_colormap") && val == QStringLiteral("true"))
        {
            spec.strip_colormap = true;
        }
        else if(key == QStringLiteral("supports_height_bands") && val == QStringLiteral("true"))
        {
            spec.height_bands = true;
        }
        else if(key == QStringLiteral("param"))
        {
            spec.params.push_back(val.toStdString());
        }
        else if(key == QStringLiteral("colors"))
        {
            const QStringList parts = val.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for(const QString& part : parts)
            {
                spec.colors.push_back(ColorFromHex(part));
            }
        }
        else if(key == QStringLiteral("slider"))
        {
            QString body = line.mid(colon + 1).trimmed();
            QString label;
            QString slider_tip;
            const int bar1 = body.indexOf(QLatin1Char('|'));
            if(bar1 >= 0)
            {
                label = body.mid(bar1 + 1).trimmed();
                body = body.left(bar1).trimmed();
                const int bar2 = label.indexOf(QLatin1Char('|'));
                if(bar2 >= 0)
                {
                    slider_tip = label.mid(bar2 + 1).trimmed();
                    label = label.left(bar2).trimmed();
                }
            }
            const QStringList parts = body.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if(parts.size() >= 4)
            {
                FolderVolumeSpec::Slider slider;
                slider.key = parts[0].toStdString();
                slider.min = parts[1].toInt();
                slider.max = parts[2].toInt();
                slider.value = parts[3].toInt();
                slider.label = label;
                slider.tip = slider_tip;
                for(int p = 4; p < parts.size(); ++p)
                {
                    if(parts[p] == QStringLiteral("unit"))
                    {
                        slider.save_unit = true;
                    }
                    if(parts[p] == QStringLiteral("pct"))
                    {
                        slider.show_percent = true;
                    }
                }
                spec.steps.push_back({1, (int)spec.sliders.size()});
                spec.sliders.push_back(std::move(slider));
            }
        }
        else if(key == QStringLiteral("flow"))
        {
            spec.flow.push_back(val.toFloat());
        }
        else if(key == QStringLiteral("pattern_index"))
        {
            spec.pattern_index_default = val.toInt();
        }
        else if(key == QStringLiteral("pattern_label"))
        {
            spec.pattern_label = val;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("hex"))
        {
            spec.finish_hex = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("hsv"))
        {
            spec.finish_hsv = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("surface"))
        {
            spec.finish_surface = true;
        }
        else if(key == QStringLiteral("sample") && val == QStringLiteral("room"))
        {
            spec.sample_room = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("atlas"))
        {
            spec.finish_hex = true;
            spec.finish_atlas = true;
        }
        else if(key == QStringLiteral("resolution"))
        {
            spec.resolution = val.toInt();
        }
        else if(key == QStringLiteral("rainbow") && val == QStringLiteral("true"))
        {
            spec.rainbow = true;
        }
        else if(key == QStringLiteral("needs_frequency"))
        {
            spec.needs_frequency = val == QStringLiteral("true");
        }
        else if(key == QStringLiteral("needs_arms") || key == QStringLiteral("show_axis")
                || key == QStringLiteral("soft_catch"))
        {
            /* Catalog / UI hints — ignore so later finish/param/slider lines still parse. */
        }
        else if(key == QStringLiteral("user_colors"))
        {
            spec.user_colors = val.toInt();
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("spiral"))
        {
            spec.finish_spiral = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("audio"))
        {
            spec.finish_audio = true;
        }
        else if(key == QStringLiteral("drive") && val == QStringLiteral("audio"))
        {
            spec.uses_audio = true;
        }
        else if(key == QStringLiteral("audio_preset"))
        {
            spec.audio_preset = val.toStdString();
        }
        else if(key == QStringLiteral("audio_media"))
        {
            spec.audio_media = val.toStdString();
        }
        else if(key == QStringLiteral("shader"))
        {
            spec.shader_override = val.toStdString();
        }
        else if(key == QStringLiteral("media_layout"))
        {
            spec.media_layout = val.toLower().toStdString();
        }
        else if(key == QStringLiteral("instances"))
        {
            spec.effect_instances = std::max(0, val.toInt());
        }
        else if(key == QStringLiteral("combo"))
        {
            const int bar = val.indexOf('|');
            QString head = bar < 0 ? val.trimmed() : val.left(bar).trimmed();
            QString label = bar < 0 ? val.trimmed() : val.mid(bar + 1).trimmed();
            const QStringList head_parts = head.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            int start = 0;
            if(head_parts.size() >= 2)
            {
                start = head_parts[1].toInt();
                head = head_parts[0];
            }
            spec.combos.push_back({head.toStdString(), label, start, {}});
            spec.steps.push_back({2, (int)spec.combos.size() - 1});
        }
        else if(key == QStringLiteral("option") && !spec.combos.empty())
        {
            spec.combos.back().options.push_back({val, tip});
        }
        else if(key == QStringLiteral("pattern") && !val.isEmpty())
        {
            if(spec.patterns.empty())
            {
                spec.steps.push_back({0, 0});
            }
            spec.patterns.push_back({val, tip});
        }
        else if(key != QStringLiteral("name") && key != QStringLiteral("description") && key != QStringLiteral("section")
                && key != QStringLiteral("index") && key != QStringLiteral("palette")
                && key != QStringLiteral("pattern_source")
                && key != QStringLiteral("drive") && key != QStringLiteral("audio_preset")
                && key != QStringLiteral("audio_media")
                && key != QStringLiteral("shader")
                && key != QStringLiteral("media_layout")
                && key != QStringLiteral("instances")
                && key != QStringLiteral("needs_arms")
                && key != QStringLiteral("show_axis")
                && key != QStringLiteral("soft_catch"))
        {
            break;
        }
    }
    if(spec.pattern_from_kernels)
    {
        spec.patterns.clear();
        for(int i = 0; i < SpatialPatternKernelCount(); ++i)
        {
            spec.patterns.push_back({QString::fromUtf8(SpatialPatternKernelDisplayName(i)), QString()});
        }
        if(!spec.patterns.empty() && spec.steps.empty())
        {
            spec.steps.push_back({0, 0});
        }
        else if(!spec.patterns.empty())
        {
            bool has_pattern_step = false;
            for(const auto& step : spec.steps)
            {
                if(step.kind == 0)
                {
                    has_pattern_step = true;
                    break;
                }
            }
            if(!has_pattern_step)
            {
                size_t insert_at = 0;
                for(size_t i = 0; i < spec.steps.size(); ++i)
                {
                    if(spec.steps[i].kind == 2)
                    {
                        insert_at = i + 1;
                        break;
                    }
                }
                spec.steps.insert(spec.steps.begin() + (std::ptrdiff_t)insert_at, {0, 0});
            }
        }
    }
    return spec;
}

} // namespace

void RegisterFolderVolumeEffects()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
    {
        return;
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    auto* rm = OpenRGB3DSpatialPlugin::APIPointer;
    const filesystem::path scan_roots[] = {
        PluginSettingsPaths::VolumeEffectsDir(rm),
        PluginSettingsPaths::AudioEffectsDir(rm),
        PluginSettingsPaths::MediaEffectsDir(rm),
    };
    for(const filesystem::path& scan_root : scan_roots)
    {
    const QString root = QString::fromStdString(scan_root.string());
    QDirIterator files(root, QStringList() << QStringLiteral("*.fs"), QDir::Files, QDirIterator::Subdirectories);
    while(files.hasNext())
    {
        const QFileInfo fi(files.next());
        FolderVolumeSpec spec = ReadSpec(fi.absoluteFilePath());
        if(spec.class_name.empty())
        {
            continue;
        }
        const QString abs = fi.absoluteFilePath();
        const std::string class_name = spec.class_name;
        const std::string ui_name = spec.ui_name;
        std::string category = "Volume";
        if(abs.contains(QStringLiteral("/audio/")) || abs.contains(QStringLiteral("\\audio\\")))
        {
            category = "Audio";
        }
        else if(abs.contains(QStringLiteral("/media/")) || abs.contains(QStringLiteral("\\media\\")))
        {
            category = "Media";
        }
        else if(abs.contains(QStringLiteral("/volume/")) || abs.contains(QStringLiteral("\\volume\\"))
                || abs.contains(QStringLiteral("/spatial/")) || abs.contains(QStringLiteral("\\spatial\\")))
        {
            category = "Volume";
        }
        else if(!spec.category.empty())
        {
            category = spec.category;
        }
        FolderVolumeSpec reg_spec = spec;
        reg_spec.category = category;
        EffectListManager3D::get()->RegisterEffect(
            class_name, ui_name, category, "", "",
            [reg_spec]() { return new FolderVolumeEffect(reg_spec); });
    }
    } // scan_roots
}
