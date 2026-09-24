// SPDX-License-Identifier: GPL-2.0-only

#include "BindingRuntime.h"
#include "EffectPacks/EffectPackLibrary.h"
#include "PluginLog.h"

#include <algorithm>

namespace EffectBinding
{

void BindingRuntime::OnEvent(const std::string& source,
                             const std::string& event,
                             bool active,
                             EventEdge edge)
{
    last_error_.clear();
    if(source.empty() || event.empty())
    {
        return;
    }

    if(!active)
    {
        if(edge == EventEdge::Pulse)
        {
            return;
        }
        // Manual stop / level-off: drop matching plays (Once mid-run included for fire/hold stop).
        if(source == "manual")
        {
            StopMatching(source, event);
            return;
        }
        for(auto it = plays_.begin(); it != plays_.end(); )
        {
            if(it->source != source || it->event != event)
            {
                ++it;
                continue;
            }
            if(it->pack.loop == EffectPack::LoopMode::WhileActive)
            {
                it = plays_.erase(it);
                continue;
            }
            it->event_active = false;
            ++it;
        }
        if(plays_.empty())
        {
            prepared_ = false;
            push_hw_ = false;
        }
        return;
    }

    for(const Binding& b : doc_.bindings)
    {
        if(!b.enabled || b.source != source || b.event != event)
        {
            continue;
        }
        StartBinding(b, true, edge);
    }
}

void BindingRuntime::StartBinding(const Binding& binding, bool event_active, EventEdge edge)
{
    plays_.erase(std::remove_if(plays_.begin(), plays_.end(),
                                [&](const ActivePlay& p) { return p.binding_id == binding.id; }),
                 plays_.end());

    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadPackById(packs_dir_, binding.pack_id, &pack, &err))
    {
        last_error_ = "Pack '" + binding.pack_id + "' failed to load: " + err;
        LOG_WARNING("[3DSpatial] Effect binding '%s': %s", binding.id.c_str(), last_error_.c_str());
        if(plays_.empty())
        {
            prepared_ = false;
            push_hw_ = false;
        }
        return;
    }

    // Pulse edges have no lasting "active" state — WhileActive would never end.
    if(edge == EventEdge::Pulse && pack.loop == EffectPack::LoopMode::WhileActive)
    {
        pack.loop = EffectPack::LoopMode::Once;
    }

    ActivePlay play;
    play.binding_id = binding.id;
    play.source = binding.source;
    play.event = binding.event;
    play.pack = std::move(pack);
    play.event_active = event_active;
    play.player.SetPack(play.pack);
    play.player.Play();
    plays_.push_back(std::move(play));
    push_hw_ = true;

    if(!prepared_ && prepare_)
    {
        prepare_();
        prepared_ = true;
    }
}

void BindingRuntime::StopMatching(const std::string& source, const std::string& event)
{
    plays_.erase(std::remove_if(plays_.begin(), plays_.end(),
                                [&](const ActivePlay& p) {
                                    return p.source == source && p.event == event;
                                }),
                 plays_.end());
    if(plays_.empty())
    {
        prepared_ = false;
        push_hw_ = false;
    }
}

void BindingRuntime::StopBinding(const std::string& binding_id)
{
    if(binding_id.empty())
    {
        return;
    }
    plays_.erase(std::remove_if(plays_.begin(), plays_.end(),
                                [&](const ActivePlay& p) { return p.binding_id == binding_id; }),
                 plays_.end());
    if(plays_.empty())
    {
        prepared_ = false;
        push_hw_ = false;
    }
}

void BindingRuntime::StopAll()
{
    plays_.clear();
    prepared_ = false;
    push_hw_ = false;
}

void BindingRuntime::ApplyPlays(bool force_hw)
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
        apply_(p->pack, p->player.LocalMs(), force_hw);
        force_hw = false;
    }
}

bool BindingRuntime::Tick(int dt_ms)
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
        push_hw_ = false;
        return false;
    }

    const bool force_hw = push_hw_;
    push_hw_ = false;
    ApplyPlays(force_hw);
    return true;
}

} // namespace EffectBinding
