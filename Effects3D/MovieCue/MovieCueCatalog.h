// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "filesystem.h"
#include <string>
#include <vector>

namespace MovieCue
{

struct CatalogEffect
{
    std::string id;
    std::string name;
    std::string category;
    std::string pack_rel; // relative to library/movie-cue root
    bool photosensitive = false;
};

struct Catalog
{
    int version = 1;
    std::vector<CatalogEffect> effects;
};

bool LoadCatalog(const filesystem::path& catalog_path, Catalog* out, std::string* error);
/** Scan category folders for *.oreffect.json and merge into catalog (disk wins for missing catalog.json). */
void ScanCategoryFolders(const filesystem::path& movie_effects_dir, Catalog* out);
void EnsureCatalogSeeded(const filesystem::path& movie_effects_dir);

const CatalogEffect* FindEffect(const Catalog& catalog, const std::string& effect_id);

bool LoadPackForEffect(const Catalog& catalog,
                       const filesystem::path& movie_effects_dir,
                       const std::string& effect_id,
                       EffectPack::Pack* out,
                       std::string* error);

} // namespace MovieCue
