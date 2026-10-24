// SPDX-License-Identifier: GPL-2.0-only

#include "ActionLibrary.h"

#include <algorithm>
#include <system_error>

namespace Action
{
namespace
{

bool AppendPackFromFile(const filesystem::path& path,
                        const std::string& category,
                        std::vector<ActionListEntry>* out)
{
    if(!out)
    {
        return false;
    }
    const std::string name = path.filename().string();
    if(!IsActionFileName(name))
    {
        return false;
    }
    Pack pack;
    std::string err;
    if(!LoadFromFile(path, &pack, &err))
    {
        return false;
    }
    ActionListEntry item;
    item.id = pack.id;
    item.name = pack.name.empty() ? pack.id : pack.name;
    item.path = path;
    item.duration_ms = pack.duration_ms;
    item.category = category;
    switch(pack.loop)
    {
        case LoopMode::Once: item.loop = "once"; break;
        case LoopMode::Forever: item.loop = "forever"; break;
        case LoopMode::WhileActive: item.loop = "while_active"; break;
        default: item.loop = "once"; break;
    }
    out->push_back(std::move(item));
    return true;
}

void ScanDirFiles(const filesystem::path& dir,
                  const std::string& category,
                  std::vector<ActionListEntry>* out)
{
    std::error_code ec;
    for(const auto& entry : filesystem::directory_iterator(dir, ec))
    {
        if(ec || !entry.is_regular_file())
        {
            continue;
        }
        AppendPackFromFile(entry.path(), category, out);
    }
}

} // namespace

std::vector<ActionListEntry> ListActions(const filesystem::path& dir)
{
    std::vector<ActionListEntry> out;
    std::error_code ec;
    if(!filesystem::is_directory(dir, ec))
    {
        return out;
    }

    // Root packs first (category empty).
    ScanDirFiles(dir, std::string(), &out);

    // One level of category subfolders (same depth as Movie Sync effects/).
    for(const auto& entry : filesystem::directory_iterator(dir, ec))
    {
        if(ec || !entry.is_directory())
        {
            continue;
        }
        const std::string category = entry.path().filename().string();
        if(category.empty() || category[0] == '.' || category[0] == '_')
        {
            continue;
        }
        ScanDirFiles(entry.path(), category, &out);
    }

    std::sort(out.begin(), out.end(),
              [](const ActionListEntry& a, const ActionListEntry& b) {
                  if(a.category != b.category)
                  {
                      return a.category < b.category;
                  }
                  return a.name < b.name;
              });
    return out;
}

bool LoadActionByPath(const filesystem::path& path, Pack* out, std::string* error)
{
    return LoadFromFile(path, out, error);
}

bool LoadActionById(const filesystem::path& dir, const std::string& id, Pack* out, std::string* error)
{
    if(id.empty() || !out)
    {
        if(error)
        {
            *error = "pack id is empty";
        }
        return false;
    }
    for(const ActionListEntry& entry : ListActions(dir))
    {
        if(entry.id == id)
        {
            return LoadFromFile(entry.path, out, error);
        }
    }
    if(error)
    {
        *error = "pack not found: " + id;
    }
    return false;
}

} // namespace Action
