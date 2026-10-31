// SPDX-License-Identifier: GPL-2.0-only

#include "EffectControlsRoot.h"

#include <QVBoxLayout>

EffectControlsRoot::EffectControlsRoot(QWidget* parent)
    : QWidget(parent)
    , main_layout_(new QVBoxLayout(this))
{
    main_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout_->setSpacing(6);
}

EffectControlsRoot::~EffectControlsRoot() = default;

QVBoxLayout* EffectControlsRoot::mainLayout() const
{
    return main_layout_;
}
#include "EffectLayerBanner.h"

#include "ui_EffectLayerBanner.h"

#include <QFont>
#include <QPushButton>

EffectLayerBanner::EffectLayerBanner(bool include_start_stop, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectLayerBanner)
{
    ui->setupUi(this);

    QFont hf = ui->layerHeadingLabel->font();
    hf.setBold(true);
    ui->layerHeadingLabel->setFont(hf);

    ui->transportContainer->setVisible(include_start_stop);
}

EffectLayerBanner::~EffectLayerBanner()
{
    delete ui;
}

QPushButton* EffectLayerBanner::startEffectButton() const
{
    return ui->startEffectButton;
}

QPushButton* EffectLayerBanner::stopEffectButton() const
{
    return ui->stopEffectButton;
}
#include "EffectSurfacesPanel.h"

#include "SpatialEffect3D.h"
#include "SpatialEffectTypes.h"
#include "ui_EffectSurfacesPanel.h"

#include <QCheckBox>

EffectSurfacesPanel::EffectSurfacesPanel(int surface_mask, SpatialEffect3D* host, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectSurfacesPanel)
{
    ui->setupUi(this);

    if(!host)
    {
        return;
    }

    ui->surfaceFloorCheck->setChecked((surface_mask & SURF_FLOOR) != 0);
    connect(ui->surfaceFloorCheck, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_FLOOR, on); });

    ui->surfaceCeilingCheck->setChecked((surface_mask & SURF_CEIL) != 0);
    connect(ui->surfaceCeilingCheck, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_CEIL, on); });

    ui->surfaceWallXmCheck->setChecked((surface_mask & SURF_WALL_XM) != 0);
    connect(ui->surfaceWallXmCheck, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_XM, on); });

    ui->surfaceWallXpCheck->setChecked((surface_mask & SURF_WALL_XP) != 0);
    connect(ui->surfaceWallXpCheck, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_XP, on); });

    ui->surfaceWallZmCheck->setChecked((surface_mask & SURF_WALL_ZM) != 0);
    connect(ui->surfaceWallZmCheck, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_ZM, on); });

    ui->surfaceWallZpCheck->setChecked((surface_mask & SURF_WALL_ZP) != 0);
    connect(ui->surfaceWallZpCheck, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_ZP, on); });
}

EffectSurfacesPanel::~EffectSurfacesPanel()
{
    delete ui;
}
#include "EffectMotionPanel.h"

#include "EffectSliderPromote.h"
#include "ui_EffectMotionPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>

