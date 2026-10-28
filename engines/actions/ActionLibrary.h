// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"
#include "filesystem.h"
#include <string>
#include <vector>

namespace Action
{

struct ActionListEntry
{
    std::string id;
    std::string name;
    filesystem::path path;
    int duration_ms = 0;
    std::string loop;
    /** Subfolder under triggers/effects/ (empty = packs in the root). */
    std::string category;
};

/**
 * List action packs in `dir` and one level of subfolders (skip `.` / `_` names).
 * Root packs have empty category; subfolder packs use the folder name.
 */
std::vector<ActionListEntry> ListActions(const filesystem::path& dir);

bool LoadActionByPath(const filesystem::path& path, Pack* out, std::string* error);

bool LoadActionById(const filesystem::path& dir, const std::string& id, Pack* out, std::string* error);

} // namespace Action
