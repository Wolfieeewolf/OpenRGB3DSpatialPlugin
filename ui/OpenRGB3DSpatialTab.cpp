/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab.cpp                                   |
|                                                           |
|   Main UI tab for 3D spatial control                     |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "ControllerLayout3D.h"
#include <QColorDialog>
#include <QDebug>

OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm)
{
    effects = new SpatialEffects();

    current_color_start = 0xFF0000;
    current_color_end = 0x0000FF;

    SetupUI();
    LoadDevices();
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
    if(effects->IsRunning())
    {
        effects->StopEffect();
    }

    delete effects;

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        delete controller_transforms[i];
    }
    controller_transforms.clear();
}

void OpenRGB3DSpatialTab::SetupUI()
{
    QHBoxLayout* main_layout = new QHBoxLayout(this);

    QVBoxLayout* left_panel = new QVBoxLayout();

    viewport = new LEDViewport3D();
    connect(viewport, SIGNAL(ControllerSelected(int)), this, SLOT(on_controller_selected(int)));
    connect(viewport, SIGNAL(ControllerPositionChanged(int,float,float,float)),
            this, SLOT(on_controller_position_changed(int,float,float,float)));
    left_panel->addWidget(viewport, 1);

    QLabel* controls_label = new QLabel("Camera: Middle mouse to rotate | Shift+Middle mouse to pan | Scroll to zoom");
    controls_label->setStyleSheet("padding: 5px; background: #333; color: #aaa;");
    left_panel->addWidget(controls_label);

    main_layout->addLayout(left_panel, 3);

    QVBoxLayout* right_panel = new QVBoxLayout();

    QGroupBox* controller_group = new QGroupBox("Controllers");
    QVBoxLayout* controller_layout = new QVBoxLayout();

    controller_list = new QListWidget();
    controller_list->setMaximumHeight(150);
    connect(controller_list, &QListWidget::currentRowChanged, [this](int row) {
        if(row >= 0)
        {
            viewport->SelectController(row);
            on_controller_selected(row);
        }
    });
    controller_layout->addWidget(controller_list);

    QGridLayout* position_layout = new QGridLayout();
    position_layout->addWidget(new QLabel("Position X:"), 0, 0);
    pos_x_spin = new QDoubleSpinBox();
    pos_x_spin->setRange(-100, 100);
    pos_x_spin->setDecimals(1);
    connect(pos_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.x = value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_x_spin, 0, 1);

    position_layout->addWidget(new QLabel("Position Y:"), 1, 0);
    pos_y_spin = new QDoubleSpinBox();
    pos_y_spin->setRange(-100, 100);
    pos_y_spin->setDecimals(1);
    connect(pos_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.y = value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_y_spin, 1, 1);

    position_layout->addWidget(new QLabel("Position Z:"), 2, 0);
    pos_z_spin = new QDoubleSpinBox();
    pos_z_spin->setRange(-100, 100);
    pos_z_spin->setDecimals(1);
    connect(pos_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.z = value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_z_spin, 2, 1);

    position_layout->addWidget(new QLabel("Rotation X:"), 3, 0);
    rot_x_spin = new QDoubleSpinBox();
    rot_x_spin->setRange(-180, 180);
    rot_x_spin->setDecimals(1);
    connect(rot_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.x = value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_x_spin, 3, 1);

    position_layout->addWidget(new QLabel("Rotation Y:"), 4, 0);
    rot_y_spin = new QDoubleSpinBox();
    rot_y_spin->setRange(-180, 180);
    rot_y_spin->setDecimals(1);
    connect(rot_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.y = value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_y_spin, 4, 1);

    position_layout->addWidget(new QLabel("Rotation Z:"), 5, 0);
    rot_z_spin = new QDoubleSpinBox();
    rot_z_spin->setRange(-180, 180);
    rot_z_spin->setDecimals(1);
    connect(rot_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.z = value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_z_spin, 5, 1);

    controller_layout->addLayout(position_layout);
    controller_group->setLayout(controller_layout);
    right_panel->addWidget(controller_group);

    QGroupBox* effect_group = new QGroupBox("Spatial Effects");
    QVBoxLayout* effect_layout = new QVBoxLayout();

    QHBoxLayout* effect_type_layout = new QHBoxLayout();
    effect_type_layout->addWidget(new QLabel("Effect:"));
    effect_type_combo = new QComboBox();
    effect_type_combo->addItem("Wave X");
    effect_type_combo->addItem("Wave Y");
    effect_type_combo->addItem("Wave Z");
    effect_type_combo->addItem("Radial Wave");
    effect_type_combo->addItem("Rain");
    effect_type_combo->addItem("Fire");
    effect_type_combo->addItem("Plasma");
    effect_type_combo->addItem("Ripple");
    effect_type_combo->addItem("Spiral");
    connect(effect_type_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_effect_type_changed(int)));
    effect_type_layout->addWidget(effect_type_combo);
    effect_layout->addLayout(effect_type_layout);

    QHBoxLayout* speed_layout = new QHBoxLayout();
    speed_layout->addWidget(new QLabel("Speed:"));
    effect_speed_slider = new QSlider(Qt::Horizontal);
    effect_speed_slider->setMinimum(1);
    effect_speed_slider->setMaximum(100);
    effect_speed_slider->setValue(50);
    connect(effect_speed_slider, SIGNAL(valueChanged(int)), this, SLOT(on_effect_speed_changed(int)));
    speed_layout->addWidget(effect_speed_slider);
    speed_label = new QLabel("50");
    speed_layout->addWidget(speed_label);
    effect_layout->addLayout(speed_layout);

    QHBoxLayout* brightness_layout = new QHBoxLayout();
    brightness_layout->addWidget(new QLabel("Brightness:"));
    effect_brightness_slider = new QSlider(Qt::Horizontal);
    effect_brightness_slider->setMinimum(0);
    effect_brightness_slider->setMaximum(100);
    effect_brightness_slider->setValue(100);
    connect(effect_brightness_slider, SIGNAL(valueChanged(int)), this, SLOT(on_effect_brightness_changed(int)));
    brightness_layout->addWidget(effect_brightness_slider);
    brightness_label = new QLabel("100");
    brightness_layout->addWidget(brightness_label);
    effect_layout->addLayout(brightness_layout);

    QHBoxLayout* color_layout = new QHBoxLayout();
    color_layout->addWidget(new QLabel("Colors:"));
    color_start_button = new QPushButton("Start Color");
    color_start_button->setStyleSheet("background-color: #FF0000;");
    connect(color_start_button, SIGNAL(clicked()), this, SLOT(on_color_start_clicked()));
    color_layout->addWidget(color_start_button);

    color_end_button = new QPushButton("End Color");
    color_end_button->setStyleSheet("background-color: #0000FF;");
    connect(color_end_button, SIGNAL(clicked()), this, SLOT(on_color_end_clicked()));
    color_layout->addWidget(color_end_button);
    effect_layout->addLayout(color_layout);

    QHBoxLayout* button_layout = new QHBoxLayout();
    start_effect_button = new QPushButton("Start Effect");
    connect(start_effect_button, SIGNAL(clicked()), this, SLOT(on_start_effect_clicked()));
    button_layout->addWidget(start_effect_button);

    stop_effect_button = new QPushButton("Stop Effect");
    stop_effect_button->setEnabled(false);
    connect(stop_effect_button, SIGNAL(clicked()), this, SLOT(on_stop_effect_clicked()));
    button_layout->addWidget(stop_effect_button);

    effect_layout->addLayout(button_layout);
    effect_group->setLayout(effect_layout);
    right_panel->addWidget(effect_group);

    right_panel->addStretch();
    main_layout->addLayout(right_panel, 1);

    setLayout(main_layout);
}

