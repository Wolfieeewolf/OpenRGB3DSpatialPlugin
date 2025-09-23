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
#include "LogManager.h"
#include <QColorDialog>

OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm)
{
    effects = new SpatialEffects();
    connect(effects, SIGNAL(EffectUpdated()), this, SLOT(on_effect_updated()));

    current_color_start = 0x0000FF;
    current_color_end = 0xFF0000;

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

    QLabel* controls_label = new QLabel("Camera: Middle mouse = Rotate | Right/Shift+Middle = Pan | Scroll = Zoom | Left click = Select/Move device");
    left_panel->addWidget(controls_label);

    main_layout->addLayout(left_panel, 3);

    QVBoxLayout* right_panel = new QVBoxLayout();

    QGroupBox* controller_group = new QGroupBox("Controllers");
    QVBoxLayout* controller_layout = new QVBoxLayout();

    QLabel* available_label = new QLabel("Available Controllers:");
    controller_layout->addWidget(available_label);

    available_controllers_list = new QListWidget();
    available_controllers_list->setMaximumHeight(100);
    connect(available_controllers_list, &QListWidget::currentRowChanged, [this](int) {
        on_granularity_changed(granularity_combo->currentIndex());
    });
    controller_layout->addWidget(available_controllers_list);

    QHBoxLayout* granularity_layout = new QHBoxLayout();
    granularity_layout->addWidget(new QLabel("Add:"));
    granularity_combo = new QComboBox();
    granularity_combo->addItem("Whole Device");
    granularity_combo->addItem("Zone");
    granularity_combo->addItem("LED");
    connect(granularity_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_granularity_changed(int)));
    granularity_layout->addWidget(granularity_combo);
    controller_layout->addLayout(granularity_layout);

    item_combo = new QComboBox();
    controller_layout->addWidget(item_combo);

    QHBoxLayout* add_remove_layout = new QHBoxLayout();
    QPushButton* add_button = new QPushButton("Add to 3D View");
    connect(add_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_add_clicked);
    add_remove_layout->addWidget(add_button);

    QPushButton* remove_button = new QPushButton("Remove");
    connect(remove_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_remove_controller_clicked);
    add_remove_layout->addWidget(remove_button);

    QPushButton* clear_button = new QPushButton("Clear All");
    connect(clear_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_clear_all_clicked);
    add_remove_layout->addWidget(clear_button);
    controller_layout->addLayout(add_remove_layout);

    QLabel* active_label = new QLabel("Active in 3D View:");
    controller_layout->addWidget(active_label);

    controller_list = new QListWidget();
    controller_list->setMaximumHeight(80);
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
    effect_type_combo->addItem("Orbit");
    effect_type_combo->addItem("Sphere Pulse");
    effect_type_combo->addItem("Cube Rotate");
    effect_type_combo->addItem("Meteor");
    effect_type_combo->addItem("DNA Helix");
    effect_type_combo->addItem("Room Sweep");
    effect_type_combo->addItem("Corners");
    effect_type_combo->addItem("Vertical Bars");
    effect_type_combo->addItem("Breathing Sphere");
    effect_type_combo->addItem("Explosion");
    effect_type_combo->addItem("Wipe Top to Bottom");
    effect_type_combo->addItem("Wipe Left to Right");
    effect_type_combo->addItem("Wipe Front to Back");
    effect_type_combo->addItem("LED Sparkle");
    effect_type_combo->addItem("LED Chase");
    effect_type_combo->addItem("LED Twinkle");
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
    connect(color_start_button, SIGNAL(clicked()), this, SLOT(on_color_start_clicked()));
    color_layout->addWidget(color_start_button);

    color_end_button = new QPushButton("End Color");
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

    available_controllers_list->clear();
    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        available_controllers_list->addItem(QString::fromStdString(controllers[i]->name));
    }

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);
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

    LOG_VERBOSE("[OpenRGB 3D Spatial] Started effect: %s (Speed: %u, Brightness: %u)", effect_type_combo->currentText().toStdString().c_str(), params.speed, params.brightness);
}

void OpenRGB3DSpatialTab::on_stop_effect_clicked()
{
    effects->StopEffect();

    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);

    LOG_VERBOSE("[OpenRGB 3D Spatial] Stopped effect");
}

