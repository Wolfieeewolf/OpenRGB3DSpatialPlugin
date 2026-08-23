// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTMOTIONPANEL_H
#define EFFECTMOTIONPANEL_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QSlider;

namespace Ui {
class EffectMotionPanel;
}

class EffectMotionPanel : public QWidget
{
public:
    EffectMotionPanel(unsigned int speed,
                      unsigned int brightness,
                      unsigned int frequency,
                      unsigned int detail,
                      unsigned int size,
                      unsigned int scale,
                      bool scale_inverted,
                      unsigned int fps,
                      QWidget* parent = nullptr);
    ~EffectMotionPanel() override;

    QSlider* speedSlider() const;
    QLabel* speedLabel() const;
    QSlider* brightnessSlider() const;
    QLabel* brightnessLabel() const;
    QSlider* frequencySlider() const;
    QLabel* frequencyLabel() const;
    QSlider* detailSlider() const;
    QLabel* detailLabel() const;
    QSlider* sizeSlider() const;
    QLabel* sizeLabel() const;
    QSlider* scaleSlider() const;
    QLabel* scaleLabel() const;
    QCheckBox* scaleInvertCheck() const;
    QSlider* fpsSlider() const;
    QLabel* fpsLabel() const;

private:
    Ui::EffectMotionPanel* ui = nullptr;
};

#endif