void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager)
    {
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        RGBController* controller = controllers[i];

        ControllerTransform* ctrl_transform = new ControllerTransform();
        ctrl_transform->controller = controller;
        ctrl_transform->transform.position = {(float)(i * 15), 0.0f, 0.0f};
        ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
        ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};
        ctrl_transform->led_positions = ControllerLayout3D::GenerateLEDPositions(controller);

        controller_transforms.push_back(ctrl_transform);
    }

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);

    controller_list->clear();
    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        controller_list->addItem(QString::fromStdString(controllers[i]->name));
        qDebug() << "Loaded controller:" << QString::fromStdString(controllers[i]->name)
                 << "LEDs:" << controller_transforms[i]->led_positions.size()
                 << "Position:" << controller_transforms[i]->transform.position.x
                 << controller_transforms[i]->transform.position.y
                 << controller_transforms[i]->transform.position.z;
    }

    qDebug() << "Total controllers loaded:" << controller_transforms.size();
}

void OpenRGB3DSpatialTab::UpdateDeviceList()
{
    LoadDevices();
}

void OpenRGB3DSpatialTab::on_controller_selected(int index)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        controller_list->setCurrentRow(index);

        ControllerTransform* ctrl = controller_transforms[index];
        pos_x_spin->setValue(ctrl->transform.position.x);
        pos_y_spin->setValue(ctrl->transform.position.y);
        pos_z_spin->setValue(ctrl->transform.position.z);
        rot_x_spin->setValue(ctrl->transform.rotation.x);
        rot_y_spin->setValue(ctrl->transform.rotation.y);
        rot_z_spin->setValue(ctrl->transform.rotation.z);
    }
    else if(index == -1)
    {
        controller_list->setCurrentRow(-1);
    }
}

