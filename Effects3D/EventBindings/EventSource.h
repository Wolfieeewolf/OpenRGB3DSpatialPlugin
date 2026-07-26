// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace EffectBinding
{

struct EventInfo
{
    std::string id;
    std::string display_name;
};

/** source + event activation callback (active=true start, false end). */
using EventSignalFn = std::function<void(const std::string& source_id,
                                         const std::string& event_id,
                                         bool active)>;

class EventSource
{
public:
    virtual ~EventSource() = default;

    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual std::vector<EventInfo> ListEvents() const = 0;

    virtual void SetListener(EventSignalFn fn) { listener_ = std::move(fn); }
    virtual void Start() {}
    virtual void Stop() {}

protected:
    void Emit(const std::string& event_id, bool active)
    {
        if(listener_)
        {
            listener_(id(), event_id, active);
        }
    }

    EventSignalFn listener_;
};

} // namespace EffectBinding
