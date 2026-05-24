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
    if(!OpenRGB3DSpatialPlugin::RMPointer)
    {
        return QString();
    }
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::RMPointer);
    return QString::fromStdString(
        PluginSettingsPaths::SpatialShadersDir(OpenRGB3DSpatialPlugin::RMPointer).string());
}

bool EnsureUserShadersFolder()
{
    if(!OpenRGB3DSpatialPlugin::RMPointer)
    {
        return false;
    }
    PluginSettingsPaths::EnsureSpatialShadersFolder(OpenRGB3DSpatialPlugin::RMPointer);
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
    return paths;
}

} // namespace SpatialShaderCatalog