void OpenRGB3DSpatialTab::on_color_start_clicked()
{
    QColor initial_color;
    initial_color.setRgb(current_color_start & 0xFF,
                         (current_color_start >> 8) & 0xFF,
                         (current_color_start >> 16) & 0xFF);

    QColor color = QColorDialog::getColor(initial_color, this, "Select Start Color");

    if(color.isValid())
    {
        current_color_start = (color.blue() << 16) | (color.green() << 8) | color.red();

        if(effects->IsRunning())
        {
            effects->SetColors(current_color_start, current_color_end, true);
        }
    }
}

void OpenRGB3DSpatialTab::on_color_end_clicked()
{
    QColor initial_color;
    initial_color.setRgb(current_color_end & 0xFF,
                         (current_color_end >> 8) & 0xFF,
                         (current_color_end >> 16) & 0xFF);

    QColor color = QColorDialog::getColor(initial_color, this, "Select End Color");

    if(color.isValid())
    {
        current_color_end = (color.blue() << 16) | (color.green() << 8) | color.red();

        if(effects->IsRunning())
        {
            effects->SetColors(current_color_start, current_color_end, true);
        }
    }
}

void OpenRGB3DSpatialTab::on_effect_updated()
{
    viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_granularity_changed(int index)
{
    item_combo->clear();

    int ctrl_row = available_controllers_list->currentRow();
    if(ctrl_row < 0 || ctrl_row >= (int)resource_manager->GetRGBControllers().size())
    {
        return;
    }

    RGBController* controller = resource_manager->GetRGBControllers()[ctrl_row];

    if(index == 0)
    {
        item_combo->addItem(QString::fromStdString(controller->name));
    }
    else if(index == 1)
    {
        for(unsigned int i = 0; i < controller->zones.size(); i++)
        {
            item_combo->addItem(QString::fromStdString(controller->zones[i].name));
        }
    }
    else if(index == 2)
    {
        for(unsigned int i = 0; i < controller->leds.size(); i++)
        {
            item_combo->addItem(QString::fromStdString(controller->leds[i].name));
        }
    }
}

void OpenRGB3DSpatialTab::on_add_clicked()
{
    int ctrl_row = available_controllers_list->currentRow();
    int granularity = granularity_combo->currentIndex();
    int item_row = item_combo->currentIndex();

    if(ctrl_row < 0 || item_row < 0)
    {
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    if(ctrl_row >= (int)controllers.size())
    {
        return;
    }

    RGBController* controller = controllers[ctrl_row];

    ControllerTransform* ctrl_transform = new ControllerTransform();
    ctrl_transform->controller = controller;
    ctrl_transform->transform.position = {(float)(controller_transforms.size() * 15), 0.0f, 0.0f};
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};

    QString name;

    if(granularity == 0)
    {
        ctrl_transform->led_positions = ControllerLayout3D::GenerateLEDPositions(controller);
        name = QString::fromStdString(controller->name);
    }
    else if(granularity == 1)
    {
        if(item_row >= (int)controller->zones.size())
        {
            delete ctrl_transform;
            return;
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateLEDPositions(controller);
        zone* z = &controller->zones[item_row];

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            if(all_positions[i].zone_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
            }
        }

        name = QString::fromStdString(controller->name) + " - " + QString::fromStdString(z->name);
    }
    else if(granularity == 2)
    {
        if(item_row >= (int)controller->leds.size())
        {
            delete ctrl_transform;
            return;
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateLEDPositions(controller);

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            unsigned int global_led_idx = controller->zones[all_positions[i].zone_idx].start_idx + all_positions[i].led_idx;
            if(global_led_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
                break;
            }
        }

        name = QString::fromStdString(controller->name) + " - " + QString::fromStdString(controller->leds[item_row].name);
    }

    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

    controller_transforms.push_back(ctrl_transform);

    QListWidgetItem* item = new QListWidgetItem(name);
    item->setBackground(QBrush(color));
    item->setForeground(QBrush(color.value() > 128 ? Qt::black : Qt::white));
    controller_list->addItem(item);

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();

    LOG_VERBOSE("[OpenRGB 3D Spatial] Added %s (%u LEDs) to 3D view", name.toStdString().c_str(), ctrl_transform->led_positions.size());
}

void OpenRGB3DSpatialTab::on_remove_controller_clicked()
{
    int selected_row = controller_list->currentRow();
    if(selected_row < 0 || selected_row >= (int)controller_transforms.size())
    {
        return;
    }

    delete controller_transforms[selected_row];
    controller_transforms.erase(controller_transforms.begin() + selected_row);

    controller_list->takeItem(selected_row);

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
}

void OpenRGB3DSpatialTab::on_clear_all_clicked()
{
    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        delete controller_transforms[i];
    }
    controller_transforms.clear();

    controller_list->clear();

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
}