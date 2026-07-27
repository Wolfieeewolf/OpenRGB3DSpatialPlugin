// SPDX-License-Identifier: GPL-2.0-only

#include "LinuxEventSource.h"
#include "PluginLog.h"

#ifdef Q_OS_LINUX
#include "LinuxLoginWatcher.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariant>
#include <unistd.h>
#endif

namespace EffectBinding
{

#ifdef Q_OS_LINUX

LinuxLoginWatcher::LinuxLoginWatcher(LinuxEventSource* owner)
    : owner_(owner)
{
}

bool LinuxLoginWatcher::ResolveSessionPath()
{
    QDBusInterface manager(QStringLiteral("org.freedesktop.login1"),
                           QStringLiteral("/org/freedesktop/login1"),
                           QStringLiteral("org.freedesktop.login1.Manager"),
                           QDBusConnection::systemBus());
    if(!manager.isValid())
    {
        return false;
    }
    QDBusReply<QDBusObjectPath> reply = manager.call(QStringLiteral("GetSessionByPID"),
                                                     static_cast<uint>(getpid()));
    if(!reply.isValid())
    {
        return false;
    }
    session_path_ = reply.value().path();
    return !session_path_.isEmpty();
}

bool LinuxLoginWatcher::Connect()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if(!bus.isConnected())
    {
        LOG_WARNING("[3DSpatial] Linux events: system D-Bus not connected");
        return false;
    }

    const bool sleep_ok = bus.connect(QStringLiteral("org.freedesktop.login1"),
                                      QStringLiteral("/org/freedesktop/login1"),
                                      QStringLiteral("org.freedesktop.login1.Manager"),
                                      QStringLiteral("PrepareForSleep"),
                                      this,
                                      SLOT(onPrepareForSleep(bool)));
    if(!sleep_ok)
    {
        LOG_WARNING("[3DSpatial] Linux events: failed to subscribe PrepareForSleep");
    }

    if(!ResolveSessionPath())
    {
        LOG_WARNING("[3DSpatial] Linux events: no logind session path (lock events unavailable)");
        return sleep_ok;
    }

    const bool lock_ok = bus.connect(QStringLiteral("org.freedesktop.login1"),
                                     session_path_,
                                     QStringLiteral("org.freedesktop.DBus.Properties"),
                                     QStringLiteral("PropertiesChanged"),
                                     this,
                                     SLOT(onSessionPropertiesChanged(QString,QVariantMap,QStringList)));
    if(!lock_ok)
    {
        LOG_WARNING("[3DSpatial] Linux events: failed to subscribe session PropertiesChanged");
    }

    PollLockedHint();
    return sleep_ok || lock_ok;
}

void LinuxLoginWatcher::Disconnect()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.disconnect(QStringLiteral("org.freedesktop.login1"),
                   QStringLiteral("/org/freedesktop/login1"),
                   QStringLiteral("org.freedesktop.login1.Manager"),
                   QStringLiteral("PrepareForSleep"),
                   this,
                   SLOT(onPrepareForSleep(bool)));
    if(!session_path_.isEmpty())
    {
        bus.disconnect(QStringLiteral("org.freedesktop.login1"),
                       session_path_,
                       QStringLiteral("org.freedesktop.DBus.Properties"),
                       QStringLiteral("PropertiesChanged"),
                       this,
                       SLOT(onSessionPropertiesChanged(QString,QVariantMap,QStringList)));
    }
    session_path_.clear();
}

void LinuxLoginWatcher::PollLockedHint()
{
    if(session_path_.isEmpty() || !owner_)
    {
        return;
    }
    QDBusInterface session(QStringLiteral("org.freedesktop.login1"),
                           session_path_,
                           QStringLiteral("org.freedesktop.DBus.Properties"),
                           QDBusConnection::systemBus());
    QDBusReply<QVariant> reply = session.call(QStringLiteral("Get"),
                                              QStringLiteral("org.freedesktop.login1.Session"),
                                              QStringLiteral("LockedHint"));
    if(!reply.isValid())
    {
        return;
    }
    const bool now_locked = reply.value().toBool();
    if(now_locked == locked_)
    {
        return;
    }
    locked_ = now_locked;
    owner_->NotifyLock(locked_);
}

void LinuxLoginWatcher::onPrepareForSleep(bool sleeping)
{
    if(owner_)
    {
        owner_->NotifySleep(sleeping);
    }
}

void LinuxLoginWatcher::onSessionPropertiesChanged(const QString& interface,
                                                   const QVariantMap& changed,
                                                   const QStringList& invalidated)
{
    (void)invalidated;
    if(interface != QStringLiteral("org.freedesktop.login1.Session"))
    {
        return;
    }
    if(changed.contains(QStringLiteral("LockedHint")))
    {
        PollLockedHint();
    }
}

LinuxEventSource::LinuxEventSource() = default;

LinuxEventSource::~LinuxEventSource()
{
    Stop();
}

std::vector<EventInfo> LinuxEventSource::ListEvents() const
{
    return {
        {"session_lock", "Session lock", EventEdge::Level},
        {"session_unlock", "Session unlock", EventEdge::Level},
        {"prepare_for_sleep", "About to sleep", EventEdge::Level},
        {"resume", "Resumed from sleep", EventEdge::Level},
    };
}

void LinuxEventSource::NotifyLock(bool locked)
{
    if(locked)
    {
        EmitLevel("session_lock", true);
        EmitLevel("session_unlock", false);
    }
    else
    {
        EmitLevel("session_lock", false);
        EmitLevel("session_unlock", true);
    }
}

void LinuxEventSource::NotifySleep(bool sleeping)
{
    if(sleeping)
    {
        EmitLevel("prepare_for_sleep", true);
        EmitLevel("resume", false);
    }
    else
    {
        EmitLevel("prepare_for_sleep", false);
        EmitLevel("resume", true);
    }
}

void LinuxEventSource::Start()
{
    if(started_)
    {
        return;
    }
    watcher_ = std::make_unique<LinuxLoginWatcher>(this);
    if(!watcher_->Connect())
    {
        watcher_.reset();
        return;
    }
    started_ = true;
    LOG_INFO("[3DSpatial] Linux event source started");
}

void LinuxEventSource::Stop()
{
    if(watcher_)
    {
        watcher_->Disconnect();
        watcher_.reset();
    }
    started_ = false;
}

#endif

std::unique_ptr<EventSource> TryCreateLinuxEventSource()
{
#ifdef Q_OS_LINUX
    return std::make_unique<LinuxEventSource>();
#else
    return nullptr;
#endif
}

} // namespace EffectBinding
