// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "OpenRGBPluginInterface.h"
#include "filesystem.h"
#include <string>

namespace PluginSettingsPaths
{
inline filesystem::path ConfigRoot(OpenRGBPluginAPIInterface* rm)
{
    return rm ? rm->GetConfigurationDirectory() : filesystem::path();
}

inline filesystem::path PluginRoot(OpenRGBPluginAPIInterface* rm)
{
    return ConfigRoot(rm) / "plugins" / "settings" / "OpenRGB3DSpatialPlugin";
}

inline filesystem::path ControllersDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "controllers";
}

inline filesystem::path SpatialShadersDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "spatial-shaders";
}

inline filesystem::path LayoutsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "layouts";
}

inline filesystem::path EffectStackFile(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effect_stack.json";
}

inline filesystem::path EffectProfileFile(OpenRGBPluginAPIInterface* rm, const std::string& profile_name)
{
    return PluginRoot(rm) / (profile_name + ".effectprofile.json");
}

inline filesystem::path StackPresetFile(OpenRGBPluginAPIInterface* rm, const std::string& preset_name)
{
    return PluginRoot(rm) / (preset_name + ".stack.json");
}

bool IsEffectProfileFile(const filesystem::path& path);

bool IsStackPresetFile(const filesystem::path& path);

constexpr const char* kControllerLayoutJsonFilter =
    "3D controller layout (*.json);;All Files (*)";

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm);

void EnsurePluginDataLayout(OpenRGBPluginAPIInterface* rm);

} // namespace PluginSettingsPaths
