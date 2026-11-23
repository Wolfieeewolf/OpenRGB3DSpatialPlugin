// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SENSITIVEFIELDWATCHLINUX_H
#define SENSITIVEFIELDWATCHLINUX_H

#include <QtGlobal>

#ifdef Q_OS_LINUX

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QString>

#include <functional>

/* Listens on the accessibility bus for focus changes and asks the newly focused object for its
   role; a password-text role means keys typed now belong to a password. */
class AtspiFocusWatcher : public QObject
{
    Q_OBJECT

public:
    explicit AtspiFocusWatcher(std::function<void(bool)> on_change);
    ~AtspiFocusWatcher() override;

    bool Connect();

private slots:
    void onStateChanged(const QDBusMessage& message);

private:
    void Report(bool in_password_field);

    std::function<void(bool)> on_change_;
    QDBusConnection           bus_;
    bool                      connected_ = false;
    QString                   focused_service_;
    QString                   focused_path_;
    quint64                   focus_serial_ = 0;
    bool                      in_password_ = false;
};

#endif

#endif
