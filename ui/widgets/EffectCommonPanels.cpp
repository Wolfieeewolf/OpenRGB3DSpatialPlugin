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

#include "ui_EffectMotionPanel.h"

#include <QCheckBox>
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
    ui->speedLabel->setText(QString::number(speed));
    ui->brightnessSlider->setValue((int)brightness);
    ui->brightnessLabel->setText(QString::number(brightness));
    ui->frequencySlider->setValue((int)frequency);
    ui->frequencyLabel->setText(QString::number(frequency));
    ui->detailSlider->setValue((int)detail);
    ui->detailLabel->setText(QString::number(detail));
    ui->sizeSlider->setValue((int)size);
    ui->sizeLabel->setText(QString::number(size));
    ui->scaleSlider->setValue((int)scale);
    ui->scaleLabel->setText(QString::number(scale));
    ui->scaleInvertCheck->setChecked(scale_inverted);
    ui->fpsSlider->setValue((int)fps);
    ui->fpsLabel->setText(QString::number(fps));
}

EffectMotionPanel::~EffectMotionPanel()
{
    delete ui;
}

QSlider* EffectMotionPanel::speedSlider() const { return ui->speedSlider; }
QLabel* EffectMotionPanel::speedLabel() const { return ui->speedLabel; }
QSlider* EffectMotionPanel::brightnessSlider() const { return ui->brightnessSlider; }
QLabel* EffectMotionPanel::brightnessLabel() const { return ui->brightnessLabel; }
QSlider* EffectMotionPanel::frequencySlider() const { return ui->frequencySlider; }
QLabel* EffectMotionPanel::frequencyLabel() const { return ui->frequencyLabel; }
QSlider* EffectMotionPanel::detailSlider() const { return ui->detailSlider; }
QLabel* EffectMotionPanel::detailLabel() const { return ui->detailLabel; }
QSlider* EffectMotionPanel::sizeSlider() const { return ui->sizeSlider; }
QLabel* EffectMotionPanel::sizeLabel() const { return ui->sizeLabel; }
QSlider* EffectMotionPanel::scaleSlider() const { return ui->scaleSlider; }
QLabel* EffectMotionPanel::scaleLabel() const { return ui->scaleLabel; }
QCheckBox* EffectMotionPanel::scaleInvertCheck() const { return ui->scaleInvertCheck; }
QSlider* EffectMotionPanel::fpsSlider() const { return ui->fpsSlider; }
QLabel* EffectMotionPanel::fpsLabel() const { return ui->fpsLabel; }
#include "EffectOutputPanel.h"

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
    ui->intensityLabel->setText(QString::number(intensity));
    ui->sharpnessSlider->setValue((int)sharpness);
    ui->sharpnessLabel->setText(QString::number(sharpness));
    ui->smoothingSlider->setValue((int)smoothing);
    ui->smoothingLabel->setText(QString::number(smoothing));
    ui->samplingResolutionSlider->setValue((int)sampling_resolution);
    ui->samplingResolutionLabel->setText(QString::number((int)sampling_resolution));
}

EffectOutputPanel::~EffectOutputPanel()
{
    delete ui;
}

QSlider* EffectOutputPanel::intensitySlider() const { return ui->intensitySlider; }
QLabel* EffectOutputPanel::intensityLabel() const { return ui->intensityLabel; }
QSlider* EffectOutputPanel::sharpnessSlider() const { return ui->sharpnessSlider; }
QLabel* EffectOutputPanel::sharpnessLabel() const { return ui->sharpnessLabel; }
QSlider* EffectOutputPanel::smoothingSlider() const { return ui->smoothingSlider; }
QLabel* EffectOutputPanel::smoothingLabel() const { return ui->smoothingLabel; }
QSlider* EffectOutputPanel::samplingResolutionSlider() const { return ui->samplingResolutionSlider; }
QLabel* EffectOutputPanel::samplingResolutionLabel() const { return ui->samplingResolutionLabel; }
#include "EffectGeometryPanel.h"

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
    ui->scaleXLabel->setText(QString::number(scale_x) + QStringLiteral("%"));
    ui->scaleYSlider->setValue((int)scale_y);
    ui->scaleYLabel->setText(QString::number(scale_y) + QStringLiteral("%"));
    ui->scaleZSlider->setValue((int)scale_z);
    ui->scaleZLabel->setText(QString::number(scale_z) + QStringLiteral("%"));

    ui->axisScaleRotYawSlider->setValue((int)axis_scale_rot_yaw);
    ui->axisScaleRotYawLabel->setText(QString::number((int)axis_scale_rot_yaw) + QChar(0x00B0));
    ui->axisScaleRotPitchSlider->setValue((int)axis_scale_rot_pitch);
    ui->axisScaleRotPitchLabel->setText(QString::number((int)axis_scale_rot_pitch) + QChar(0x00B0));
    ui->axisScaleRotRollSlider->setValue((int)axis_scale_rot_roll);
    ui->axisScaleRotRollLabel->setText(QString::number((int)axis_scale_rot_roll) + QChar(0x00B0));

    ui->offsetXSlider->setValue(offset_x);
    ui->offsetXLabel->setText(QString::number(offset_x) + QStringLiteral("%"));
    ui->offsetYSlider->setValue(offset_y);
    ui->offsetYLabel->setText(QString::number(offset_y) + QStringLiteral("%"));
    ui->offsetZSlider->setValue(offset_z);
    ui->offsetZLabel->setText(QString::number(offset_z) + QStringLiteral("%"));

    ui->rotationYawSlider->setValue((int)rotation_yaw);
    ui->rotationYawLabel->setText(QString::number((int)rotation_yaw) + QChar(0x00B0));
    ui->rotationPitchSlider->setValue((int)rotation_pitch);
    ui->rotationPitchLabel->setText(QString::number((int)rotation_pitch) + QChar(0x00B0));
    ui->rotationRollSlider->setValue((int)rotation_roll);
    ui->rotationRollLabel->setText(QString::number((int)rotation_roll) + QChar(0x00B0));
}

