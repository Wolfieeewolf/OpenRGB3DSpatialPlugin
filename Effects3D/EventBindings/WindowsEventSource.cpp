// SPDX-License-Identifier: GPL-2.0-only

#include "WindowsEventSource.h"
#include "PluginLog.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QWidget>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbt.h>
#include <wtsapi32.h>
#include <initguid.h>
#include <usbiodef.h>
#include <hidclass.h>
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "User32.lib")

DEFINE_GUID(GUID_CONSOLE_DISPLAY_STATE_LOCAL,
            0x6fe69556, 0x704a, 0x47a0, 0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47);
DEFINE_GUID(GUID_BATTERY_PERCENTAGE_REMAINING_LOCAL,
            0xa7ad8041, 0xb45a, 0x4cae, 0x87, 0xa3, 0xee, 0xcb, 0xb4, 0x68, 0xa9, 0xe1);

namespace
{
constexpr UINT kMsgForeground = WM_APP + 40;
constexpr int kBatteryLowPct = 10;
constexpr int kBatteryCriticalPct = 5;

EffectBinding::WindowsEventSource* g_foreground_owner = nullptr;

void CALLBACK ForegroundWinEventProc(HWINEVENTHOOK /*hook*/,
                                     DWORD event,
                                     HWND /*hwnd*/,
                                     LONG /*idObject*/,
                                     LONG /*idChild*/,
                                     DWORD /*idEventThread*/,
                                     DWORD /*dwmsEventTime*/)
{
    if(event != EVENT_SYSTEM_FOREGROUND || !g_foreground_owner)
    {
        return;
    }
    HWND sink = static_cast<HWND>(g_foreground_owner->SinkHwnd());
    if(sink)
    {
        PostMessageW(sink, kMsgForeground, 0, 0);
    }
}
} // namespace
#endif

namespace EffectBinding
{

#ifdef _WIN32

class WindowsEventSource::SinkWidget : public QWidget
{
public:
    explicit SinkWidget(WindowsEventSource* owner)
        : QWidget(nullptr)
        , owner_(owner)
    {
        setAttribute(Qt::WA_DontShowOnScreen, true);
        setAttribute(Qt::WA_NativeWindow, true);
        resize(1, 1);
        move(-10000, -10000);
    }

    WindowsEventSource* owner_ = nullptr;
};

class WindowsEventSource::Watcher : public QObject, public QAbstractNativeEventFilter
{
public:
    explicit Watcher(WindowsEventSource* owner)
        : owner_(owner)
    {
        if(qApp)
        {
            qApp->installNativeEventFilter(this);
        }
    }

