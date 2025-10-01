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
#include "Colors.h"

SpatialEffect3D::SpatialEffect3D(QWidget* parent) : QWidget(parent)
{
    effect_enabled = false;
    effect_running = false;
    effect_speed = 50;
    effect_brightness = 100;
    effect_frequency = 50;
    rainbow_mode = false;
    rainbow_progress = 0.0f;

    // Initialize default colors
    colors.push_back(COLOR_RED);
    colors.push_back(COLOR_BLUE);

    // Initialize universal axis controls
    effect_axis = AXIS_RADIAL;          // Default to radial (most effects work well radially)
    effect_reverse = false;             // Default forward direction
    custom_direction = {1.0f, 0.0f, 0.0f};  // Default X direction for custom

    effect_controls_group = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    speed_label = nullptr;
    brightness_label = nullptr;
    frequency_label = nullptr;

    // Color controls
    color_controls_group = nullptr;
    rainbow_mode_check = nullptr;
    color_buttons_widget = nullptr;
    color_buttons_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    // Effect control buttons
    start_effect_button = nullptr;
    stop_effect_button = nullptr;

    // Universal axis controls
    axis_combo = nullptr;
    reverse_check = nullptr;
    custom_direction_x = nullptr;
    custom_direction_y = nullptr;
    custom_direction_z = nullptr;

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
    | Effect Control Buttons                                   |
    \*---------------------------------------------------------*/
    QHBoxLayout* button_layout = new QHBoxLayout();
    start_effect_button = new QPushButton("Start Effect");
    start_effect_button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    stop_effect_button = new QPushButton("Stop Effect");
    stop_effect_button->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    stop_effect_button->setEnabled(false);

    button_layout->addWidget(start_effect_button);
    button_layout->addWidget(stop_effect_button);
    button_layout->addStretch();
    main_layout->addLayout(button_layout);

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
    | Frequency control                                        |
    \*---------------------------------------------------------*/
    QHBoxLayout* frequency_layout = new QHBoxLayout();
    frequency_layout->addWidget(new QLabel("Frequency:"));
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(1, 100);
    frequency_slider->setValue(effect_frequency);
    frequency_layout->addWidget(frequency_slider);
    frequency_label = new QLabel(QString::number(effect_frequency));
    frequency_label->setMinimumWidth(30);
    frequency_layout->addWidget(frequency_label);
    main_layout->addLayout(frequency_layout);

    /*---------------------------------------------------------*\
    | Create and add color controls                            |
    \*---------------------------------------------------------*/
    CreateColorControls(parent);
    main_layout->addWidget(color_controls_group);

    /*---------------------------------------------------------*\
    | Universal Axis & Direction Controls                      |
    \*---------------------------------------------------------*/
    QHBoxLayout* axis_layout = new QHBoxLayout();
    axis_layout->addWidget(new QLabel("Axis:"));
    axis_combo = new QComboBox();
    axis_combo->addItem("X-Axis (Left ↔ Right)");
    axis_combo->addItem("Y-Axis (Front ↔ Back)");
    axis_combo->addItem("Z-Axis (Floor ↔ Ceiling)");
    axis_combo->addItem("Radial (Outward)");
    axis_combo->addItem("Custom Direction");
    axis_combo->setCurrentIndex((int)effect_axis);
    axis_layout->addWidget(axis_combo);

    reverse_check = new QCheckBox("Reverse");
    reverse_check->setChecked(effect_reverse);
    axis_layout->addWidget(reverse_check);

    axis_layout->addStretch();
    main_layout->addLayout(axis_layout);

    // Custom direction controls (initially hidden)
    QHBoxLayout* custom_dir_layout = new QHBoxLayout();
    custom_dir_layout->addWidget(new QLabel("Direction:"));
    custom_dir_layout->addWidget(new QLabel("X:"));
    custom_direction_x = new QDoubleSpinBox();
    custom_direction_x->setRange(-1.0, 1.0);
    custom_direction_x->setSingleStep(0.1);
    custom_direction_x->setValue(custom_direction.x);
    custom_dir_layout->addWidget(custom_direction_x);

    custom_dir_layout->addWidget(new QLabel("Y:"));
    custom_direction_y = new QDoubleSpinBox();
    custom_direction_y->setRange(-1.0, 1.0);
    custom_direction_y->setSingleStep(0.1);
    custom_direction_y->setValue(custom_direction.y);
    custom_dir_layout->addWidget(custom_direction_y);

    custom_dir_layout->addWidget(new QLabel("Z:"));
    custom_direction_z = new QDoubleSpinBox();
    custom_direction_z->setRange(-1.0, 1.0);
    custom_direction_z->setSingleStep(0.1);
    custom_direction_z->setValue(custom_direction.z);
    custom_dir_layout->addWidget(custom_direction_z);

    custom_dir_layout->addStretch();
    main_layout->addLayout(custom_dir_layout);

    // Hide custom direction controls if not using custom axis
    bool show_custom = (effect_axis == AXIS_CUSTOM);
    custom_direction_x->setVisible(show_custom);
    custom_direction_y->setVisible(show_custom);
    custom_direction_z->setVisible(show_custom);

    effect_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(speed_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);

    // Effect control buttons - NOT connected here!
    // The parent tab needs to connect these to its on_start_effect_clicked/on_stop_effect_clicked handlers
    // to actually start the effect timer

    // Universal axis & direction controls
    connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpatialEffect3D::OnAxisChanged);
    connect(reverse_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnReverseChanged);
    connect(custom_direction_x, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnCustomDirectionChanged);
    connect(custom_direction_y, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnCustomDirectionChanged);
    connect(custom_direction_z, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpatialEffect3D::OnCustomDirectionChanged);

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
    connect(frequency_slider, &QSlider::valueChanged, frequency_label, [this](int value) {
        frequency_label->setText(QString::number(value));
        effect_frequency = value;
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
    EffectInfo3D info = GetEffectInfo();

    /*---------------------------------------------------------*\
    | Only create 3D controls if the effect actually needs them|
    \*---------------------------------------------------------*/
    bool needs_any_3d_controls = info.needs_3d_origin || info.needs_direction;

    if(!needs_any_3d_controls)
    {
        return; // Don't create empty 3D controls box
    }

    spatial_controls_group = new QGroupBox("3D Spatial Controls");
    QVBoxLayout* main_layout = new QVBoxLayout();
    spatial_controls_group->setLayout(main_layout);  // Set layout first

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
}

void SpatialEffect3D::OnParameterChanged()
{
    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| Universal Axis & Direction Control Slots                |
\*---------------------------------------------------------*/
void SpatialEffect3D::OnAxisChanged()
{
    effect_axis = (EffectAxis)axis_combo->currentIndex();

    // Show/hide custom direction controls
    bool show_custom = (effect_axis == AXIS_CUSTOM);
    custom_direction_x->setVisible(show_custom);
    custom_direction_y->setVisible(show_custom);
    custom_direction_z->setVisible(show_custom);

    emit ParametersChanged();
}

void SpatialEffect3D::OnReverseChanged()
{
    effect_reverse = reverse_check->isChecked();
    emit ParametersChanged();
}

void SpatialEffect3D::OnCustomDirectionChanged()
{
    custom_direction.x = (float)custom_direction_x->value();
    custom_direction.y = (float)custom_direction_y->value();
    custom_direction.z = (float)custom_direction_z->value();

    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| Create Color Controls                                    |
\*---------------------------------------------------------*/
void SpatialEffect3D::CreateColorControls(QWidget* /* parent */)
{
    color_controls_group = new QGroupBox("Colors");
    QVBoxLayout* color_layout = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Rainbow Mode Toggle                                      |
    \*---------------------------------------------------------*/
    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    color_layout->addWidget(rainbow_mode_check);

    /*---------------------------------------------------------*\
    | Color Buttons Container                                  |
    \*---------------------------------------------------------*/
    color_buttons_widget = new QWidget();
    color_buttons_layout = new QHBoxLayout();
    color_buttons_widget->setLayout(color_buttons_layout);

    // Create initial color buttons
    for(unsigned int i = 0; i < colors.size(); i++)
    {
        CreateColorButton(colors[i]);
    }

    /*---------------------------------------------------------*\
    | Add/Remove Color Buttons                                 |
    \*---------------------------------------------------------*/
    add_color_button = new QPushButton("+");
    add_color_button->setMaximumSize(30, 30);
    add_color_button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");

    remove_color_button = new QPushButton("-");
    remove_color_button->setMaximumSize(30, 30);
    remove_color_button->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    remove_color_button->setEnabled(colors.size() > 1);

    color_buttons_layout->addWidget(add_color_button);
    color_buttons_layout->addWidget(remove_color_button);
    color_buttons_layout->addStretch();

    color_layout->addWidget(color_buttons_widget);
    color_controls_group->setLayout(color_layout);

    // Hide color buttons when rainbow mode is enabled
    color_buttons_widget->setVisible(!rainbow_mode);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnRainbowModeChanged);
    connect(add_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnRemoveColorClicked);
}

void SpatialEffect3D::CreateColorButton(RGBColor color)
{
    QPushButton* color_button = new QPushButton();
    color_button->setMinimumSize(40, 30);
    color_button->setMaximumSize(40, 30);
    color_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid #333;")
                              .arg((color >> 16) & 0xFF)
                              .arg((color >> 8) & 0xFF)
                              .arg(color & 0xFF));

    connect(color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnColorButtonClicked);

    color_buttons.push_back(color_button);

    // Insert before the add/remove buttons
    int insert_pos = color_buttons_layout->count() - 3; // Before +, -, and stretch
    if(insert_pos < 0) insert_pos = 0;
    color_buttons_layout->insertWidget(insert_pos, color_button);
}

void SpatialEffect3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* last_button = color_buttons.back();
        color_buttons_layout->removeWidget(last_button);
        color_buttons.pop_back();
        last_button->deleteLater();
    }
}

