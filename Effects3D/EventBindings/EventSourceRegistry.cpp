// SPDX-License-Identifier: GPL-2.0-only

#include "EventSourceRegistry.h"
#include "WindowsEventSource.h"

namespace EffectBinding
{

void EventSourceRegistry::BuildForPlatform()
{
    StopAll();
    sources_.clear();
    manual_ = nullptr;

    auto manual = std::make_unique<ManualEventSource>();
    manual_ = manual.get();
    sources_.push_back(std::move(manual));

    if(auto win = TryCreateWindowsEventSource())
    {
        sources_.push_back(std::move(win));
    }
}

void EventSourceRegistry::SetListener(EventSignalFn fn)
{
    for(auto& src : sources_)
    {
        if(src)
        {
            src->SetListener(fn);
        }
    }
}

void EventSourceRegistry::StartAll()
{
    for(auto& src : sources_)
    {
        if(src)
        {
            src->Start();
        }
    }
}

void EventSourceRegistry::StopAll()
{
    for(auto& src : sources_)
    {
        if(src)
        {
            src->Stop();
        }
    }
}

EventSource* EventSourceRegistry::Find(const std::string& source_id) const
{
    for(const auto& src : sources_)
    {
        if(src && source_id == src->id())
        {
            return src.get();
        }
    }
    return nullptr;
}

} // namespace EffectBinding
