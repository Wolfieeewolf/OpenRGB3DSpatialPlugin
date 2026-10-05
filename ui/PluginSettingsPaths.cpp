// SPDX-License-Identifier: GPL-2.0-only

#include "PluginSettingsPaths.h"

#include "PluginLog.h"
#include "viewport/ScenePropMeshPaths.h"

namespace PluginSettingsPaths
{
namespace
{

bool DirIsEmptyOrMissing(const filesystem::path& dir)
{
    std::error_code ec;
    if(!filesystem::exists(dir, ec) || !filesystem::is_directory(dir, ec))
    {
        return true;
    }
    const auto it = filesystem::directory_iterator(dir, ec);
    return it == filesystem::directory_iterator{};
}

void MigrateDirIfNeeded(const filesystem::path& from, const filesystem::path& to, const char* label)
{
    std::error_code ec;
    if(!filesystem::exists(from, ec) || !filesystem::is_directory(from, ec))
    {
        return;
    }
    if(!DirIsEmptyOrMissing(to))
    {
        return;
    }
    filesystem::create_directories(to.parent_path(), ec);
    filesystem::rename(from, to, ec);
    if(ec)
    {
        ec.clear();
        filesystem::copy(from, to, filesystem::copy_options::recursive, ec);
        if(!ec)
        {
            LOG_INFO("[3DSpatial] Migrated %s → %s (copied)", label, to.string().c_str());
        }
        else
        {
            LOG_WARNING("[3DSpatial] Failed to migrate %s → %s: %s",
                        label,
                        to.string().c_str(),
                        ec.message().c_str());
        }
        return;
    }
    LOG_INFO("[3DSpatial] Migrated %s → %s", label, to.string().c_str());
}

} // namespace

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

    // One-time layout migration (old top-level content roots → library/<category>/).
    const filesystem::path root = PluginRoot(rm);
    MigrateDirIfNeeded(root / "effects" / "spatial", VolumeLibraryDir(rm), "effects/spatial");
    MigrateDirIfNeeded(root / "effects" / "audio", AudioLibraryDir(rm), "effects/audio");
    MigrateDirIfNeeded(root / "effects" / "media", MediaLibraryDir(rm), "effects/media");
    MigrateDirIfNeeded(root / "effects" / "shader-field", ShaderFieldDir(rm), "effects/shader-field");
    MigrateDirIfNeeded(root / "patterns", PatternsDir(rm), "patterns");
    MigrateDirIfNeeded(root / "timelines", PacksDir(rm), "timelines");
    MigrateDirIfNeeded(root / "movie-effects", MovieCueLibraryDir(rm), "movie-effects");

    filesystem::create_directories(LibraryDir(rm), ec);
    filesystem::create_directories(VolumeLibraryDir(rm), ec);
    filesystem::create_directories(AudioLibraryDir(rm), ec);
    filesystem::create_directories(MediaLibraryDir(rm), ec);
    filesystem::create_directories(ShaderFieldDir(rm), ec);
    filesystem::create_directories(PatternsDir(rm), ec);
    filesystem::create_directories(PacksDir(rm), ec);
    filesystem::create_directories(PackBlocksDir(rm), ec);
    filesystem::create_directories(MovieCueLibraryDir(rm), ec);
    filesystem::create_directories(MovieCueLibraryDir(rm) / "explosions", ec);
    filesystem::create_directories(MovieCueLibraryDir(rm) / "pulses", ec);
    filesystem::create_directories(MovieCueLibraryDir(rm) / "washes", ec);
    filesystem::create_directories(MovieCueLibraryDir(rm) / "wraps", ec);
    filesystem::create_directories(AmbilightLibraryDir(rm), ec);
    filesystem::create_directories(ReactiveLibraryDir(rm), ec);
    filesystem::create_directories(GameLibraryDir(rm), ec);
    filesystem::create_directories(BindingsDir(rm), ec);
}

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm)
{
    EnsurePluginDataLayout(rm);
}

} // namespace PluginSettingsPaths
