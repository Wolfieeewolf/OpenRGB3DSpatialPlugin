// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace EffectBinding
{

/** Level: true/false pairs (lock, hold, display). Pulse: one-shot edge (device plug, logon). */
enum class EventEdge
{
    Level,
    Pulse,
};

struct EventInfo
{
    std::string id;
    std::string display_name;
    EventEdge edge = EventEdge::Pulse;
};

using EventSignalFn = std::function<void(const std::string& source_id,
                                         const std::string& event_id,
                                         bool active,
                                         EventEdge edge)>;

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
    void Emit(const std::string& event_id, bool active, EventEdge edge)
    {
        if(listener_)
        {
            listener_(id(), event_id, active, edge);
        }
    }

    void EmitLevel(const std::string& event_id, bool active)
    {
        Emit(event_id, active, EventEdge::Level);
    }

    void EmitPulse(const std::string& event_id)
    {
        Emit(event_id, true, EventEdge::Pulse);
    }

    EventSignalFn listener_;
};

} // namespace EffectBinding
