// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"
#include "filesystem.h"
#include <string>
#include <vector>

namespace EffectPack
{

struct PackListEntry
{
    std::string id;
    std::string name;
    filesystem::path path;
    int duration_ms = 0;
    std::string loop;
};

/** Ensure directory exists; write rainbow example if the folder has no packs. */
void EnsureLibrarySeeded(const filesystem::path& dir);

std::vector<PackListEntry> ListPacks(const filesystem::path& dir);

bool LoadPackByPath(const filesystem::path& path, Pack* out, std::string* error);

} // namespace EffectPack
