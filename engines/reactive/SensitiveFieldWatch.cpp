// SPDX-License-Identifier: GPL-2.0-or-later

#include "SensitiveFieldWatch.h"
#include "PluginLog.h"

#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
/* The build defines WIN32_LEAN_AND_MEAN, which leaves out the COM headers UI Automation needs. */
#include <ole2.h>
#include <UIAutomation.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#pragma comment(lib, "ole32.lib")
#elif defined(__linux__)
#include "SensitiveFieldWatchLinux.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#endif

#ifdef _WIN32

namespace
{
/* Called on a UI Automation thread whenever focus moves anywhere on the desktop. */
class FocusHandler : public IUIAutomationFocusChangedEventHandler
{
public:
    explicit FocusHandler(SensitiveFieldWatch::Callback on_change)
        : on_change_(std::move(on_change))
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++refs_;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG left = --refs_;
        if(left == 0)
        {
            delete this;
        }
        return left;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
    {
        if(!out)
        {
            return E_POINTER;
        }
        if(riid == __uuidof(IUnknown) || riid == __uuidof(IUIAutomationFocusChangedEventHandler))
        {
            *out = static_cast<IUIAutomationFocusChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) override
    {
        BOOL is_password = FALSE;
        if(sender)
        {
            sender->get_CachedIsPassword(&is_password);
        }
        on_change_(is_password != FALSE);
        return S_OK;
    }

private:
    std::atomic<ULONG> refs_{1};
    SensitiveFieldWatch::Callback on_change_;
};
} // namespace

/* UI Automation wants its own multithreaded COM apartment, away from the GUI thread. */
class SensitiveFieldWatch::Impl
{
public:
    void Start(Callback on_change)
    {
        Stop();
        stop_requested_ = false;
        thread_ = std::thread([this, on_change = std::move(on_change)]() { Run(on_change); });
    }

    void Stop()
    {
        if(!thread_.joinable())
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested_ = true;
        }
        wake_.notify_all();
        thread_.join();
    }

    ~Impl()
    {
        Stop();
    }

private:
    void Run(const Callback& on_change)
    {
        if(FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        {
            return;
        }
        IUIAutomation* automation = nullptr;
        IUIAutomationCacheRequest* cache = nullptr;
        FocusHandler* handler = nullptr;
        if(SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                      __uuidof(IUIAutomation), reinterpret_cast<void**>(&automation))) &&
           SUCCEEDED(automation->CreateCacheRequest(&cache)) &&
           SUCCEEDED(cache->AddProperty(UIA_IsPasswordPropertyId)))
        {
            /* Where focus is right now, then every move from here on. */
            IUIAutomationElement* focused = nullptr;
            if(SUCCEEDED(automation->GetFocusedElementBuildCache(cache, &focused)) && focused)
            {
                BOOL is_password = FALSE;
                focused->get_CachedIsPassword(&is_password);
                on_change(is_password != FALSE);
                focused->Release();
            }
            handler = new FocusHandler(on_change);
            if(FAILED(automation->AddFocusChangedEventHandler(cache, handler)))
            {
                LOG_WARNING("[Reactive] Password-field detection unavailable (UI Automation refused focus events)");
                handler->Release();
                handler = nullptr;
            }
        }
        else
        {
            LOG_WARNING("[Reactive] Password-field detection unavailable (UI Automation not available)");
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this]() { return stop_requested_; });
        }

        if(handler)
        {
            automation->RemoveFocusChangedEventHandler(handler);
            handler->Release();
        }
        if(cache)
        {
            cache->Release();
        }
        if(automation)
        {
            automation->Release();
        }
        CoUninitialize();
        on_change(false);
    }

    std::thread             thread_;
    std::mutex              mutex_;
    std::condition_variable wake_;
    bool                    stop_requested_ = false;
};

#elif defined(__linux__)

namespace
{
const QString kRegistryService = QStringLiteral("org.a11y.atspi.Registry");
const QString kRegistryPath    = QStringLiteral("/org/a11y/atspi/registry");
const QString kRegistryIface   = QStringLiteral("org.a11y.atspi.Registry");
const QString kFocusEvent      = QStringLiteral("object:state-changed:focused");
constexpr uint kRolePasswordText = 40; /* ATSPI_ROLE_PASSWORD_TEXT */

/* Desktops register the event either with one argument (older AT-SPI) or three (2.46+). */
void CallRegistry(QDBusConnection& bus, const QString& method)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kRegistryService, kRegistryPath, kRegistryIface, method);
    call << kFocusEvent;
    const QDBusMessage reply = bus.call(call, QDBus::Block, 2000);
    if(reply.type() == QDBusMessage::ErrorMessage)
    {
        QDBusMessage call3 = QDBusMessage::createMethodCall(kRegistryService, kRegistryPath, kRegistryIface, method);
        call3 << kFocusEvent << QStringList() << QString();
        bus.call(call3, QDBus::Block, 2000);
    }
}
} // namespace

