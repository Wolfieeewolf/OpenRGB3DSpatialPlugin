// SPDX-License-Identifier: GPL-2.0-only

#include "StripKernelColormapPanel.h"
#include "StripPatternSurface.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "EffectSliderPromote.h"
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

    repeats_row_ = PromoteSliderLabelInGrid(ui->secondaryGrid, ui->repeatsSlider, ui->repeatsLabel);
    dir_row_ = PromoteSliderLabelInGrid(ui->secondaryGrid, ui->dirSlider, ui->dirLabel,
                                         QString(QChar(0x00B0)));
    AlignGridCaptionColumn(ui->secondaryGrid, 0, 90);

    populateCombos();

    connect(ui->sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onSourceChanged);
    connect(ui->kernelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onKernelChanged);
    connect(ui->unfoldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StripKernelColormapPanel::onUnfoldChanged);
    if(repeats_row_)
    {
        connect(repeats_row_->slider(), &QSlider::valueChanged, this,
                &StripKernelColormapPanel::onRepeatsChanged);
    }
    if(dir_row_)
    {
        connect(dir_row_->slider(), &QSlider::valueChanged, this, &StripKernelColormapPanel::onDirChanged);
    }

    refreshSecondaryEnabled();
}

StripKernelColormapPanel::~StripKernelColormapPanel()
{
    delete ui;
}

void StripKernelColormapPanel::populateCombos()
{
    ui->sourceCombo->addItem(QStringLiteral("Effect Colors"));
    ui->sourceCombo->addItem(QStringLiteral("Pattern"));
    ui->sourceCombo->setItemData(
        0, QStringLiteral("Paint the effect with rainbow / color stops."), Qt::ToolTipRole);
    ui->sourceCombo->setItemData(
        1,
        QStringLiteral("Stamp a pattern kernel onto the lit surface — pattern owns its colors and animation."),
        Qt::ToolTipRole);

    for(int i = 0; i < SpatialPatternKernelCount(); i++)
    {
        ui->kernelCombo->addItem(SpatialPatternKernelDisplayName(i));
    }

    for(int i = 0; i < (int)StripPatternSurface::UnfoldMode::COUNT; i++)
    {
        ui->unfoldCombo->addItem(UnfoldLabel(i));
    }
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
    const QSlider* sl = repeats_row_ ? repeats_row_->slider() : nullptr;
    return (float)std::max(1, sl ? sl->value() : 1);
}

int StripKernelColormapPanel::unfoldMode() const
{
    return std::clamp(ui->unfoldCombo->currentIndex(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
}

float StripKernelColormapPanel::directionDeg() const
{
    const QSlider* sl = dir_row_ ? dir_row_->slider() : nullptr;
    return (float)(sl ? sl->value() : 0);
}

void StripKernelColormapPanel::mirrorStateFromEffect(bool on, int kernel, float rep, int unfold, float dir_deg)
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
        QSignalBlocker b(ui->kernelCombo);
        ui->kernelCombo->setCurrentIndex(std::clamp(kernel, 0, SpatialPatternKernelCount() - 1));
    }
    {
        QSignalBlocker b(ui->unfoldCombo);
        ui->unfoldCombo->setCurrentIndex(
            std::clamp(unfold, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    }
    SyncEffectSliderValue(repeats_row_ ? repeats_row_->slider() : nullptr,
                          (int)std::max(1.0f, std::min(40.0f, rep)));
    const int d = (int)std::fmod(dir_deg + 360.0f, 360.0f);
    SyncEffectSliderValue(dir_row_ ? dir_row_->slider() : nullptr, d);
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

    ui->secondaryPanel->setVisible(true);
    ui->kernelCaption->setVisible(on);
    ui->kernelCombo->setVisible(on);
    ui->kernelCombo->setEnabled(on);
    ui->repeatsCaption->setVisible(on);
    if(repeats_row_)
    {
        repeats_row_->setVisible(on);
        repeats_row_->setEnabled(on);
    }
    ui->unfoldCombo->setEnabled(true);
    if(dir_row_)
    {
        dir_row_->setEnabled(!disable_angle);
    }
}

void StripKernelColormapPanel::onSourceChanged(int)
{
    refreshSecondaryEnabled();
    emit colormapChanged();
}

void StripKernelColormapPanel::onKernelChanged(int) { emit colormapChanged(); }
void StripKernelColormapPanel::onUnfoldChanged(int)
{
    refreshSecondaryEnabled();
    emit colormapChanged();
}

void StripKernelColormapPanel::onRepeatsChanged(int) { emit colormapChanged(); }
void StripKernelColormapPanel::onDirChanged(int) { emit colormapChanged(); }
