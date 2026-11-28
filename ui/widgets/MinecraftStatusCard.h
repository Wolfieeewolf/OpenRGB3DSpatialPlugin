// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QProgressBar;
class QTimer;
class QToolButton;
class MinecraftLinkDiagram;

/** Top of every Minecraft effect panel: is the mod connected, the player's vitals as bars,
    what Room Ambilight is doing, live rates, and a short log of what happened. Refreshes four
    times a second while shown. */
class MinecraftStatusCard : public QWidget
{
    Q_OBJECT

public:
    explicit MinecraftStatusCard(QWidget* parent = nullptr);

protected:
    void showEvent(class QShowEvent* event) override;
    void hideEvent(class QHideEvent* event) override;

private:
    void refresh();
    void log(const QString& text, bool warn = false);

    QLabel* title_ = nullptr;
    QLabel* lights_ = nullptr;
    QProgressBar* health_ = nullptr;
    QProgressBar* hunger_ = nullptr;
    QProgressBar* air_ = nullptr;
    QProgressBar* durability_ = nullptr;
    QLabel* health_text_ = nullptr;
    QLabel* hunger_text_ = nullptr;
    QLabel* air_text_ = nullptr;
    QLabel* durability_text_ = nullptr;
    QWidget* air_row_ = nullptr;
    QWidget* durability_row_ = nullptr;
    QLabel* room_ = nullptr;
    QLabel* rates_ = nullptr;
    QListWidget* log_ = nullptr;
    QToolButton* details_toggle_ = nullptr;
    QLabel* details_ = nullptr;
    MinecraftLinkDiagram* diagram_ = nullptr; /* unused (replaced by meters) */
    QWidget* meters_ = nullptr;
    QProgressBar* vitals_meter_ = nullptr;
    QProgressBar* room_meter_ = nullptr;
    QProgressBar* led_meter_ = nullptr;
    QLabel* vitals_meter_text_ = nullptr;
    QLabel* room_meter_text_ = nullptr;
    QLabel* led_meter_text_ = nullptr;
    double vitals_peak_ = 0.0;
    double room_peak_ = 0.0;
    unsigned long long rates_msgs_ = 0;
    QTimer* timer_ = nullptr;

    /* What the log last saw, so it reports only changes. */
    int seen_link_ = -1; /* -1 unknown, 0 waiting, 1 connected, 2 lost */
    unsigned long long seen_damage_ms_ = 0;
    unsigned int seen_room_config_ = 0;
    std::uint32_t seen_options_ = 0xFFFFFFFFu;

    /* Rates between refreshes. */
    unsigned long long rates_ms_ = 0;
    unsigned long long rates_room_ = 0;
    unsigned long long rates_led_ = 0;
    QString rates_text_;
};
