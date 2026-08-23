// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"
#include <memory>

namespace EffectBinding
{

#ifdef _WIN32

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
    void HandleForegroundChanged();
    void HandlePowerSetting(const void* guid, const void* data, unsigned long data_len);
    void HandleDeviceChange(unsigned long wparam, long long lparam);

    void* SinkHwnd() const { return sink_hwnd_; }

private:
    class Watcher;
    class SinkWidget;

    void UpdateAppFocusState(bool openrgb_foreground);
    void UnregisterPowerNotify(void*& handle);
    void* RegisterDeviceInterface(const void* guid);

    std::unique_ptr<SinkWidget> sink_;
    std::unique_ptr<Watcher> watcher_;
    void* sink_hwnd_ = nullptr;
    void* foreground_hook_ = nullptr;
    void* power_display_ = nullptr;
    void* power_battery_ = nullptr;
    void* device_notify_usb_ = nullptr;
    void* device_notify_hid_ = nullptr;
    bool started_ = false;
    bool app_foreground_ = false;
    int last_battery_pct_ = -1;
    bool emitted_battery_low_ = false;
    bool emitted_battery_critical_ = false;
};

#endif

std::unique_ptr<EventSource> TryCreateWindowsEventSource();

} // namespace EffectBinding
