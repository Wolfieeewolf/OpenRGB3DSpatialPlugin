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
};

std::vector<ActionListEntry> ListActions(const filesystem::path& dir);

bool LoadActionByPath(const filesystem::path& path, Pack* out, std::string* error);

bool LoadActionById(const filesystem::path& dir, const std::string& id, Pack* out, std::string* error);

} // namespace Action
