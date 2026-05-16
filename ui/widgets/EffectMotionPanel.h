// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTMOTIONPANEL_H
#define EFFECTMOTIONPANEL_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QSlider;

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

    QSlider* speedSlider() const { return speed_slider_; }
    QLabel* speedLabel() const { return speed_label_; }
    QSlider* brightnessSlider() const { return brightness_slider_; }
    QLabel* brightnessLabel() const { return brightness_label_; }
    QSlider* frequencySlider() const { return frequency_slider_; }
    QLabel* frequencyLabel() const { return frequency_label_; }
    QSlider* detailSlider() const { return detail_slider_; }
    QLabel* detailLabel() const { return detail_label_; }
    QSlider* sizeSlider() const { return size_slider_; }
    QLabel* sizeLabel() const { return size_label_; }
    QSlider* scaleSlider() const { return scale_slider_; }
    QLabel* scaleLabel() const { return scale_label_; }
    QCheckBox* scaleInvertCheck() const { return scale_invert_check_; }
    QSlider* fpsSlider() const { return fps_slider_; }
    QLabel* fpsLabel() const { return fps_label_; }

private:
    QSlider* speed_slider_ = nullptr;
    QLabel* speed_label_ = nullptr;
    QSlider* brightness_slider_ = nullptr;
    QLabel* brightness_label_ = nullptr;
    QSlider* frequency_slider_ = nullptr;
    QLabel* frequency_label_ = nullptr;
    QSlider* detail_slider_ = nullptr;
    QLabel* detail_label_ = nullptr;
    QSlider* size_slider_ = nullptr;
    QLabel* size_label_ = nullptr;
    QSlider* scale_slider_ = nullptr;
    QLabel* scale_label_ = nullptr;
    QCheckBox* scale_invert_check_ = nullptr;
    QSlider* fps_slider_ = nullptr;
    QLabel* fps_label_ = nullptr;
};

#endif
