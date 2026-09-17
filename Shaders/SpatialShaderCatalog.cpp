// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderCatalog.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"

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

} // namespace SpatialShaderCatalog
