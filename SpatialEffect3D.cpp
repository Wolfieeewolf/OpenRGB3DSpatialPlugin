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

void SpatialEffect3D::OnParameterChanged()
{
    emit ParametersChanged();
}