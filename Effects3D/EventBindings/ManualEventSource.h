// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"

namespace EffectBinding
{

/** Always available. Events: fire (start until stopped), hold (press/release). */
class ManualEventSource : public EventSource
{
public:
    const char* id() const override { return "manual"; }
    const char* displayName() const override { return "Manual"; }

    std::vector<EventInfo> ListEvents() const override
    {
        return {
            {"fire", "Fire (run until stop / once ends)"},
            {"hold", "Hold (while_active while held)"},
        };
    }

    void Fire() { Emit("fire", true); }
    void StopFire() { Emit("fire", false); }
    void BeginHold() { Emit("hold", true); }
    void EndHold() { Emit("hold", false); }
};

} // namespace EffectBinding
