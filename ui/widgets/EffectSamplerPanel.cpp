// SPDX-License-Identifier: GPL-2.0-only

#include "EffectSamplerPanel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

EffectSamplerPanel::EffectSamplerPanel(SpatialMappingMode spatial_mapping_mode,
                                                                   int compass_layer_spin_preset,
                                                                   int sampler_influence_centi,
                                                                   int compass_north_offset_deg,
                                                                   unsigned int voxel_volume_mix,
                                                                   int voxel_room_scale_centi,
                                                                   int voxel_heading_offset,
                                                                   VoxelDriveMode voxel_drive_mode,
                                                                   QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    QLabel* intro = new QLabel(
        QStringLiteral("Optional room-driven colour mapping (this block is attached only on layers that use it). "
                       "Start with Room mapping mode = Off. Use Compass for directional layers, or Voxel volume for "
                       "game voxel tint / palette drive."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    compass_sampler_group_ = new QGroupBox(QStringLiteral("Room Mapping"));
    compass_sampler_group_->setToolTip(
        QStringLiteral("Horizontal direction (8 sectors) and vertical band (floor / mid / ceiling) choose where you sit on the rainbow or color strip."));
    QVBoxLayout* cg = new QVBoxLayout(compass_sampler_group_);

    cg->addWidget(new QLabel(QStringLiteral("Room mapping mode:")));
    spatial_mapping_combo_ = new QComboBox();
    spatial_mapping_combo_->addItem(QStringLiteral("Off — flat (no direction layers)"), (int)SpatialMappingMode::Off);
    spatial_mapping_combo_->addItem(QStringLiteral("Subtle — gentle crawl with position"), (int)SpatialMappingMode::SubtleTint);
    spatial_mapping_combo_->addItem(QStringLiteral("Compass — sectors + height bands drive palette"), (int)SpatialMappingMode::CompassPalette);
    spatial_mapping_combo_->addItem(QStringLiteral("Voxel volume — full room grid (reference still biases)"), (int)SpatialMappingMode::VoxelVolume);
    spatial_mapping_combo_->setToolTip(
        QStringLiteral("Subtle: soft palette crawl from position. Compass: sectors + floor/mid/ceiling bands; each band uses a compass hub at that height through your reference X/Z. "
                       "Voxel volume: palette varies across the whole room; reference point still nudges mapping."));
    for(int i = 0; i < spatial_mapping_combo_->count(); i++)
    {
        if(spatial_mapping_combo_->itemData(i).toInt() == (int)spatial_mapping_mode)
        {
            spatial_mapping_combo_->setCurrentIndex(i);
            break;
        }
    }
    cg->addWidget(spatial_mapping_combo_);

    cg->addWidget(new QLabel(QStringLiteral("Compass band spin:")));
    compass_layer_spin_combo_ = new QComboBox();
    compass_layer_spin_combo_->addItem(QStringLiteral("All bands clockwise"), 0);
    compass_layer_spin_combo_->addItem(QStringLiteral("All bands counter-clockwise"), 1);
    compass_layer_spin_combo_->addItem(QStringLiteral("Floor CW · mid CCW · ceiling CW"), 2);
    compass_layer_spin_combo_->addItem(QStringLiteral("Floor CCW · mid CW · ceiling CCW"), 3);
    compass_layer_spin_combo_->setToolTip(
        QStringLiteral("Per vertical band spin direction. Used only in Compass mode."));
    compass_layer_spin_combo_->setCurrentIndex(std::clamp(compass_layer_spin_preset, 0, 3));
    cg->addWidget(compass_layer_spin_combo_);

    QHBoxLayout* infl_row = new QHBoxLayout();
    infl_row->addWidget(new QLabel(QStringLiteral("Mapping strength:")));
    sampler_influence_slider_ = new QSlider(Qt::Horizontal);
    sampler_influence_slider_->setRange(0, 250);
    sampler_influence_slider_->setValue(sampler_influence_centi);
    sampler_influence_slider_->setToolTip(
        QStringLiteral("0 = off. 100 = normal. Higher values increase room/voxel influence."));
    infl_row->addWidget(sampler_influence_slider_, 1);
    sampler_influence_label_ = new QLabel(QString::number(sampler_influence_centi) + QStringLiteral("%"));
    sampler_influence_label_->setMinimumWidth(40);
    infl_row->addWidget(sampler_influence_label_);
    cg->addLayout(infl_row);

    QHBoxLayout* north_row = new QHBoxLayout();
    north_row->addWidget(new QLabel(QStringLiteral("Compass north offset:")));
    sampler_compass_north_slider_ = new QSlider(Qt::Horizontal);
    sampler_compass_north_slider_->setRange(-180, 180);
    sampler_compass_north_slider_->setValue(compass_north_offset_deg);
    sampler_compass_north_slider_->setToolTip(
        QStringLiteral("Rotates which compass sector counts as \"forward\" for mapping (degrees). Align sectors with your real room front."));
    north_row->addWidget(sampler_compass_north_slider_, 1);
    sampler_compass_north_label_ =
        new QLabel(QString::number(compass_north_offset_deg) + QStringLiteral("°"));
    sampler_compass_north_label_->setMinimumWidth(44);
    north_row->addWidget(sampler_compass_north_label_);
    cg->addLayout(north_row);

    root->addWidget(compass_sampler_group_);

    voxel_volume_group_ = new QGroupBox(QStringLiteral("Voxel Mapping"));
    voxel_volume_group_->setToolTip(
        QStringLiteral("Requires live voxel telemetry (voxel_frame, e.g. Minecraft UDP)."));
    QVBoxLayout* voxel_layout = new QVBoxLayout(voxel_volume_group_);

    QHBoxLayout* voxel_mix_row = new QHBoxLayout();
    voxel_mix_row->addWidget(new QLabel(QStringLiteral("Color mix:")));
    voxel_volume_mix_slider_ = new QSlider(Qt::Horizontal);
    voxel_volume_mix_slider_->setRange(0, 100);
    voxel_volume_mix_slider_->setValue((int)voxel_volume_mix);
    voxel_volume_mix_slider_->setToolTip(
        QStringLiteral("Blend sampled voxel RGB into the effect (0 = off). Requires live voxel_frame."));
    voxel_mix_row->addWidget(voxel_volume_mix_slider_);
    voxel_volume_mix_label_ = new QLabel(QString::number((int)voxel_volume_mix) + QStringLiteral("%"));
    voxel_volume_mix_label_->setMinimumWidth(44);
    voxel_mix_row->addWidget(voxel_volume_mix_label_);
    voxel_layout->addLayout(voxel_mix_row);

    QHBoxLayout* voxel_scale_row = new QHBoxLayout();
    voxel_scale_row->addWidget(new QLabel(QStringLiteral("Room ↔ volume scale:")));
    voxel_volume_scale_slider_ = new QSlider(Qt::Horizontal);
    voxel_volume_scale_slider_->setRange(2, 80);
    voxel_volume_scale_slider_->setValue(voxel_room_scale_centi);
    voxel_volume_scale_slider_->setToolTip(
        QStringLiteral("Minecraft / sender units per room step (×0.01). Fix stretched or offset blocks."));
    voxel_scale_row->addWidget(voxel_volume_scale_slider_);
    voxel_volume_scale_label_ = new QLabel(QString::number(voxel_room_scale_centi / 100.0f, 'f', 2));
    voxel_volume_scale_label_->setMinimumWidth(40);
    voxel_scale_row->addWidget(voxel_volume_scale_label_);
    voxel_layout->addLayout(voxel_scale_row);

    QHBoxLayout* voxel_heading_row = new QHBoxLayout();
    voxel_heading_row->addWidget(new QLabel(QStringLiteral("Volume heading:")));
    voxel_volume_heading_slider_ = new QSlider(Qt::Horizontal);
    voxel_volume_heading_slider_->setRange(-180, 180);
    voxel_volume_heading_slider_->setValue(voxel_heading_offset);
    voxel_volume_heading_slider_->setToolTip(QStringLiteral("Yaw offset when projecting the volume into the room."));
    voxel_heading_row->addWidget(voxel_volume_heading_slider_);
    voxel_volume_heading_label_ =
        new QLabel(QString::number(voxel_heading_offset) + QStringLiteral("°"));
    voxel_volume_heading_label_->setMinimumWidth(44);
    voxel_heading_row->addWidget(voxel_volume_heading_label_);
    voxel_layout->addLayout(voxel_heading_row);

    QHBoxLayout* voxel_drive_row = new QHBoxLayout();
    voxel_drive_row->addWidget(new QLabel(QStringLiteral("Palette drive:")));
    voxel_drive_combo_ = new QComboBox();
    voxel_drive_combo_->addItem(QStringLiteral("None"), (int)VoxelDriveMode::Off);
    voxel_drive_combo_->addItem(QStringLiteral("From voxel brightness"), (int)VoxelDriveMode::LumaField);
    voxel_drive_combo_->addItem(QStringLiteral("Scroll with room X"), (int)VoxelDriveMode::ScrollRoomX);
    voxel_drive_combo_->addItem(QStringLiteral("Scroll with room Y"), (int)VoxelDriveMode::ScrollRoomY);
    voxel_drive_combo_->addItem(QStringLiteral("Scroll with room Z"), (int)VoxelDriveMode::ScrollRoomZ);
    voxel_drive_combo_->addItem(QStringLiteral("Roll (angle × voxel)"), (int)VoxelDriveMode::VolumeRoll);
    voxel_drive_combo_->setToolTip(
        QStringLiteral("Shifts rainbow / color stops along the LED (wheel rolling through space). Uses voxels when relevant; room axes work without a game."));
    for(int i = 0; i < voxel_drive_combo_->count(); i++)
    {
        if(voxel_drive_combo_->itemData(i).toInt() == (int)voxel_drive_mode)
        {
            voxel_drive_combo_->setCurrentIndex(i);
            break;
        }
    }
    voxel_drive_row->addWidget(voxel_drive_combo_, 1);
    voxel_layout->addLayout(voxel_drive_row);

    root->addWidget(voxel_volume_group_);
}
