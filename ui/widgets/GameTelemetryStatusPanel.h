// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMETELEMETRYSTATUSPANEL_H
#define GAMETELEMETRYSTATUSPANEL_H

#include <QGroupBox>

class QLabel;
class QTimer;

class GameTelemetryStatusPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit GameTelemetryStatusPanel(QWidget* parent = nullptr);
    ~GameTelemetryStatusPanel() override;

private slots:
    void RefreshStatus();

private:
    QLabel* status_label;
    QLabel* counters_label;
    QTimer* refresh_timer;
};

#endif