EffectGeometryPanel::~EffectGeometryPanel()
{
    delete ui;
}

QSlider* EffectGeometryPanel::scaleXSlider() const { return ui->scaleXSlider; }
QLabel* EffectGeometryPanel::scaleXLabel() const { return ui->scaleXLabel; }
QSlider* EffectGeometryPanel::scaleYSlider() const { return ui->scaleYSlider; }
QLabel* EffectGeometryPanel::scaleYLabel() const { return ui->scaleYLabel; }
QSlider* EffectGeometryPanel::scaleZSlider() const { return ui->scaleZSlider; }
QLabel* EffectGeometryPanel::scaleZLabel() const { return ui->scaleZLabel; }
QPushButton* EffectGeometryPanel::axisScaleResetButton() const { return ui->axisScaleResetButton; }

QSlider* EffectGeometryPanel::axisScaleRotYawSlider() const { return ui->axisScaleRotYawSlider; }
QLabel* EffectGeometryPanel::axisScaleRotYawLabel() const { return ui->axisScaleRotYawLabel; }
QSlider* EffectGeometryPanel::axisScaleRotPitchSlider() const { return ui->axisScaleRotPitchSlider; }
QLabel* EffectGeometryPanel::axisScaleRotPitchLabel() const { return ui->axisScaleRotPitchLabel; }
QSlider* EffectGeometryPanel::axisScaleRotRollSlider() const { return ui->axisScaleRotRollSlider; }
QLabel* EffectGeometryPanel::axisScaleRotRollLabel() const { return ui->axisScaleRotRollLabel; }
QPushButton* EffectGeometryPanel::axisScaleRotResetButton() const { return ui->axisScaleRotResetButton; }

QGroupBox* EffectGeometryPanel::positionOffsetGroup() const { return ui->positionOffsetGroup; }
QSlider* EffectGeometryPanel::offsetXSlider() const { return ui->offsetXSlider; }
QLabel* EffectGeometryPanel::offsetXLabel() const { return ui->offsetXLabel; }
QSlider* EffectGeometryPanel::offsetYSlider() const { return ui->offsetYSlider; }
QLabel* EffectGeometryPanel::offsetYLabel() const { return ui->offsetYLabel; }
QSlider* EffectGeometryPanel::offsetZSlider() const { return ui->offsetZSlider; }
QLabel* EffectGeometryPanel::offsetZLabel() const { return ui->offsetZLabel; }
QPushButton* EffectGeometryPanel::offsetCenterResetButton() const { return ui->offsetCenterResetButton; }

QSlider* EffectGeometryPanel::rotationYawSlider() const { return ui->rotationYawSlider; }
QLabel* EffectGeometryPanel::rotationYawLabel() const { return ui->rotationYawLabel; }
QSlider* EffectGeometryPanel::rotationPitchSlider() const { return ui->rotationPitchSlider; }
QLabel* EffectGeometryPanel::rotationPitchLabel() const { return ui->rotationPitchLabel; }
QSlider* EffectGeometryPanel::rotationRollSlider() const { return ui->rotationRollSlider; }
QLabel* EffectGeometryPanel::rotationRollLabel() const { return ui->rotationRollLabel; }
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
