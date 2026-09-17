// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLAYERBANNER_H
#define EFFECTLAYERBANNER_H

#include <QWidget>

class QPushButton;

namespace Ui {
class EffectLayerBanner;
}

class EffectLayerBanner : public QWidget
{
public:
    explicit EffectLayerBanner(bool include_start_stop, QWidget* parent = nullptr);
    ~EffectLayerBanner() override;

    QPushButton* startEffectButton() const;
    QPushButton* stopEffectButton() const;

private:
    Ui::EffectLayerBanner* ui = nullptr;
};

#endif
