// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"

#include <QtGlobal>
#include <memory>

namespace EffectBinding
{

#ifdef Q_OS_LINUX

class LinuxLoginWatcher;

class LinuxEventSource : public EventSource
{
public:
    LinuxEventSource();
    ~LinuxEventSource() override;

    const char* id() const override { return "linux"; }
    const char* displayName() const override { return "Linux"; }
    std::vector<EventInfo> ListEvents() const override;

    void Start() override;
    void Stop() override;

    void NotifyLock(bool locked);
    void NotifySleep(bool sleeping);

private:
    std::unique_ptr<LinuxLoginWatcher> watcher_;
    bool started_ = false;
};

#endif

std::unique_ptr<EventSource> TryCreateLinuxEventSource();

} // namespace EffectBinding