EffectMotionPanel::EffectMotionPanel(unsigned int speed,
                                     unsigned int brightness,
                                     unsigned int frequency,
                                     unsigned int detail,
                                     unsigned int size,
                                     unsigned int scale,
                                     bool scale_inverted,
                                     unsigned int fps,
                                     QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectMotionPanel)
{
    ui->setupUi(this);

    ui->speedSlider->setValue((int)speed);
    ui->brightnessSlider->setValue((int)brightness);
    ui->frequencySlider->setValue((int)frequency);
    ui->detailSlider->setValue((int)detail);
    ui->sizeSlider->setValue((int)size);
    ui->scaleSlider->setValue((int)scale);
    ui->scaleInvertCheck->setChecked(scale_inverted);
    ui->fpsSlider->setValue((int)fps);
    ui->pathAxisCombo->addItem(QStringLiteral("X (left/right)"));
    ui->pathAxisCombo->addItem(QStringLiteral("Y (up/down)"));
    ui->pathAxisCombo->addItem(QStringLiteral("Z (depth)"));
    ui->pathAxisCombo->setCurrentIndex(1);
    ui->thicknessSlider->setValue(15);
    ui->edgeFadeSlider->setValue(0);
    ui->countSlider->setValue(12);

    speed_row_ = PromoteSliderLabelToEffectRow(ui->speedRow, ui->speedSlider, ui->speedLabel);
    brightness_row_ = PromoteSliderLabelToEffectRow(ui->brightnessRow, ui->brightnessSlider, ui->brightnessLabel);
    frequency_row_ = PromoteSliderLabelToEffectRow(ui->frequencyRow, ui->frequencySlider, ui->frequencyLabel);
    detail_row_ = PromoteSliderLabelToEffectRow(ui->detailRow, ui->detailSlider, ui->detailLabel);
    size_row_ = PromoteSliderLabelToEffectRow(ui->sizeRow, ui->sizeSlider, ui->sizeLabel);
    scale_row_ = PromoteSliderLabelToEffectRow(ui->scaleRow, ui->scaleSlider, ui->scaleLabel);
    fps_row_ = PromoteSliderLabelToEffectRow(ui->fpsRow, ui->fpsSlider, ui->fpsLabel);
    thickness_row_ = PromoteSliderLabelToEffectRow(ui->thicknessRow, ui->thicknessSlider, ui->thicknessLabel,
                                                    QStringLiteral("%"));
    edge_fade_row_ = PromoteSliderLabelToEffectRow(ui->edgeFadeRow, ui->edgeFadeSlider, ui->edgeFadeLabel,
                                                     QStringLiteral("%"));
    count_row_ = PromoteSliderLabelToEffectRow(ui->countRow, ui->countSlider, ui->countLabel);

    AlignCaptionLabels({ui->speedCaption, ui->brightnessCaption, ui->frequencyCaption, ui->detailCaption,
                        ui->sizeCaption, ui->scaleCaption, ui->fpsCaption, ui->thicknessCaption,
                        ui->edgeFadeCaption, ui->countCaption, ui->pathCaption},
                       110);
}

EffectMotionPanel::~EffectMotionPanel()
{
    delete ui;
}

