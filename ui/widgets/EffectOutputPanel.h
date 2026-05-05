// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTOUTPUTPANEL_H
#define EFFECTOUTPUTPANEL_H

#include <QWidget>

class QLabel;
class QSlider;

/** Intensity, sharpness, smoothing, sampling — shared output-shaping row set. */
class EffectOutputPanel : public QWidget
{
public:
    EffectOutputPanel(unsigned int intensity,
                                      unsigned int sharpness,
                                      unsigned int smoothing,
                                      unsigned int sampling_resolution,
                                      QWidget* parent = nullptr);

    QSlider* intensitySlider() const { return intensity_slider_; }
    QLabel* intensityLabel() const { return intensity_label_; }
    QSlider* sharpnessSlider() const { return sharpness_slider_; }
    QLabel* sharpnessLabel() const { return sharpness_label_; }
    QSlider* smoothingSlider() const { return smoothing_slider_; }
    QLabel* smoothingLabel() const { return smoothing_label_; }
    QSlider* samplingResolutionSlider() const { return sampling_resolution_slider_; }
    QLabel* samplingResolutionLabel() const { return sampling_resolution_label_; }

private:
    QSlider* intensity_slider_ = nullptr;
    QLabel* intensity_label_ = nullptr;
    QSlider* sharpness_slider_ = nullptr;
    QLabel* sharpness_label_ = nullptr;
    QSlider* smoothing_slider_ = nullptr;
    QLabel* smoothing_label_ = nullptr;
    QSlider* sampling_resolution_slider_ = nullptr;
    QLabel* sampling_resolution_label_ = nullptr;
};

#endif