RGBColor SpatialEffect3D::GetRainbowColor(float hue)
{
    // Convert HSV to RGB (Hue: 0-360, Saturation: 1.0, Value: 1.0)
    hue = fmod(hue, 360.0f);
    if(hue < 0) hue += 360.0f;

    float c = 1.0f; // Chroma (since saturation = 1, value = 1)
    float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));

    float r, g, b;
    if(hue < 60) { r = c; g = x; b = 0; }
    else if(hue < 120) { r = x; g = c; b = 0; }
    else if(hue < 180) { r = 0; g = c; b = x; }
    else if(hue < 240) { r = 0; g = x; b = c; }
    else if(hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    // OpenRGB uses BGR format: 0x00BBGGRR
    return ((int)(b * 255) << 16) | ((int)(g * 255) << 8) | (int)(r * 255);
}

RGBColor SpatialEffect3D::GetColorAtPosition(float position)
{
    if(rainbow_mode)
    {
        return GetRainbowColor(position * 360.0f);
    }

    if(colors.empty())
    {
        return COLOR_WHITE;
    }

    if(colors.size() == 1)
    {
        return colors[0];
    }

    // Interpolate between colors
    float scaled_pos = position * (colors.size() - 1);
    int index = (int)scaled_pos;
    float frac = scaled_pos - index;

    if(index >= (int)colors.size() - 1)
    {
        return colors.back();
    }

    RGBColor color1 = colors[index];
    RGBColor color2 = colors[index + 1];

    // Linear interpolation - OpenRGB uses BGR format: 0x00BBGGRR
    int b1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int r1 = color1 & 0xFF;

    int b2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int r2 = color2 & 0xFF;

    int r = (int)(r1 + (r2 - r1) * frac);
    int g = (int)(g1 + (g2 - g1) * frac);
    int b = (int)(b1 + (b2 - b1) * frac);

    // Return in BGR format
    return (b << 16) | (g << 8) | r;
}

