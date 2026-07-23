// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackLibrary.h"

#include <algorithm>
#include <system_error>

namespace EffectPack
{

void EnsureLibrarySeeded(const filesystem::path& dir)
{
    std::error_code ec;
    filesystem::create_directories(dir, ec);

    bool any = false;
    if(filesystem::is_directory(dir, ec))
    {
        for(const auto& entry : filesystem::directory_iterator(dir, ec))
        {
            if(ec)
            {
                break;
            }
            if(!entry.is_regular_file())
            {
                continue;
            }
            const std::string name = entry.path().filename().string();
            if(IsPackFileName(name))
            {
                any = true;
                break;
            }
        }
    }
    if(any)
    {
        return;
    }

    const Pack example = MakeExampleRainbowWash();
    const filesystem::path out = dir / (example.id + kFileSuffix);
    std::string err;
    SaveToFile(out, example, &err);
}

std::vector<PackListEntry> ListPacks(const filesystem::path& dir)
{
    std::vector<PackListEntry> out;
    std::error_code ec;
    if(!filesystem::is_directory(dir, ec))
    {
        return out;
    }
    for(const auto& entry : filesystem::directory_iterator(dir, ec))
    {
        if(ec || !entry.is_regular_file())
        {
            continue;
        }
        const filesystem::path path = entry.path();
        const std::string name = path.filename().string();
        if(!IsPackFileName(name))
        {
            continue;
        }
        Pack pack;
        std::string err;
        if(!LoadFromFile(path, &pack, &err))
        {
            continue;
        }
        PackListEntry item;
        item.id = pack.id;
        item.name = pack.name.empty() ? pack.id : pack.name;
        item.path = path;
        item.duration_ms = pack.duration_ms;
        switch(pack.loop)
        {
            case LoopMode::Once: item.loop = "once"; break;
            case LoopMode::Forever: item.loop = "forever"; break;
            case LoopMode::WhileActive: item.loop = "while_active"; break;
            default: item.loop = "once"; break;
        }
        out.push_back(std::move(item));
    }
    std::sort(out.begin(), out.end(),
              [](const PackListEntry& a, const PackListEntry& b) { return a.name < b.name; });
    return out;
}

bool LoadPackByPath(const filesystem::path& path, Pack* out, std::string* error)
{
    return LoadFromFile(path, out, error);
}

} // namespace EffectPack
