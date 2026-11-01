// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "OpenRGBPluginInterface.h"
#include "filesystem.h"
#include <string>
#include <system_error>

/* AppData layout — must match OpenRGB3DSpatialPresets folder names (copy-paste install):
 *
 *   …/OpenRGB3DSpatialPlugin/
 *     controllers/<category>/<brand>/      (presets-style; scanned recursively)
 *     effects/volume|audio|media|shader-field|patterns/
 *     action-editor/
 *       toolbox/<section>/*.fx
 *       gradients.json | colors.json | curves.json   (create-on-save)
 *     triggers/
 *       effects/*.action.json                        (root packs)
 *       effects/<category>/*.action.json             (optional folders; Actions Category filter)
 *       triggers.json                                (create-on-save)
 *     movie-sync/
 *       catalog.json
 *       effects/<category>/*.action.json
 *       vlc/
 *     meshes/                                        (plugin-only scene props)
 *     *.stack.json                                   (plugin-only effect-stack presets)
 */

namespace PluginSettingsPaths
{
    /** Fail closed: reject path components that walk above a root. */
    inline bool PathHasDotDot(const filesystem::path& p)
    {
        for(const filesystem::path& part : p)
        {
            if(part == "..")
            {
                return true;
            }
        }
        return false;
    }

    /** Fail closed: candidate must stay under root. Prefers weakly_canonical; falls back to
     *  absolute() so not-yet-created children of an existing root still validate. */
    inline bool PathStaysUnderRoot(const filesystem::path& root, const filesystem::path& candidate)
    {
        if(root.empty() || candidate.empty() || PathHasDotDot(candidate))
        {
            return false;
        }
        std::error_code ec;
        auto resolve = [&](const filesystem::path& p) {
            filesystem::path abs = filesystem::weakly_canonical(p, ec);
            if(ec || abs.empty())
            {
                ec.clear();
                abs = filesystem::absolute(p, ec);
            }
            return (ec || abs.empty()) ? filesystem::path{} : abs;
        };
        const filesystem::path root_abs = resolve(root);
        const filesystem::path cand_abs = resolve(candidate);
        if(root_abs.empty() || cand_abs.empty())
        {
            return false;
        }
        auto root_it = root_abs.begin();
        auto cand_it = cand_abs.begin();
        for(; root_it != root_abs.end(); ++root_it, ++cand_it)
        {
            if(cand_it == cand_abs.end() || *cand_it != *root_it)
            {
                return false;
            }
        }
        return true;
    }

    /** Single filename leaf under a plugin data root — no separators, `..`, or absolute. */
    inline bool IsSafePathLeaf(const std::string& name)
    {
        if(name.empty() || name == "." || name == "..")
        {
            return false;
        }
        const filesystem::path p(name);
        if(p.is_absolute() || PathHasDotDot(p))
        {
            return false;
        }
        return p.filename() == p && p.has_filename();
    }

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

inline filesystem::path StackPresetFile(OpenRGBPluginAPIInterface* rm, const std::string& preset_name)
{
    // Leaf-only name — reject `..` / separators so PluginRoot / name cannot escape.
    if(!rm || !IsSafePathLeaf(preset_name))
    {
        return {};
    }
    return PluginRoot(rm) / (preset_name + ".stack.json");
}

bool IsStackPresetFile(const filesystem::path& path);

constexpr const char* kControllerLayoutJsonFilter =
    "3D controller layout (*.json);;All Files (*)";

void EnsureSpatialShadersFolder(OpenRGBPluginAPIInterface* rm);

void EnsurePluginDataLayout(OpenRGBPluginAPIInterface* rm);

} // namespace PluginSettingsPaths
