/*---------------------------------------------------------*\
| SpatialEffect3D.cpp                                       |
|                                                           |
|   Base class for 3D spatial effects with custom UI      |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialEffect3D.h"

SpatialEffect3D::SpatialEffect3D(QWidget* parent) : QWidget(parent)
{
    effect_enabled = false;
    effect_speed = 50;
    effect_brightness = 100;
    color_start = 0xFF0000;
    color_end = 0x0000FF;
    use_gradient = true;

    effect_controls_group = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    color_start_button = nullptr;
    color_end_button = nullptr;
    gradient_check = nullptr;
    speed_label = nullptr;
    brightness_label = nullptr;

    spatial_controls_group = nullptr;
    origin_x_spin = nullptr;
    origin_y_spin = nullptr;
    origin_z_spin = nullptr;
    scale_x_spin = nullptr;
    scale_y_spin = nullptr;
    scale_z_spin = nullptr;
    rotation_x_slider = nullptr;
    rotation_y_slider = nullptr;
    rotation_z_slider = nullptr;
    direction_x_spin = nullptr;
    direction_y_spin = nullptr;
    direction_z_spin = nullptr;
    mirror_x_check = nullptr;
    mirror_y_check = nullptr;
    mirror_z_check = nullptr;
}

SpatialEffect3D::~SpatialEffect3D()
{
}

void SpatialEffect3D::CreateCommonEffectControls(QWidget* parent)
{
    effect_controls_group = new QGroupBox("Effect Controls");
    QVBoxLayout* main_layout = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Speed control                                            |
    \*---------------------------------------------------------*/
    QHBoxLayout* speed_layout = new QHBoxLayout();
    speed_layout->addWidget(new QLabel("Speed:"));
    speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(1, 100);
    speed_slider->setValue(effect_speed);
    speed_layout->addWidget(speed_slider);
    speed_label = new QLabel(QString::number(effect_speed));
    speed_label->setMinimumWidth(30);
    speed_layout->addWidget(speed_label);
    main_layout->addLayout(speed_layout);

    /*---------------------------------------------------------*\
    | Brightness control                                       |
    \*---------------------------------------------------------*/
    QHBoxLayout* brightness_layout = new QHBoxLayout();
    brightness_layout->addWidget(new QLabel("Brightness:"));
    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(1, 100);
    brightness_slider->setValue(effect_brightness);
    brightness_layout->addWidget(brightness_slider);
    brightness_label = new QLabel(QString::number(effect_brightness));
    brightness_label->setMinimumWidth(30);
    brightness_layout->addWidget(brightness_label);
    main_layout->addLayout(brightness_layout);

    /*---------------------------------------------------------*\
    | Color controls                                           |
    \*---------------------------------------------------------*/
    QHBoxLayout* color_layout = new QHBoxLayout();
    color_layout->addWidget(new QLabel("Colors:"));

    color_start_button = new QPushButton();
    color_start_button->setMinimumSize(40, 30);
    color_start_button->setMaximumSize(40, 30);
    color_start_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                      .arg((color_start >> 16) & 0xFF)
                                      .arg((color_start >> 8) & 0xFF)
                                      .arg(color_start & 0xFF));
    color_layout->addWidget(color_start_button);

    color_end_button = new QPushButton();
    color_end_button->setMinimumSize(40, 30);
    color_end_button->setMaximumSize(40, 30);
    color_end_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                    .arg((color_end >> 16) & 0xFF)
                                    .arg((color_end >> 8) & 0xFF)
                                    .arg(color_end & 0xFF));
    color_layout->addWidget(color_end_button);

    gradient_check = new QCheckBox("Gradient");
    gradient_check->setChecked(use_gradient);
    color_layout->addWidget(gradient_check);

    color_layout->addStretch();
    main_layout->addLayout(color_layout);

    effect_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(speed_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(color_start_button, &QPushButton::clicked, this, &SpatialEffect3D::OnColorStartClicked);
    connect(color_end_button, &QPushButton::clicked, this, &SpatialEffect3D::OnColorEndClicked);
    connect(gradient_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);

    /*---------------------------------------------------------*\
    | Update labels when sliders change                        |
    \*---------------------------------------------------------*/
    connect(speed_slider, &QSlider::valueChanged, speed_label, [this](int value) {
        speed_label->setText(QString::number(value));
        effect_speed = value;
    });
    connect(brightness_slider, &QSlider::valueChanged, brightness_label, [this](int value) {
        brightness_label->setText(QString::number(value));
        effect_brightness = value;
    });

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(effect_controls_group);
    }
}

