// SPDX-License-Identifier: GPL-2.0-only

#include "GameTelemetryStatusPanel.h"
#include "GameTelemetryBridge.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>

GameTelemetryStatusPanel::GameTelemetryStatusPanel(QWidget* parent) : QGroupBox(parent)
{
    setTitle("Game telemetry (UDP)");

    QVBoxLayout* game_layout = new QVBoxLayout(this);
    QLabel* game_help = new QLabel(
        "Listens on UDP 127.0.0.1:9876 — the Minecraft Fabric mod sends JSON here directly. "
        "You do not need a separate 'OpenRGB server' for this. "
        "Enable the 3D Spatial plugin; counters should increase while the game runs with the sender mod.");
    game_help->setWordWrap(true);
    game_help->setStyleSheet("color: gray; font-size: 10px;");
    game_layout->addWidget(game_help);

    status_label = new QLabel("Listener: waiting");
    counters_label = new QLabel("Packets 0 | Valid 0 | Errors 0");
    game_layout->addWidget(status_label);
    game_layout->addWidget(counters_label);

    refresh_timer = new QTimer(this);
    refresh_timer->setInterval(500);
    connect(refresh_timer, &QTimer::timeout, this, &GameTelemetryStatusPanel::RefreshStatus);
    refresh_timer->start();
}

GameTelemetryStatusPanel::~GameTelemetryStatusPanel()
{
}

void GameTelemetryStatusPanel::RefreshStatus()
{
    if(!status_label || !counters_label)
    {
        return;
    }

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
    status_label->setText(bridge_line);
    counters_label->setText(QString("Packets %1 | Valid %2 | Errors %3")
                                .arg(total)
                                .arg(valid)
                                .arg(error));
}
