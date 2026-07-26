// SPDX-License-Identifier: GPL-2.0-only

#include "BindingRuntime.h"
#include "EffectPacks/EffectPackLibrary.h"

#include <algorithm>

namespace EffectBinding
{

void BindingRuntime::OnEvent(const std::string& source, const std::string& event, bool active)
{
    if(source.empty() || event.empty())
    {
        return;
    }

    if(!active)
    {
        // Edge off: stop plays tied to this event (while_active / hold / OS session).
        // Manual Fire uses StopFire the same way; Once packs also end via Tick.
        if(source == "manual" && event == "fire")
        {
            StopMatching(source, event);
        }
        else if(source == "manual" && event == "hold")
        {
            StopMatching(source, event);
        }
        else
        {
            for(auto it = plays_.begin(); it != plays_.end(); )
            {
                if(it->source == source && it->event == event
                   && it->pack.loop == EffectPack::LoopMode::WhileActive)
                {
                    it = plays_.erase(it);
                    continue;
                }
                if(it->source == source && it->event == event)
                {
                    it->event_active = false;
                }
                ++it;
            }
            if(plays_.empty())
            {
                prepared_ = false;
            }
        }
        return;
    }

    for(const Binding& b : doc_.bindings)
    {
        if(!b.enabled || b.source != source || b.event != event)
        {
            continue;
        }
        StartBinding(b, true);
    }

    // Momentary OS pulses (unlock): after starting, if Once they run; if WhileActive
    // keep active until matching false. For unlock as one-shot pulse, leave event_active true
    // until Once ends — Forever would run until StopAll.
}

void BindingRuntime::StartBinding(const Binding& binding, bool event_active)
{
    // Replace existing play for same binding id.
    plays_.erase(std::remove_if(plays_.begin(), plays_.end(),
                                [&](const ActivePlay& p) { return p.binding_id == binding.id; }),
                 plays_.end());

    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadPackById(packs_dir_, binding.pack_id, &pack, &err))
    {
        return;
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
    }
}

void BindingRuntime::StopAll()
{
    plays_.clear();
    prepared_ = false;
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
        return false;
    }

    ApplyPlays(false);
    return true;
}

} // namespace EffectBinding