void SpatialEffect3D::CreateCommon3DControls(QWidget* parent)
{
    spatial_controls_group = new QGroupBox("3D Spatial Controls");
    QVBoxLayout* main_layout = new QVBoxLayout();

    EffectInfo3D info = GetEffectInfo();

    /*---------------------------------------------------------*\
    | Create controls based on effect requirements            |
    \*---------------------------------------------------------*/
    if(info.needs_3d_origin)
    {
        CreateOriginControls(spatial_controls_group);
    }

    CreateScaleControls(spatial_controls_group);
    CreateRotationControls(spatial_controls_group);

    if(info.needs_direction)
    {
        CreateDirectionControls(spatial_controls_group);
    }

    CreateMirrorControls(spatial_controls_group);

    spatial_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Add to parent layout if it exists                       |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(spatial_controls_group);
    }
}

void SpatialEffect3D::CreateOriginControls(QWidget* parent)
{
    QGroupBox* origin_group = new QGroupBox("Origin Point");
    QGridLayout* layout = new QGridLayout();

    layout->addWidget(new QLabel("X:"), 0, 0);
    origin_x_spin = new QDoubleSpinBox();
    origin_x_spin->setRange(-1000.0, 1000.0);
    origin_x_spin->setValue(0.0);
    origin_x_spin->setSingleStep(1.0);
    layout->addWidget(origin_x_spin, 0, 1);

    layout->addWidget(new QLabel("Y:"), 1, 0);
    origin_y_spin = new QDoubleSpinBox();
    origin_y_spin->setRange(-1000.0, 1000.0);
    origin_y_spin->setValue(0.0);
    origin_y_spin->setSingleStep(1.0);
    layout->addWidget(origin_y_spin, 1, 1);

    layout->addWidget(new QLabel("Z:"), 2, 0);
    origin_z_spin = new QDoubleSpinBox();
    origin_z_spin->setRange(-1000.0, 1000.0);
    origin_z_spin->setValue(0.0);
    origin_z_spin->setSingleStep(1.0);
    layout->addWidget(origin_z_spin, 2, 1);

    origin_group->setLayout(layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(origin_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);
    connect(origin_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);
    connect(origin_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(origin_group);
    }
}

void SpatialEffect3D::CreateScaleControls(QWidget* parent)
{
    QGroupBox* scale_group = new QGroupBox("Scale");
    QGridLayout* layout = new QGridLayout();

    layout->addWidget(new QLabel("X:"), 0, 0);
    scale_x_spin = new QDoubleSpinBox();
    scale_x_spin->setRange(0.1, 10.0);
    scale_x_spin->setValue(1.0);
    scale_x_spin->setSingleStep(0.1);
    layout->addWidget(scale_x_spin, 0, 1);

    layout->addWidget(new QLabel("Y:"), 1, 0);
    scale_y_spin = new QDoubleSpinBox();
    scale_y_spin->setRange(0.1, 10.0);
    scale_y_spin->setValue(1.0);
    scale_y_spin->setSingleStep(0.1);
    layout->addWidget(scale_y_spin, 1, 1);

    layout->addWidget(new QLabel("Z:"), 2, 0);
    scale_z_spin = new QDoubleSpinBox();
    scale_z_spin->setRange(0.1, 10.0);
    scale_z_spin->setValue(1.0);
    scale_z_spin->setSingleStep(0.1);
    layout->addWidget(scale_z_spin, 2, 1);

    scale_group->setLayout(layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(scale_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(scale_group);
    }
}

void SpatialEffect3D::CreateRotationControls(QWidget* parent)
{
    QGroupBox* rotation_group = new QGroupBox("Rotation");
    QGridLayout* layout = new QGridLayout();

    layout->addWidget(new QLabel("X:"), 0, 0);
    rotation_x_slider = new QSlider(Qt::Horizontal);
    rotation_x_slider->setRange(0, 360);
    rotation_x_slider->setValue(0);
    layout->addWidget(rotation_x_slider, 0, 1);

    layout->addWidget(new QLabel("Y:"), 1, 0);
    rotation_y_slider = new QSlider(Qt::Horizontal);
    rotation_y_slider->setRange(0, 360);
    rotation_y_slider->setValue(0);
    layout->addWidget(rotation_y_slider, 1, 1);

    layout->addWidget(new QLabel("Z:"), 2, 0);
    rotation_z_slider = new QSlider(Qt::Horizontal);
    rotation_z_slider->setRange(0, 360);
    rotation_z_slider->setValue(0);
    layout->addWidget(rotation_z_slider, 2, 1);

    rotation_group->setLayout(layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(rotation_x_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(rotation_y_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(rotation_z_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(rotation_group);
    }
}

void SpatialEffect3D::CreateDirectionControls(QWidget* parent)
{
    QGroupBox* direction_group = new QGroupBox("Direction Vector");
    QGridLayout* layout = new QGridLayout();

    layout->addWidget(new QLabel("X:"), 0, 0);
    direction_x_spin = new QDoubleSpinBox();
    direction_x_spin->setRange(-1.0, 1.0);
    direction_x_spin->setValue(1.0);
    direction_x_spin->setSingleStep(0.1);
    layout->addWidget(direction_x_spin, 0, 1);

    layout->addWidget(new QLabel("Y:"), 1, 0);
    direction_y_spin = new QDoubleSpinBox();
    direction_y_spin->setRange(-1.0, 1.0);
    direction_y_spin->setValue(0.0);
    direction_y_spin->setSingleStep(0.1);
    layout->addWidget(direction_y_spin, 1, 1);

    layout->addWidget(new QLabel("Z:"), 2, 0);
    direction_z_spin = new QDoubleSpinBox();
    direction_z_spin->setRange(-1.0, 1.0);
    direction_z_spin->setValue(0.0);
    direction_z_spin->setSingleStep(0.1);
    layout->addWidget(direction_z_spin, 2, 1);

    direction_group->setLayout(layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(direction_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);
    connect(direction_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);
    connect(direction_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnParameterChanged);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(direction_group);
    }
}

void SpatialEffect3D::CreateMirrorControls(QWidget* parent)
{
    QGroupBox* mirror_group = new QGroupBox("Mirror");
    QHBoxLayout* layout = new QHBoxLayout();

    mirror_x_check = new QCheckBox("X Axis");
    mirror_y_check = new QCheckBox("Y Axis");
    mirror_z_check = new QCheckBox("Z Axis");

    layout->addWidget(mirror_x_check);
    layout->addWidget(mirror_y_check);
    layout->addWidget(mirror_z_check);

    mirror_group->setLayout(layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(mirror_x_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);
    connect(mirror_y_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);
    connect(mirror_z_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(mirror_group);
    }
}

void SpatialEffect3D::UpdateCommon3DParams(SpatialEffectParams& params)
{
    /*---------------------------------------------------------*\
    | Update origin                                            |
    \*---------------------------------------------------------*/
    if(origin_x_spin && origin_y_spin && origin_z_spin)
    {
        params.origin.x = (float)origin_x_spin->value();
        params.origin.y = (float)origin_y_spin->value();
        params.origin.z = (float)origin_z_spin->value();
    }

    /*---------------------------------------------------------*\
    | Update scale                                             |
    \*---------------------------------------------------------*/
    if(scale_x_spin && scale_y_spin && scale_z_spin)
    {
        params.scale_3d.x = (float)scale_x_spin->value();
        params.scale_3d.y = (float)scale_y_spin->value();
        params.scale_3d.z = (float)scale_z_spin->value();
    }

    /*---------------------------------------------------------*\
    | Update rotation                                          |
    \*---------------------------------------------------------*/
    if(rotation_x_slider && rotation_y_slider && rotation_z_slider)
    {
        params.rotation.x = (float)rotation_x_slider->value();
        params.rotation.y = (float)rotation_y_slider->value();
        params.rotation.z = (float)rotation_z_slider->value();
    }

    /*---------------------------------------------------------*\
    | Update direction                                         |
    \*---------------------------------------------------------*/
    if(direction_x_spin && direction_y_spin && direction_z_spin)
    {
        params.direction.x = (float)direction_x_spin->value();
        params.direction.y = (float)direction_y_spin->value();
        params.direction.z = (float)direction_z_spin->value();
    }

    /*---------------------------------------------------------*\
    | Update mirror settings                                   |
    \*---------------------------------------------------------*/
    if(mirror_x_check && mirror_y_check && mirror_z_check)
    {
        params.mirror_x = mirror_x_check->isChecked();
        params.mirror_y = mirror_y_check->isChecked();
        params.mirror_z = mirror_z_check->isChecked();
    }
}

void SpatialEffect3D::SetColors(RGBColor start, RGBColor end, bool gradient)
{
    color_start = start;
    color_end = end;
    use_gradient = gradient;
    emit ParametersChanged();
}

void SpatialEffect3D::GetColors(RGBColor& start, RGBColor& end, bool& gradient)
{
    start = color_start;
    end = color_end;
    gradient = use_gradient;
}

void SpatialEffect3D::UpdateCommonEffectParams(SpatialEffectParams& params)
{
    /*---------------------------------------------------------*\
    | Update common effect parameters                          |
    \*---------------------------------------------------------*/
    if(speed_slider)
    {
        params.speed = speed_slider->value();
        effect_speed = params.speed;
    }

    if(brightness_slider)
    {
        params.brightness = brightness_slider->value();
        effect_brightness = params.brightness;
    }

    if(gradient_check)
    {
        params.use_gradient = gradient_check->isChecked();
        use_gradient = params.use_gradient;
    }

    params.color_start = color_start;
    params.color_end = color_end;
}

void SpatialEffect3D::OnParameterChanged()
{
    emit ParametersChanged();
}

void SpatialEffect3D::OnColorStartClicked()
{
    QColor initial_color = QColor((color_start >> 16) & 0xFF,
                                  (color_start >> 8) & 0xFF,
                                  color_start & 0xFF);

    QColor new_color = QColorDialog::getColor(initial_color, this, "Select Start Color");

    if(new_color.isValid())
    {
        color_start = (new_color.red() << 16) | (new_color.green() << 8) | new_color.blue();

        color_start_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                          .arg(new_color.red())
                                          .arg(new_color.green())
                                          .arg(new_color.blue()));

        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnColorEndClicked()
{
    QColor initial_color = QColor((color_end >> 16) & 0xFF,
                                  (color_end >> 8) & 0xFF,
                                  color_end & 0xFF);

    QColor new_color = QColorDialog::getColor(initial_color, this, "Select End Color");

    if(new_color.isValid())
    {
        color_end = (new_color.red() << 16) | (new_color.green() << 8) | new_color.blue();

        color_end_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                        .arg(new_color.red())
                                        .arg(new_color.green())
                                        .arg(new_color.blue()));

        emit ParametersChanged();
    }
}