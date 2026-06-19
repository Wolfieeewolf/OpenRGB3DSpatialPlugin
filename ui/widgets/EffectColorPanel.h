// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCOLORPANEL_H
#define EFFECTCOLORPANEL_H

#include <QWidget>

class QCheckBox;
class QHBoxLayout;
class QPushButton;
class QVBoxLayout;

namespace Ui {
class EffectColorPanel;
}

class EffectColorPanel : public QWidget
{
public:
    explicit EffectColorPanel(bool rainbow_mode, QWidget* parent = nullptr);
    ~EffectColorPanel() override;

    QCheckBox* rainbowModeCheck() const;
    QWidget* colorButtonsWidget() const;
    QHBoxLayout* colorButtonsLayout() const;
    QWidget* patternHostWidget() const;
    QVBoxLayout* patternHostLayout() const;
    QPushButton* addColorButton() const;
    QPushButton* removeColorButton() const;

private:
    Ui::EffectColorPanel* ui = nullptr;
};

#endif
