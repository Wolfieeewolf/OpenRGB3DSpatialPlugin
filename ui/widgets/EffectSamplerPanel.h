// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSAMPLERPANEL_H
#define EFFECTSAMPLERPANEL_H

#include "SpatialEffectTypes.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QSlider;

namespace Ui {
class EffectSamplerPanel;
}

class EffectSamplerPanel : public QWidget
{
public:
    EffectSamplerPanel(SpatialMappingMode spatial_mapping_mode,
                       int compass_layer_spin_preset,
                       int sampler_influence_centi,
                       int compass_north_offset_deg,
                       bool compass_discrete_zones,
                       unsigned int voxel_volume_mix,
                       int voxel_room_scale_centi,
                       int voxel_heading_offset,
                       VoxelDriveMode voxel_drive_mode,
                       QWidget* parent = nullptr);
    ~EffectSamplerPanel() override;

    QGroupBox* compassSamplerGroup() const;
    QComboBox* spatialMappingCombo() const;
    QComboBox* compassLayerSpinCombo() const;
    QSlider* samplerInfluenceSlider() const;
    QLabel* samplerInfluenceLabel() const;
    QSlider* samplerCompassNorthSlider() const;
    QLabel* samplerCompassNorthLabel() const;
    QCheckBox* compassDiscreteZonesCheck() const;
    QGroupBox* voxelVolumeGroup() const;
    QSlider* voxelVolumeMixSlider() const;
    QLabel* voxelVolumeMixLabel() const;
    QSlider* voxelVolumeScaleSlider() const;
    QLabel* voxelVolumeScaleLabel() const;
    QSlider* voxelVolumeHeadingSlider() const;
    QLabel* voxelVolumeHeadingLabel() const;
    QComboBox* voxelDriveCombo() const;

private:
    void populateCombos(SpatialMappingMode spatial_mapping_mode, VoxelDriveMode voxel_drive_mode);
    void applyInitialValues(int compass_layer_spin_preset,
                            int sampler_influence_centi,
                            int compass_north_offset_deg,
                            bool compass_discrete_zones,
                            unsigned int voxel_volume_mix,
                            int voxel_room_scale_centi,
                            int voxel_heading_offset);

    Ui::EffectSamplerPanel* ui = nullptr;
};

#endif
