// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameTelemetryStatusPanel.h"
#include "Game/GameTelemetryBridge.h"
#include "PluginUiUtils.h"
#include "ui_GameTelemetryStatusPanel.h"

#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>

#include <chrono>

GameTelemetryStatusPanel::GameTelemetryStatusPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::GameTelemetryStatusPanel)
{
    ui->setupUi(this);
    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());
    /* Narrow docks: long lines wrap instead of being cut off. */
    ui->statusLabel->setWordWrap(true);
    ui->countersLabel->setWordWrap(true);

    signals_label = new QLabel(this);
    signals_label->setWordWrap(true);
    ui->mainLayout->addWidget(signals_label);
    rates_label = new QLabel(this);
    rates_label->setWordWrap(true);
    rates_label->setToolTip(QStringLiteral(
        "LED updates: how many times a second the effect stack sends colours to your devices.\n"
        "Mod frames: Room Ambilight frames arriving from Minecraft.\n"
        "Mod \u2192 plugin: how long the newest frame took from the mod writing it to the plugin reading it.\n"
        "Each device then updates at its own speed (slow ones never hold up the others)."));
    ui->mainLayout->addWidget(rates_label);

    refresh_timer = new QTimer(this);
    refresh_timer->setTimerType(Qt::CoarseTimer);
    refresh_timer->setInterval(1000);
    connect(refresh_timer, &QTimer::timeout, this, &GameTelemetryStatusPanel::RefreshStatus);
}

void GameTelemetryStatusPanel::showEvent(QShowEvent* event)
{
    QGroupBox::showEvent(event);
    if(refresh_timer && !refresh_timer->isActive())
    {
        RefreshStatus();
        refresh_timer->start();
    }
}

void GameTelemetryStatusPanel::hideEvent(QHideEvent* event)
{
    if(refresh_timer)
    {
        refresh_timer->stop();
    }
    QGroupBox::hideEvent(event);
}

GameTelemetryStatusPanel::~GameTelemetryStatusPanel()
{
    delete ui;
}

void GameTelemetryStatusPanel::RefreshStatus()
{
    unsigned int total = 0;
    unsigned int valid = 0;
    unsigned int error = 0;
    std::string source;
    std::string type;
    GameTelemetryBridge::GetStats(total, valid, error, source, type);

    QString bridge_line = "UDP 127.0.0.1:9876";
    if(!source.empty() || !type.empty())
    {
        bridge_line += QString(" | Last: %1 / %2")
                           .arg(QString::fromStdString(source.empty() ? "-" : source),
                                QString::fromStdString(type.empty() ? "-" : type));
    }
    ui->statusLabel->setText(bridge_line);
    ui->countersLabel->setText(QString("Packets %1 | Valid %2 | Errors %3")
                                   .arg(total)
                                   .arg(valid)
                                   .arg(error));

    const GameTelemetryBridge::TelemetrySnapshot snap = GameTelemetryBridge::GetTelemetrySnapshot();
    if(signals_label)
    {
        QString room_detail = QStringLiteral("no");
        if(snap.room_sample.has_frame)
        {
            room_detail = QStringLiteral("yes (SHM %1×%2×%3)")
                              .arg(snap.room_sample.size_x)
                              .arg(snap.room_sample.size_y)
                              .arg(snap.room_sample.size_z);
        }
        signals_label->setText(QStringLiteral("Pose: %1 | Room sample: %2")
                                   .arg(snap.has_player_pose ? QStringLiteral("yes") : QStringLiteral("no"),
                                        room_detail));
    }

    /* Live rates since the last refresh (about once a second). */
    if(rates_label)
    {
        const GameTelemetryBridge::LiveRates r = GameTelemetryBridge::GetLiveRates();
        const unsigned long long now = (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count();
        if(last_rates_ms > 0 && now > last_rates_ms)
        {
            const double secs = (double)(now - last_rates_ms) / 1000.0;
            const double led_fps = (double)(r.led_frames - last_led_frames) / secs;
            const double room_fps = (double)(r.room_frames - last_room_frames) / secs;
            QString line = QStringLiteral("LED updates: %1/s | Mod frames: %2/s").arg(led_fps, 0, 'f', 0).arg(room_fps, 0, 'f', 0);
            if(r.room_delay_ms >= 0 && r.room_last_ms > 0 && now - r.room_last_ms < 2000)
            {
                line += QStringLiteral(" | Mod \u2192 plugin: %1 ms").arg(r.room_delay_ms);
            }
            rates_label->setText(line);
        }
        last_rates_ms = now;
        last_led_frames = r.led_frames;
        last_room_frames = r.room_frames;
    }
}
