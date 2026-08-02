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
        return 4;
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
    return QFileInfo(path).fileName();
}

} // namespace SpatialShaderCatalog
