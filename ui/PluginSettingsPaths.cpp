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
    // Volume engine content folder (library category is Volume, not "Spatial").
    filesystem::create_directories(EffectsDir(rm) / "spatial", ec);
    filesystem::create_directories(EffectsDir(rm) / "audio", ec);
    filesystem::create_directories(EffectsDir(rm) / "media", ec);
    filesystem::create_directories(ShaderFieldDir(rm), ec);
    filesystem::create_directories(PatternsDir(rm), ec);
    filesystem::create_directories(TimelinesDir(rm), ec);
    filesystem::create_directories(TimelineBlocksDir(rm), ec);
    filesystem::create_directories(BindingsDir(rm), ec);
}

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm)
{
    EnsurePluginDataLayout(rm);
}

} // namespace PluginSettingsPaths
