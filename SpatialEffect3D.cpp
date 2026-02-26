// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialEffect3D.h"
#include "Colors.h"
#include <cmath>
#include <algorithm>
#include <QSignalBlocker>

SpatialEffect3D::SpatialEffect3D(QWidget* parent) : QWidget(parent)
{
    effect_enabled = false;
    effect_running = false;
    effect_speed = 1;
    effect_brightness = 100;
    effect_frequency = 1;
    effect_size = 50;
    effect_scale = 200;
    scale_inverted = false;
    effect_fps = 30;
    rainbow_mode = false;
    rainbow_progress = 0.0f;
    boundary_prevalidated = false;

    effect_rotation_yaw = 0.0f;
    effect_rotation_pitch = 0.0f;
    effect_rotation_roll = 0.0f;

    reference_mode = REF_MODE_ROOM_CENTER;
    global_reference_point = {0.0f, 0.0f, 0.0f};
    custom_reference_point = {0.0f, 0.0f, 0.0f};
    use_custom_reference = false;

    colors.push_back(COLOR_RED);
    colors.push_back(COLOR_BLUE);

    effect_controls_group = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    size_slider = nullptr;
    scale_slider = nullptr;
    scale_invert_check = nullptr;
    fps_slider = nullptr;
    speed_label = nullptr;
    brightness_label = nullptr;
    frequency_label = nullptr;
    size_label = nullptr;
    scale_label = nullptr;
    fps_label = nullptr;

    rotation_yaw_slider = nullptr;
    rotation_pitch_slider = nullptr;
    rotation_roll_slider = nullptr;
    rotation_yaw_label = nullptr;
    rotation_pitch_label = nullptr;
    rotation_roll_label = nullptr;
    rotation_reset_button = nullptr;

    color_controls_group = nullptr;
    rainbow_mode_check = nullptr;
    color_buttons_widget = nullptr;
    color_buttons_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    start_effect_button = nullptr;
    stop_effect_button = nullptr;

    intensity_slider = nullptr;
    intensity_label = nullptr;
    sharpness_slider = nullptr;
    sharpness_label = nullptr;
    effect_intensity = 100;
    effect_sharpness = 100;

    scale_x_slider = nullptr;
    scale_x_label  = nullptr;
    scale_y_slider = nullptr;
    scale_y_label  = nullptr;
    scale_z_slider = nullptr;
    scale_z_label  = nullptr;
    effect_scale_x = 100;
    effect_scale_y = 100;
    effect_scale_z = 100;
}

SpatialEffect3D::~SpatialEffect3D()
{
}

