// SPDX-License-Identifier: GPL-2.0-only

#include "EffectSamplerPanel.h"
#include "PluginUiUtils.h"
#include "ui_EffectSamplerPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>

#include <algorithm>

EffectSamplerPanel::EffectSamplerPanel(SpatialMappingMode spatial_mapping_mode,
                                       int compass_layer_spin_preset,
                                       int sampler_influence_centi,
                                       int compass_north_offset_deg,
                                       bool compass_discrete_zones,
                                       unsigned int voxel_volume_mix,
                                       int voxel_room_scale_centi,
                                       int voxel_heading_offset,
                                       VoxelDriveMode voxel_drive_mode,
                                       QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectSamplerPanel)
{
    ui->setupUi(this);
    PluginUiApplyMutedSecondaryLabel(ui->introLabel->label());
    populateCombos(spatial_mapping_mode, voxel_drive_mode);
    applyInitialValues(compass_layer_spin_preset,
                       sampler_influence_centi,
                       compass_north_offset_deg,
                       compass_discrete_zones,
                       voxel_volume_mix,
                       voxel_room_scale_centi,
                       voxel_heading_offset);
}

EffectSamplerPanel::~EffectSamplerPanel()
{
    delete ui;
}

QGroupBox* EffectSamplerPanel::compassSamplerGroup() const { return ui->compassSamplerGroup; }
QComboBox* EffectSamplerPanel::spatialMappingCombo() const { return ui->spatialMappingCombo; }
QComboBox* EffectSamplerPanel::compassLayerSpinCombo() const { return ui->compassLayerSpinCombo; }
QSlider* EffectSamplerPanel::samplerInfluenceSlider() const { return ui->samplerInfluenceSlider; }
QLabel* EffectSamplerPanel::samplerInfluenceLabel() const { return ui->samplerInfluenceLabel; }
QSlider* EffectSamplerPanel::samplerCompassNorthSlider() const { return ui->samplerCompassNorthSlider; }
QLabel* EffectSamplerPanel::samplerCompassNorthLabel() const { return ui->samplerCompassNorthLabel; }
QCheckBox* EffectSamplerPanel::compassDiscreteZonesCheck() const { return ui->compassDiscreteZonesCheck; }
QGroupBox* EffectSamplerPanel::voxelVolumeGroup() const { return ui->voxelVolumeGroup; }
QSlider* EffectSamplerPanel::voxelVolumeMixSlider() const { return ui->voxelVolumeMixSlider; }
QLabel* EffectSamplerPanel::voxelVolumeMixLabel() const { return ui->voxelVolumeMixLabel; }
QSlider* EffectSamplerPanel::voxelVolumeScaleSlider() const { return ui->voxelVolumeScaleSlider; }
QLabel* EffectSamplerPanel::voxelVolumeScaleLabel() const { return ui->voxelVolumeScaleLabel; }
QSlider* EffectSamplerPanel::voxelVolumeHeadingSlider() const { return ui->voxelVolumeHeadingSlider; }
QLabel* EffectSamplerPanel::voxelVolumeHeadingLabel() const { return ui->voxelVolumeHeadingLabel; }
QComboBox* EffectSamplerPanel::voxelDriveCombo() const { return ui->voxelDriveCombo; }

