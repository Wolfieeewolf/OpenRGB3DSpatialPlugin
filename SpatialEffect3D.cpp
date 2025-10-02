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

    // Initialize axis parameters
    effect_axis = AXIS_RADIAL;
    effect_reverse = false;

    // Initialize default colors
    colors.push_back(COLOR_RED);
    colors.push_back(COLOR_BLUE);

    effect_controls_group = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    speed_label = nullptr;
    brightness_label = nullptr;
    frequency_label = nullptr;

    // Axis controls
    axis_combo = nullptr;
    reverse_check = nullptr;

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
    | Axis control                                             |
    \*---------------------------------------------------------*/
    QHBoxLayout* axis_layout = new QHBoxLayout();
    axis_layout->addWidget(new QLabel("Axis:"));
    axis_combo = new QComboBox();
    axis_combo->addItem("Radial");
    axis_combo->addItem("X-Axis (Left to Right)");
    axis_combo->addItem("Y-Axis (Floor to Ceiling)");
    axis_combo->addItem("Z-Axis (Front to Back)");
    axis_combo->setCurrentIndex(effect_axis);
    axis_layout->addWidget(axis_combo);

    reverse_check = new QCheckBox("Reverse");
    reverse_check->setChecked(effect_reverse);
    axis_layout->addWidget(reverse_check);
    axis_layout->addStretch();
    main_layout->addLayout(axis_layout);

    /*---------------------------------------------------------*\
    | Create and add color controls                            |
    \*---------------------------------------------------------*/
    CreateColorControls(parent);
    main_layout->addWidget(color_controls_group);

    effect_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(speed_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpatialEffect3D::OnAxisChanged);
    connect(reverse_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnReverseChanged);

    // Effect control buttons - NOT connected here!
    // The parent tab needs to connect these to its on_start_effect_clicked/on_stop_effect_clicked handlers
    // to actually start the effect timer

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

/*---------------------------------------------------------*\
| Reference Point Methods                                  |
\*---------------------------------------------------------*/
void SpatialEffect3D::SetReferenceMode(ReferenceMode mode)
{
    reference_mode = mode;
}

ReferenceMode SpatialEffect3D::GetReferenceMode() const
{
    return reference_mode;
}

void SpatialEffect3D::SetGlobalReferencePoint(const Vector3D& point)
{
    global_reference_point = point;
}

Vector3D SpatialEffect3D::GetGlobalReferencePoint() const
{
    return global_reference_point;
}

void SpatialEffect3D::SetCustomReferencePoint(const Vector3D& point)
{
    custom_reference_point = point;
}

void SpatialEffect3D::SetUseCustomReference(bool use_custom)
{
    use_custom_reference = use_custom;
}

Vector3D SpatialEffect3D::GetEffectOrigin() const
{
    if(use_custom_reference)
    {
        return custom_reference_point;
    }

    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_ROOM_CENTER:
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

/*---------------------------------------------------------*\
| Parameter Update Methods                                 |
\*---------------------------------------------------------*/
void SpatialEffect3D::UpdateCommonEffectParams(SpatialEffectParams& /* params */)
{
    // Empty implementation - old 3D controls removed
}

void SpatialEffect3D::OnParameterChanged()
{
    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| Axis Control Slots                                       |
\*---------------------------------------------------------*/
void SpatialEffect3D::OnAxisChanged()
{
    if(axis_combo)
    {
        effect_axis = (EffectAxis)axis_combo->currentIndex();
    }
    emit ParametersChanged();
}

void SpatialEffect3D::OnReverseChanged()
{
    if(reverse_check)
    {
        effect_reverse = reverse_check->isChecked();
    }
    emit ParametersChanged();
}