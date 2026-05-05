// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSAMPLERPANEL_H
#define EFFECTSAMPLERPANEL_H

#include "SpatialEffectTypes.h"

#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QSlider;

/** Room sampler: compass palette, strength, voxel volume controls. */
class EffectSamplerPanel : public QWidget
{
public:
    EffectSamplerPanel(SpatialMappingMode spatial_mapping_mode,
                                      int compass_layer_spin_preset,
                                      int sampler_influence_centi,
                                      int compass_north_offset_deg,
                                      unsigned int voxel_volume_mix,
                                      int voxel_room_scale_centi,
                                      int voxel_heading_offset,
                                      VoxelDriveMode voxel_drive_mode,
                                      QWidget* parent = nullptr);

    QGroupBox* compassSamplerGroup() const { return compass_sampler_group_; }
    QComboBox* spatialMappingCombo() const { return spatial_mapping_combo_; }
    QComboBox* compassLayerSpinCombo() const { return compass_layer_spin_combo_; }
    QSlider* samplerInfluenceSlider() const { return sampler_influence_slider_; }
    QLabel* samplerInfluenceLabel() const { return sampler_influence_label_; }
    QSlider* samplerCompassNorthSlider() const { return sampler_compass_north_slider_; }
    QLabel* samplerCompassNorthLabel() const { return sampler_compass_north_label_; }
    QGroupBox* voxelVolumeGroup() const { return voxel_volume_group_; }
    QSlider* voxelVolumeMixSlider() const { return voxel_volume_mix_slider_; }
    QLabel* voxelVolumeMixLabel() const { return voxel_volume_mix_label_; }
    QSlider* voxelVolumeScaleSlider() const { return voxel_volume_scale_slider_; }
    QLabel* voxelVolumeScaleLabel() const { return voxel_volume_scale_label_; }
    QSlider* voxelVolumeHeadingSlider() const { return voxel_volume_heading_slider_; }
    QLabel* voxelVolumeHeadingLabel() const { return voxel_volume_heading_label_; }
    QComboBox* voxelDriveCombo() const { return voxel_drive_combo_; }

private:
    QGroupBox* compass_sampler_group_ = nullptr;
    QComboBox* spatial_mapping_combo_ = nullptr;
    QComboBox* compass_layer_spin_combo_ = nullptr;
    QSlider* sampler_influence_slider_ = nullptr;
    QLabel* sampler_influence_label_ = nullptr;
    QSlider* sampler_compass_north_slider_ = nullptr;
    QLabel* sampler_compass_north_label_ = nullptr;
    QGroupBox* voxel_volume_group_ = nullptr;
    QSlider* voxel_volume_mix_slider_ = nullptr;
    QLabel* voxel_volume_mix_label_ = nullptr;
    QSlider* voxel_volume_scale_slider_ = nullptr;
    QLabel* voxel_volume_scale_label_ = nullptr;
    QSlider* voxel_volume_heading_slider_ = nullptr;
    QLabel* voxel_volume_heading_label_ = nullptr;
    QComboBox* voxel_drive_combo_ = nullptr;
};

#endif
