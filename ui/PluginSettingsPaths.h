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

/** All drop-in content: library/<category-slug>/… */
inline filesystem::path LibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "library";
}

inline filesystem::path VolumeLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "volume";
}

inline filesystem::path AudioLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "audio";
}

inline filesystem::path MediaLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "media";
}

inline filesystem::path ShaderFieldDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "shader-field";
}

inline filesystem::path PatternsDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "patterns";
}

inline filesystem::path PacksDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "packs";
}

inline filesystem::path PackBlocksDir(OpenRGBPluginAPIInterface* rm)
{
    return PacksDir(rm) / "blocks";
}

inline filesystem::path MovieCueLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "movie-cue";
}

inline filesystem::path AmbilightLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "ambilight";
}

inline filesystem::path ReactiveLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "reactive";
}

inline filesystem::path GameLibraryDir(OpenRGBPluginAPIInterface* rm)
{
    return LibraryDir(rm) / "game";
}

/** @deprecated Prefer LibraryDir(); kept for migration helpers. */
inline filesystem::path EffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effects";
}

/** @deprecated Prefer PacksDir(). */
inline filesystem::path TimelinesDir(OpenRGBPluginAPIInterface* rm)
{
    return PacksDir(rm);
}

/** @deprecated Prefer PackBlocksDir(). */
inline filesystem::path TimelineBlocksDir(OpenRGBPluginAPIInterface* rm)
{
    return PackBlocksDir(rm);
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