void EffectSamplerPanel::populateCombos(SpatialMappingMode spatial_mapping_mode, VoxelDriveMode voxel_drive_mode)
{
    ui->spatialMappingCombo->addItem(QStringLiteral("Off — flat (no direction layers)"),
                                     (int)SpatialMappingMode::Off);
    ui->spatialMappingCombo->addItem(QStringLiteral("Subtle — gentle crawl with position"),
                                     (int)SpatialMappingMode::SubtleTint);
    ui->spatialMappingCombo->addItem(QStringLiteral("Compass — sectors + height bands drive palette"),
                                     (int)SpatialMappingMode::CompassPalette);
    ui->spatialMappingCombo->addItem(QStringLiteral("Voxel volume — full room grid (reference still biases)"),
                                     (int)SpatialMappingMode::VoxelVolume);
    ui->spatialMappingCombo->setToolTip(
        QStringLiteral("Subtle: soft palette crawl from position. Compass: sectors + floor/mid/ceiling bands; each band uses a compass hub at that height through your reference X/Z. "
                       "Voxel volume: palette varies across the whole room; reference point still nudges mapping."));
    for(int i = 0; i < ui->spatialMappingCombo->count(); i++)
    {
        if(ui->spatialMappingCombo->itemData(i).toInt() == (int)spatial_mapping_mode)
        {
            ui->spatialMappingCombo->setCurrentIndex(i);
            break;
        }
    }

    ui->compassLayerSpinCombo->addItem(QStringLiteral("All bands clockwise"), 0);
    ui->compassLayerSpinCombo->addItem(QStringLiteral("All bands counter-clockwise"), 1);
    ui->compassLayerSpinCombo->addItem(QStringLiteral("Floor CW · mid CCW · ceiling CW"), 2);
    ui->compassLayerSpinCombo->addItem(QStringLiteral("Floor CCW · mid CW · ceiling CCW"), 3);
    ui->compassLayerSpinCombo->setToolTip(
        QStringLiteral("Per vertical band spin direction. Used only in Compass mode."));

    ui->samplerInfluenceSlider->setToolTip(
        QStringLiteral("0 = off. 100 = normal. Higher values increase room/voxel influence."));
    ui->samplerCompassNorthSlider->setToolTip(
        QStringLiteral("Rotates which compass sector counts as \"forward\" for mapping (degrees). Align sectors with your real room front."));
    ui->voxelVolumeMixSlider->setToolTip(
        QStringLiteral("Blend sampled voxel RGB into the effect (0 = off). Requires live voxel_frame."));
    ui->voxelVolumeScaleSlider->setToolTip(
        QStringLiteral("Minecraft / sender units per room step (×0.01). Fix stretched or offset blocks."));
    ui->voxelVolumeHeadingSlider->setToolTip(QStringLiteral("Yaw offset when projecting the volume into the room."));

    ui->voxelDriveCombo->addItem(QStringLiteral("None"), (int)VoxelDriveMode::Off);
    ui->voxelDriveCombo->addItem(QStringLiteral("From voxel brightness"), (int)VoxelDriveMode::LumaField);
    ui->voxelDriveCombo->addItem(QStringLiteral("Scroll with room X"), (int)VoxelDriveMode::ScrollRoomX);
    ui->voxelDriveCombo->addItem(QStringLiteral("Scroll with room Y"), (int)VoxelDriveMode::ScrollRoomY);
    ui->voxelDriveCombo->addItem(QStringLiteral("Scroll with room Z"), (int)VoxelDriveMode::ScrollRoomZ);
    ui->voxelDriveCombo->addItem(QStringLiteral("Roll (angle × voxel)"), (int)VoxelDriveMode::VolumeRoll);
    ui->voxelDriveCombo->setToolTip(
        QStringLiteral("Shifts rainbow / color stops along the LED (wheel rolling through space). Uses voxels when relevant; room axes work without a game."));
    for(int i = 0; i < ui->voxelDriveCombo->count(); i++)
    {
        if(ui->voxelDriveCombo->itemData(i).toInt() == (int)voxel_drive_mode)
        {
            ui->voxelDriveCombo->setCurrentIndex(i);
            break;
        }
    }
}

void EffectSamplerPanel::applyInitialValues(int compass_layer_spin_preset,
                                            int sampler_influence_centi,
                                            int compass_north_offset_deg,
                                            bool compass_discrete_zones,
                                            unsigned int voxel_volume_mix,
                                            int voxel_room_scale_centi,
                                            int voxel_heading_offset)
{
    ui->compassLayerSpinCombo->setCurrentIndex(std::clamp(compass_layer_spin_preset, 0, 3));
    ui->samplerInfluenceSlider->setValue(sampler_influence_centi);
    ui->samplerInfluenceLabel->setText(QString::number(sampler_influence_centi) + QStringLiteral("%"));
    ui->samplerCompassNorthSlider->setValue(compass_north_offset_deg);
    ui->samplerCompassNorthLabel->setText(QString::number(compass_north_offset_deg) + QStringLiteral("°"));
    ui->compassDiscreteZonesCheck->setChecked(compass_discrete_zones);
    ui->voxelVolumeMixSlider->setValue((int)voxel_volume_mix);
    ui->voxelVolumeMixLabel->setText(QString::number((int)voxel_volume_mix) + QStringLiteral("%"));
    ui->voxelVolumeScaleSlider->setValue(voxel_room_scale_centi);
    ui->voxelVolumeScaleLabel->setText(QString::number(voxel_room_scale_centi / 100.0f, 'f', 2));
    ui->voxelVolumeHeadingSlider->setValue(voxel_heading_offset);
    ui->voxelVolumeHeadingLabel->setText(QString::number(voxel_heading_offset) + QStringLiteral("°"));
}
