// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventSource.h"
#include <memory>

#ifdef Q_OS_LINUX
#include <QObject>
#include <QString>
#include <QVariantMap>
#endif

namespace EffectBinding
{

#ifdef Q_OS_LINUX

class LinuxEventSource;

class LinuxLoginWatcher : public QObject
{
    Q_OBJECT
public:
    explicit LinuxLoginWatcher(LinuxEventSource* owner);

    bool Connect();
    void Disconnect();

public slots:
    void onPrepareForSleep(bool sleeping);
    void onSessionPropertiesChanged(const QString& interface,
                                    const QVariantMap& changed,
                                    const QStringList& invalidated);

private:
    bool ResolveSessionPath();
    void PollLockedHint();

    LinuxEventSource* owner_ = nullptr;
    QString session_path_;
    bool locked_ = false;
};

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
