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

inline filesystem::path MeshesDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "meshes";
}

inline filesystem::path EffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effects";
}

inline filesystem::path ShaderFieldDir(OpenRGBPluginAPIInterface* rm)
{
    return EffectsDir(rm) / "shader-field";
}

inline filesystem::path PatternsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "patterns";
}

inline filesystem::path TimelinesDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "timelines";
}

inline filesystem::path TimelineBlocksDir(OpenRGBPluginAPIInterface* rm)
{
    return TimelinesDir(rm) / "blocks";
}

inline filesystem::path UserGradientsFile(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "user-gradients.json";
}

inline filesystem::path UserColorsFile(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "user-colors.json";
}

inline filesystem::path UserCurvesFile(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "user-curves.json";
}

inline filesystem::path BindingsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "bindings";
}

inline filesystem::path EffectBindingsFile(OpenRGBPluginAPIInterface* rm)
{
    return BindingsDir(rm) / "effect-bindings.json";
}

inline filesystem::path StackPresetFile(OpenRGBPluginAPIInterface* rm, const std::string& preset_name)
{
    return PluginRoot(rm) / (preset_name + ".stack.json");
}

bool IsStackPresetFile(const filesystem::path& path);

constexpr const char* kControllerLayoutJsonFilter =
    "3D controller layout (*.json);;All Files (*)";

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm);

void EnsurePluginDataLayout(OpenRGBPluginAPIInterface* rm);

} // namespace PluginSettingsPaths
