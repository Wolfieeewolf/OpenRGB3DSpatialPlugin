// SPDX-License-Identifier: GPL-2.0-only

#include "EffectGeometryPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

EffectGeometryPanel::EffectGeometryPanel(unsigned int scale_x,
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
                                                           QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* geometry_layout = new QVBoxLayout(this);
    geometry_layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* axis_scale_group = new QGroupBox(QStringLiteral("Effect scale (X / Y / Z %)"));
    axis_scale_group->setToolTip(
        QStringLiteral("Scale the effect along each axis. 100% = normal. Does not move scene objects or the camera."));
    QVBoxLayout* axis_scale_layout = new QVBoxLayout();

    QHBoxLayout* scale_x_layout = new QHBoxLayout();
    scale_x_layout->addWidget(new QLabel(QStringLiteral("X:")));
    scale_x_slider_ = new QSlider(Qt::Horizontal);
    scale_x_slider_->setRange(1, 400);
    scale_x_slider_->setValue((int)scale_x);
    scale_x_slider_->setToolTip(QStringLiteral("Effect scale along X (left ↔ right). 100% = normal."));
    scale_x_layout->addWidget(scale_x_slider_);
    scale_x_label_ = new QLabel(QString::number(scale_x) + QStringLiteral("%"));
    scale_x_label_->setMinimumWidth(45);
    scale_x_layout->addWidget(scale_x_label_);
    axis_scale_layout->addLayout(scale_x_layout);

    QHBoxLayout* scale_y_layout = new QHBoxLayout();
    scale_y_layout->addWidget(new QLabel(QStringLiteral("Y:")));
    scale_y_slider_ = new QSlider(Qt::Horizontal);
    scale_y_slider_->setRange(1, 400);
    scale_y_slider_->setValue((int)scale_y);
    scale_y_slider_->setToolTip(QStringLiteral("Effect scale along Y (floor ↔ ceiling). 100% = normal."));
    scale_y_layout->addWidget(scale_y_slider_);
    scale_y_label_ = new QLabel(QString::number(scale_y) + QStringLiteral("%"));
    scale_y_label_->setMinimumWidth(45);
    scale_y_layout->addWidget(scale_y_label_);
    axis_scale_layout->addLayout(scale_y_layout);

    QHBoxLayout* scale_z_layout = new QHBoxLayout();
    scale_z_layout->addWidget(new QLabel(QStringLiteral("Z:")));
    scale_z_slider_ = new QSlider(Qt::Horizontal);
    scale_z_slider_->setRange(1, 400);
    scale_z_slider_->setValue((int)scale_z);
    scale_z_slider_->setToolTip(QStringLiteral("Effect scale along Z (front ↔ back). 100% = normal."));
    scale_z_layout->addWidget(scale_z_slider_);
    scale_z_label_ = new QLabel(QString::number(scale_z) + QStringLiteral("%"));
    scale_z_label_->setMinimumWidth(45);
    scale_z_layout->addWidget(scale_z_label_);
    axis_scale_layout->addLayout(scale_z_layout);

    axis_scale_reset_button_ = new QPushButton(QStringLiteral("Reset to defaults"));
    axis_scale_reset_button_->setToolTip(QStringLiteral("Reset effect scale X, Y, Z to 100%"));
    axis_scale_layout->addWidget(axis_scale_reset_button_);

    axis_scale_group->setLayout(axis_scale_layout);
    geometry_layout->addWidget(axis_scale_group);

    QGroupBox* axis_scale_rot_group = new QGroupBox(QStringLiteral("Effect scale rotation (°)"));
    axis_scale_rot_group->setToolTip(QStringLiteral(
        "Rotate the direction of the effect scale axes. Per-axis scale is applied in this orientation (before effect rotation below)."));
    QVBoxLayout* axis_scale_rot_layout = new QVBoxLayout();
    QHBoxLayout* asr_yaw_layout = new QHBoxLayout();
    asr_yaw_layout->addWidget(new QLabel(QStringLiteral("Yaw:")));
    axis_scale_rot_yaw_slider_ = new QSlider(Qt::Horizontal);
    axis_scale_rot_yaw_slider_->setRange(-180, 180);
    axis_scale_rot_yaw_slider_->setValue((int)axis_scale_rot_yaw);
    axis_scale_rot_yaw_slider_->setToolTip(
        QStringLiteral("Yaw: rotate scale axes horizontally. Scale is applied in this frame."));
    axis_scale_rot_yaw_label_ = new QLabel(QString::number((int)axis_scale_rot_yaw) + QStringLiteral("°"));
    axis_scale_rot_yaw_label_->setMinimumWidth(40);
    asr_yaw_layout->addWidget(axis_scale_rot_yaw_slider_);
    asr_yaw_layout->addWidget(axis_scale_rot_yaw_label_);
    axis_scale_rot_layout->addLayout(asr_yaw_layout);
    QHBoxLayout* asr_pitch_layout = new QHBoxLayout();
    asr_pitch_layout->addWidget(new QLabel(QStringLiteral("Pitch:")));
    axis_scale_rot_pitch_slider_ = new QSlider(Qt::Horizontal);
    axis_scale_rot_pitch_slider_->setRange(-180, 180);
    axis_scale_rot_pitch_slider_->setValue((int)axis_scale_rot_pitch);
    axis_scale_rot_pitch_label_ = new QLabel(QString::number((int)axis_scale_rot_pitch) + QStringLiteral("°"));
    axis_scale_rot_pitch_label_->setMinimumWidth(40);
    asr_pitch_layout->addWidget(axis_scale_rot_pitch_slider_);
    asr_pitch_layout->addWidget(axis_scale_rot_pitch_label_);
    axis_scale_rot_layout->addLayout(asr_pitch_layout);
    QHBoxLayout* asr_roll_layout = new QHBoxLayout();
    asr_roll_layout->addWidget(new QLabel(QStringLiteral("Roll:")));
    axis_scale_rot_roll_slider_ = new QSlider(Qt::Horizontal);
    axis_scale_rot_roll_slider_->setRange(-180, 180);
    axis_scale_rot_roll_slider_->setValue((int)axis_scale_rot_roll);
    axis_scale_rot_roll_label_ = new QLabel(QString::number((int)axis_scale_rot_roll) + QStringLiteral("°"));
    axis_scale_rot_roll_label_->setMinimumWidth(40);
    asr_roll_layout->addWidget(axis_scale_rot_roll_slider_);
    asr_roll_layout->addWidget(axis_scale_rot_roll_label_);
    axis_scale_rot_layout->addLayout(asr_roll_layout);

    axis_scale_rot_reset_button_ = new QPushButton(QStringLiteral("Reset to defaults"));
    axis_scale_rot_reset_button_->setToolTip(QStringLiteral("Reset effect scale rotation (yaw, pitch, roll) to 0°"));
    axis_scale_rot_layout->addWidget(axis_scale_rot_reset_button_);

    axis_scale_rot_group->setLayout(axis_scale_rot_layout);
    geometry_layout->addWidget(axis_scale_rot_group);

    position_offset_group_ = new QGroupBox(QStringLiteral("Effect center offset (%)"));
    position_offset_group_->setToolTip(QStringLiteral(
        "Shift the effect origin from room center or the chosen reference point. Percent of half-room size per axis. Does not move scene objects or the camera."));
    QVBoxLayout* offset_layout = new QVBoxLayout();
    QHBoxLayout* offset_x_layout = new QHBoxLayout();
    offset_x_layout->addWidget(new QLabel(QStringLiteral("X:")));
    offset_x_slider_ = new QSlider(Qt::Horizontal);
    offset_x_slider_->setRange(-100, 100);
    offset_x_slider_->setValue(offset_x);
    offset_x_slider_->setToolTip(QStringLiteral("Effect offset left (-) or right (+) as % of half-room width"));
    offset_x_layout->addWidget(offset_x_slider_);
    offset_x_label_ = new QLabel(QString::number(offset_x) + QStringLiteral("%"));
    offset_x_label_->setMinimumWidth(45);
    offset_x_layout->addWidget(offset_x_label_);
    offset_layout->addLayout(offset_x_layout);
    QHBoxLayout* offset_y_layout = new QHBoxLayout();
    offset_y_layout->addWidget(new QLabel(QStringLiteral("Y:")));
    offset_y_slider_ = new QSlider(Qt::Horizontal);
    offset_y_slider_->setRange(-100, 100);
    offset_y_slider_->setValue(offset_y);
    offset_y_slider_->setToolTip(QStringLiteral("Effect offset down (-) or up (+) as % of half-room height"));
    offset_y_layout->addWidget(offset_y_slider_);
    offset_y_label_ = new QLabel(QString::number(offset_y) + QStringLiteral("%"));
    offset_y_label_->setMinimumWidth(45);
    offset_y_layout->addWidget(offset_y_label_);
    offset_layout->addLayout(offset_y_layout);
    QHBoxLayout* offset_z_layout = new QHBoxLayout();
    offset_z_layout->addWidget(new QLabel(QStringLiteral("Z:")));
    offset_z_slider_ = new QSlider(Qt::Horizontal);
    offset_z_slider_->setRange(-100, 100);
    offset_z_slider_->setValue(offset_z);
    offset_z_slider_->setToolTip(QStringLiteral("Effect offset forward (-) or back (+) as % of half-room depth"));
    offset_z_layout->addWidget(offset_z_slider_);
    offset_z_label_ = new QLabel(QString::number(offset_z) + QStringLiteral("%"));
    offset_z_label_->setMinimumWidth(45);
    offset_z_layout->addWidget(offset_z_label_);
    offset_layout->addLayout(offset_z_layout);
    offset_center_reset_button_ = new QPushButton(QStringLiteral("Reset to center"));
    offset_center_reset_button_->setToolTip(QStringLiteral("Set effect position offset X, Y, Z to 0%"));
    offset_layout->addWidget(offset_center_reset_button_);
    position_offset_group_->setLayout(offset_layout);
    geometry_layout->addWidget(position_offset_group_);

    QGroupBox* rotation_group = new QGroupBox(QStringLiteral("Effect rotation (°)"));
    rotation_group->setToolTip(QStringLiteral(
        "Rotate the pattern around the effect origin (after per-axis scale and scale-axis rotation). Does not move the camera or scene objects."));
    QVBoxLayout* rotation_layout = new QVBoxLayout();

    QHBoxLayout* yaw_layout = new QHBoxLayout();
    yaw_layout->addWidget(new QLabel(QStringLiteral("Yaw:")));
    rotation_yaw_slider_ = new QSlider(Qt::Horizontal);
    rotation_yaw_slider_->setRange(0, 360);
    rotation_yaw_slider_->setValue((int)rotation_yaw);
    rotation_yaw_slider_->setToolTip(QStringLiteral("Effect rotation around Y (horizontal). 0–360°."));
    rotation_yaw_label_ = new QLabel(QString::number((int)rotation_yaw) + QStringLiteral("°"));
    rotation_yaw_label_->setMinimumWidth(50);
    yaw_layout->addWidget(rotation_yaw_slider_);
    yaw_layout->addWidget(rotation_yaw_label_);
    rotation_layout->addLayout(yaw_layout);

    QHBoxLayout* pitch_layout = new QHBoxLayout();
    pitch_layout->addWidget(new QLabel(QStringLiteral("Pitch:")));
    rotation_pitch_slider_ = new QSlider(Qt::Horizontal);
    rotation_pitch_slider_->setRange(0, 360);
    rotation_pitch_slider_->setValue((int)rotation_pitch);
    rotation_pitch_slider_->setToolTip(QStringLiteral("Effect rotation around X (vertical). 0–360°."));
    rotation_pitch_label_ = new QLabel(QString::number((int)rotation_pitch) + QStringLiteral("°"));
    rotation_pitch_label_->setMinimumWidth(50);
    pitch_layout->addWidget(rotation_pitch_slider_);
    pitch_layout->addWidget(rotation_pitch_label_);
    rotation_layout->addLayout(pitch_layout);

    QHBoxLayout* roll_layout = new QHBoxLayout();
    roll_layout->addWidget(new QLabel(QStringLiteral("Roll:")));
    rotation_roll_slider_ = new QSlider(Qt::Horizontal);
    rotation_roll_slider_->setRange(0, 360);
    rotation_roll_slider_->setValue((int)rotation_roll);
    rotation_roll_slider_->setToolTip(QStringLiteral("Effect rotation around Z (twist). 0–360°."));
    rotation_roll_label_ = new QLabel(QString::number((int)rotation_roll) + QStringLiteral("°"));
    rotation_roll_label_->setMinimumWidth(50);
    roll_layout->addWidget(rotation_roll_slider_);
    roll_layout->addWidget(rotation_roll_label_);
    rotation_layout->addLayout(roll_layout);

    rotation_reset_button_ = new QPushButton(QStringLiteral("Reset rotation"));
    rotation_reset_button_->setToolTip(QStringLiteral("Reset effect rotation (yaw, pitch, roll) to 0°"));
    rotation_layout->addWidget(rotation_reset_button_);

    rotation_group->setLayout(rotation_layout);
    geometry_layout->addWidget(rotation_group);
}
