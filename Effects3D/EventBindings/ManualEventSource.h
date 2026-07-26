// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"

namespace EffectBinding
{

class ManualEventSource : public EventSource
{
public:
    const char* id() const override { return "manual"; }
    const char* displayName() const override { return "Manual"; }

    std::vector<EventInfo> ListEvents() const override
    {
        return {
            {"fire", "Fire (run until stop / once ends)", EventEdge::Level},
            {"hold", "Hold (while_active while held)", EventEdge::Level},
        };
    }

    void Fire() { EmitLevel("fire", true); }
    void StopFire() { EmitLevel("fire", false); }
    void BeginHold() { EmitLevel("hold", true); }
    void EndHold() { EmitLevel("hold", false); }
};

} // namespace EffectBinding
