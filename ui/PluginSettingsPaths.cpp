// SPDX-License-Identifier: GPL-2.0-only

#include "PluginSettingsPaths.h"

#include "viewport/ScenePropMeshPaths.h"

namespace PluginSettingsPaths
{

bool IsStackPresetFile(const filesystem::path& path)
{
    if(!path.has_filename() || path.extension() != ".json")
    {
        return false;
    }

    const std::string stem = path.stem().string();
    return stem.length() > 6 && stem.compare(stem.length() - 6, 6, ".stack") == 0;
}

void EnsurePluginDataLayout(OpenRGBPluginAPIInterface* rm)
{
    if(!rm)
    {
        return;
    }

    std::error_code ec;
    filesystem::create_directories(PluginRoot(rm), ec);
    filesystem::create_directories(ControllersDir(rm), ec);
    filesystem::create_directories(MeshesDir(rm), ec);
    ScenePropMeshPaths::SetMeshesRoot(MeshesDir(rm));

    filesystem::create_directories(EffectsDir(rm), ec);
    filesystem::create_directories(VolumeEffectsDir(rm), ec);
    filesystem::create_directories(AudioEffectsDir(rm), ec);
    filesystem::create_directories(MediaEffectsDir(rm), ec);
    filesystem::create_directories(ShaderFieldDir(rm), ec);
    filesystem::create_directories(PatternsDir(rm), ec);

    filesystem::create_directories(ActionEditorDir(rm), ec);
    filesystem::create_directories(ActionToolboxDir(rm), ec);

    filesystem::create_directories(TriggersDir(rm), ec);
    filesystem::create_directories(TriggerEffectsDir(rm), ec);

    filesystem::create_directories(MovieSyncDir(rm), ec);
    filesystem::create_directories(MovieSyncEffectsDir(rm), ec);
    filesystem::create_directories(MovieSyncEffectsDir(rm) / "explosions", ec);
    filesystem::create_directories(MovieSyncEffectsDir(rm) / "pulses", ec);
    filesystem::create_directories(MovieSyncEffectsDir(rm) / "washes", ec);
    filesystem::create_directories(MovieSyncEffectsDir(rm) / "wraps", ec);
    filesystem::create_directories(MovieSyncDir(rm) / "vlc", ec);

    filesystem::create_directories(GameIntegrationsDir(rm), ec);
    filesystem::create_directories(MinecraftIntegrationDir(rm), ec);
}

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm)
{
    EnsurePluginDataLayout(rm);
}

} // namespace PluginSettingsPaths
