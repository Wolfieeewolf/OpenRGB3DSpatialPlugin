// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTGEOMETRYPANEL_H
#define EFFECTGEOMETRYPANEL_H

#include <QWidget>

class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;

namespace Ui {
class EffectGeometryPanel;
}

class EffectGeometryPanel : public QWidget
{
public:
    EffectGeometryPanel(unsigned int scale_x,
                        unsigned int scale_y,
                        unsigned int scale_z,
                        float axis_scale_rot_yaw,
                        float axis_scale_rot_pitch,
                        float axis_scale_rot_roll,
                        int offset_x,
                        int offset_y,
                        int offset_z,
                        float rotation_yaw,
                        float rotation_pitch,
                        float rotation_roll,
                        QWidget* parent = nullptr);
    ~EffectGeometryPanel() override;

    QSlider* scaleXSlider() const;
    QLabel* scaleXLabel() const;
    QSlider* scaleYSlider() const;
    QLabel* scaleYLabel() const;
    QSlider* scaleZSlider() const;
    QLabel* scaleZLabel() const;
    QPushButton* axisScaleResetButton() const;

    QSlider* axisScaleRotYawSlider() const;
    QLabel* axisScaleRotYawLabel() const;
    QSlider* axisScaleRotPitchSlider() const;
    QLabel* axisScaleRotPitchLabel() const;
    QSlider* axisScaleRotRollSlider() const;
    QLabel* axisScaleRotRollLabel() const;
    QPushButton* axisScaleRotResetButton() const;

    QGroupBox* positionOffsetGroup() const;
    QSlider* offsetXSlider() const;
    QLabel* offsetXLabel() const;
    QSlider* offsetYSlider() const;
    QLabel* offsetYLabel() const;
    QSlider* offsetZSlider() const;
    QLabel* offsetZLabel() const;
    QPushButton* offsetCenterResetButton() const;

    QSlider* rotationYawSlider() const;
    QLabel* rotationYawLabel() const;
    QSlider* rotationPitchSlider() const;
    QLabel* rotationPitchLabel() const;
    QSlider* rotationRollSlider() const;
    QLabel* rotationRollLabel() const;
    QPushButton* rotationResetButton() const;

private:
    Ui::EffectGeometryPanel* ui = nullptr;
};

#endif