QSlider* EffectMotionPanel::speedSlider() const { return speed_row_ ? speed_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::speedLabel() const { return speed_row_ ? speed_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::brightnessSlider() const { return brightness_row_ ? brightness_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::brightnessLabel() const { return brightness_row_ ? brightness_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::frequencySlider() const { return frequency_row_ ? frequency_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::frequencyLabel() const { return frequency_row_ ? frequency_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::detailSlider() const { return detail_row_ ? detail_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::detailLabel() const { return detail_row_ ? detail_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::sizeSlider() const { return size_row_ ? size_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::sizeLabel() const { return size_row_ ? size_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::scaleSlider() const { return scale_row_ ? scale_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::scaleLabel() const { return scale_row_ ? scale_row_->valueLabel() : nullptr; }
QCheckBox* EffectMotionPanel::scaleInvertCheck() const { return ui->scaleInvertCheck; }
QSlider* EffectMotionPanel::fpsSlider() const { return fps_row_ ? fps_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::fpsLabel() const { return fps_row_ ? fps_row_->valueLabel() : nullptr; }
QComboBox* EffectMotionPanel::pathAxisCombo() const { return ui->pathAxisCombo; }
QSlider* EffectMotionPanel::thicknessSlider() const { return thickness_row_ ? thickness_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::thicknessLabel() const { return thickness_row_ ? thickness_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::edgeFadeSlider() const { return edge_fade_row_ ? edge_fade_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::edgeFadeLabel() const { return edge_fade_row_ ? edge_fade_row_->valueLabel() : nullptr; }
QSlider* EffectMotionPanel::countSlider() const { return count_row_ ? count_row_->slider() : nullptr; }
QLabel* EffectMotionPanel::countLabel() const { return count_row_ ? count_row_->valueLabel() : nullptr; }
#include "EffectOutputPanel.h"

#include "EffectSliderPromote.h"
#include "ui_EffectOutputPanel.h"

#include <QLabel>
#include <QSlider>

EffectOutputPanel::EffectOutputPanel(unsigned int intensity,
                                     unsigned int sharpness,
                                     unsigned int smoothing,
                                     unsigned int sampling_resolution,
                                     QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectOutputPanel)
{
    ui->setupUi(this);

    ui->intensitySlider->setValue((int)intensity);
    ui->sharpnessSlider->setValue((int)sharpness);
    ui->smoothingSlider->setValue((int)smoothing);
    ui->samplingResolutionSlider->setValue((int)sampling_resolution);

    intensity_row_ = PromoteSliderLabelToEffectRow(ui->intensityRow, ui->intensitySlider, ui->intensityLabel);
    sharpness_row_ = PromoteSliderLabelToEffectRow(ui->sharpnessRow, ui->sharpnessSlider, ui->sharpnessLabel);
    smoothing_row_ = PromoteSliderLabelToEffectRow(ui->smoothingRow, ui->smoothingSlider, ui->smoothingLabel);
    sampling_row_ = PromoteSliderLabelToEffectRow(ui->samplingRow, ui->samplingResolutionSlider,
                                                    ui->samplingResolutionLabel);

    AlignCaptionLabels(
        {ui->intensityCaption, ui->sharpnessCaption, ui->smoothingCaption, ui->samplingCaption}, 110);
}

EffectOutputPanel::~EffectOutputPanel()
{
    delete ui;
}

QSlider* EffectOutputPanel::intensitySlider() const { return intensity_row_ ? intensity_row_->slider() : nullptr; }
QLabel* EffectOutputPanel::intensityLabel() const { return intensity_row_ ? intensity_row_->valueLabel() : nullptr; }
QSlider* EffectOutputPanel::sharpnessSlider() const { return sharpness_row_ ? sharpness_row_->slider() : nullptr; }
QLabel* EffectOutputPanel::sharpnessLabel() const { return sharpness_row_ ? sharpness_row_->valueLabel() : nullptr; }
QSlider* EffectOutputPanel::smoothingSlider() const { return smoothing_row_ ? smoothing_row_->slider() : nullptr; }
QLabel* EffectOutputPanel::smoothingLabel() const { return smoothing_row_ ? smoothing_row_->valueLabel() : nullptr; }
QSlider* EffectOutputPanel::samplingResolutionSlider() const
{
    return sampling_row_ ? sampling_row_->slider() : nullptr;
}
QLabel* EffectOutputPanel::samplingResolutionLabel() const
{
    return sampling_row_ ? sampling_row_->valueLabel() : nullptr;
}
#include "EffectGeometryPanel.h"

#include "EffectSliderPromote.h"
#include "ui_EffectGeometryPanel.h"

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

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
    , ui(new Ui::EffectGeometryPanel)
{
    ui->setupUi(this);

    ui->scaleXSlider->setValue((int)scale_x);
    ui->scaleYSlider->setValue((int)scale_y);
    ui->scaleZSlider->setValue((int)scale_z);
    ui->axisScaleRotYawSlider->setValue((int)axis_scale_rot_yaw);
    ui->axisScaleRotPitchSlider->setValue((int)axis_scale_rot_pitch);
    ui->axisScaleRotRollSlider->setValue((int)axis_scale_rot_roll);
    ui->offsetXSlider->setValue(offset_x);
    ui->offsetYSlider->setValue(offset_y);
    ui->offsetZSlider->setValue(offset_z);
    ui->rotationYawSlider->setValue((int)rotation_yaw);
    ui->rotationPitchSlider->setValue((int)rotation_pitch);
    ui->rotationRollSlider->setValue((int)rotation_roll);

    const QString pct = QStringLiteral("%");
    const QString deg = QString(QChar(0x00B0));

    scale_x_row_ = PromoteSliderLabelToEffectRow(ui->scaleXRow, ui->scaleXSlider, ui->scaleXLabel, pct);
    scale_y_row_ = PromoteSliderLabelToEffectRow(ui->scaleYRow, ui->scaleYSlider, ui->scaleYLabel, pct);
    scale_z_row_ = PromoteSliderLabelToEffectRow(ui->scaleZRow, ui->scaleZSlider, ui->scaleZLabel, pct);
    asr_yaw_row_ = PromoteSliderLabelToEffectRow(ui->asrYawRow, ui->axisScaleRotYawSlider,
                                                   ui->axisScaleRotYawLabel, deg);
    asr_pitch_row_ = PromoteSliderLabelToEffectRow(ui->asrPitchRow, ui->axisScaleRotPitchSlider,
                                                     ui->axisScaleRotPitchLabel, deg);
    asr_roll_row_ = PromoteSliderLabelToEffectRow(ui->asrRollRow, ui->axisScaleRotRollSlider,
                                                    ui->axisScaleRotRollLabel, deg);
    offset_x_row_ = PromoteSliderLabelToEffectRow(ui->offsetXRow, ui->offsetXSlider, ui->offsetXLabel, pct);
    offset_y_row_ = PromoteSliderLabelToEffectRow(ui->offsetYRow, ui->offsetYSlider, ui->offsetYLabel, pct);
    offset_z_row_ = PromoteSliderLabelToEffectRow(ui->offsetZRow, ui->offsetZSlider, ui->offsetZLabel, pct);
    rot_yaw_row_ = PromoteSliderLabelToEffectRow(ui->rotYawRow, ui->rotationYawSlider, ui->rotationYawLabel, deg);
    rot_pitch_row_ = PromoteSliderLabelToEffectRow(ui->rotPitchRow, ui->rotationPitchSlider,
                                                     ui->rotationPitchLabel, deg);
    rot_roll_row_ = PromoteSliderLabelToEffectRow(ui->rotRollRow, ui->rotationRollSlider,
                                                    ui->rotationRollLabel, deg);

    AlignCaptionLabels({ui->scaleXCaption, ui->scaleYCaption, ui->scaleZCaption}, 72);
    AlignCaptionLabels({ui->asrYawCaption, ui->asrPitchCaption, ui->asrRollCaption}, 72);
    AlignCaptionLabels({ui->offsetXCaption, ui->offsetYCaption, ui->offsetZCaption}, 72);
    AlignCaptionLabels({ui->rotYawCaption, ui->rotPitchCaption, ui->rotRollCaption}, 72);
}

EffectGeometryPanel::~EffectGeometryPanel()
{
    delete ui;
}

QSlider* EffectGeometryPanel::scaleXSlider() const { return scale_x_row_ ? scale_x_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::scaleXLabel() const { return scale_x_row_ ? scale_x_row_->valueLabel() : nullptr; }
QSlider* EffectGeometryPanel::scaleYSlider() const { return scale_y_row_ ? scale_y_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::scaleYLabel() const { return scale_y_row_ ? scale_y_row_->valueLabel() : nullptr; }
QSlider* EffectGeometryPanel::scaleZSlider() const { return scale_z_row_ ? scale_z_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::scaleZLabel() const { return scale_z_row_ ? scale_z_row_->valueLabel() : nullptr; }
QPushButton* EffectGeometryPanel::axisScaleResetButton() const { return ui->axisScaleResetButton; }

QSlider* EffectGeometryPanel::axisScaleRotYawSlider() const
{
    return asr_yaw_row_ ? asr_yaw_row_->slider() : nullptr;
}
QLabel* EffectGeometryPanel::axisScaleRotYawLabel() const
{
    return asr_yaw_row_ ? asr_yaw_row_->valueLabel() : nullptr;
}
QSlider* EffectGeometryPanel::axisScaleRotPitchSlider() const
{
    return asr_pitch_row_ ? asr_pitch_row_->slider() : nullptr;
}
QLabel* EffectGeometryPanel::axisScaleRotPitchLabel() const
{
    return asr_pitch_row_ ? asr_pitch_row_->valueLabel() : nullptr;
}
QSlider* EffectGeometryPanel::axisScaleRotRollSlider() const
{
    return asr_roll_row_ ? asr_roll_row_->slider() : nullptr;
}
QLabel* EffectGeometryPanel::axisScaleRotRollLabel() const
{
    return asr_roll_row_ ? asr_roll_row_->valueLabel() : nullptr;
}
QPushButton* EffectGeometryPanel::axisScaleRotResetButton() const { return ui->axisScaleRotResetButton; }

QGroupBox* EffectGeometryPanel::positionOffsetGroup() const { return ui->positionOffsetGroup; }
QSlider* EffectGeometryPanel::offsetXSlider() const { return offset_x_row_ ? offset_x_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::offsetXLabel() const { return offset_x_row_ ? offset_x_row_->valueLabel() : nullptr; }
QSlider* EffectGeometryPanel::offsetYSlider() const { return offset_y_row_ ? offset_y_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::offsetYLabel() const { return offset_y_row_ ? offset_y_row_->valueLabel() : nullptr; }
QSlider* EffectGeometryPanel::offsetZSlider() const { return offset_z_row_ ? offset_z_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::offsetZLabel() const { return offset_z_row_ ? offset_z_row_->valueLabel() : nullptr; }
QPushButton* EffectGeometryPanel::offsetCenterResetButton() const { return ui->offsetCenterResetButton; }

QSlider* EffectGeometryPanel::rotationYawSlider() const { return rot_yaw_row_ ? rot_yaw_row_->slider() : nullptr; }
QLabel* EffectGeometryPanel::rotationYawLabel() const { return rot_yaw_row_ ? rot_yaw_row_->valueLabel() : nullptr; }
QSlider* EffectGeometryPanel::rotationPitchSlider() const
{
    return rot_pitch_row_ ? rot_pitch_row_->slider() : nullptr;
}
QLabel* EffectGeometryPanel::rotationPitchLabel() const
{
    return rot_pitch_row_ ? rot_pitch_row_->valueLabel() : nullptr;
}
QSlider* EffectGeometryPanel::rotationRollSlider() const
{
    return rot_roll_row_ ? rot_roll_row_->slider() : nullptr;
}
QLabel* EffectGeometryPanel::rotationRollLabel() const
{
    return rot_roll_row_ ? rot_roll_row_->valueLabel() : nullptr;
}
QPushButton* EffectGeometryPanel::rotationResetButton() const { return ui->rotationResetButton; }
#include "EffectColorPanel.h"

#include "ui_EffectColorPanel.h"

#include <QCheckBox>
#include <QPushButton>

EffectColorPanel::EffectColorPanel(bool rainbow_mode, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectColorPanel)
{
    ui->setupUi(this);
    ui->rainbowModeCheck->setChecked(rainbow_mode);
}

EffectColorPanel::~EffectColorPanel()
{
    delete ui;
}

QCheckBox* EffectColorPanel::rainbowModeCheck() const { return ui->rainbowModeCheck; }
QWidget* EffectColorPanel::colorButtonsWidget() const { return ui->colorButtonsWidget; }
QHBoxLayout* EffectColorPanel::colorButtonsLayout() const { return ui->colorButtonsLayout; }
QWidget* EffectColorPanel::patternHostWidget() const { return ui->patternHostWidget; }
QVBoxLayout* EffectColorPanel::patternHostLayout() const { return ui->patternHostLayout; }
QPushButton* EffectColorPanel::addColorButton() const { return ui->addColorButton; }
QPushButton* EffectColorPanel::removeColorButton() const { return ui->removeColorButton; }
