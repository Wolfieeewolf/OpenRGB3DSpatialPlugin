// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "TriggerSource.h"

#include <QtGlobal>
#include <memory>

namespace Trigger
{

#ifdef Q_OS_MACOS

class MacTriggerSource : public EventSource
{
public:
    MacTriggerSource();
    ~MacTriggerSource() override;

    const char* id() const override { return "macos"; }
    const char* displayName() const override { return "macOS"; }
    std::vector<EventInfo> ListEvents() const override;

    void Start() override;
    void Stop() override;

    void NotifyLock(bool locked);
    void NotifySleep(bool sleeping);
    unsigned int power_connection_for_ack() const { return power_connection_; }

private:
    void* power_port_ = nullptr;
    unsigned int power_notifier_ = 0;
    unsigned int power_connection_ = 0;
    bool lock_watching_ = false;
    bool started_ = false;
};

#endif

std::unique_ptr<EventSource> TryCreateMacTriggerSource();

} // namespace Trigger
