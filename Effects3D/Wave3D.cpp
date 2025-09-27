/*---------------------------------------------------------*\
| Wave3D.cpp                                                |
|                                                           |
|   3D Wave effect with custom UI controls                 |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Wave3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>

Wave3D::Wave3D(QWidget* parent) : SpatialEffect3D(parent)
{
    wave_type_combo = nullptr;
    frequency_slider = nullptr;
    amplitude_slider = nullptr;
    phase_slider = nullptr;
    standing_wave_check = nullptr;
    thickness_slider = nullptr;
    falloff_combo = nullptr;

    wave_type = 0;
    frequency = 10;
    amplitude = 1.0f;
    phase = 0.0f;
    standing_wave = false;
    thickness = 1.0f;
    falloff_type = 0;
}

Wave3D::~Wave3D()
{
}

EffectInfo3D Wave3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Wave";
    info.effect_description = "3D wave effect with configurable direction and properties";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE_X;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = true;
    info.needs_thickness = true;
    info.needs_arms = false;
    info.needs_frequency = true;
    return info;
}

void Wave3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Create common effect controls first                      |
    \*---------------------------------------------------------*/
    CreateCommonEffectControls(parent);

    /*---------------------------------------------------------*\
    | Create wave-specific controls group                      |
    \*---------------------------------------------------------*/
    QGroupBox* wave_controls_group = new QGroupBox("Wave Settings");
    QVBoxLayout* main_layout = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Wave type selection                                      |
    \*---------------------------------------------------------*/
    QHBoxLayout* type_layout = new QHBoxLayout();
    type_layout->addWidget(new QLabel("Wave Type:"));
    wave_type_combo = new QComboBox();
    wave_type_combo->addItem("X Axis");
    wave_type_combo->addItem("Y Axis");
    wave_type_combo->addItem("Z Axis");
    wave_type_combo->addItem("Radial");
    wave_type_combo->addItem("Spherical");
    wave_type_combo->setCurrentIndex(wave_type);
    type_layout->addWidget(wave_type_combo);
    main_layout->addLayout(type_layout);

    /*---------------------------------------------------------*\
    | Wave parameters grid                                     |
    \*---------------------------------------------------------*/
    QGridLayout* params_layout = new QGridLayout();

    /*---------------------------------------------------------*\
    | Frequency control                                        |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Frequency:"), 0, 0);
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(1, 50);
    frequency_slider->setValue(frequency);
    params_layout->addWidget(frequency_slider, 0, 1);
    QLabel* freq_value = new QLabel(QString::number(frequency));
    params_layout->addWidget(freq_value, 0, 2);

    /*---------------------------------------------------------*\
    | Amplitude control                                        |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Amplitude:"), 1, 0);
    amplitude_slider = new QSlider(Qt::Horizontal);
    amplitude_slider->setRange(1, 100);
    amplitude_slider->setValue((int)(amplitude * 100));
    params_layout->addWidget(amplitude_slider, 1, 1);
    QLabel* amp_value = new QLabel(QString::number(amplitude, 'f', 2));
    params_layout->addWidget(amp_value, 1, 2);

    /*---------------------------------------------------------*\
    | Phase control                                            |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Phase:"), 2, 0);
    phase_slider = new QSlider(Qt::Horizontal);
    phase_slider->setRange(0, 360);
    phase_slider->setValue((int)(phase * 180.0f / 3.14159f));
    params_layout->addWidget(phase_slider, 2, 1);
    QLabel* phase_value = new QLabel(QString::number(phase * 180.0f / 3.14159f, 'f', 1) + "°");
    params_layout->addWidget(phase_value, 2, 2);

    /*---------------------------------------------------------*\
    | Thickness control                                        |
    \*---------------------------------------------------------*/
    params_layout->addWidget(new QLabel("Thickness:"), 3, 0);
    thickness_slider = new QSlider(Qt::Horizontal);
    thickness_slider->setRange(1, 100);
    thickness_slider->setValue((int)(thickness * 100));
    params_layout->addWidget(thickness_slider, 3, 1);
    QLabel* thick_value = new QLabel(QString::number(thickness, 'f', 2));
    params_layout->addWidget(thick_value, 3, 2);

    main_layout->addLayout(params_layout);

    /*---------------------------------------------------------*\
    | Additional wave options                                  |
    \*---------------------------------------------------------*/
    QHBoxLayout* options_layout = new QHBoxLayout();

    standing_wave_check = new QCheckBox("Standing Wave");
    standing_wave_check->setChecked(standing_wave);
    options_layout->addWidget(standing_wave_check);

    options_layout->addWidget(new QLabel("Falloff:"));
    falloff_combo = new QComboBox();
    falloff_combo->addItem("Linear");
    falloff_combo->addItem("Quadratic");
    falloff_combo->addItem("Exponential");
    falloff_combo->setCurrentIndex(falloff_type);
    options_layout->addWidget(falloff_combo);

    main_layout->addLayout(options_layout);

    wave_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Connect signals to update parameters                     |
    \*---------------------------------------------------------*/
    connect(wave_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(amplitude_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(phase_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(thickness_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(standing_wave_check, &QCheckBox::toggled, this, &Wave3D::OnWaveParameterChanged);
    connect(falloff_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);

    /*---------------------------------------------------------*\
    | Update value labels when sliders change                 |
    \*---------------------------------------------------------*/
    connect(frequency_slider, &QSlider::valueChanged, freq_value, [freq_value](int value) {
        freq_value->setText(QString::number(value));
    });
    connect(amplitude_slider, &QSlider::valueChanged, amp_value, [amp_value](int value) {
        amp_value->setText(QString::number(value / 100.0f, 'f', 2));
    });
    connect(phase_slider, &QSlider::valueChanged, phase_value, [phase_value](int value) {
        phase_value->setText(QString::number(value, 'f', 1) + "°");
    });
    connect(thickness_slider, &QSlider::valueChanged, thick_value, [thick_value](int value) {
        thick_value->setText(QString::number(value / 100.0f, 'f', 2));
    });

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(wave_controls_group);
    }

    /*---------------------------------------------------------*\
    | Create common 3D controls                                |
    \*---------------------------------------------------------*/
    CreateCommon3DControls(parent);
}

void Wave3D::UpdateParams(SpatialEffectParams& params)
{
    /*---------------------------------------------------------*\
    | Update common effect parameters first                    |
    \*---------------------------------------------------------*/
    UpdateCommonEffectParams(params);

    /*---------------------------------------------------------*\
    | Update wave-specific parameters                          |
    \*---------------------------------------------------------*/
    if(wave_type_combo)
    {
        wave_type = wave_type_combo->currentIndex();
        switch(wave_type)
        {
            case 0: params.type = SPATIAL_EFFECT_WAVE_X; break;
            case 1: params.type = SPATIAL_EFFECT_WAVE_Y; break;
            case 2: params.type = SPATIAL_EFFECT_WAVE_Z; break;
            case 3: params.type = SPATIAL_EFFECT_WAVE_RADIAL; break;
            case 4: params.type = SPATIAL_EFFECT_WAVE_RADIAL; break; // Spherical uses radial for now
        }
    }

    if(frequency_slider)
    {
        frequency = frequency_slider->value();
        params.frequency = frequency;
    }

    if(amplitude_slider)
    {
        amplitude = amplitude_slider->value() / 100.0f;
        params.intensity = amplitude;
    }

    if(phase_slider)
    {
        phase = phase_slider->value() * 3.14159f / 180.0f;
        // Phase can be applied through time offset in calculation
    }

    if(thickness_slider)
    {
        thickness = thickness_slider->value() / 100.0f;
        params.thickness = thickness;
    }

    if(standing_wave_check)
    {
        standing_wave = standing_wave_check->isChecked();
        // Standing wave affects calculation method
    }

    if(falloff_combo)
    {
        falloff_type = falloff_combo->currentIndex();
        switch(falloff_type)
        {
            case 0: params.falloff = 1.0f; break;      // Linear
            case 1: params.falloff = 2.0f; break;      // Quadratic
            case 2: params.falloff = 0.5f; break;      // Exponential
        }
    }

    /*---------------------------------------------------------*\
    | Update common 3D parameters                              |
    \*---------------------------------------------------------*/
    UpdateCommon3DParams(params);
}

void Wave3D::OnWaveParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(wave_type_combo)       wave_type = wave_type_combo->currentIndex();
    if(frequency_slider)      frequency = frequency_slider->value();
    if(amplitude_slider)      amplitude = amplitude_slider->value() / 100.0f;
    if(phase_slider)          phase = phase_slider->value() * 3.14159f / 180.0f;
    if(thickness_slider)      thickness = thickness_slider->value() / 100.0f;
    if(standing_wave_check)   standing_wave = standing_wave_check->isChecked();
    if(falloff_combo)         falloff_type = falloff_combo->currentIndex();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}