AtspiFocusWatcher::AtspiFocusWatcher(std::function<void(bool)> on_change)
    : on_change_(std::move(on_change))
    , bus_(QString())
{
}

AtspiFocusWatcher::~AtspiFocusWatcher()
{
    if(connected_)
    {
        bus_.disconnect(QString(), QString(), QStringLiteral("org.a11y.atspi.Event.Object"),
                        QStringLiteral("StateChanged"), this, SLOT(onStateChanged(QDBusMessage)));
        CallRegistry(bus_, QStringLiteral("DeregisterEvent"));
        QDBusConnection::disconnectFromBus(bus_.name());
    }
    Report(false);
}

bool AtspiFocusWatcher::Connect()
{
    /* The accessibility bus is separate from the session bus; the session bus says where it is. */
    QDBusMessage ask = QDBusMessage::createMethodCall(QStringLiteral("org.a11y.Bus"), QStringLiteral("/org/a11y/bus"),
                                                      QStringLiteral("org.a11y.Bus"), QStringLiteral("GetAddress"));
    const QDBusReply<QString> address = QDBusConnection::sessionBus().call(ask, QDBus::Block, 2000);
    if(!address.isValid() || address.value().isEmpty())
    {
        return false;
    }
    bus_ = QDBusConnection::connectToBus(address.value(), QStringLiteral("spatial3d-a11y"));
    if(!bus_.isConnected())
    {
        return false;
    }
    connected_ = bus_.connect(QString(), QString(), QStringLiteral("org.a11y.atspi.Event.Object"),
                              QStringLiteral("StateChanged"), this, SLOT(onStateChanged(QDBusMessage)));
    if(!connected_)
    {
        QDBusConnection::disconnectFromBus(bus_.name());
        return false;
    }
    CallRegistry(bus_, QStringLiteral("RegisterEvent"));
    return true;
}

void AtspiFocusWatcher::onStateChanged(const QDBusMessage& message)
{
    const QList<QVariant> args = message.arguments();
    if(args.size() < 2 || args[0].toString() != QStringLiteral("focused"))
    {
        return;
    }
    const bool gained = args[1].toInt() != 0;
    if(!gained)
    {
        if(message.service() == focused_service_ && message.path() == focused_path_)
        {
            focused_service_.clear();
            focused_path_.clear();
            ++focus_serial_;
            Report(false);
        }
        return;
    }

    focused_service_ = message.service();
    focused_path_    = message.path();
    const quint64 serial = ++focus_serial_;
    QDBusMessage ask = QDBusMessage::createMethodCall(focused_service_, focused_path_,
                                                      QStringLiteral("org.a11y.atspi.Accessible"),
                                                      QStringLiteral("GetRole"));
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(ask, 1000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, serial](QDBusPendingCallWatcher* w) {
        const QDBusPendingReply<uint> reply = *w;
        w->deleteLater();
        if(serial != focus_serial_)
        {
            return; /* focus already moved on */
        }
        Report(reply.isValid() && reply.value() == kRolePasswordText);
    });
}

void AtspiFocusWatcher::Report(bool in_password_field)
{
    if(in_password_ == in_password_field)
    {
        return;
    }
    in_password_ = in_password_field;
    if(on_change_)
    {
        on_change_(in_password_field);
    }
}

class SensitiveFieldWatch::Impl
{
public:
    void Start(Callback on_change)
    {
        Stop();
        watcher_ = std::make_unique<AtspiFocusWatcher>(std::move(on_change));
        if(!watcher_->Connect())
        {
            LOG_INFO("[Reactive] Password-field detection unavailable (no accessibility bus)");
        }
    }

    void Stop()
    {
        watcher_.reset();
    }

private:
    std::unique_ptr<AtspiFocusWatcher> watcher_;
};

#else

class SensitiveFieldWatch::Impl
{
public:
    void Start(Callback) {}
    void Stop() {}
};

#endif

SensitiveFieldWatch::SensitiveFieldWatch()
    : impl_(std::make_unique<Impl>())
{
}

SensitiveFieldWatch::~SensitiveFieldWatch() = default;

void SensitiveFieldWatch::Start(Callback on_change)
{
    impl_->Start(std::move(on_change));
}

void SensitiveFieldWatch::Stop()
{
    impl_->Stop();
}
