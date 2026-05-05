// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTGEOMETRYPANEL_H
#define EFFECTGEOMETRYPANEL_H

#include <QWidget>

class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;

/** Per-axis scale, scale-axis rotation, center offset, and effect rotation — shared geometry block. */
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

    QSlider* scaleXSlider() const { return scale_x_slider_; }
    QLabel* scaleXLabel() const { return scale_x_label_; }
    QSlider* scaleYSlider() const { return scale_y_slider_; }
    QLabel* scaleYLabel() const { return scale_y_label_; }
    QSlider* scaleZSlider() const { return scale_z_slider_; }
    QLabel* scaleZLabel() const { return scale_z_label_; }
    QPushButton* axisScaleResetButton() const { return axis_scale_reset_button_; }

    QSlider* axisScaleRotYawSlider() const { return axis_scale_rot_yaw_slider_; }
    QLabel* axisScaleRotYawLabel() const { return axis_scale_rot_yaw_label_; }
    QSlider* axisScaleRotPitchSlider() const { return axis_scale_rot_pitch_slider_; }
    QLabel* axisScaleRotPitchLabel() const { return axis_scale_rot_pitch_label_; }
    QSlider* axisScaleRotRollSlider() const { return axis_scale_rot_roll_slider_; }
    QLabel* axisScaleRotRollLabel() const { return axis_scale_rot_roll_label_; }
    QPushButton* axisScaleRotResetButton() const { return axis_scale_rot_reset_button_; }

    QGroupBox* positionOffsetGroup() const { return position_offset_group_; }
    QSlider* offsetXSlider() const { return offset_x_slider_; }
    QLabel* offsetXLabel() const { return offset_x_label_; }
    QSlider* offsetYSlider() const { return offset_y_slider_; }
    QLabel* offsetYLabel() const { return offset_y_label_; }
    QSlider* offsetZSlider() const { return offset_z_slider_; }
    QLabel* offsetZLabel() const { return offset_z_label_; }
    QPushButton* offsetCenterResetButton() const { return offset_center_reset_button_; }

    QSlider* rotationYawSlider() const { return rotation_yaw_slider_; }
    QLabel* rotationYawLabel() const { return rotation_yaw_label_; }
    QSlider* rotationPitchSlider() const { return rotation_pitch_slider_; }
    QLabel* rotationPitchLabel() const { return rotation_pitch_label_; }
    QSlider* rotationRollSlider() const { return rotation_roll_slider_; }
    QLabel* rotationRollLabel() const { return rotation_roll_label_; }
    QPushButton* rotationResetButton() const { return rotation_reset_button_; }

private:
    QSlider* scale_x_slider_ = nullptr;
    QLabel* scale_x_label_ = nullptr;
    QSlider* scale_y_slider_ = nullptr;
    QLabel* scale_y_label_ = nullptr;
    QSlider* scale_z_slider_ = nullptr;
    QLabel* scale_z_label_ = nullptr;
    QPushButton* axis_scale_reset_button_ = nullptr;

    QSlider* axis_scale_rot_yaw_slider_ = nullptr;
    QLabel* axis_scale_rot_yaw_label_ = nullptr;
    QSlider* axis_scale_rot_pitch_slider_ = nullptr;
    QLabel* axis_scale_rot_pitch_label_ = nullptr;
    QSlider* axis_scale_rot_roll_slider_ = nullptr;
    QLabel* axis_scale_rot_roll_label_ = nullptr;
    QPushButton* axis_scale_rot_reset_button_ = nullptr;

    QGroupBox* position_offset_group_ = nullptr;
    QSlider* offset_x_slider_ = nullptr;
    QLabel* offset_x_label_ = nullptr;
    QSlider* offset_y_slider_ = nullptr;
    QLabel* offset_y_label_ = nullptr;
    QSlider* offset_z_slider_ = nullptr;
    QLabel* offset_z_label_ = nullptr;
    QPushButton* offset_center_reset_button_ = nullptr;

    QSlider* rotation_yaw_slider_ = nullptr;
    QLabel* rotation_yaw_label_ = nullptr;
    QSlider* rotation_pitch_slider_ = nullptr;
    QLabel* rotation_pitch_label_ = nullptr;
    QSlider* rotation_roll_slider_ = nullptr;
    QLabel* rotation_roll_label_ = nullptr;
    QPushButton* rotation_reset_button_ = nullptr;
};

#endif
