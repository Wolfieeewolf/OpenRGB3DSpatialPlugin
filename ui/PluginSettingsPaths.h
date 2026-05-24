// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "ResourceManagerInterface.h"
#include "filesystem.h"
#include <string>

namespace PluginSettingsPaths
{
inline filesystem::path ConfigRoot(ResourceManagerInterface* rm)
{
    return rm ? rm->GetConfigurationDirectory() : filesystem::path();
}

inline filesystem::path PluginRoot(ResourceManagerInterface* rm)
{
    return ConfigRoot(rm) / "plugins" / "settings" / "OpenRGB3DSpatialPlugin";
}

inline filesystem::path ControllersDir(ResourceManagerInterface* rm)
{
    return PluginRoot(rm) / "controllers";
}

inline filesystem::path SpatialShadersDir(ResourceManagerInterface* rm)
{
    return PluginRoot(rm) / "spatial-shaders";
}

inline filesystem::path LayoutsDir(ResourceManagerInterface* rm)
{
    return PluginRoot(rm) / "layouts";
}

inline filesystem::path EffectStackFile(ResourceManagerInterface* rm)
{
    return PluginRoot(rm) / "effect_stack.json";
}

inline filesystem::path EffectProfileFile(ResourceManagerInterface* rm, const std::string& profile_name)
{
    return PluginRoot(rm) / (profile_name + ".effectprofile.json");
}

inline filesystem::path StackPresetFile(ResourceManagerInterface* rm, const std::string& preset_name)
{
    return PluginRoot(rm) / (preset_name + ".stack.json");
}

bool IsEffectProfileFile(const filesystem::path& path);

bool IsStackPresetFile(const filesystem::path& path);

constexpr const char* kControllerLayoutJsonFilter =
    "3D controller layout (*.json);;All Files (*)";

void EnsureSpatialShadersFolder(ResourceManagerInterface* rm);

void EnsurePluginDataLayout(ResourceManagerInterface* rm);

} // namespace PluginSettingsPaths