void SpatialEffect3D::CreateCommonEffectControls(QWidget* parent, bool include_start_stop)
{
    effect_controls_group = new QGroupBox("Effect Controls");
    QVBoxLayout* main_layout = new QVBoxLayout();

    if(include_start_stop)
    {
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
    }

    QHBoxLayout* speed_layout = new QHBoxLayout();
    speed_layout->addWidget(new QLabel("Speed:"));
    speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(0, 200);
    speed_slider->setValue(effect_speed);
    speed_slider->setToolTip("Effect animation speed (uses logarithmic curve for smooth control)");
    speed_layout->addWidget(speed_slider);
    speed_label = new QLabel(QString::number(effect_speed));
    speed_label->setMinimumWidth(30);
    speed_layout->addWidget(speed_label);
    main_layout->addLayout(speed_layout);

    QHBoxLayout* brightness_layout = new QHBoxLayout();
    brightness_layout->addWidget(new QLabel("Brightness:"));
    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(1, 100);
    brightness_slider->setToolTip("Overall effect brightness (applied after intensity/sharpness)");
    brightness_slider->setValue(effect_brightness);
    brightness_layout->addWidget(brightness_slider);
    brightness_label = new QLabel(QString::number(effect_brightness));
    brightness_label->setMinimumWidth(30);
    brightness_layout->addWidget(brightness_label);
    main_layout->addLayout(brightness_layout);

    QHBoxLayout* frequency_layout = new QHBoxLayout();
    frequency_layout->addWidget(new QLabel("Frequency:"));
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(0, 200);
    frequency_slider->setValue(effect_frequency);
    frequency_slider->setToolTip("Pattern frequency - controls color wave/pattern density");
    frequency_layout->addWidget(frequency_slider);
    frequency_label = new QLabel(QString::number(effect_frequency));
    frequency_label->setMinimumWidth(30);
    frequency_layout->addWidget(frequency_label);
    main_layout->addLayout(frequency_layout);

    QHBoxLayout* size_layout = new QHBoxLayout();
    size_layout->addWidget(new QLabel("Size:"));
    size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(0, 200);
    size_slider->setValue(effect_size);
    size_slider->setToolTip("Pattern size - controls how tight/spread out the pattern is");
    size_layout->addWidget(size_slider);
    size_label = new QLabel(QString::number(effect_size));
    size_label->setMinimumWidth(30);
    size_layout->addWidget(size_label);
    main_layout->addLayout(size_layout);

    QHBoxLayout* scale_layout = new QHBoxLayout();
    scale_layout->addWidget(new QLabel("Scale:"));
    scale_slider = new QSlider(Qt::Horizontal);
    scale_slider->setRange(0, 250);
    scale_slider->setValue(effect_scale);
    scale_slider->setToolTip("Effect coverage: 0-200 = 0-100% of room (200=whole room), 201-250 = 101-150% (beyond room)");
    scale_layout->addWidget(scale_slider);
    scale_label = new QLabel(QString::number(effect_scale));
    scale_label->setMinimumWidth(30);
    scale_layout->addWidget(scale_label);
    scale_invert_check = new QCheckBox("Invert");
    scale_invert_check->setToolTip("Collapse effect toward the reference point instead of expanding outward.");
    scale_invert_check->setChecked(scale_inverted);
    scale_layout->addWidget(scale_invert_check);
    main_layout->addLayout(scale_layout);

    QHBoxLayout* fps_layout = new QHBoxLayout();
    fps_layout->addWidget(new QLabel("FPS:"));
    fps_slider = new QSlider(Qt::Horizontal);
    fps_slider->setRange(1, 120);
    fps_slider->setValue(effect_fps);
    fps_slider->setToolTip("Frames per second (1-60) - lower values reduce CPU usage");
    fps_layout->addWidget(fps_slider);
    fps_label = new QLabel(QString::number(effect_fps));
    fps_label->setMinimumWidth(30);
    fps_layout->addWidget(fps_label);
    main_layout->addLayout(fps_layout);

    QHBoxLayout* intensity_layout = new QHBoxLayout();
    intensity_layout->addWidget(new QLabel("Intensity:"));
    intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(0, 200);
    intensity_slider->setValue(effect_intensity);
    intensity_slider->setToolTip("Global intensity multiplier (0 = off, 100 = normal, 200 = 2x)");
    intensity_layout->addWidget(intensity_slider);
    intensity_label = new QLabel(QString::number(effect_intensity));
    intensity_label->setMinimumWidth(30);
    intensity_layout->addWidget(intensity_label);
    main_layout->addLayout(intensity_layout);

    QHBoxLayout* sharpness_layout = new QHBoxLayout();
    sharpness_layout->addWidget(new QLabel("Sharpness:"));
    sharpness_slider = new QSlider(Qt::Horizontal);
    sharpness_slider->setRange(0, 200);
    sharpness_slider->setValue(effect_sharpness);
    sharpness_slider->setToolTip("Edge contrast: lower = softer, higher = crisper (gamma-like)");
    sharpness_layout->addWidget(sharpness_slider);
    sharpness_label = new QLabel(QString::number(effect_sharpness));
    sharpness_label->setMinimumWidth(30);
    sharpness_layout->addWidget(sharpness_label);
    main_layout->addLayout(sharpness_layout);

    QGroupBox* axis_scale_group = new QGroupBox("Axis Scale");
    QVBoxLayout* axis_scale_layout = new QVBoxLayout();

    QHBoxLayout* scale_x_layout = new QHBoxLayout();
    scale_x_layout->addWidget(new QLabel("Width (X):"));
    scale_x_slider = new QSlider(Qt::Horizontal);
    scale_x_slider->setRange(1, 400);
    scale_x_slider->setValue((int)effect_scale_x);
    scale_x_slider->setToolTip("Stretch or squash the effect along the X axis (100% = normal)");
    scale_x_layout->addWidget(scale_x_slider);
    scale_x_label = new QLabel(QString::number(effect_scale_x) + "%");
    scale_x_label->setMinimumWidth(45);
    scale_x_layout->addWidget(scale_x_label);
    axis_scale_layout->addLayout(scale_x_layout);

    QHBoxLayout* scale_y_layout = new QHBoxLayout();
    scale_y_layout->addWidget(new QLabel("Height (Y):"));
    scale_y_slider = new QSlider(Qt::Horizontal);
    scale_y_slider->setRange(1, 400);
    scale_y_slider->setValue((int)effect_scale_y);
    scale_y_slider->setToolTip("Stretch or squash the effect along the Y axis (100% = normal)");
    scale_y_layout->addWidget(scale_y_slider);
    scale_y_label = new QLabel(QString::number(effect_scale_y) + "%");
    scale_y_label->setMinimumWidth(45);
    scale_y_layout->addWidget(scale_y_label);
    axis_scale_layout->addLayout(scale_y_layout);

    QHBoxLayout* scale_z_layout = new QHBoxLayout();
    scale_z_layout->addWidget(new QLabel("Depth (Z):"));
    scale_z_slider = new QSlider(Qt::Horizontal);
    scale_z_slider->setRange(1, 400);
    scale_z_slider->setValue((int)effect_scale_z);
    scale_z_slider->setToolTip("Stretch or squash the effect along the Z axis (100% = normal)");
    scale_z_layout->addWidget(scale_z_slider);
    scale_z_label = new QLabel(QString::number(effect_scale_z) + "%");
    scale_z_label->setMinimumWidth(45);
    scale_z_layout->addWidget(scale_z_label);
    axis_scale_layout->addLayout(scale_z_layout);

    axis_scale_group->setLayout(axis_scale_layout);
    main_layout->addWidget(axis_scale_group);

    QGroupBox* rotation_group = new QGroupBox("3D Rotation (around Origin)");
    QVBoxLayout* rotation_layout = new QVBoxLayout();
    
    QHBoxLayout* yaw_layout = new QHBoxLayout();
    yaw_layout->addWidget(new QLabel("Yaw (Horizontal):"));
    rotation_yaw_slider = new QSlider(Qt::Horizontal);
    rotation_yaw_slider->setRange(0, 360);
    rotation_yaw_slider->setValue((int)effect_rotation_yaw);
    rotation_yaw_slider->setToolTip("Horizontal rotation around Y axis (0-360°)");
    rotation_yaw_label = new QLabel(QString::number((int)effect_rotation_yaw) + "°");
    rotation_yaw_label->setMinimumWidth(50);
    yaw_layout->addWidget(rotation_yaw_slider);
    yaw_layout->addWidget(rotation_yaw_label);
    rotation_layout->addLayout(yaw_layout);
    
    QHBoxLayout* pitch_layout = new QHBoxLayout();
    pitch_layout->addWidget(new QLabel("Pitch (Vertical):"));
    rotation_pitch_slider = new QSlider(Qt::Horizontal);
    rotation_pitch_slider->setRange(0, 360);
    rotation_pitch_slider->setValue((int)effect_rotation_pitch);
    rotation_pitch_slider->setToolTip("Vertical rotation around X axis (0-360°)");
    rotation_pitch_label = new QLabel(QString::number((int)effect_rotation_pitch) + "°");
    rotation_pitch_label->setMinimumWidth(50);
    pitch_layout->addWidget(rotation_pitch_slider);
    pitch_layout->addWidget(rotation_pitch_label);
    rotation_layout->addLayout(pitch_layout);
    
    QHBoxLayout* roll_layout = new QHBoxLayout();
    roll_layout->addWidget(new QLabel("Roll (Twist):"));
    rotation_roll_slider = new QSlider(Qt::Horizontal);
    rotation_roll_slider->setRange(0, 360);
    rotation_roll_slider->setValue((int)effect_rotation_roll);
    rotation_roll_slider->setToolTip("Twist rotation around Z axis (0-360°)");
    rotation_roll_label = new QLabel(QString::number((int)effect_rotation_roll) + "°");
    rotation_roll_label->setMinimumWidth(50);
    roll_layout->addWidget(rotation_roll_slider);
    roll_layout->addWidget(rotation_roll_label);
    rotation_layout->addLayout(roll_layout);
    
    rotation_reset_button = new QPushButton("Reset Rotation");
    rotation_reset_button->setToolTip("Reset all rotations to 0°");
    rotation_layout->addWidget(rotation_reset_button);
    
    rotation_group->setLayout(rotation_layout);
    main_layout->addWidget(rotation_group);

    CreateColorControls();
    main_layout->addWidget(color_controls_group);

    effect_controls_group->setLayout(main_layout);

    connect(speed_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(size_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(fps_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    if(scale_invert_check)
    {
        connect(scale_invert_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);
    }
    connect(rotation_yaw_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnRotationChanged);
    connect(rotation_pitch_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnRotationChanged);
    connect(rotation_roll_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnRotationChanged);
    connect(rotation_reset_button, &QPushButton::clicked, this, &SpatialEffect3D::OnRotationResetClicked);
    connect(intensity_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(sharpness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_x_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_y_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_z_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);

    ApplyControlVisibility();

    connect(rotation_yaw_slider, &QSlider::valueChanged, rotation_yaw_label, [this](int value) {
        rotation_yaw_label->setText(QString::number(value) + "°");
        effect_rotation_yaw = (float)value;
    });
    connect(rotation_pitch_slider, &QSlider::valueChanged, rotation_pitch_label, [this](int value) {
        rotation_pitch_label->setText(QString::number(value) + "°");
        effect_rotation_pitch = (float)value;
    });
    connect(rotation_roll_slider, &QSlider::valueChanged, rotation_roll_label, [this](int value) {
        rotation_roll_label->setText(QString::number(value) + "°");
        effect_rotation_roll = (float)value;
    });
    
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
    connect(size_slider, &QSlider::valueChanged, size_label, [this](int value) {
        size_label->setText(QString::number(value));
        effect_size = value;
    });
    connect(scale_slider, &QSlider::valueChanged, scale_label, [this](int value) {
        scale_label->setText(QString::number(value));
        effect_scale = value;
    });
    connect(fps_slider, &QSlider::valueChanged, fps_label, [this](int value) {
        fps_label->setText(QString::number(value));
        effect_fps = value;
    });
    connect(intensity_slider, &QSlider::valueChanged, intensity_label, [this](int value) {
        intensity_label->setText(QString::number(value));
        effect_intensity = value;
    });
    connect(sharpness_slider, &QSlider::valueChanged, sharpness_label, [this](int value) {
        sharpness_label->setText(QString::number(value));
        effect_sharpness = value;
    });
    connect(scale_x_slider, &QSlider::valueChanged, scale_x_label, [this](int value) {
        scale_x_label->setText(QString::number(value) + "%");
        effect_scale_x = (unsigned int)value;
    });
    connect(scale_y_slider, &QSlider::valueChanged, scale_y_label, [this](int value) {
        scale_y_label->setText(QString::number(value) + "%");
        effect_scale_y = (unsigned int)value;
    });
    connect(scale_z_slider, &QSlider::valueChanged, scale_z_label, [this](int value) {
        scale_z_label->setText(QString::number(value) + "%");
        effect_scale_z = (unsigned int)value;
    });

    AddWidgetToParent(effect_controls_group, parent);
}

void SpatialEffect3D::ApplyAxisScale(float& x, float& y, float& z, const GridContext3D& grid) const
{
    if(effect_scale_x == 100 && effect_scale_y == 100 && effect_scale_z == 100)
    {
        return;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    if(effect_scale_x != 100)
    {
        x = origin.x + (x - origin.x) * (100.0f / (float)effect_scale_x);
    }
    if(effect_scale_y != 100)
    {
        y = origin.y + (y - origin.y) * (100.0f / (float)effect_scale_y);
    }
    if(effect_scale_z != 100)
    {
        z = origin.z + (z - origin.z) * (100.0f / (float)effect_scale_z);
    }
}

void SpatialEffect3D::ApplyEffectRotation(float& x, float& y, float& z, const GridContext3D& grid) const
{
    if(effect_rotation_yaw == 0.0f && effect_rotation_pitch == 0.0f && effect_rotation_roll == 0.0f)
    {
        return;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated = TransformPointByRotation(x, y, z, origin);
    x = rotated.x;
    y = rotated.y;
    z = rotated.z;
}

void SpatialEffect3D::AddWidgetToParent(QWidget* w, QWidget* container)
{
    if(w && container && container->layout())
    {
        container->layout()->addWidget(w);
    }
}

void SpatialEffect3D::CreateColorControls()
{
    color_controls_group = new QGroupBox("Colors");
    QVBoxLayout* color_layout = new QVBoxLayout();

    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    rainbow_mode_check->setToolTip("Use rainbow gradient instead of custom colors");
    color_layout->addWidget(rainbow_mode_check);

    color_buttons_widget = new QWidget();
    color_buttons_layout = new QHBoxLayout();
    color_buttons_widget->setLayout(color_buttons_layout);

    for(unsigned int i = 0; i < colors.size(); i++)
    {
        CreateColorButton(colors[i]);
    }

    add_color_button = new QPushButton("+");
    add_color_button->setMaximumSize(30, 30);
    add_color_button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    add_color_button->setToolTip("Add a new color stop");

    remove_color_button = new QPushButton("-");
    remove_color_button->setMaximumSize(30, 30);
    remove_color_button->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    remove_color_button->setEnabled(colors.size() > 1);
    remove_color_button->setToolTip("Remove the last color stop");

    color_buttons_layout->addWidget(add_color_button);
    color_buttons_layout->addWidget(remove_color_button);
    color_buttons_layout->addStretch();

    color_layout->addWidget(color_buttons_widget);
    color_controls_group->setLayout(color_layout);

    color_buttons_widget->setVisible(!rainbow_mode);

    connect(rainbow_mode_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnRainbowModeChanged);
    connect(add_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnRemoveColorClicked);
}

void SpatialEffect3D::CreateColorButton(RGBColor color)
{
    QPushButton* color_button = new QPushButton();
    color_button->setMinimumSize(40, 30);
    color_button->setMaximumSize(40, 30);
    color_button->setToolTip("Click to change color");
    color_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid #333;")
                              .arg((color >> 16) & 0xFF)
                              .arg((color >> 8) & 0xFF)
                              .arg(color & 0xFF));

    connect(color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnColorButtonClicked);

    color_buttons.push_back(color_button);

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
    hue = std::fmod(hue, 360.0f);
    if(hue < 0) hue += 360.0f;

    float c = 1.0f; // Chroma (since saturation = 1, value = 1)
    float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));

    float r, g, b;
    if(hue < 60) { r = c; g = x; b = 0; }
    else if(hue < 120) { r = x; g = c; b = 0; }
    else if(hue < 180) { r = 0; g = c; b = x; }
    else if(hue < 240) { r = 0; g = x; b = c; }
    else if(hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    /* OpenRGB BGR: 0x00BBGGRR */
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

    float scaled_pos = position * (colors.size() - 1);
    int index = (int)scaled_pos;
    float frac = scaled_pos - index;

    if(index >= (int)colors.size() - 1)
    {
        return colors.back();
    }

    RGBColor color1 = colors[index];
    RGBColor color2 = colors[index + 1];

    int b1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int r1 = color1 & 0xFF;

    int b2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int r2 = color2 & 0xFF;

    int r = (int)(r1 + (r2 - r1) * frac);
    int g = (int)(g1 + (g2 - g1) * frac);
    int b = (int)(b1 + (b2 - b1) * frac);

    return (b << 16) | (g << 8) | r;
}

void SpatialEffect3D::OnRainbowModeChanged()
{
    rainbow_mode = rainbow_mode_check->isChecked();
    color_buttons_widget->setVisible(!rainbow_mode);
    emit ParametersChanged();
}

void SpatialEffect3D::OnAddColorClicked()
{
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

    std::vector<QPushButton*>::iterator it = std::find(color_buttons.begin(), color_buttons.end(), clicked_button);
    if(it == color_buttons.end()) return;

    int index = std::distance(color_buttons.begin(), it);
    if(index >= (int)colors.size()) return;

    QColorDialog color_dialog;
    QColor current_color = QColor((colors[index] >> 16) & 0xFF,
                                 (colors[index] >> 8) & 0xFF,
                                 colors[index] & 0xFF);
    color_dialog.setCurrentColor(current_color);

    if(color_dialog.exec() == QDialog::Accepted)
    {
        QColor new_color = color_dialog.currentColor();
        colors[index] = (new_color.red() << 16) | (new_color.green() << 8) | new_color.blue();

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

Vector3D SpatialEffect3D::GetEffectOriginGrid(const GridContext3D& grid) const
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
            return {grid.center_x, grid.center_y, grid.center_z};
    }
}

float SpatialEffect3D::GetNormalizedSpeed() const
{
    float normalized = effect_speed / 200.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedFrequency() const
{
    float normalized = effect_frequency / 200.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedSize() const
{
    return (effect_size / 200.0f) * 3.0f;
}

float SpatialEffect3D::GetNormalizedScale() const
{
    float normalized;

    if(effect_scale <= 200)
    {
        normalized = effect_scale / 200.0f;  // 0.0 to 1.0
    }
    else
    {
        normalized = 1.0f + ((effect_scale - 200) / 100.0f);  // 1.01 to 1.5
    }

    if(scale_inverted)
    {
        if(normalized <= 1.0f)
        {
            normalized = 1.0f - normalized;
        }
        else
        {
            float extra = normalized - 1.0f;
            normalized = std::max(0.0f, 1.0f - extra);
        }
    }

    return std::max(0.0f, normalized);
}

unsigned int SpatialEffect3D::GetTargetFPS() const
{
    return effect_fps;
}

float SpatialEffect3D::GetScaledSpeed() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float speed_scale = (info.default_speed_scale > 0.0f) ? info.default_speed_scale : 10.0f;
    return GetNormalizedSpeed() * speed_scale;
}

float SpatialEffect3D::GetScaledFrequency() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float freq_scale = (info.default_frequency_scale > 0.0f) ? info.default_frequency_scale : 10.0f;
    return GetNormalizedFrequency() * freq_scale;
}

float SpatialEffect3D::CalculateProgress(float time) const
{
    return time * GetScaledSpeed();
}

RGBColor SpatialEffect3D::PostProcessColorGrid(RGBColor color) const
{
    float intensity_normalized = effect_intensity / 200.0f;
    float intensity_mul = std::pow(intensity_normalized, 0.7f) * 1.7f;
    float brightness_mul = effect_brightness / 100.0f;
    float factor = intensity_mul * brightness_mul;
    if(factor <= 0.0f) return 0x00000000;

    unsigned char r = color & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = (color >> 16) & 0xFF;
    int rr = (int)(r * factor); if(rr > 255) rr = 255;
    int gg = (int)(g * factor); if(gg > 255) gg = 255;
    int bb = (int)(b * factor); if(bb > 255) bb = 255;
    return (bb << 16) | (gg << 8) | rr;
}

Vector3D SpatialEffect3D::TransformPointByRotation(float x, float y, float z, const Vector3D& origin) const
{
    float tx = x - origin.x;
    float ty = y - origin.y;
    float tz = z - origin.z;
    
    float yaw_rad = effect_rotation_yaw * 3.14159265359f / 180.0f;
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float nx = tx * cos_yaw - tz * sin_yaw;
    float nz = tx * sin_yaw + tz * cos_yaw;
    float ny = ty;

    float pitch_rad = effect_rotation_pitch * 3.14159265359f / 180.0f;
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);
    float py = ny * cos_pitch - nz * sin_pitch;
    float pz = ny * sin_pitch + nz * cos_pitch;
    float px = nx;

    float roll_rad = effect_rotation_roll * 3.14159265359f / 180.0f;
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);
    float fx = px * cos_roll - py * sin_roll;
    float fy = px * sin_roll + py * cos_roll;
    float fz = pz;

    return {fx + origin.x, fy + origin.y, fz + origin.z};
}

bool SpatialEffect3D::IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z) const
{
    if(boundary_prevalidated)
    {
        return true;
    }
    float scale_radius = GetNormalizedScale() * 10.0f;
    float distance_from_origin = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    return distance_from_origin <= scale_radius;
}

bool SpatialEffect3D::IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    float half_width = grid.width / 2.0f;
    float half_depth = grid.depth / 2.0f;
    float half_height = grid.height / 2.0f;
    float max_distance_from_center = sqrt(half_width * half_width +
                                         half_depth * half_depth +
                                         half_height * half_height);

    float scale_percentage = GetNormalizedScale();
    float scale_radius = max_distance_from_center * scale_percentage;

    float distance_from_origin = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    return distance_from_origin <= scale_radius;
}

