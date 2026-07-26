// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"
#include <memory>

class QWidget;

namespace EffectBinding
{

#ifdef _WIN32

/** Windows session lock / unlock via WM_WTSSESSION_CHANGE on a private HWND. */
class WindowsEventSource : public EventSource
{
public:
    WindowsEventSource();
    ~WindowsEventSource() override;

    const char* id() const override { return "windows"; }
    const char* displayName() const override { return "Windows"; }
    std::vector<EventInfo> ListEvents() const override;

    void Start() override;
    void Stop() override;

    void HandleSessionEvent(unsigned long session_event);

private:
    class Watcher;
    class SinkWidget;
    std::unique_ptr<SinkWidget> sink_;
    std::unique_ptr<Watcher> watcher_;
    bool started_ = false;
};

#endif

std::unique_ptr<EventSource> TryCreateWindowsEventSource();

} // namespace EffectBinding
