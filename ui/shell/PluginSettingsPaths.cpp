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
    // Paths only — do not create empty content folders. Writers create parents when saving.
    ScenePropMeshPaths::SetMeshesRoot(MeshesDir(rm));
}

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm)
{
    EnsurePluginDataLayout(rm);
}

} // namespace PluginSettingsPaths