void SpatialEffect3D::UpdateCommonEffectParams(SpatialEffectParams& /* params */)
{
}

void SpatialEffect3D::SetControlGroupVisibility(QSlider* slider, QLabel* value_label, const QString& label_text, bool visible)
{
    if(slider && value_label)
    {
        slider->setVisible(visible);
        value_label->setVisible(visible);

        QWidget* parent = slider->parentWidget();
        if(parent)
        {
            QList<QLabel*> labels = parent->findChildren<QLabel*>();
            for(int i = 0; i < labels.size(); i++)
            {
                QLabel* label = labels[i];
                if(label->text() == label_text)
                {
                    label->setVisible(visible);
                    break;
                }
            }
        }

        if(slider == scale_slider && scale_invert_check)
        {
            scale_invert_check->setVisible(visible);
        }
    }
}

void SpatialEffect3D::ApplyControlVisibility()
{
    EffectInfo3D info = GetEffectInfo();

    bool is_versioned_effect = (info.info_version >= 2);

    bool show_speed = is_versioned_effect ? info.show_speed_control : true;
    bool show_brightness = is_versioned_effect ? info.show_brightness_control : true;
    bool show_frequency = is_versioned_effect ? info.show_frequency_control : true;
    bool show_size = is_versioned_effect ? info.show_size_control : true;
    bool show_scale = is_versioned_effect ? info.show_scale_control : true;
    bool show_fps = is_versioned_effect ? info.show_fps_control : true;
    bool show_colors = is_versioned_effect ? info.show_color_controls : true;

    SetControlGroupVisibility(speed_slider, speed_label, "Speed:", show_speed);
    SetControlGroupVisibility(brightness_slider, brightness_label, "Brightness:", show_brightness);
    SetControlGroupVisibility(frequency_slider, frequency_label, "Frequency:", show_frequency);
    SetControlGroupVisibility(size_slider, size_label, "Size:", show_size);
    SetControlGroupVisibility(scale_slider, scale_label, "Scale:", show_scale);
    SetControlGroupVisibility(fps_slider, fps_label, "FPS:", show_fps);

    if(color_controls_group)
    {
        color_controls_group->setVisible(show_colors);
    }
}

