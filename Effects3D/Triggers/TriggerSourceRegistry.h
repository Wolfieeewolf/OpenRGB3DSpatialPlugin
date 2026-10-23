// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "TriggerSource.h"
#include "ManualTriggerSource.h"
#include <memory>
#include <vector>

namespace Trigger
{

class TriggerSourceRegistry
{
public:
    void BuildForPlatform();

    void SetListener(EventSignalFn fn);
    void StartAll();
    void StopAll();

    const std::vector<std::unique_ptr<EventSource>>& sources() const { return sources_; }

    EventSource* Find(const std::string& source_id) const;
    ManualTriggerSource* manual() const { return manual_; }

    bool HasSource(const std::string& source_id) const { return Find(source_id) != nullptr; }

private:
    std::vector<std::unique_ptr<EventSource>> sources_;
    ManualTriggerSource* manual_ = nullptr;
};

} // namespace Trigger
