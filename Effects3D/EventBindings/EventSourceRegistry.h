// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"
#include "ManualEventSource.h"
#include <memory>
#include <vector>

namespace EffectBinding
{

class EventSourceRegistry
{
public:
    /** Manual always; Windows / Linux / macOS OS factories when available. */
    void BuildForPlatform();

    void SetListener(EventSignalFn fn);
    void StartAll();
    void StopAll();

    const std::vector<std::unique_ptr<EventSource>>& sources() const { return sources_; }

    EventSource* Find(const std::string& source_id) const;
    ManualEventSource* manual() const { return manual_; }

    bool HasSource(const std::string& source_id) const { return Find(source_id) != nullptr; }

private:
    std::vector<std::unique_ptr<EventSource>> sources_;
    ManualEventSource* manual_ = nullptr;
};

} // namespace EffectBinding