void SpatialEffect3D::OnParameterChanged()
{
    if(speed_slider)
    {
        effect_speed = speed_slider->value();
        if(speed_label)
        {
            speed_label->setText(QString::number(effect_speed));
        }
    }

    if(brightness_slider && brightness_label)
    {
        effect_brightness = brightness_slider->value();
        brightness_label->setText(QString::number(effect_brightness));
    }

    if(frequency_slider && frequency_label)
    {
        effect_frequency = frequency_slider->value();
        frequency_label->setText(QString::number(effect_frequency));
    }

    if(size_slider && size_label)
    {
        effect_size = size_slider->value();
        size_label->setText(QString::number(effect_size));
    }

    if(scale_slider && scale_label)
    {
        effect_scale = scale_slider->value();
        scale_label->setText(QString::number(effect_scale));
    }

    if(scale_invert_check)
    {
        scale_inverted = scale_invert_check->isChecked();
    }

    if(fps_slider && fps_label)
    {
        effect_fps = fps_slider->value();
        fps_label->setText(QString::number(effect_fps));
    }
    if(intensity_slider)
    {
        effect_intensity = intensity_slider->value();
        if(intensity_label)
        {
            intensity_label->setText(QString::number(effect_intensity));
        }
    }
    if(sharpness_slider)
    {
        effect_sharpness = sharpness_slider->value();
        if(sharpness_label)
        {
            sharpness_label->setText(QString::number(effect_sharpness));
        }
    }
    emit ParametersChanged();
}

