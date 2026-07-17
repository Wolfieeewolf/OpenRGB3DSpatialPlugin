// SPDX-License-Identifier: GPL-2.0-only

#include "PluginSettingsPaths.h"

namespace PluginSettingsPaths
{

bool IsEffectProfileFile(const filesystem::path& path)
{
    if(!path.has_filename() || path.extension() != ".json")
    {
        return false;
    }

    const std::string stem = path.stem().string();
    return stem.length() > 14 && stem.compare(stem.length() - 14, 14, ".effectprofile") == 0;
}

bool IsStackPresetFile(const filesystem::path& path)
{
    if(!path.has_filename() || path.extension() != ".json")
    {
        return false;
    }

    const std::string stem = path.stem().string();
    return stem.length() > 6 && stem.compare(stem.length() - 6, 6, ".stack") == 0;
}

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm)
{
    if(!rm)
    {
        return;
    }

    std::error_code ec;
    filesystem::create_directories(SpatialShadersDir(rm), ec);
}

void EnsurePluginDataLayout(OpenRGBPluginAPIInterface* rm)
{
    if(!rm)
    {
        return;
    }

    std::error_code ec;
    filesystem::create_directories(ControllersDir(rm), ec);
    filesystem::create_directories(LayoutsDir(rm), ec);
    filesystem::create_directories(SpatialShadersDir(rm), ec);
    filesystem::create_directories(PluginRoot(rm), ec);
}

} // namespace PluginSettingsPaths
