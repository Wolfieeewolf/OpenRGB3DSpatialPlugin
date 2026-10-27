// SPDX-License-Identifier: GPL-2.0-only

#include "MovieSyncRuntime.h"
#include "MovieSyncCatalog.h"
#include "PluginLog.h"

#include <algorithm>

namespace MovieSync
{

void CueRuntime::OnEffect(const std::string& effect_id, const std::string& edge, bool active)
{
    last_error_.clear();
    last_warning_.clear();
    if(effect_id.empty())
    {
        last_error_ = "effect_id required";
        return;
    }

    const CatalogEffect* meta = (catalog_) ? FindEffect(*catalog_, effect_id) : nullptr;
    if(meta && meta->photosensitive && !allow_photosensitive_)
    {
        last_warning_ = "Blocked photosensitive effect '" + effect_id + "' (enable Allow photosensitive looks).";
        LOG_WARNING("[3DSpatial] Movie Sync: %s", last_warning_.c_str());
        return;
    }

    const bool is_level = (edge == "level");
    if(!active)
    {
        if(!is_level)
        {
            return;
        }
        plays_.erase(std::remove_if(plays_.begin(), plays_.end(),
                                    [&](const ActivePlay& p) {
                                        return p.level && p.effect_id == effect_id;
                                    }),
                     plays_.end());
        if(plays_.empty())
        {
            prepared_ = false;
        }
        return;
    }

    Action::Pack pack;
    std::string err;
    const bool loaded = catalog_ && !movie_effects_dir_.empty()
                            && LoadActionForEffect(*catalog_, movie_effects_dir_, effect_id, &pack, &err);
    if(!loaded)
    {
        last_error_ = err.empty() ? ("unknown effect_id: " + effect_id) : err;
        LOG_WARNING("[3DSpatial] Movie Sync: %s", last_error_.c_str());
        return;
    }

    if(!is_level && pack.loop == Action::LoopMode::WhileActive)
    {
        pack.loop = Action::LoopMode::Once;
    }

    // Replace existing play of same effect_id for level holds.
    if(is_level)
    {
        plays_.erase(std::remove_if(plays_.begin(), plays_.end(),
                                    [&](const ActivePlay& p) {
                                        return p.level && p.effect_id == effect_id;
                                    }),
                     plays_.end());
    }

    ActivePlay play;
    play.effect_id = effect_id;
    play.pack = std::move(pack);
    play.event_active = true;
    play.level = is_level;
    play.player.SetPack(play.pack);
    play.player.Play();
    plays_.push_back(std::move(play));

    if(!prepared_ && prepare_)
    {
        prepare_();
        prepared_ = true;
    }
    ApplyPlays(true);
}

void CueRuntime::StopAll()
{
    plays_.clear();
    prepared_ = false;
}

void CueRuntime::ApplyPlays(bool force_hw)
{
    if(!apply_ || plays_.empty())
    {
        return;
    }
    std::vector<ActivePlay*> ordered;
    ordered.reserve(plays_.size());
    for(ActivePlay& p : plays_)
    {
        ordered.push_back(&p);
    }
    std::sort(ordered.begin(), ordered.end(), [](const ActivePlay* a, const ActivePlay* b) {
        return a->pack.priority < b->pack.priority;
    });
    for(ActivePlay* p : ordered)
    {
        // Continuous elapsed for GPU u_time (LocalMs wraps on forever/while_active).
        Action::SetGpuAnimMs(p->player.ElapsedMs());
        apply_(p->pack, p->player.LocalMs(), force_hw);
        Action::SetGpuAnimMs(-1);
        force_hw = false;
    }
}

bool CueRuntime::Tick(int dt_ms)
{
    if(plays_.empty())
    {
        return false;
    }
    for(auto it = plays_.begin(); it != plays_.end(); )
    {
        if(!it->player.Tick(dt_ms, it->event_active))
        {
            it = plays_.erase(it);
            continue;
        }
        ++it;
    }
    if(plays_.empty())
    {
        prepared_ = false;
        return false;
    }
    ApplyPlays(false);
    return true;
}

} // namespace MovieSync
