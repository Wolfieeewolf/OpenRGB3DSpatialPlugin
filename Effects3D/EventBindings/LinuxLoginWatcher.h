// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtGlobal>

#ifdef Q_OS_LINUX

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace EffectBinding
{

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

} // namespace EffectBinding

#endif
