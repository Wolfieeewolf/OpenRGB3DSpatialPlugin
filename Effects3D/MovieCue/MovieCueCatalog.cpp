// SPDX-License-Identifier: GPL-2.0-only

#include "MovieCueCatalog.h"
#include "EffectPacks/EffectPackLibrary.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <system_error>

namespace MovieCue
{
namespace
{

EffectPack::Pack MakeBurst(const std::string& id, const std::string& name, float intensity)
{
    EffectPack::Pack pack;
    pack.id = id;
    pack.name = name;
    pack.duration_ms = 1600;
    pack.loop = EffectPack::LoopMode::Once;
    pack.priority = 40;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block burst;
    burst.effect_id = "burst";
    burst.start_ms = 0;
    burst.end_ms = pack.duration_ms;
    burst.axis_space = EffectPack::AxisSpace::Room;
    burst.pulse_length = 0.2f;
    burst.intensity = intensity;
    EffectPack::ApplyGradientPresetId(&burst, "fire");
    track.blocks.push_back(burst);
    pack.tracks.push_back(std::move(track));
    return pack;
}

EffectPack::Pack MakeWipe(const std::string& id, const std::string& name, float intensity)
{
    EffectPack::Pack pack;
    pack.id = id;
    pack.name = name;
    pack.duration_ms = 1800;
    pack.loop = EffectPack::LoopMode::Once;
    pack.priority = 35;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block wipe;
    wipe.effect_id = "wipe";
    wipe.start_ms = 0;
    wipe.end_ms = pack.duration_ms;
    wipe.axis_space = EffectPack::AxisSpace::Room;
    wipe.intensity = intensity;
    EffectPack::ApplyGradientPresetId(&wipe, "cyber");
    track.blocks.push_back(wipe);
    pack.tracks.push_back(std::move(track));
    return pack;
}

EffectPack::Pack MakeWash(const std::string& id, const std::string& name, float intensity)
{
    EffectPack::Pack pack;
    pack.id = id;
    pack.name = name;
    pack.duration_ms = 8000;
    pack.loop = EffectPack::LoopMode::WhileActive;
    pack.priority = 18;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block solid;
    solid.effect_id = "solid";
    solid.start_ms = 0;
    solid.end_ms = pack.duration_ms;
    solid.color_from = ToRGBColor(20, 90, 170);
    solid.color = solid.color_from;
    solid.intensity = intensity;
    track.blocks.push_back(solid);
    pack.tracks.push_back(std::move(track));
    return pack;
}

EffectPack::Pack MakeOceanHorizon(const std::string& id, const std::string& name, float intensity, float speed)
{
    EffectPack::Pack pack;
    pack.id = id;
    pack.name = name;
    pack.duration_ms = 10000;
    pack.loop = EffectPack::LoopMode::WhileActive;
    pack.priority = 18;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block ocean;
    ocean.effect_id = "ocean_horizon";
    ocean.start_ms = 0;
    ocean.end_ms = pack.duration_ms;
    ocean.axis_space = EffectPack::AxisSpace::Room;
    ocean.intensity = intensity;
    ocean.speed = speed;
    ocean.period_ms = 10000;
    ocean.pulse_length = 0.25f;
    ocean.gradient = {
        {0.0f, ToRGBColor(10, 42, 74)},
        {0.25f, ToRGBColor(14, 90, 122)},
        {0.45f, ToRGBColor(42, 168, 200)},
        {0.55f, ToRGBColor(168, 212, 240)},
        {0.75f, ToRGBColor(91, 163, 224)},
        {1.0f, ToRGBColor(232, 244, 255)},
    };
    ocean.color = ocean.gradient.front().color;
    ocean.color_from = ocean.gradient.front().color;
    ocean.color_to = ocean.gradient.back().color;
    track.blocks.push_back(ocean);
    pack.tracks.push_back(std::move(track));
    return pack;
}

EffectPack::Pack MakeFlash(const std::string& id, const std::string& name, float intensity)
{
    EffectPack::Pack pack;
    pack.id = id;
    pack.name = name;
    pack.duration_ms = 900;
    pack.loop = EffectPack::LoopMode::Once;
    pack.priority = 50;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block strobe;
    strobe.effect_id = "pulse";
    strobe.start_ms = 0;
    strobe.end_ms = pack.duration_ms;
    strobe.color_from = ToRGBColor(255, 255, 255);
    strobe.color = strobe.color_from;
    strobe.intensity = intensity;
    strobe.pulse_length = 0.08f;
    track.blocks.push_back(strobe);
    pack.tracks.push_back(std::move(track));
    return pack;
}

bool WritePack(const filesystem::path& path, const EffectPack::Pack& pack)
{
    std::string err;
    return EffectPack::SaveToFile(path, pack, &err);
}

void WriteCatalogFile(const filesystem::path& path, const Catalog& catalog)
{
    nlohmann::json j;
    j["format"] = "openrgb3d.movie_effect_catalog";
    j["version"] = catalog.version;
    j["effects"] = nlohmann::json::array();
    for(const CatalogEffect& e : catalog.effects)
    {
        nlohmann::json ej;
        ej["id"] = e.id;
        ej["name"] = e.name;
        ej["category"] = e.category;
        ej["pack"] = e.pack_rel;
        ej["photosensitive"] = e.photosensitive;
        j["effects"].push_back(ej);
    }
    std::ofstream out(path.string(), std::ios::binary);
    if(out)
    {
        out << j.dump(2);
    }
}

} // namespace

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
        e.pack_rel = ej.value("pack", "");
        e.photosensitive = ej.value("photosensitive", false);
        if(!e.id.empty() && !e.pack_rel.empty())
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
        for(const auto& file_entry : filesystem::directory_iterator(cat_entry.path(), ec))
        {
            if(ec || !file_entry.is_regular_file())
            {
                continue;
            }
            const filesystem::path path = file_entry.path();
            if(!EffectPack::IsPackFileName(path.filename().string()))
            {
                continue;
            }
            EffectPack::Pack pack;
            std::string err;
            if(!EffectPack::LoadFromFile(path, &pack, &err) || pack.id.empty())
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
            e.pack_rel = category + "/" + path.filename().string();
            e.photosensitive = false;
            out->effects.push_back(std::move(e));
        }
    }
}

