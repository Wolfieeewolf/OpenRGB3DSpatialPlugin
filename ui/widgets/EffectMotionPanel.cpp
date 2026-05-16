// SPDX-License-Identifier: GPL-2.0-only

#include "EffectMotionPanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

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
{
    QVBoxLayout* motion_layout = new QVBoxLayout(this);
    motion_layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* speed_layout = new QHBoxLayout();
    speed_layout->addWidget(new QLabel(QStringLiteral("Speed:")));
    speed_slider_ = new QSlider(Qt::Horizontal);
    speed_slider_->setRange(0, 200);
    speed_slider_->setValue((int)speed);
    speed_slider_->setToolTip(QStringLiteral("Effect animation speed (uses logarithmic curve for smooth control)"));
    speed_layout->addWidget(speed_slider_);
    speed_label_ = new QLabel(QString::number(speed));
    speed_label_->setMinimumWidth(30);
    speed_layout->addWidget(speed_label_);
    motion_layout->addLayout(speed_layout);

    QHBoxLayout* brightness_layout = new QHBoxLayout();
    brightness_layout->addWidget(new QLabel(QStringLiteral("Brightness:")));
    brightness_slider_ = new QSlider(Qt::Horizontal);
    brightness_slider_->setRange(1, 100);
    brightness_slider_->setToolTip(QStringLiteral("Overall effect brightness (applied after intensity/sharpness)"));
    brightness_slider_->setValue((int)brightness);
    brightness_layout->addWidget(brightness_slider_);
    brightness_label_ = new QLabel(QString::number(brightness));
    brightness_label_->setMinimumWidth(30);
    brightness_layout->addWidget(brightness_label_);
    motion_layout->addLayout(brightness_layout);

    QHBoxLayout* frequency_layout = new QHBoxLayout();
    frequency_layout->addWidget(new QLabel(QStringLiteral("Frequency:")));
    frequency_slider_ = new QSlider(Qt::Horizontal);
    frequency_slider_->setRange(0, 200);
    frequency_slider_->setValue((int)frequency);
    frequency_slider_->setToolTip(
        QStringLiteral("Temporal rate (pattern motion and color cycles). When a layer exposes Room & voxel mapping and Compass mode is on, this also speeds compass-based palette scrolling."));
    frequency_layout->addWidget(frequency_slider_);
    frequency_label_ = new QLabel(QString::number(frequency));
    frequency_label_->setMinimumWidth(30);
    frequency_layout->addWidget(frequency_label_);
    motion_layout->addLayout(frequency_layout);

    QHBoxLayout* detail_layout = new QHBoxLayout();
    detail_layout->addWidget(new QLabel(QStringLiteral("Detail:")));
    detail_slider_ = new QSlider(Qt::Horizontal);
    detail_slider_->setRange(0, 200);
    detail_slider_->setValue((int)detail);
    detail_slider_->setToolTip(QStringLiteral("Spatial detail (higher = more pattern/color detail across space)"));
    detail_layout->addWidget(detail_slider_);
    detail_label_ = new QLabel(QString::number(detail));
    detail_label_->setMinimumWidth(30);
    detail_layout->addWidget(detail_label_);
    motion_layout->addLayout(detail_layout);

    QHBoxLayout* size_layout = new QHBoxLayout();
    size_layout->addWidget(new QLabel(QStringLiteral("Size:")));
    size_slider_ = new QSlider(Qt::Horizontal);
    size_slider_->setRange(0, 200);
    size_slider_->setValue((int)size);
    size_slider_->setToolTip(QStringLiteral("Spatial scale (bigger = larger features / wider bands)"));
    size_layout->addWidget(size_slider_);
    size_label_ = new QLabel(QString::number(size));
    size_label_->setMinimumWidth(30);
    size_layout->addWidget(size_label_);
    motion_layout->addLayout(size_layout);

    QHBoxLayout* scale_layout = new QHBoxLayout();
    scale_layout->addWidget(new QLabel(QStringLiteral("Scale:")));
    scale_slider_ = new QSlider(Qt::Horizontal);
    scale_slider_->setRange(0, 300);
    scale_slider_->setValue((int)scale);
    scale_slider_->setToolTip(
        QStringLiteral("Effect coverage: 0-200 = 0-100% of room (200=fill grid), 201-300 = 101-200% (beyond room)"));
    scale_layout->addWidget(scale_slider_);
    scale_label_ = new QLabel(QString::number(scale));
    scale_label_->setMinimumWidth(30);
    scale_layout->addWidget(scale_label_);
    scale_invert_check_ = new QCheckBox(QStringLiteral("Invert"));
    scale_invert_check_->setToolTip(QStringLiteral("Collapse effect toward the reference point instead of expanding outward."));
    scale_invert_check_->setChecked(scale_inverted);
    scale_layout->addWidget(scale_invert_check_);
    motion_layout->addLayout(scale_layout);

    QHBoxLayout* fps_layout = new QHBoxLayout();
    fps_layout->addWidget(new QLabel(QStringLiteral("FPS:")));
    fps_slider_ = new QSlider(Qt::Horizontal);
    fps_slider_->setRange(1, 120);
    fps_slider_->setValue((int)fps);
    fps_slider_->setToolTip(
        QStringLiteral("Effect refresh rate (1–120 Hz). When an effect is running, the plugin timer uses this layer’s value "
                       "(single effect) or the maximum across enabled stack layers so motion stays smooth."));
    fps_layout->addWidget(fps_slider_);
    fps_label_ = new QLabel(QString::number(fps));
    fps_label_->setMinimumWidth(30);
    fps_layout->addWidget(fps_label_);
    motion_layout->addLayout(fps_layout);
}
