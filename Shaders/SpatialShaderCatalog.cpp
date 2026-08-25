// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderCatalog.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"
#include "filesystem.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

namespace SpatialShaderCatalog
{

QString UserShadersFolderPath()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
    {
        return QString();
    }
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::APIPointer);
    return QString::fromStdString(
        PluginSettingsPaths::SpatialShadersDir(OpenRGB3DSpatialPlugin::APIPointer).string());
}

bool EnsureUserShadersFolder()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
    {
        return false;
    }
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::APIPointer);
    return true;
}

std::vector<QString> ListPresetPaths()
{
    std::vector<QString> paths;

    QDirIterator bundled(QStringLiteral(":/spatial_shaders"),
                         QStringList() << QStringLiteral("*.fs"),
                         QDir::Files,
                         QDirIterator::Subdirectories);
    while(bundled.hasNext())
    {
        paths.push_back(bundled.next());
    }

    // Fallback if the qrc iterator misses aliases (flat :/spatial_shaders/*.fs).
    if(paths.empty())
    {
        static const char* kBundled[] = {
            "slow_waves.fs",
            "room_plasma.fs",
            "spectrum_glow.fs",
            "ember_field.fs",
            "soft_aurora.fs",
            "soft_ripples.fs",
            "hex_drift.fs",
        };
        for(const char* name : kBundled)
        {
            const QString path = QStringLiteral(":/spatial_shaders/") + QString::fromUtf8(name);
            if(QFileInfo::exists(path))
                paths.push_back(path);
        }
    }

    const QString custom_root = UserShadersFolderPath();
    if(!custom_root.isEmpty())
    {
        QDir custom_dir(custom_root);
        if(custom_dir.exists())
        {
            const QFileInfoList files =
                custom_dir.entryInfoList(QStringList() << QStringLiteral("*.fs"), QDir::Files, QDir::Name);
            for(const QFileInfo& fi : files)
            {
                paths.push_back(fi.absoluteFilePath());
            }
        }
    }

    std::sort(paths.begin(), paths.end(), [](const QString& a, const QString& b) {
        return QFileInfo(a).fileName().compare(QFileInfo(b).fileName(), Qt::CaseInsensitive) < 0;
    });

    // Prefer a motion preset that is readable as the default (first entry).
    auto prefer = [](const QString& path) {
        const QString name = QFileInfo(path).fileName().toLower();
        if(name.contains(QStringLiteral("slow_waves")))
            return 0;
        if(name.contains(QStringLiteral("room_plasma")))
            return 1;
        if(name.contains(QStringLiteral("spectrum")) || name.contains(QStringLiteral("checker")))
            return 2;
        if(name.contains(QStringLiteral("ember")))
            return 3;
        if(name.contains(QStringLiteral("aurora")))
            return 4;
        if(name.contains(QStringLiteral("ripple")))
            return 5;
        if(name.contains(QStringLiteral("hex")))
            return 6;
        return 7;
    };
    std::stable_sort(paths.begin(), paths.end(), [&](const QString& a, const QString& b) {
        return prefer(a) < prefer(b);
    });
    return paths;
}

QString PresetDisplayName(const QString& path)
{
    const QString stem = QFileInfo(path).completeBaseName().toLower();
    if(stem == QStringLiteral("slow_waves"))
        return QStringLiteral("Slow Waves — soft blue bands");
    if(stem == QStringLiteral("room_plasma"))
        return QStringLiteral("Room Plasma — colorful swirl");
    if(stem == QStringLiteral("spectrum_glow"))
        return QStringLiteral("Checker Drift — moving lattice");
    if(stem == QStringLiteral("ember_field"))
        return QStringLiteral("Ripple Ember — fire rings");
    if(stem == QStringLiteral("soft_aurora"))
        return QStringLiteral("Soft Aurora — curtain bands");
    if(stem == QStringLiteral("soft_ripples"))
        return QStringLiteral("Soft Ripples — expanding rings");
    if(stem == QStringLiteral("hex_drift"))
        return QStringLiteral("Hex Drift — lattice wash");
    return QFileInfo(path).fileName();
}

} // namespace SpatialShaderCatalog
