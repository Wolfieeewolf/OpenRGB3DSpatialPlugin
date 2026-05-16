// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLAYERBANNER_H
#define EFFECTLAYERBANNER_H

#include <QWidget>

class QPushButton;

class EffectLayerBanner : public QWidget
{
public:
    explicit EffectLayerBanner(bool include_start_stop, QWidget* parent = nullptr);

    QPushButton* startEffectButton() const { return start_effect_button_; }
    QPushButton* stopEffectButton() const { return stop_effect_button_; }

private:
    QPushButton* start_effect_button_ = nullptr;
    QPushButton* stop_effect_button_ = nullptr;
};

#endif
