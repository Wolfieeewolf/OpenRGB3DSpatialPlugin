// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTOUTPUTPANEL_H
#define EFFECTOUTPUTPANEL_H

#include <QWidget>

class QLabel;
class QSlider;

namespace Ui {
class EffectOutputPanel;
}

class EffectOutputPanel : public QWidget
{
public:
    EffectOutputPanel(unsigned int intensity,
                        unsigned int sharpness,
                        unsigned int smoothing,
                        unsigned int sampling_resolution,
                        QWidget* parent = nullptr);
    ~EffectOutputPanel() override;

    QSlider* intensitySlider() const;
    QLabel* intensityLabel() const;
    QSlider* sharpnessSlider() const;
    QLabel* sharpnessLabel() const;
    QSlider* smoothingSlider() const;
    QLabel* smoothingLabel() const;
    QSlider* samplingResolutionSlider() const;
    QLabel* samplingResolutionLabel() const;

private:
    Ui::EffectOutputPanel* ui = nullptr;
};

#endif