void SpatialEffect3D::OnRotationChanged()
{
    if(rotation_yaw_slider)
    {
        effect_rotation_yaw = (float)rotation_yaw_slider->value();
        if(rotation_yaw_label)
        {
            rotation_yaw_label->setText(QString::number((int)effect_rotation_yaw) + "°");
        }
    }
    if(rotation_pitch_slider)
    {
        effect_rotation_pitch = (float)rotation_pitch_slider->value();
        if(rotation_pitch_label)
        {
            rotation_pitch_label->setText(QString::number((int)effect_rotation_pitch) + "°");
        }
    }
    if(rotation_roll_slider)
    {
        effect_rotation_roll = (float)rotation_roll_slider->value();
        if(rotation_roll_label)
        {
            rotation_roll_label->setText(QString::number((int)effect_rotation_roll) + "°");
        }
    }
    emit ParametersChanged();
}

void SpatialEffect3D::OnRotationResetClicked()
{
    effect_rotation_yaw = 0.0f;
    effect_rotation_pitch = 0.0f;
    effect_rotation_roll = 0.0f;
    
    if(rotation_yaw_slider)
    {
        rotation_yaw_slider->setValue(0);
    }
    if(rotation_pitch_slider)
    {
        rotation_pitch_slider->setValue(0);
    }
    if(rotation_roll_slider)
    {
        rotation_roll_slider->setValue(0);
    }
    
    emit ParametersChanged();
}

