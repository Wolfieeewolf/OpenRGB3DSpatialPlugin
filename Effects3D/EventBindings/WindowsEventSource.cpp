// SPDX-License-Identifier: GPL-2.0-only

#include "WindowsEventSource.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QWidget>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")
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
        // Tiny off-screen sink — do not parent to the plugin tab (winId there can hang startup).
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
        if(!msg || msg->message != WM_WTSSESSION_CHANGE || !owner_)
        {
            return false;
        }
        owner_->HandleSessionEvent(static_cast<unsigned long>(msg->wParam));
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
        {"session_lock", "Session lock"},
        {"session_unlock", "Session unlock"},
    };
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
    const HWND hwnd = reinterpret_cast<HWND>(sink_->winId());
    if(!hwnd)
    {
        return;
    }
    watcher_ = std::make_unique<Watcher>(this);
    if(!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION))
    {
        watcher_.reset();
        return;
    }
    started_ = true;
}

void WindowsEventSource::Stop()
{
    if(started_ && sink_)
    {
        const HWND hwnd = reinterpret_cast<HWND>(sink_->winId());
        if(hwnd)
        {
            WTSUnRegisterSessionNotification(hwnd);
        }
    }
    started_ = false;
    watcher_.reset();
    sink_.reset();
}

void WindowsEventSource::HandleSessionEvent(unsigned long session_event)
{
    switch(session_event)
    {
        case WTS_SESSION_LOCK:
            Emit("session_lock", true);
            Emit("session_unlock", false);
            break;
        case WTS_SESSION_UNLOCK:
            Emit("session_lock", false);
            Emit("session_unlock", true);
            break;
        default:
            break;
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
