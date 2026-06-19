// SPDX-License-Identifier: GPL-2.0-only

#include "StripKernelColormapPanel.h"
#include "Game/StripPatternSurface.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "ui_StripKernelColormapPanel.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QSlider>

#include <algorithm>
#include <cmath>

const char* StripKernelColormapPanel::UnfoldLabel(int m)
{
    switch(m)
    {
    case 0: return "Along X";
    case 1: return "Along Y";
    case 2: return "Along Z";
    case 3: return "Plane XZ (angled)";
    case 4: return "Radial XZ";
    case 5: return "Diagonal x+y+z";
    case 6: return "Manhattan";
    case 7: return "Effect animation only (no room projection)";
    case 8: return "Static room projection (angle)";
    default: return "Along X";
    }
}

StripKernelColormapPanel::StripKernelColormapPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StripKernelColormapPanel)
{
    ui->setupUi(this);
    populateCombos();

    connect(ui->sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onSourceChanged);
    connect(ui->colorStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onColorStyleChanged);
    connect(ui->kernelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onKernelChanged);
    connect(ui->unfoldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onUnfoldChanged);
    connect(ui->repeatsSlider, &QSlider::valueChanged, this, &StripKernelColormapPanel::onRepeatsChanged);
    connect(ui->dirSlider, &QSlider::valueChanged, this, &StripKernelColormapPanel::onDirChanged);

    refreshSecondaryEnabled();
}

StripKernelColormapPanel::~StripKernelColormapPanel()
{
    delete ui;
}

void StripKernelColormapPanel::populateCombos()
{
    ui->sourceCombo->addItem("Colors only (Rainbow/Stops)");
    ui->sourceCombo->addItem("Colors + Pattern kernel");

    ui->colorStyleCombo->addItem("Pattern palette");
    ui->colorStyleCombo->addItem("Color stops (from Colors)");
    ui->colorStyleCombo->setCurrentIndex(1);

    for(int i = 0; i < SpatialPatternKernelCount(); i++)
    {
        ui->kernelCombo->addItem(SpatialPatternKernelDisplayName(i));
    }

    for(int i = 0; i < (int)StripPatternSurface::UnfoldMode::COUNT; i++)
    {
        ui->unfoldCombo->addItem(UnfoldLabel(i));
    }

    ui->repeatsLabel->setText(QString::number(ui->repeatsSlider->value()));
    ui->dirLabel->setText(QString::number(ui->dirSlider->value()) + QChar(0x00B0));
}

bool StripKernelColormapPanel::useStripColormap() const
{
    return ui->sourceCombo->currentIndex() == 1;
}

int StripKernelColormapPanel::kernelId() const
{
    return std::clamp(ui->kernelCombo->currentIndex(), 0, SpatialPatternKernelCount() - 1);
}

float StripKernelColormapPanel::kernelRepeats() const
{
    return (float)std::max(1, ui->repeatsSlider->value());
}

int StripKernelColormapPanel::unfoldMode() const
{
    return std::clamp(ui->unfoldCombo->currentIndex(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
}

float StripKernelColormapPanel::directionDeg() const
{
    return (float)ui->dirSlider->value();
}

int StripKernelColormapPanel::colorStyle() const
{
    return std::clamp(ui->colorStyleCombo->currentIndex(), 0, 1);
}

void StripKernelColormapPanel::mirrorStateFromEffect(bool on, int kernel, float rep, int unfold, float dir_deg,
                                                     int color_style)
{
    if(!on)
    {
        rep = 1.0f;
        unfold = (int)StripPatternSurface::UnfoldMode::EffectPhaseOnly;
    }

    {
        QSignalBlocker b(ui->sourceCombo);
        ui->sourceCombo->setCurrentIndex(on ? 1 : 0);
    }
    {
        QSignalBlocker b(ui->colorStyleCombo);
        ui->colorStyleCombo->setCurrentIndex(std::clamp(color_style, 0, 1));
    }
    {
        QSignalBlocker b(ui->kernelCombo);
        ui->kernelCombo->setCurrentIndex(std::clamp(kernel, 0, SpatialPatternKernelCount() - 1));
    }
    {
        QSignalBlocker b(ui->unfoldCombo);
        ui->unfoldCombo->setCurrentIndex(
            std::clamp(unfold, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    }
    {
        QSignalBlocker b(ui->repeatsSlider);
        ui->repeatsSlider->setValue((int)std::max(1.0f, std::min(40.0f, rep)));
        ui->repeatsLabel->setText(QString::number(ui->repeatsSlider->value()));
    }
    {
        QSignalBlocker b(ui->dirSlider);
        const int d = (int)std::fmod(dir_deg + 360.0f, 360.0f);
        ui->dirSlider->setValue(d);
        ui->dirLabel->setText(QString::number(d) + QChar(0x00B0));
    }
    refreshSecondaryEnabled();
}

void StripKernelColormapPanel::refreshSecondaryEnabled()
{
    const bool on = useStripColormap();
    const int unfold_idx = ui->unfoldCombo->currentIndex();
    const bool disable_angle =
        unfold_idx == (int)StripPatternSurface::UnfoldMode::EffectPhaseOnly ||
        unfold_idx == (int)StripPatternSurface::UnfoldMode::AlongX ||
        unfold_idx == (int)StripPatternSurface::UnfoldMode::AlongY ||
        unfold_idx == (int)StripPatternSurface::UnfoldMode::AlongZ ||
        unfold_idx == (int)StripPatternSurface::UnfoldMode::DiagonalXYZ ||
        unfold_idx == (int)StripPatternSurface::UnfoldMode::Manhattan01;

    ui->colorStyleCombo->setEnabled(on);
    ui->kernelCombo->setEnabled(on);
    ui->unfoldCombo->setEnabled(on);
    ui->repeatsSlider->setEnabled(on);
    ui->repeatsLabel->setEnabled(on);
    ui->dirSlider->setEnabled(on && !disable_angle);
    ui->dirLabel->setEnabled(on && !disable_angle);
}

void StripKernelColormapPanel::onSourceChanged(int)
{
    refreshSecondaryEnabled();
    emit colormapChanged();
}

void StripKernelColormapPanel::onColorStyleChanged(int) { emit colormapChanged(); }
void StripKernelColormapPanel::onKernelChanged(int) { emit colormapChanged(); }
void StripKernelColormapPanel::onUnfoldChanged(int) { emit colormapChanged(); }

void StripKernelColormapPanel::onRepeatsChanged(int v)
{
    ui->repeatsLabel->setText(QString::number(v));
    emit colormapChanged();
}

void StripKernelColormapPanel::onDirChanged(int v)
{
    ui->dirLabel->setText(QString::number(v) + QChar(0x00B0));
    emit colormapChanged();
}
