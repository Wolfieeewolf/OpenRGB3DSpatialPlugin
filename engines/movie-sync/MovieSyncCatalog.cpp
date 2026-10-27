// SPDX-License-Identifier: GPL-2.0-only

#include "MovieSyncCatalog.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <system_error>

namespace MovieSync
{

filesystem::path CatalogFilePath(const filesystem::path& movie_effects_dir)
{
    if(movie_effects_dir.filename() == "effects")
    {
        return movie_effects_dir.parent_path() / "catalog.json";
    }
    return movie_effects_dir / "catalog.json";
}

bool LoadCatalog(const filesystem::path& catalog_path, Catalog* out, std::string* error)
{
    if(!out)
    {
        if(error) *error = "null catalog";
        return false;
    }
    out->effects.clear();
    std::ifstream in(catalog_path.string(), std::ios::binary);
    if(!in)
    {
        if(error) *error = "cannot open catalog";
        return false;
    }
    nlohmann::json j;
    try
    {
        in >> j;
    }
    catch(...)
    {
        if(error) *error = "invalid catalog json";
        return false;
    }
    out->version = j.value("version", 1);
    if(!j.contains("effects") || !j["effects"].is_array())
    {
        return true;
    }
    for(const auto& ej : j["effects"])
    {
        if(!ej.is_object())
        {
            continue;
        }
        CatalogEffect e;
        e.id = ej.value("id", "");
        e.name = ej.value("name", e.id);
        e.category = ej.value("category", "");
        e.action_rel = ej.value("action", "");
        e.photosensitive = ej.value("photosensitive", false);
        if(!e.id.empty() && !e.action_rel.empty())
        {
            out->effects.push_back(std::move(e));
        }
    }
    return true;
}

void ScanCategoryFolders(const filesystem::path& movie_effects_dir, Catalog* out)
{
    if(!out)
    {
        return;
    }
    std::error_code ec;
    if(!filesystem::is_directory(movie_effects_dir, ec))
    {
        return;
    }
    for(const auto& cat_entry : filesystem::directory_iterator(movie_effects_dir, ec))
    {
        if(ec || !cat_entry.is_directory())
        {
            continue;
        }
        const std::string category = cat_entry.path().filename().string();
        if(category.empty() || category[0] == '.' || category[0] == '_')
        {
            continue;
        }
        std::error_code ec2;
        for(const auto& file_entry : filesystem::directory_iterator(cat_entry.path(), ec2))
        {
            if(ec2 || !file_entry.is_regular_file())
            {
                continue;
            }
            const filesystem::path path = file_entry.path();
            if(!Action::IsActionFileName(path.filename().string()))
            {
                continue;
            }
            Action::Pack pack;
            std::string err;
            if(!Action::LoadFromFile(path, &pack, &err) || pack.id.empty())
            {
                continue;
            }
            bool exists = false;
            for(const CatalogEffect& e : out->effects)
            {
                if(e.id == pack.id)
                {
                    exists = true;
                    break;
                }
            }
            if(exists)
            {
                continue;
            }
            CatalogEffect e;
            e.id = pack.id;
            e.name = pack.name.empty() ? pack.id : pack.name;
            e.category = category;
            e.action_rel = category + "/" + path.filename().string();
            e.photosensitive = false;
            out->effects.push_back(std::move(e));
        }
    }
}

const CatalogEffect* FindEffect(const Catalog& catalog, const std::string& effect_id)
{
    for(const CatalogEffect& e : catalog.effects)
    {
        if(e.id == effect_id)
        {
            return &e;
        }
    }
    return nullptr;
}

bool LoadActionForEffect(const Catalog& catalog,
                       const filesystem::path& movie_effects_dir,
                       const std::string& effect_id,
                       Action::Pack* out,
                       std::string* error)
{
    const CatalogEffect* e = FindEffect(catalog, effect_id);
    if(!e)
    {
        if(error) *error = "unknown effect_id: " + effect_id;
        return false;
    }
    const filesystem::path path = movie_effects_dir / e->action_rel;
    return Action::LoadFromFile(path, out, error);
}

} // namespace MovieSync
