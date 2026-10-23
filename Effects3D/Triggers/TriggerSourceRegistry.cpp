// SPDX-License-Identifier: GPL-2.0-only

#include "TriggerSourceRegistry.h"
#include "WindowsTriggerSource.h"
#include "LinuxTriggerSource.h"
#include "MacTriggerSource.h"

namespace Trigger
{

void TriggerSourceRegistry::BuildForPlatform()
{
    StopAll();
    sources_.clear();
    manual_ = nullptr;

    auto manual = std::make_unique<ManualTriggerSource>();
    manual_ = manual.get();
    sources_.push_back(std::move(manual));

    if(auto win = TryCreateWindowsTriggerSource())
    {
        sources_.push_back(std::move(win));
    }
    if(auto linux_src = TryCreateLinuxTriggerSource())
    {
        sources_.push_back(std::move(linux_src));
    }
    if(auto mac = TryCreateMacTriggerSource())
    {
        sources_.push_back(std::move(mac));
    }
}

void TriggerSourceRegistry::SetListener(EventSignalFn fn)
{
    for(auto& src : sources_)
    {
        if(src)
        {
            src->SetListener(fn);
        }
    }
}

void TriggerSourceRegistry::StartAll()
{
    for(auto& src : sources_)
    {
        if(src)
        {
            src->Start();
        }
    }
}

void TriggerSourceRegistry::StopAll()
{
    for(auto& src : sources_)
    {
        if(src)
        {
            src->Stop();
        }
    }
}

EventSource* TriggerSourceRegistry::Find(const std::string& source_id) const
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

} // namespace Trigger
