// SPDX-License-Identifier: GPL-2.0-only

#include "EffectOutputPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

EffectOutputPanel::EffectOutputPanel(unsigned int intensity,
                                                                     unsigned int sharpness,
                                                                     unsigned int smoothing,
                                                                     unsigned int sampling_resolution,
                                                                     QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* output_layout = new QVBoxLayout(this);
    output_layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* intensity_layout = new QHBoxLayout();
    intensity_layout->addWidget(new QLabel(QStringLiteral("Intensity:")));
    intensity_slider_ = new QSlider(Qt::Horizontal);
    intensity_slider_->setRange(0, 200);
    intensity_slider_->setValue((int)intensity);
    intensity_slider_->setToolTip(QStringLiteral("Global intensity multiplier (0 = off, 100 = normal, 200 = 2x)"));
    intensity_layout->addWidget(intensity_slider_);
    intensity_label_ = new QLabel(QString::number(intensity));
    intensity_label_->setMinimumWidth(30);
    intensity_layout->addWidget(intensity_label_);
    output_layout->addLayout(intensity_layout);

    QHBoxLayout* sharpness_layout = new QHBoxLayout();
    sharpness_layout->addWidget(new QLabel(QStringLiteral("Sharpness:")));
    sharpness_slider_ = new QSlider(Qt::Horizontal);
    sharpness_slider_->setRange(0, 200);
    sharpness_slider_->setValue((int)sharpness);
    sharpness_slider_->setToolTip(
        QStringLiteral("Edge contrast: lower = softer, higher = crisper (gamma-like)"));
    sharpness_layout->addWidget(sharpness_slider_);
    sharpness_label_ = new QLabel(QString::number(sharpness));
    sharpness_label_->setMinimumWidth(30);
    sharpness_layout->addWidget(sharpness_label_);
    output_layout->addLayout(sharpness_layout);

    QHBoxLayout* smoothing_layout = new QHBoxLayout();
    smoothing_layout->addWidget(new QLabel(QStringLiteral("Smoothing:")));
    smoothing_slider_ = new QSlider(Qt::Horizontal);
    smoothing_slider_->setRange(0, 100);
    smoothing_slider_->setValue((int)smoothing);
    smoothing_slider_->setToolTip(
        QStringLiteral("Global temporal smoothing hint (0 = off). Effects that support it blend frame-to-frame to reduce low-FPS stepping."));
    smoothing_layout->addWidget(smoothing_slider_);
    smoothing_label_ = new QLabel(QString::number(smoothing));
    smoothing_label_->setMinimumWidth(30);
    smoothing_layout->addWidget(smoothing_label_);
    output_layout->addLayout(smoothing_layout);

    QHBoxLayout* sampling_layout = new QHBoxLayout();
    sampling_layout->addWidget(new QLabel(QStringLiteral("Sampling:")));
    sampling_resolution_slider_ = new QSlider(Qt::Horizontal);
    sampling_resolution_slider_->setRange(0, 100);
    sampling_resolution_slider_->setValue((int)sampling_resolution);
    sampling_resolution_slider_->setToolTip(
        QStringLiteral("Global sampling detail (100 = full, 0 = blocky). Image/GIF layers: UV quantization (× per-media Resolution). "
                       "Other effects: LED positions snap to a coarser voxel grid in room bounds (retro / low-res look)."));
    sampling_layout->addWidget(sampling_resolution_slider_);
    sampling_resolution_label_ = new QLabel(QString::number((int)sampling_resolution));
    sampling_resolution_label_->setMinimumWidth(30);
    sampling_layout->addWidget(sampling_resolution_label_);
    output_layout->addLayout(sampling_layout);
}
