/*---------------------------------------------------------*\
| Plasma3D.cpp                                              |
|                                                           |
|   3D Plasma effect with custom UI controls               |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Plasma3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>

Plasma3D::Plasma3D(QWidget* parent) : SpatialEffect3D(parent)
{
    complexity_slider = nullptr;
    time_scale_slider = nullptr;
    noise_scale_slider = nullptr;
    pattern_combo = nullptr;
    smooth_check = nullptr;
    color_shift_slider = nullptr;

    complexity = 3.0f;
    time_scale = 1.0f;
    noise_scale = 2.0f;
    pattern_type = 0;
    smooth_interpolation = true;
    color_shift = 0.0f;
}

Plasma3D::~Plasma3D()
{
}

EffectInfo3D Plasma3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Plasma";
    info.effect_description = "Animated plasma effect with configurable patterns and complexity";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_PLASMA;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;
    return info;
}

void Plasma3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Create common effect controls first                      |
    \*---------------------------------------------------------*/
    CreateCommonEffectControls(parent);

    /*---------------------------------------------------------*\
    | Create plasma-specific controls group                   |
    \*---------------------------------------------------------*/
    QGroupBox* plasma_controls_group = new QGroupBox("Plasma Settings");
    QVBoxLayout* main_layout = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Pattern type selection                                   |
    \*---------------------------------------------------------*/
    QHBoxLayout* pattern_layout = new QHBoxLayout();
    pattern_layout->addWidget(new QLabel("Pattern:"));
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Classic");
    pattern_combo->addItem("Swirl");
    pattern_combo->addItem("Ripple");
    pattern_combo->addItem("Organic");
    pattern_combo->setCurrentIndex(pattern_type);
    pattern_layout->addWidget(pattern_combo);
    main_layout->addLayout(pattern_layout);

    /*---------------------------------------------------------*\
    | Plasma parameters grid                                   |
    \*---------------------------------------------------------*/
    QGridLayout* params_layout = new QGridLayout();

    /*---------------------------------------------------------*\
    | Complexity control                                       |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Complexity:"), 0, 0);
    complexity_slider = new QSlider(Qt::Horizontal);
    complexity_slider->setRange(1, 10);
    complexity_slider->setValue((int)complexity);
    params_layout->addWidget(complexity_slider, 0, 1);
    QLabel* complexity_value = new QLabel(QString::number(complexity, 'f', 1));
    params_layout->addWidget(complexity_value, 0, 2);

    /*---------------------------------------------------------*\
    | Time scale control                                       |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Time Scale:"), 1, 0);
    time_scale_slider = new QSlider(Qt::Horizontal);
    time_scale_slider->setRange(1, 50);
    time_scale_slider->setValue((int)(time_scale * 10));
    params_layout->addWidget(time_scale_slider, 1, 1);
    QLabel* time_value = new QLabel(QString::number(time_scale, 'f', 1));
    params_layout->addWidget(time_value, 1, 2);

    /*---------------------------------------------------------*\
    | Noise scale control                                      |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Noise Scale:"), 2, 0);
    noise_scale_slider = new QSlider(Qt::Horizontal);
    noise_scale_slider->setRange(1, 100);
    noise_scale_slider->setValue((int)(noise_scale * 10));
    params_layout->addWidget(noise_scale_slider, 2, 1);
    QLabel* noise_value = new QLabel(QString::number(noise_scale, 'f', 1));
    params_layout->addWidget(noise_value, 2, 2);

    /*---------------------------------------------------------*\
    | Color shift control                                      |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Color Shift:"), 3, 0);
    color_shift_slider = new QSlider(Qt::Horizontal);
    color_shift_slider->setRange(0, 360);
    color_shift_slider->setValue((int)color_shift);
    params_layout->addWidget(color_shift_slider, 3, 1);
    QLabel* shift_value = new QLabel(QString::number(color_shift, 'f', 0) + "°");
    params_layout->addWidget(shift_value, 3, 2);

    main_layout->addLayout(params_layout);

    /*---------------------------------------------------------*\
    | Additional plasma options                                |
    \*---------------------------------------------------------*/
    QHBoxLayout* options_layout = new QHBoxLayout();

    smooth_check = new QCheckBox("Smooth Interpolation");
    smooth_check->setChecked(smooth_interpolation);
    options_layout->addWidget(smooth_check);

    options_layout->addStretch();
    main_layout->addLayout(options_layout);

    plasma_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Connect signals to update parameters                     |
    \*---------------------------------------------------------*/
    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Plasma3D::OnPlasmaParameterChanged);
    connect(complexity_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(time_scale_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(noise_scale_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(color_shift_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(smooth_check, &QCheckBox::toggled, this, &Plasma3D::OnPlasmaParameterChanged);

    /*---------------------------------------------------------*\
    | Update value labels when sliders change                 |
    \*---------------------------------------------------------*/
    connect(complexity_slider, &QSlider::valueChanged, complexity_value, [complexity_value](int value) {
        complexity_value->setText(QString::number(value, 'f', 1));
    });
    connect(time_scale_slider, &QSlider::valueChanged, time_value, [time_value](int value) {
        time_value->setText(QString::number(value / 10.0f, 'f', 1));
    });
    connect(noise_scale_slider, &QSlider::valueChanged, noise_value, [noise_value](int value) {
        noise_value->setText(QString::number(value / 10.0f, 'f', 1));
    });
    connect(color_shift_slider, &QSlider::valueChanged, shift_value, [shift_value](int value) {
        shift_value->setText(QString::number(value, 'f', 0) + "°");
    });

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(plasma_controls_group);
    }

    /*---------------------------------------------------------*\
    | Create common 3D controls (optional for plasma)         |
    \*---------------------------------------------------------*/
    CreateCommon3DControls(parent);
}

void Plasma3D::UpdateParams(SpatialEffectParams& params)
{
    /*---------------------------------------------------------*\
    | Update common effect parameters first                    |
    \*---------------------------------------------------------*/
    UpdateCommonEffectParams(params);

    /*---------------------------------------------------------*\
    | Update plasma-specific parameters                       |
    \*---------------------------------------------------------*/
    if(pattern_combo)
    {
        pattern_type = pattern_combo->currentIndex();
        // Pattern type affects the base plasma effect calculation
    }

    if(complexity_slider)
    {
        complexity = complexity_slider->value();
        params.intensity = complexity / 10.0f; // Map to intensity parameter
    }

    if(time_scale_slider)
    {
        time_scale = time_scale_slider->value() / 10.0f;
        // Time scale affects animation speed multiplier
    }

    if(noise_scale_slider)
    {
        noise_scale = noise_scale_slider->value() / 10.0f;
        params.thickness = noise_scale; // Map to thickness parameter for noise scaling
    }

    if(color_shift_slider)
    {
        color_shift = color_shift_slider->value();
        // Color shift can be applied to hue rotation
    }

    if(smooth_check)
    {
        smooth_interpolation = smooth_check->isChecked();
        // Smooth interpolation affects rendering quality
    }

    /*---------------------------------------------------------*\
    | Update common 3D parameters                              |
    \*---------------------------------------------------------*/
    UpdateCommon3DParams(params);
}

void Plasma3D::OnPlasmaParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(pattern_combo)         pattern_type = pattern_combo->currentIndex();
    if(complexity_slider)     complexity = complexity_slider->value();
    if(time_scale_slider)     time_scale = time_scale_slider->value() / 10.0f;
    if(noise_scale_slider)    noise_scale = noise_scale_slider->value() / 10.0f;
    if(color_shift_slider)    color_shift = color_shift_slider->value();
    if(smooth_check)          smooth_interpolation = smooth_check->isChecked();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}