// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GAMETELEMETRYSTATUSPANEL_H
#define GAMETELEMETRYSTATUSPANEL_H

#include <QGroupBox>

class QLabel;
class QTimer;

namespace Ui {
class GameTelemetryStatusPanel;
}

class GameTelemetryStatusPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit GameTelemetryStatusPanel(QWidget* parent = nullptr);
    ~GameTelemetryStatusPanel() override;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void RefreshStatus();

private:
    Ui::GameTelemetryStatusPanel* ui = nullptr;
    QTimer* refresh_timer = nullptr;
    QLabel* signals_label = nullptr;
    QLabel* rates_label = nullptr;
    unsigned long long last_rates_ms = 0;
    unsigned long long last_room_frames = 0;
    unsigned long long last_led_frames = 0;
};

#endif
