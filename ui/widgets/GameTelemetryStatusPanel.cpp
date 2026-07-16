// SPDX-License-Identifier: GPL-2.0-only

#include "GameTelemetryStatusPanel.h"
#include "GameTelemetryBridge.h"
#include "PluginUiUtils.h"
#include "ui_GameTelemetryStatusPanel.h"

#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>

GameTelemetryStatusPanel::GameTelemetryStatusPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::GameTelemetryStatusPanel)
{
    ui->setupUi(this);
    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    signals_label = new QLabel(this);
    ui->mainLayout->addWidget(signals_label);

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
        signals_label->setText(QStringLiteral("Pose: %1 | Room sample: %2 | World light: %3")
                                   .arg(snap.has_player_pose ? QStringLiteral("yes") : QStringLiteral("no"),
                                        room_detail,
                                        snap.has_world_light ? QStringLiteral("yes") : QStringLiteral("no")));
    }
}