nlohmann::json SpatialEffect3D::SaveSettings() const
{
    nlohmann::json j;

    j["speed"] = effect_speed;
    j["brightness"] = effect_brightness;
    j["frequency"] = effect_frequency;
    j["size"] = effect_size;
    j["scale_value"] = effect_scale;
    j["scale_inverted"] = scale_inverted;
    j["rainbow_mode"] = rainbow_mode;
    j["intensity"] = effect_intensity;
    j["sharpness"] = effect_sharpness;
    j["axis_scale_x"] = effect_scale_x;
    j["axis_scale_y"] = effect_scale_y;
    j["axis_scale_z"] = effect_scale_z;
    j["rotation_yaw"] = effect_rotation_yaw;
    j["rotation_pitch"] = effect_rotation_pitch;
    j["rotation_roll"] = effect_rotation_roll;

    nlohmann::json colors_array = nlohmann::json::array();
    for(size_t i = 0; i < colors.size(); i++)
    {
        RGBColor color = colors[i];
        colors_array.push_back({
            {"r", RGBGetRValue(color)},
            {"g", RGBGetGValue(color)},
            {"b", RGBGetBValue(color)}
        });
    }
    j["colors"] = colors_array;

    j["reference_mode"] = (int)reference_mode;
    j["global_ref_x"] = global_reference_point.x;
    j["global_ref_y"] = global_reference_point.y;
    j["global_ref_z"] = global_reference_point.z;
    j["custom_ref_x"] = custom_reference_point.x;
    j["custom_ref_y"] = custom_reference_point.y;
    j["custom_ref_z"] = custom_reference_point.z;
    j["use_custom_ref"] = use_custom_reference;

    return j;
}

