// SPDX-License-Identifier: GPL-2.0-only

#include "StripKernelColormapPanel.h"
#include "StripShellPattern/StripShellPatternKernels.h"
#include "Game/StripPatternSurface.h"
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>
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
    case 7: return "Effect phase (no spatial unfold)";
    default: return "Along X";
    }
}

StripKernelColormapPanel::StripKernelColormapPanel(QWidget* parent) : QWidget(parent)
{
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);

    source_combo = new QComboBox(this);
    source_combo->addItem("Colors only (Rainbow/Stops)");
    source_combo->addItem("Colors + Pattern kernel");
    source_combo->setToolTip("Pattern kernel is a room-space color/palette modifier layered on top of base Colors.");
    main->addWidget(source_combo);

    secondary_row = new QWidget(this);
    auto* g = new QGridLayout(secondary_row);
    g->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    g->addWidget(new QLabel("Kernel color source:"), row, 0);
    color_style_combo = new QComboBox();
    color_style_combo->addItem("Pattern palette");
    color_style_combo->addItem("Color stops (from Colors)");
    color_style_combo->addItem("Rainbow (from Colors)");
    color_style_combo->setCurrentIndex(1);
    color_style_combo->setToolTip(
        "Pattern palette = each kernel's default colors. Effect color stops = your palette. Rainbow = spectrum.");
    g->addWidget(color_style_combo, row, 1, 1, 2);
    row++;
    g->addWidget(new QLabel("Pattern shape:"), row, 0);
    kernel_combo = new QComboBox();
    for(int i = 0; i < StripShellKernelCount(); i++)
        kernel_combo->addItem(StripShellKernelDisplayName(i));
    g->addWidget(kernel_combo, row, 1, 1, 2);
    row++;
    g->addWidget(new QLabel("Pattern projection:"), row, 0);
    unfold_combo = new QComboBox();
    for(int i = 0; i < (int)StripPatternSurface::UnfoldMode::COUNT; i++)
        unfold_combo->addItem(UnfoldLabel(i));
    g->addWidget(unfold_combo, row, 1, 1, 2);
    row++;
    g->addWidget(new QLabel("Pattern density:"), row, 0);
    repeats_slider = new QSlider(Qt::Horizontal);
    repeats_slider->setRange(1, 40);
    repeats_slider->setValue(4);
    repeats_label = new QLabel(QStringLiteral("4"));
    repeats_label->setMinimumWidth(24);
    g->addWidget(repeats_slider, row, 1);
    g->addWidget(repeats_label, row, 2);
    row++;
    g->addWidget(new QLabel("Projection angle:"), row, 0);
    dir_slider = new QSlider(Qt::Horizontal);
    dir_slider->setRange(0, 359);
    dir_slider->setValue(0);
    dir_label = new QLabel(QStringLiteral("0\u00B0"));
    dir_label->setMinimumWidth(36);
    g->addWidget(dir_slider, row, 1);
    g->addWidget(dir_label, row, 2);
    main->addWidget(secondary_row);

    connect(source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StripKernelColormapPanel::onSourceChanged);
    connect(color_style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StripKernelColormapPanel::onColorStyleChanged);
    connect(kernel_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StripKernelColormapPanel::onKernelChanged);
    connect(unfold_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StripKernelColormapPanel::onUnfoldChanged);
    connect(repeats_slider, &QSlider::valueChanged, this, &StripKernelColormapPanel::onRepeatsChanged);
    connect(dir_slider, &QSlider::valueChanged, this, &StripKernelColormapPanel::onDirChanged);

    refreshSecondaryEnabled();
}

bool StripKernelColormapPanel::useStripColormap() const
{
    return source_combo && source_combo->currentIndex() == 1;
}

int StripKernelColormapPanel::kernelId() const
{
    return kernel_combo ? std::clamp(kernel_combo->currentIndex(), 0, StripShellKernelCount() - 1) : 0;
}

float StripKernelColormapPanel::kernelRepeats() const
{
    return repeats_slider ? (float)std::max(1, repeats_slider->value()) : 4.0f;
}

int StripKernelColormapPanel::unfoldMode() const
{
    return unfold_combo ? std::clamp(unfold_combo->currentIndex(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1) : 0;
}

float StripKernelColormapPanel::directionDeg() const
{
    return dir_slider ? (float)dir_slider->value() : 0.0f;
}

int StripKernelColormapPanel::colorStyle() const
{
    return color_style_combo ? std::clamp(color_style_combo->currentIndex(), 0, 2) : 1;
}

void StripKernelColormapPanel::mirrorStateFromEffect(bool on, int kernel, float rep, int unfold, float dir_deg, int color_style)
{
    if(source_combo)
    {
        QSignalBlocker b(source_combo);
        source_combo->setCurrentIndex(on ? 1 : 0);
    }
    if(color_style_combo)
    {
        QSignalBlocker b(color_style_combo);
        color_style_combo->setCurrentIndex(std::clamp(color_style, 0, 2));
    }
    if(kernel_combo)
    {
        QSignalBlocker b(kernel_combo);
        kernel_combo->setCurrentIndex(std::clamp(kernel, 0, StripShellKernelCount() - 1));
    }
    if(unfold_combo)
    {
        QSignalBlocker b(unfold_combo);
        unfold_combo->setCurrentIndex(std::clamp(unfold, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    }
    if(repeats_slider)
    {
        QSignalBlocker b(repeats_slider);
        repeats_slider->setValue((int)std::max(1.0f, std::min(40.0f, rep)));
        if(repeats_label)
            repeats_label->setText(QString::number(repeats_slider->value()));
    }
    if(dir_slider)
    {
        QSignalBlocker b(dir_slider);
        int d = (int)std::fmod(dir_deg + 360.0f, 360.0f);
        dir_slider->setValue(d);
        if(dir_label)
            dir_label->setText(QString::number(d) + QChar(0x00B0));
    }
    refreshSecondaryEnabled();
}

void StripKernelColormapPanel::refreshSecondaryEnabled()
{
    bool on = useStripColormap();
    if(color_style_combo)
        color_style_combo->setEnabled(on);
    if(kernel_combo)
        kernel_combo->setEnabled(on);
    if(unfold_combo)
        unfold_combo->setEnabled(on);
    if(repeats_slider)
        repeats_slider->setEnabled(on);
    if(repeats_label)
        repeats_label->setEnabled(on);
    if(dir_slider)
        dir_slider->setEnabled(on);
    if(dir_label)
        dir_label->setEnabled(on);
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
    if(repeats_label)
        repeats_label->setText(QString::number(v));
    emit colormapChanged();
}

void StripKernelColormapPanel::onDirChanged(int v)
{
    if(dir_label)
        dir_label->setText(QString::number(v) + QChar(0x00B0));
    emit colormapChanged();
}
