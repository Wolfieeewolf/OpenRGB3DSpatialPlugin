// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Actions/Action.h"
#include "filesystem.h"
#include <string>
#include <vector>

namespace MovieSync
{

struct CatalogEffect
{
    std::string id;
    std::string name;
    std::string category;
    std::string action_rel;
    bool photosensitive = false;
};

struct Catalog
{
    int version = 1;
    std::vector<CatalogEffect> effects;
};

filesystem::path CatalogFilePath(const filesystem::path& movie_effects_dir);

bool LoadCatalog(const filesystem::path& catalog_path, Catalog* out, std::string* error);
void ScanCategoryFolders(const filesystem::path& movie_effects_dir, Catalog* out);
void EnsureCatalogDirs(const filesystem::path& movie_effects_dir);

const CatalogEffect* FindEffect(const Catalog& catalog, const std::string& effect_id);

bool LoadActionForEffect(const Catalog& catalog,
                         const filesystem::path& movie_effects_dir,
                         const std::string& effect_id,
                         Action::Pack* out,
                         std::string* error);

} // namespace MovieSync