void SpatialEffect3D::LoadSettings(const nlohmann::json& settings)
{
    if(settings.contains("speed"))
        SetSpeed(settings["speed"].get<unsigned int>());

    if(settings.contains("brightness"))
        SetBrightness(settings["brightness"].get<unsigned int>());

    if(settings.contains("frequency"))
        SetFrequency(settings["frequency"].get<unsigned int>());

    if(settings.contains("rainbow_mode"))
        SetRainbowMode(settings["rainbow_mode"].get<bool>());

    if(settings.contains("intensity"))
        effect_intensity = settings["intensity"].get<unsigned int>();
    if(settings.contains("sharpness"))
        effect_sharpness = settings["sharpness"].get<unsigned int>();

    if(settings.contains("axis_scale_x"))
        effect_scale_x = std::clamp(settings["axis_scale_x"].get<unsigned int>(), 1u, 400u);
    if(settings.contains("axis_scale_y"))
        effect_scale_y = std::clamp(settings["axis_scale_y"].get<unsigned int>(), 1u, 400u);
    if(settings.contains("axis_scale_z"))
        effect_scale_z = std::clamp(settings["axis_scale_z"].get<unsigned int>(), 1u, 400u);
    
    if(settings.contains("rotation_yaw"))
        effect_rotation_yaw = settings["rotation_yaw"].get<float>();
    else
        effect_rotation_yaw = 0.0f;
    if(settings.contains("rotation_pitch"))
        effect_rotation_pitch = settings["rotation_pitch"].get<float>();
    else
        effect_rotation_pitch = 0.0f;
    if(settings.contains("rotation_roll"))
        effect_rotation_roll = settings["rotation_roll"].get<float>();
    else
        effect_rotation_roll = 0.0f;
    
    if(rotation_yaw_slider)
    {
        rotation_yaw_slider->setValue((int)effect_rotation_yaw);
    }
    if(rotation_pitch_slider)
    {
        rotation_pitch_slider->setValue((int)effect_rotation_pitch);
    }
    if(rotation_roll_slider)
    {
        rotation_roll_slider->setValue((int)effect_rotation_roll);
    }
    if(settings.contains("size"))
        effect_size = std::clamp(settings["size"].get<unsigned int>(), 0u, 200u);
    if(settings.contains("scale_value"))
        effect_scale = std::clamp(settings["scale_value"].get<unsigned int>(), 0u, 250u);
    if(settings.contains("scale_inverted"))
        scale_inverted = settings["scale_inverted"].get<bool>();


    if(settings.contains("colors"))
    {
        std::vector<RGBColor> loaded_colors;
        const nlohmann::json& colors_array = settings["colors"];
        for(size_t i = 0; i < colors_array.size(); i++)
        {
            const nlohmann::json& color_json = colors_array[i];
            unsigned char r = color_json["r"].get<unsigned char>();
            unsigned char g = color_json["g"].get<unsigned char>();
            unsigned char b = color_json["b"].get<unsigned char>();
            loaded_colors.push_back(ToRGBColor(r, g, b));
        }
        SetColors(loaded_colors);
    }

    if(settings.contains("reference_mode"))
        SetReferenceMode((ReferenceMode)settings["reference_mode"].get<int>());

    if(settings.contains("global_ref_x") && settings.contains("global_ref_y") && settings.contains("global_ref_z"))
    {
        Vector3D ref_point;
        ref_point.x = settings["global_ref_x"].get<float>();
        ref_point.y = settings["global_ref_y"].get<float>();
        ref_point.z = settings["global_ref_z"].get<float>();
        SetGlobalReferencePoint(ref_point);
    }

    if(settings.contains("custom_ref_x") && settings.contains("custom_ref_y") && settings.contains("custom_ref_z"))
    {
        Vector3D ref_point;
        ref_point.x = settings["custom_ref_x"].get<float>();
        ref_point.y = settings["custom_ref_y"].get<float>();
        ref_point.z = settings["custom_ref_z"].get<float>();
        SetCustomReferencePoint(ref_point);
    }

    if(settings.contains("use_custom_ref"))
        SetUseCustomReference(settings["use_custom_ref"].get<bool>());

    if(speed_slider)
    {
        QSignalBlocker blocker(speed_slider);
        speed_slider->setValue(effect_speed);
    }
    if(speed_label)
    {
        speed_label->setText(QString::number(effect_speed));
    }

    if(brightness_slider)
    {
        QSignalBlocker blocker(brightness_slider);
        brightness_slider->setValue(effect_brightness);
    }
    if(brightness_label)
    {
        brightness_label->setText(QString::number(effect_brightness));
    }

    if(frequency_slider)
    {
        QSignalBlocker blocker(frequency_slider);
        frequency_slider->setValue(effect_frequency);
    }
    if(frequency_label)
    {
        frequency_label->setText(QString::number(effect_frequency));
    }

    if(rainbow_mode_check)
    {
        QSignalBlocker blocker(rainbow_mode_check);
        rainbow_mode_check->setChecked(rainbow_mode);
    }

    if(intensity_slider)
    {
        QSignalBlocker blocker(intensity_slider);
        intensity_slider->setValue(effect_intensity);
    }

    if(sharpness_slider)
    {
        QSignalBlocker blocker(sharpness_slider);
        sharpness_slider->setValue(effect_sharpness);
    }

    if(size_slider)
    {
        QSignalBlocker blocker(size_slider);
        size_slider->setValue(effect_size);
    }
    if(size_label)
    {
        size_label->setText(QString::number(effect_size));
    }

    if(scale_slider)
    {
        QSignalBlocker blocker(scale_slider);
        scale_slider->setValue(effect_scale);
    }
    if(scale_label)
    {
        scale_label->setText(QString::number(effect_scale));
    }

    if(scale_invert_check)
    {
        QSignalBlocker blocker(scale_invert_check);
        scale_invert_check->setChecked(scale_inverted);
    }

    if(fps_slider)
    {
        QSignalBlocker blocker(fps_slider);
        fps_slider->setValue(effect_fps);
    }
    if(fps_label)
    {
        fps_label->setText(QString::number(effect_fps));
    }

    if(scale_x_slider)
    {
        QSignalBlocker blocker(scale_x_slider);
        scale_x_slider->setValue((int)effect_scale_x);
    }
    if(scale_x_label)
    {
        scale_x_label->setText(QString::number(effect_scale_x) + "%");
    }
    if(scale_y_slider)
    {
        QSignalBlocker blocker(scale_y_slider);
        scale_y_slider->setValue((int)effect_scale_y);
    }
    if(scale_y_label)
    {
        scale_y_label->setText(QString::number(effect_scale_y) + "%");
    }
    if(scale_z_slider)
    {
        QSignalBlocker blocker(scale_z_slider);
        scale_z_slider->setValue((int)effect_scale_z);
    }
    if(scale_z_label)
    {
        scale_z_label->setText(QString::number(effect_scale_z) + "%");
    }
}

void SpatialEffect3D::SetScaleInverted(bool inverted)
{
    if(scale_inverted == inverted)
    {
        return;
    }

    scale_inverted = inverted;
    if(scale_invert_check)
    {
        QSignalBlocker blocker(scale_invert_check);
        scale_invert_check->setChecked(inverted);
    }
    emit ParametersChanged();
}