void OpenRGB3DSpatialTab::on_controller_position_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index];
        ctrl->transform.position.x = x;
        ctrl->transform.position.y = y;
        ctrl->transform.position.z = z;

        pos_x_spin->setValue(x);
        pos_y_spin->setValue(y);
        pos_z_spin->setValue(z);
    }
}

void OpenRGB3DSpatialTab::on_effect_type_changed(int /*index*/)
{

}

void OpenRGB3DSpatialTab::on_effect_speed_changed(int value)
{
    speed_label->setText(QString::number(value));

    if(effects->IsRunning())
    {
        effects->SetSpeed(value);
    }
}

void OpenRGB3DSpatialTab::on_effect_brightness_changed(int value)
{
    brightness_label->setText(QString::number(value));

    if(effects->IsRunning())
    {
        effects->SetBrightness(value);
    }
}

void OpenRGB3DSpatialTab::on_start_effect_clicked()
{
    SpatialEffectParams params;
    params.type = (SpatialEffectType)effect_type_combo->currentIndex();
    params.speed = effect_speed_slider->value();
    params.brightness = effect_brightness_slider->value();
    params.color_start = current_color_start;
    params.color_end = current_color_end;
    params.use_gradient = true;
    params.scale = 1.0f;
    params.origin = {0.0f, 0.0f, 0.0f};

    effects->StartEffect(params);

    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);
}

void OpenRGB3DSpatialTab::on_stop_effect_clicked()
{
    effects->StopEffect();

    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);
}

void OpenRGB3DSpatialTab::on_color_start_clicked()
{
    QColor initial_color;
    initial_color.setRgb((current_color_start >> 16) & 0xFF,
                         (current_color_start >> 8) & 0xFF,
                         current_color_start & 0xFF);

    QColor color = QColorDialog::getColor(initial_color, this, "Select Start Color");

    if(color.isValid())
    {
        current_color_start = (color.red() << 16) | (color.green() << 8) | color.blue();

        QString style = QString("background-color: #%1;")
                        .arg(current_color_start, 6, 16, QChar('0'));
        color_start_button->setStyleSheet(style);

        if(effects->IsRunning())
        {
            effects->SetColors(current_color_start, current_color_end, true);
        }
    }
}

void OpenRGB3DSpatialTab::on_color_end_clicked()
{
    QColor initial_color;
    initial_color.setRgb((current_color_end >> 16) & 0xFF,
                         (current_color_end >> 8) & 0xFF,
                         current_color_end & 0xFF);

    QColor color = QColorDialog::getColor(initial_color, this, "Select End Color");

    if(color.isValid())
    {
        current_color_end = (color.red() << 16) | (color.green() << 8) | color.blue();

        QString style = QString("background-color: #%1;")
                        .arg(current_color_end, 6, 16, QChar('0'));
        color_end_button->setStyleSheet(style);

        if(effects->IsRunning())
        {
            effects->SetColors(current_color_start, current_color_end, true);
        }
    }
}