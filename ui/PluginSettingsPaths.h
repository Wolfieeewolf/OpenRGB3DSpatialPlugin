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

/* --- Effect Library (stack engines: Volume / Audio / Media / Shader Field / Patterns) --- */

inline filesystem::path EffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "effects";
}

inline filesystem::path VolumeEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return EffectsDir(rm) / "volume";
}

inline filesystem::path AudioEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return EffectsDir(rm) / "audio";
}

inline filesystem::path MediaEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return EffectsDir(rm) / "media";
}

inline filesystem::path ShaderFieldDir(OpenRGBPluginAPIInterface* rm)
{
    return EffectsDir(rm) / "shader-field";
}

inline filesystem::path PatternsDir(OpenRGBPluginAPIInterface* rm)
{
    return EffectsDir(rm) / "patterns";
}

/* --- Action Editor toolbox (tools / gradients / curves / colors only) --- */

inline filesystem::path ActionEditorDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "action-editor";
}

inline filesystem::path ActionToolboxDir(OpenRGBPluginAPIInterface* rm)
{
    return ActionEditorDir(rm) / "toolbox";
}

inline filesystem::path ActionEditorGradientsFile(OpenRGBPluginAPIInterface* rm)
{
    return ActionEditorDir(rm) / "gradients.json";
}

inline filesystem::path ActionEditorColorsFile(OpenRGBPluginAPIInterface* rm)
{
    return ActionEditorDir(rm) / "colors.json";
}

inline filesystem::path ActionEditorCurvesFile(OpenRGBPluginAPIInterface* rm)
{
    return ActionEditorDir(rm) / "curves.json";
}

/* --- Triggers (when → play action) --- */

inline filesystem::path TriggersDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "triggers";
}

inline filesystem::path TriggerEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return TriggersDir(rm) / "effects";
}

inline filesystem::path TriggersFile(OpenRGBPluginAPIInterface* rm)
{
    return TriggersDir(rm) / "triggers.json";
}

/* --- Movie Sync --- */

inline filesystem::path MovieSyncDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "movie-sync";
}

inline filesystem::path MovieSyncEffectsDir(OpenRGBPluginAPIInterface* rm)
{
    return MovieSyncDir(rm) / "effects";
}

/* --- Game integrations (drop-in jars / notes; not Effect Library) --- */

inline filesystem::path GameIntegrationsDir(OpenRGBPluginAPIInterface* rm)
{
    return PluginRoot(rm) / "game-integrations";
}

inline filesystem::path MinecraftIntegrationDir(OpenRGBPluginAPIInterface* rm)
{
    return GameIntegrationsDir(rm) / "minecraft";
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