/*---------------------------------------------------------*\
| New Color Control Slots                                  |
\*---------------------------------------------------------*/
void SpatialEffect3D::OnRainbowModeChanged()
{
    rainbow_mode = rainbow_mode_check->isChecked();
    color_buttons_widget->setVisible(!rainbow_mode);
    emit ParametersChanged();
}

void SpatialEffect3D::OnAddColorClicked()
{
    // Add a new random color
    RGBColor new_color = GetRainbowColor(colors.size() * 60.0f); // Space colors around hue wheel
    colors.push_back(new_color);
    CreateColorButton(new_color);

    remove_color_button->setEnabled(colors.size() > 1);
    emit ParametersChanged();
}

void SpatialEffect3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        remove_color_button->setEnabled(colors.size() > 1);
        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnColorButtonClicked()
{
    QPushButton* clicked_button = qobject_cast<QPushButton*>(sender());
    if(!clicked_button) return;

    // Find which color button was clicked
    std::vector<QPushButton*>::iterator it = std::find(color_buttons.begin(), color_buttons.end(), clicked_button);
    if(it == color_buttons.end()) return;

    int index = std::distance(color_buttons.begin(), it);
    if(index >= (int)colors.size()) return;

    // Open color dialog
    QColorDialog color_dialog;
    QColor current_color = QColor((colors[index] >> 16) & 0xFF,
                                 (colors[index] >> 8) & 0xFF,
                                 colors[index] & 0xFF);
    color_dialog.setCurrentColor(current_color);

    if(color_dialog.exec() == QDialog::Accepted)
    {
        QColor new_color = color_dialog.currentColor();
        colors[index] = (new_color.red() << 16) | (new_color.green() << 8) | new_color.blue();

        // Update button color
        clicked_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid #333;")
                                    .arg(new_color.red())
                                    .arg(new_color.green())
                                    .arg(new_color.blue()));

        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnStartEffectClicked()
{
    effect_running = true;
    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);
    emit ParametersChanged();
}

void SpatialEffect3D::OnStopEffectClicked()
{
    effect_running = false;
    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);
    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| New getter/setter methods                                |
\*---------------------------------------------------------*/
void SpatialEffect3D::SetColors(const std::vector<RGBColor>& new_colors)
{
    colors = new_colors;
    if(colors.empty())
    {
        colors.push_back(COLOR_RED);
    }
}

std::vector<RGBColor> SpatialEffect3D::GetColors() const
{
    return colors;
}

void SpatialEffect3D::SetRainbowMode(bool enabled)
{
    rainbow_mode = enabled;
    if(rainbow_mode_check)
    {
        rainbow_mode_check->setChecked(enabled);
    }
}

bool SpatialEffect3D::GetRainbowMode() const
{
    return rainbow_mode;
}

void SpatialEffect3D::SetFrequency(unsigned int frequency)
{
    effect_frequency = frequency;
    if(frequency_slider)
    {
        frequency_slider->setValue(frequency);
    }
}

unsigned int SpatialEffect3D::GetFrequency() const
{
    return effect_frequency;
}