    ~Watcher() override
    {
        if(qApp)
        {
            qApp->removeNativeEventFilter(this);
        }
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
    {
        (void)result;
        if(eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        {
            return false;
        }
        MSG* msg = static_cast<MSG*>(message);
        if(!msg || !owner_)
        {
            return false;
        }
        if(msg->message == WM_WTSSESSION_CHANGE)
        {
            owner_->HandleSessionEvent(static_cast<unsigned long>(msg->wParam));
            return false;
        }
        if(msg->message == kMsgForeground)
        {
            owner_->HandleForegroundChanged();
            return false;
        }
        if(msg->message == WM_DEVICECHANGE)
        {
            owner_->HandleDeviceChange(static_cast<unsigned long>(msg->wParam),
                                       static_cast<long long>(msg->lParam));
            return false;
        }
        if(msg->message == WM_POWERBROADCAST && msg->wParam == PBT_POWERSETTINGCHANGE)
        {
            const POWERBROADCAST_SETTING* setting =
                reinterpret_cast<const POWERBROADCAST_SETTING*>(msg->lParam);
            if(setting)
            {
                owner_->HandlePowerSetting(&setting->PowerSetting, setting->Data, setting->DataLength);
            }
            return false;
        }
        return false;
    }

private:
    WindowsEventSource* owner_ = nullptr;
};

WindowsEventSource::WindowsEventSource() = default;

WindowsEventSource::~WindowsEventSource()
{
    Stop();
}

std::vector<EventInfo> WindowsEventSource::ListEvents() const
{
    return {
        {"session_lock", "Windows Lock", EventEdge::Level},
        {"session_unlock", "Windows Unlock", EventEdge::Level},
        {"session_logon", "Windows Logon", EventEdge::Pulse},
        {"session_logoff", "Windows Logoff", EventEdge::Pulse},
        {"remote_connect", "Remote desktop connect", EventEdge::Level},
        {"remote_disconnect", "Remote desktop disconnect", EventEdge::Level},
        {"device_connect", "Device Connect", EventEdge::Pulse},
        {"device_disconnect", "Device Disconnect", EventEdge::Pulse},
        {"device_fail", "Device Failed to Connect", EventEdge::Pulse},
        {"battery_low", "Low Battery Alarm", EventEdge::Level},
        {"battery_critical", "Critical Battery Alarm", EventEdge::Level},
        {"display_off", "Display turned off", EventEdge::Level},
        {"display_on", "Display turned on", EventEdge::Level},
        {"foreground_changed", "Foreground window changed", EventEdge::Pulse},
        {"app_activated", "OpenRGB became foreground", EventEdge::Level},
        {"app_deactivated", "OpenRGB left foreground", EventEdge::Level},
    };
}

void WindowsEventSource::UnregisterPowerNotify(void*& handle)
{
    if(handle)
    {
        UnregisterPowerSettingNotification(static_cast<HPOWERNOTIFY>(handle));
        handle = nullptr;
    }
}

void* WindowsEventSource::RegisterDeviceInterface(const void* guid)
{
    if(!sink_hwnd_ || !guid)
    {
        return nullptr;
    }
    DEV_BROADCAST_DEVICEINTERFACE filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = *static_cast<const GUID*>(guid);
    return RegisterDeviceNotificationW(static_cast<HWND>(sink_hwnd_),
                                       &filter,
                                       DEVICE_NOTIFY_WINDOW_HANDLE);
}

void WindowsEventSource::Start()
{
    if(started_)
    {
        return;
    }
    if(!sink_)
    {
        sink_ = std::make_unique<SinkWidget>(this);
    }
    HWND hwnd = reinterpret_cast<HWND>(sink_->winId());
    if(!hwnd)
    {
        LOG_WARNING("[3DSpatial] Windows events: failed to create notification HWND");
        return;
    }
    sink_hwnd_ = hwnd;
    watcher_ = std::make_unique<Watcher>(this);
    if(!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION))
    {
        LOG_WARNING("[3DSpatial] Windows events: WTSRegisterSessionNotification failed (%lu)",
                    GetLastError());
        watcher_.reset();
        sink_hwnd_ = nullptr;
        return;
    }

    g_foreground_owner = this;
    foreground_hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                                       EVENT_SYSTEM_FOREGROUND,
                                       nullptr,
                                       ForegroundWinEventProc,
                                       0,
                                       0,
                                       WINEVENT_OUTOFCONTEXT);
    if(!foreground_hook_)
    {
        LOG_WARNING("[3DSpatial] Windows events: SetWinEventHook failed (%lu)", GetLastError());
    }

    power_display_ = RegisterPowerSettingNotification(hwnd,
                                                      &GUID_CONSOLE_DISPLAY_STATE_LOCAL,
                                                      DEVICE_NOTIFY_WINDOW_HANDLE);
    power_battery_ = RegisterPowerSettingNotification(hwnd,
                                                      &GUID_BATTERY_PERCENTAGE_REMAINING_LOCAL,
                                                      DEVICE_NOTIFY_WINDOW_HANDLE);

    device_notify_usb_ = RegisterDeviceInterface(&GUID_DEVINTERFACE_USB_DEVICE);
    device_notify_hid_ = RegisterDeviceInterface(&GUID_DEVINTERFACE_HID);

    started_ = true;
    HandleForegroundChanged();

    SYSTEM_POWER_STATUS ps{};
    if(GetSystemPowerStatus(&ps) && ps.BatteryFlag != 128)
    {
        if(ps.BatteryLifePercent <= 100)
        {
            last_battery_pct_ = ps.BatteryLifePercent;
        }
    }
    LOG_INFO("[3DSpatial] Windows event source started");
}

void WindowsEventSource::Stop()
{
    if(foreground_hook_)
    {
        UnhookWinEvent(static_cast<HWINEVENTHOOK>(foreground_hook_));
        foreground_hook_ = nullptr;
    }
    if(g_foreground_owner == this)
    {
        g_foreground_owner = nullptr;
    }
    UnregisterPowerNotify(power_display_);
    UnregisterPowerNotify(power_battery_);
    if(device_notify_usb_)
    {
        UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(device_notify_usb_));
        device_notify_usb_ = nullptr;
    }
    if(device_notify_hid_)
    {
        UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(device_notify_hid_));
        device_notify_hid_ = nullptr;
    }
    if(started_ && sink_hwnd_)
    {
        WTSUnRegisterSessionNotification(static_cast<HWND>(sink_hwnd_));
    }
    started_ = false;
    sink_hwnd_ = nullptr;
    watcher_.reset();
    sink_.reset();
    app_foreground_ = false;
    last_battery_pct_ = -1;
    emitted_battery_low_ = false;
    emitted_battery_critical_ = false;
}