void EnsureCatalogSeeded(const filesystem::path& movie_effects_dir)
{
    std::error_code ec;
    filesystem::create_directories(movie_effects_dir / "explosions", ec);
    filesystem::create_directories(movie_effects_dir / "pulses", ec);
    filesystem::create_directories(movie_effects_dir / "washes", ec);
    filesystem::create_directories(movie_effects_dir / "wraps", ec);

    struct Seed
    {
        const char* category;
        EffectPack::Pack pack;
        bool photosensitive;
    };
    const Seed seeds[] = {
        {"explosions", MakeBurst("explosion", "Explosion", 0.85f), false},
        {"wraps", MakeWipe("laser_sweep", "Laser sweep", 0.8f), false},
        {"washes", MakeWash("room_wash", "Room wash", 0.7f), false},
        {"washes", MakeOceanHorizon("ocean_horizon", "Ocean horizon", 0.8f, 0.5f), false},
        {"pulses", MakeFlash("title_flash", "Title flash", 0.75f), true},
    };

    Catalog catalog;
    catalog.version = 1;
    for(const Seed& seed : seeds)
    {
        const filesystem::path path =
            movie_effects_dir / seed.category / (seed.pack.id + EffectPack::kFileSuffix);
        if(!filesystem::is_regular_file(path, ec))
        {
            WritePack(path, seed.pack);
        }
        CatalogEffect e;
        e.id = seed.pack.id;
        e.name = seed.pack.name;
        e.category = seed.category;
        e.pack_rel = std::string(seed.category) + "/" + seed.pack.id + EffectPack::kFileSuffix;
        e.photosensitive = seed.photosensitive;
        catalog.effects.push_back(std::move(e));
    }

    const filesystem::path catalog_path = movie_effects_dir / "catalog.json";
    if(!filesystem::is_regular_file(catalog_path, ec))
    {
        WriteCatalogFile(catalog_path, catalog);
    }
    else
    {
        Catalog existing;
        if(LoadCatalog(catalog_path, &existing, nullptr))
        {
            for(const CatalogEffect& e : catalog.effects)
            {
                bool has = false;
                for(const CatalogEffect& x : existing.effects)
                {
                    if(x.id == e.id)
                    {
                        has = true;
                        break;
                    }
                }
                if(!has)
                {
                    existing.effects.push_back(e);
                }
            }
            WriteCatalogFile(catalog_path, existing);
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

bool LoadPackForEffect(const Catalog& catalog,
                       const filesystem::path& movie_effects_dir,
                       const std::string& effect_id,
                       EffectPack::Pack* out,
                       std::string* error)
{
    const CatalogEffect* e = FindEffect(catalog, effect_id);
    if(!e)
    {
        if(error) *error = "unknown effect_id: " + effect_id;
        return false;
    }
    const filesystem::path path = movie_effects_dir / e->pack_rel;
    return EffectPack::LoadFromFile(path, out, error);
}

} // namespace MovieCue
