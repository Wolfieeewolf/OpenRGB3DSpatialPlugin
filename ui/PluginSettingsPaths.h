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

inline filesystem::path SpatialEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "spatial-effects";
}

inline filesystem::path PatternKernelsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "pattern-kernels";
}

inline filesystem::path EffectPacksDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effect-packs";
}

inline filesystem::path EffectPackEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effect-pack-effects";
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

inline filesystem::path EffectBindingsFile(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effect-bindings.json";
}

inline filesystem::path BindingCatalogDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "binding-catalog";
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