void WindowsEventSource::HandleSessionEvent(unsigned long session_event)
{
    switch(session_event)
    {
        case WTS_SESSION_LOCK:
            EmitLevel("session_lock", true);
            EmitLevel("session_unlock", false);
            break;
        case WTS_SESSION_UNLOCK:
            EmitLevel("session_lock", false);
            EmitLevel("session_unlock", true);
            break;
        case WTS_SESSION_LOGON:
            EmitPulse("session_logon");
            break;
        case WTS_SESSION_LOGOFF:
            EmitPulse("session_logoff");
            break;
        case WTS_REMOTE_CONNECT:
            EmitLevel("remote_connect", true);
            EmitLevel("remote_disconnect", false);
            break;
        case WTS_REMOTE_DISCONNECT:
            EmitLevel("remote_connect", false);
            EmitLevel("remote_disconnect", true);
            break;
        default:
            break;
    }
}

void WindowsEventSource::HandleDeviceChange(unsigned long wparam, long long lparam)
{
    if(lparam)
    {
        const DEV_BROADCAST_HDR* hdr = reinterpret_cast<const DEV_BROADCAST_HDR*>(lparam);
        if(hdr && hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
        {
            return;
        }
    }
    switch(wparam)
    {
        case DBT_DEVICEARRIVAL:
            EmitPulse("device_connect");
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            EmitPulse("device_disconnect");
            break;
        case DBT_DEVICEQUERYREMOVEFAILED:
            EmitPulse("device_fail");
            break;
        default:
            break;
    }
}

void WindowsEventSource::HandleForegroundChanged()
{
    EmitPulse("foreground_changed");

    HWND fg = GetForegroundWindow();
    bool ours = false;
    if(fg)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        ours = (pid == GetCurrentProcessId());
    }
    UpdateAppFocusState(ours);
}

void WindowsEventSource::UpdateAppFocusState(bool openrgb_foreground)
{
    if(openrgb_foreground == app_foreground_)
    {
        return;
    }
    app_foreground_ = openrgb_foreground;
    if(app_foreground_)
    {
        EmitLevel("app_activated", true);
        EmitLevel("app_deactivated", false);
    }
    else
    {
        EmitLevel("app_activated", false);
        EmitLevel("app_deactivated", true);
    }
}

void WindowsEventSource::HandlePowerSetting(const void* guid, const void* data, unsigned long data_len)
{
    if(!guid || !data || data_len < sizeof(DWORD))
    {
        return;
    }
    const GUID& g = *static_cast<const GUID*>(guid);
    const DWORD value = *static_cast<const DWORD*>(data);

    if(IsEqualGUID(g, GUID_CONSOLE_DISPLAY_STATE_LOCAL))
    {
        if(value == 0)
        {
            EmitLevel("display_off", true);
            EmitLevel("display_on", false);
        }
        else
        {
            EmitLevel("display_off", false);
            EmitLevel("display_on", true);
        }
        return;
    }

    if(IsEqualGUID(g, GUID_BATTERY_PERCENTAGE_REMAINING_LOCAL))
    {
        const int pct = static_cast<int>(value);
        if(pct > kBatteryLowPct)
        {
            if(emitted_battery_low_)
            {
                EmitLevel("battery_low", false);
                emitted_battery_low_ = false;
            }
            if(emitted_battery_critical_)
            {
                EmitLevel("battery_critical", false);
                emitted_battery_critical_ = false;
            }
        }
        else if(pct > kBatteryCriticalPct)
        {
            if(emitted_battery_critical_)
            {
                EmitLevel("battery_critical", false);
                emitted_battery_critical_ = false;
            }
            if(!emitted_battery_low_)
            {
                EmitLevel("battery_low", true);
                emitted_battery_low_ = true;
            }
        }
        else
        {
            if(!emitted_battery_critical_)
            {
                EmitLevel("battery_critical", true);
                emitted_battery_critical_ = true;
            }
            if(!emitted_battery_low_)
            {
                EmitLevel("battery_low", true);
                emitted_battery_low_ = true;
            }
        }
        last_battery_pct_ = pct;
    }
}

#endif // _WIN32

std::unique_ptr<EventSource> TryCreateWindowsEventSource()
{
#ifdef _WIN32
    return std::make_unique<WindowsEventSource>();
#else
    return nullptr;
#endif
}

} // namespace EffectBinding
