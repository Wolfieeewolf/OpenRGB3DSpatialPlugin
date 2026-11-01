// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTMOTIONPANEL_H
#define EFFECTMOTIONPANEL_H

#include <QWidget>

class EffectSliderRow;
class QComboBox;
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
    QComboBox* pathAxisCombo() const;
    QSlider* thicknessSlider() const;
    QLabel* thicknessLabel() const;
    QSlider* edgeFadeSlider() const;
    QLabel* edgeFadeLabel() const;
    QSlider* countSlider() const;
    QLabel* countLabel() const;

private:
    Ui::EffectMotionPanel* ui = nullptr;
    EffectSliderRow* speed_row_ = nullptr;
    EffectSliderRow* brightness_row_ = nullptr;
    EffectSliderRow* frequency_row_ = nullptr;
    EffectSliderRow* detail_row_ = nullptr;
    EffectSliderRow* size_row_ = nullptr;
    EffectSliderRow* scale_row_ = nullptr;
    EffectSliderRow* fps_row_ = nullptr;
    EffectSliderRow* thickness_row_ = nullptr;
    EffectSliderRow* edge_fade_row_ = nullptr;
    EffectSliderRow* count_row_ = nullptr;
};

#endif
