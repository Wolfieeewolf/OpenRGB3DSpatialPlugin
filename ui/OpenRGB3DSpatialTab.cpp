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
#include "CustomControllerDialog.h"
#include <QColorDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <QFileInfo>
#include <QInputDialog>
#include <QFileDialog>
#include <filesystem>
#include "SettingsManager.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm),
    first_load(true)
{
    effects = new SpatialEffects();
    connect(effects, SIGNAL(EffectUpdated()), this, SLOT(on_effect_updated()));

    current_color_start = 0x0000FF;
    current_color_end = 0xFF0000;

    SetupUI();
    LoadDevices();
    LoadCustomControllers();

    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(500);
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
    if(auto_load_timer)
    {
        auto_load_timer->stop();
        delete auto_load_timer;
    }

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

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        delete virtual_controllers[i];
    }
    virtual_controllers.clear();
}

void OpenRGB3DSpatialTab::SetupUI()
{
    QHBoxLayout* main_layout = new QHBoxLayout(this);

    QVBoxLayout* left_panel = new QVBoxLayout();
    left_panel->setSpacing(8);

    QGroupBox* available_group = new QGroupBox("Available Controllers");
    QVBoxLayout* available_layout = new QVBoxLayout();
    available_layout->setSpacing(5);

    available_controllers_list = new QListWidget();
    available_controllers_list->setMinimumHeight(200);
    connect(available_controllers_list, &QListWidget::currentRowChanged, [this](int) {
        on_granularity_changed(granularity_combo->currentIndex());
    });
    available_layout->addWidget(available_controllers_list);

    QHBoxLayout* granularity_layout = new QHBoxLayout();
    granularity_layout->addWidget(new QLabel("Add:"));
    granularity_combo = new QComboBox();
    granularity_combo->addItem("Whole Device");
    granularity_combo->addItem("Zone");
    granularity_combo->addItem("LED");
    connect(granularity_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_granularity_changed(int)));
    granularity_layout->addWidget(granularity_combo);
    available_layout->addLayout(granularity_layout);

    item_combo = new QComboBox();
    available_layout->addWidget(item_combo);

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
    available_layout->addLayout(add_remove_layout);

    available_group->setLayout(available_layout);
    left_panel->addWidget(available_group);

    QGroupBox* custom_group = new QGroupBox("Custom 3D Controllers");
    QVBoxLayout* custom_layout = new QVBoxLayout();
    custom_layout->setSpacing(5);

    QPushButton* custom_controller_button = new QPushButton("Create Custom 3D Controller");
    connect(custom_controller_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_create_custom_controller_clicked);
    custom_layout->addWidget(custom_controller_button);

    QHBoxLayout* custom_io_layout = new QHBoxLayout();
    QPushButton* import_button = new QPushButton("Import");
    connect(import_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_import_custom_controller_clicked);
    custom_io_layout->addWidget(import_button);

    QPushButton* export_button = new QPushButton("Export");
    connect(export_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_export_custom_controller_clicked);
    custom_io_layout->addWidget(export_button);

    QPushButton* edit_button = new QPushButton("Edit");
    connect(edit_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_edit_custom_controller_clicked);
    custom_io_layout->addWidget(edit_button);

    custom_layout->addLayout(custom_io_layout);
    custom_group->setLayout(custom_layout);
    left_panel->addWidget(custom_group);

    QGroupBox* controller_group = new QGroupBox("Controllers in 3D Scene");
    QVBoxLayout* controller_layout = new QVBoxLayout();
    controller_layout->setSpacing(5);

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

    controller_group->setLayout(controller_layout);
    left_panel->addWidget(controller_group);

    main_layout->addLayout(left_panel, 1);

    QVBoxLayout* middle_panel = new QVBoxLayout();

    QLabel* controls_label = new QLabel("Camera: Middle mouse = Rotate | Right/Shift+Middle = Pan | Scroll = Zoom | Left click = Select/Move device");
    middle_panel->addWidget(controls_label);

    viewport = new LEDViewport3D();
    viewport->SetControllerTransforms(&controller_transforms);
    connect(viewport, SIGNAL(ControllerSelected(int)), this, SLOT(on_controller_selected(int)));
    connect(viewport, SIGNAL(ControllerPositionChanged(int,float,float,float)),
            this, SLOT(on_controller_position_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerScaleChanged(int,float,float,float)),
            this, SLOT(on_controller_scale_changed(int,float,float,float)));
    middle_panel->addWidget(viewport, 1);

    effects->SetControllerTransforms(&controller_transforms);

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
    middle_panel->addWidget(effect_group);
    main_layout->addLayout(middle_panel, 2);

    QVBoxLayout* right_panel = new QVBoxLayout();
    QGroupBox* transform_group = new QGroupBox("Position, Rotation & Scale");
    QGridLayout* position_layout = new QGridLayout();

    position_layout->addWidget(new QLabel("Position X:"), 0, 0);

    pos_x_slider = new QSlider(Qt::Horizontal);
    pos_x_slider->setRange(-1000, 1000);
    pos_x_slider->setValue(0);
    connect(pos_x_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_x_spin->setValue(pos_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.x = pos_value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_x_slider, 0, 1);

    pos_x_spin = new QDoubleSpinBox();
    pos_x_spin->setRange(-100, 100);
    pos_x_spin->setDecimals(1);
    pos_x_spin->setMaximumWidth(80);
    connect(pos_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_x_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.x = value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_x_spin, 0, 2);

    position_layout->addWidget(new QLabel("Position Y:"), 1, 0);

    pos_y_slider = new QSlider(Qt::Horizontal);
    pos_y_slider->setRange(-1000, 1000);
    pos_y_slider->setValue(0);
    connect(pos_y_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_y_spin->setValue(pos_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.y = pos_value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_y_slider, 1, 1);

    pos_y_spin = new QDoubleSpinBox();
    pos_y_spin->setRange(-100, 100);
    pos_y_spin->setDecimals(1);
    pos_y_spin->setMaximumWidth(80);
    connect(pos_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_y_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.y = value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_y_spin, 1, 2);

    position_layout->addWidget(new QLabel("Position Z:"), 2, 0);

    pos_z_slider = new QSlider(Qt::Horizontal);
    pos_z_slider->setRange(-1000, 1000);
    pos_z_slider->setValue(0);
    connect(pos_z_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_z_spin->setValue(pos_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.z = pos_value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_z_slider, 2, 1);

    pos_z_spin = new QDoubleSpinBox();
    pos_z_spin->setRange(-100, 100);
    pos_z_spin->setDecimals(1);
    pos_z_spin->setMaximumWidth(80);
    connect(pos_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_z_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.z = value;
            viewport->update();
        }
    });
    position_layout->addWidget(pos_z_spin, 2, 2);

    position_layout->addWidget(new QLabel("Rotation X:"), 3, 0);

    rot_x_slider = new QSlider(Qt::Horizontal);
    rot_x_slider->setRange(-1800, 1800);
    rot_x_slider->setValue(0);
    connect(rot_x_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value / 10.0;
        rot_x_spin->setValue(rot_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.x = rot_value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_x_slider, 3, 1);

    rot_x_spin = new QDoubleSpinBox();
    rot_x_spin->setRange(-180, 180);
    rot_x_spin->setDecimals(1);
    rot_x_spin->setMaximumWidth(80);
    connect(rot_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        rot_x_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.x = value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_x_spin, 3, 2);

    position_layout->addWidget(new QLabel("Rotation Y:"), 4, 0);

    rot_y_slider = new QSlider(Qt::Horizontal);
    rot_y_slider->setRange(-1800, 1800);
    rot_y_slider->setValue(0);
    connect(rot_y_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value / 10.0;
        rot_y_spin->setValue(rot_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.y = rot_value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_y_slider, 4, 1);

    rot_y_spin = new QDoubleSpinBox();
    rot_y_spin->setRange(-180, 180);
    rot_y_spin->setDecimals(1);
    rot_y_spin->setMaximumWidth(80);
    connect(rot_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        rot_y_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.y = value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_y_spin, 4, 2);

    position_layout->addWidget(new QLabel("Rotation Z:"), 5, 0);

    rot_z_slider = new QSlider(Qt::Horizontal);
    rot_z_slider->setRange(-1800, 1800);
    rot_z_slider->setValue(0);
    connect(rot_z_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value / 10.0;
        rot_z_spin->setValue(rot_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.z = rot_value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_z_slider, 5, 1);

    rot_z_spin = new QDoubleSpinBox();
    rot_z_spin->setRange(-180, 180);
    rot_z_spin->setDecimals(1);
    rot_z_spin->setMaximumWidth(80);
    connect(rot_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        rot_z_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.z = value;
            viewport->update();
        }
    });
    position_layout->addWidget(rot_z_spin, 5, 2);

    position_layout->addWidget(new QLabel("Scale X:"), 6, 0);

    scale_x_slider = new QSlider(Qt::Horizontal);
    scale_x_slider->setRange(10, 1000);
    scale_x_slider->setValue(100);
    connect(scale_x_slider, &QSlider::valueChanged, [this](int value) {
        double scale_value = value / 100.0;
        scale_x_spin->setValue(scale_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.scale.x = scale_value;
            viewport->update();
        }
    });
    position_layout->addWidget(scale_x_slider, 6, 1);

    scale_x_spin = new QDoubleSpinBox();
    scale_x_spin->setRange(0.1, 10.0);
    scale_x_spin->setDecimals(2);
    scale_x_spin->setValue(1.0);
    scale_x_spin->setMaximumWidth(80);
    connect(scale_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        scale_x_slider->setValue((int)(value * 100));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.scale.x = value;
            viewport->update();
        }
    });
    position_layout->addWidget(scale_x_spin, 6, 2);

    position_layout->addWidget(new QLabel("Scale Y:"), 7, 0);

    scale_y_slider = new QSlider(Qt::Horizontal);
    scale_y_slider->setRange(10, 1000);
    scale_y_slider->setValue(100);
    connect(scale_y_slider, &QSlider::valueChanged, [this](int value) {
        double scale_value = value / 100.0;
        scale_y_spin->setValue(scale_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.scale.y = scale_value;
            viewport->update();
        }
    });
    position_layout->addWidget(scale_y_slider, 7, 1);

    scale_y_spin = new QDoubleSpinBox();
    scale_y_spin->setRange(0.1, 10.0);
    scale_y_spin->setDecimals(2);
    scale_y_spin->setValue(1.0);
    scale_y_spin->setMaximumWidth(80);
    connect(scale_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        scale_y_slider->setValue((int)(value * 100));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.scale.y = value;
            viewport->update();
        }
    });
    position_layout->addWidget(scale_y_spin, 7, 2);

    position_layout->addWidget(new QLabel("Scale Z:"), 8, 0);

    scale_z_slider = new QSlider(Qt::Horizontal);
    scale_z_slider->setRange(10, 1000);
    scale_z_slider->setValue(100);
    connect(scale_z_slider, &QSlider::valueChanged, [this](int value) {
        double scale_value = value / 100.0;
        scale_z_spin->setValue(scale_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.scale.z = scale_value;
            viewport->update();
        }
    });
    position_layout->addWidget(scale_z_slider, 8, 1);

    scale_z_spin = new QDoubleSpinBox();
    scale_z_spin->setRange(0.1, 10.0);
    scale_z_spin->setDecimals(2);
    scale_z_spin->setValue(1.0);
    scale_z_spin->setMaximumWidth(80);
    connect(scale_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        scale_z_slider->setValue((int)(value * 100));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.scale.z = value;
            viewport->update();
        }
    });
    position_layout->addWidget(scale_z_spin, 8, 2);

    transform_group->setLayout(position_layout);
    right_panel->addWidget(transform_group);

    right_panel->addStretch();

    QGroupBox* layout_group = new QGroupBox("Layout Profiles");
    QVBoxLayout* layout_layout = new QVBoxLayout();

    layout_profiles_combo = new QComboBox();
    connect(layout_profiles_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_layout_profile_changed(int)));
    layout_layout->addWidget(layout_profiles_combo);

    QHBoxLayout* save_load_layout = new QHBoxLayout();
    QPushButton* save_button = new QPushButton("Save Layout");
    connect(save_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_save_layout_clicked);
    save_load_layout->addWidget(save_button);

    QPushButton* load_button = new QPushButton("Load Layout");
    connect(load_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_load_layout_clicked);
    save_load_layout->addWidget(load_button);
    layout_layout->addLayout(save_load_layout);

    QPushButton* delete_button = new QPushButton("Delete Profile");
    connect(delete_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_delete_layout_clicked);
    layout_layout->addWidget(delete_button);

    auto_load_checkbox = new QCheckBox("Auto-load selected profile on startup");

    json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    bool auto_load_enabled = false;
    if(settings.contains("AutoLoad"))
    {
        auto_load_enabled = settings["AutoLoad"];
    }
    auto_load_checkbox->setChecked(auto_load_enabled);
    LOG_INFO("[OpenRGB 3D Spatial] Auto-load setting loaded: %s", auto_load_enabled ? "enabled" : "disabled");

    connect(auto_load_checkbox, &QCheckBox::toggled, [this](bool checked) {
        json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        settings["AutoLoad"] = checked;
        resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
        resource_manager->GetSettingsManager()->SaveSettings();
        LOG_INFO("[OpenRGB 3D Spatial] Auto-load setting changed to: %s", checked ? "enabled" : "disabled");
    });
    layout_layout->addWidget(auto_load_checkbox);

    PopulateLayoutDropdown();

    layout_group->setLayout(layout_layout);
    right_panel->addWidget(layout_group);
    main_layout->addLayout(right_panel, 1);

    setLayout(main_layout);
}

void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager)
    {
        return;
    }

    UpdateAvailableControllersList();

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);
}

void OpenRGB3DSpatialTab::UpdateAvailableControllersList()
{
    available_controllers_list->clear();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        int unassigned_zones = GetUnassignedZoneCount(controllers[i]);
        int unassigned_leds = GetUnassignedLEDCount(controllers[i]);

        if(unassigned_leds > 0)
        {
            QString display_text = QString::fromStdString(controllers[i]->name) +
                                   QString(" [%1 zones, %2 LEDs available]").arg(unassigned_zones).arg(unassigned_leds);
            available_controllers_list->addItem(display_text);
        }
    }

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_controllers[i]->GetName()));
    }
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
        scale_x_spin->setValue(ctrl->transform.scale.x);
        scale_y_spin->setValue(ctrl->transform.scale.y);
        scale_z_spin->setValue(ctrl->transform.scale.z);

        pos_x_slider->setValue((int)(ctrl->transform.position.x * 10));
        pos_y_slider->setValue((int)(ctrl->transform.position.y * 10));
        pos_z_slider->setValue((int)(ctrl->transform.position.z * 10));
        rot_x_slider->setValue((int)(ctrl->transform.rotation.x * 10));
        rot_y_slider->setValue((int)(ctrl->transform.rotation.y * 10));
        rot_z_slider->setValue((int)(ctrl->transform.rotation.z * 10));
        scale_x_slider->setValue((int)(ctrl->transform.scale.x * 100));
        scale_y_slider->setValue((int)(ctrl->transform.scale.y * 100));
        scale_z_slider->setValue((int)(ctrl->transform.scale.z * 100));
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

void OpenRGB3DSpatialTab::on_controller_scale_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index];
        ctrl->transform.scale.x = x;
        ctrl->transform.scale.y = y;
        ctrl->transform.scale.z = z;

        scale_x_spin->setValue(x);
        scale_y_spin->setValue(y);
        scale_z_spin->setValue(z);
    }
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

void OpenRGB3DSpatialTab::on_granularity_changed(int /*index*/)
{
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::UpdateAvailableItemCombo()
{
    item_combo->clear();

    int list_row = available_controllers_list->currentRow();
    if(list_row < 0)
    {
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    int actual_ctrl_idx = -1;
    int visible_idx = 0;

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        if(GetUnassignedLEDCount(controllers[i]) > 0)
        {
            if(visible_idx == list_row)
            {
                actual_ctrl_idx = i;
                break;
            }
            visible_idx++;
        }
    }

    if(actual_ctrl_idx >= 0)
    {
        RGBController* controller = controllers[actual_ctrl_idx];
        int granularity = granularity_combo->currentIndex();

        if(granularity == 0)
        {
            if(!IsItemInScene(controller, granularity, 0))
            {
                item_combo->addItem(QString::fromStdString(controller->name), QVariant::fromValue(qMakePair(actual_ctrl_idx, 0)));
            }
        }
        else if(granularity == 1)
        {
            for(unsigned int i = 0; i < controller->zones.size(); i++)
            {
                if(!IsItemInScene(controller, granularity, i))
                {
                    item_combo->addItem(QString::fromStdString(controller->zones[i].name), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        else if(granularity == 2)
        {
            for(unsigned int i = 0; i < controller->leds.size(); i++)
            {
                if(!IsItemInScene(controller, granularity, i))
                {
                    item_combo->addItem(QString::fromStdString(controller->leds[i].name), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        return;
    }

    int virtual_offset = visible_idx;
    if(list_row >= virtual_offset && list_row < virtual_offset + (int)virtual_controllers.size())
    {
        item_combo->addItem("Whole Device", QVariant::fromValue(qMakePair(-1, list_row - virtual_offset)));
    }
}

void OpenRGB3DSpatialTab::on_add_clicked()
{
    int granularity = granularity_combo->currentIndex();
    int combo_idx = item_combo->currentIndex();

    if(combo_idx < 0)
    {
        return;
    }

    QPair<int, int> data = item_combo->currentData().value<QPair<int, int>>();
    int ctrl_idx = data.first;
    int item_row = data.second;

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    if(ctrl_idx < 0)
    {
        if(item_row >= (int)virtual_controllers.size())
        {
            LOG_ERROR("[OpenRGB 3D Spatial] Invalid virtual controller index: %d (max: %d)", item_row, (int)virtual_controllers.size());
            return;
        }

        VirtualController3D* virtual_ctrl = virtual_controllers[item_row];

        LOG_INFO("[OpenRGB 3D Spatial] Adding virtual controller '%s' to scene", virtual_ctrl->GetName().c_str());

        ControllerTransform* ctrl_transform = new ControllerTransform();
        ctrl_transform->controller = nullptr;
        ctrl_transform->transform.position = {-40.0f, 0.0f, -50.0f};
        ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
        ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};

        LOG_INFO("[OpenRGB 3D Spatial] Generating LED positions for virtual controller");
        ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions();
        LOG_INFO("[OpenRGB 3D Spatial] Generated %d LED positions", (int)ctrl_transform->led_positions.size());

        int hue = (controller_transforms.size() * 137) % 360;
        QColor color = QColor::fromHsv(hue, 200, 255);
        ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

        controller_transforms.push_back(ctrl_transform);

        QString name = QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(name);
        list_item->setBackground(QBrush(color));
        list_item->setForeground(QBrush(color.value() > 128 ? Qt::black : Qt::white));
        controller_list->addItem(list_item);

        effects->SetControllerTransforms(&controller_transforms);
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();
        return;
    }

    if(ctrl_idx >= (int)controllers.size())
    {
        return;
    }

    RGBController* controller = controllers[ctrl_idx];

    ControllerTransform* ctrl_transform = new ControllerTransform();
    ctrl_transform->controller = controller;
    ctrl_transform->transform.position = {-40.0f, 0.0f, -50.0f};
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
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();

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
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
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
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_save_layout_clicked()
{
    bool ok;
    QString profile_name = QInputDialog::getText(this, "Save Layout Profile",
                                                 "Profile name:", QLineEdit::Normal,
                                                 layout_profiles_combo->currentText(), &ok);

    if(!ok || profile_name.isEmpty())
    {
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    SaveLayout(layout_path);

    PopulateLayoutDropdown();

    int index = layout_profiles_combo->findText(profile_name);
    if(index >= 0)
    {
        layout_profiles_combo->setCurrentIndex(index);
    }

    QMessageBox::information(this, "Layout Saved",
                            QString("Profile '%1' saved to plugins directory").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_load_layout_clicked()
{
    QString profile_name = layout_profiles_combo->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to load");
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    QFileInfo check_file(QString::fromStdString(layout_path));

    if(!check_file.exists())
    {
        QMessageBox::warning(this, "Profile Not Found", "Selected profile file not found");
        return;
    }

    LoadLayout(layout_path);
    QMessageBox::information(this, "Layout Loaded",
                            QString("Profile '%1' loaded successfully").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_delete_layout_clicked()
{
    QString profile_name = layout_profiles_combo->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to delete");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Profile",
                                        QString("Are you sure you want to delete profile '%1'?").arg(profile_name),
                                        QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        std::string layout_path = GetLayoutPath(profile_name.toStdString());
        QFile file(QString::fromStdString(layout_path));

        if(file.remove())
        {
            PopulateLayoutDropdown();
            QMessageBox::information(this, "Profile Deleted",
                                    QString("Profile '%1' deleted successfully").arg(profile_name));
        }
        else
        {
            QMessageBox::warning(this, "Delete Failed", "Failed to delete profile file");
        }
    }
}

void OpenRGB3DSpatialTab::on_layout_profile_changed(int /*index*/)
{
    SaveCurrentLayoutName();
}

void OpenRGB3DSpatialTab::on_create_custom_controller_clicked()
{
    CustomControllerDialog dialog(resource_manager, this);

    if(dialog.exec() == QDialog::Accepted)
    {
        VirtualController3D* virtual_ctrl = new VirtualController3D(
            dialog.GetControllerName().toStdString(),
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings()
        );

        virtual_controllers.push_back(virtual_ctrl);

        LOG_INFO("[OpenRGB 3D Spatial] Created custom controller: %s (%dx%dx%d)",
                 virtual_ctrl->GetName().c_str(),
                 virtual_ctrl->GetWidth(),
                 virtual_ctrl->GetHeight(),
                 virtual_ctrl->GetDepth());

        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));

        SaveCustomControllers();

        QMessageBox::information(this, "Custom Controller Created",
                                QString("Custom controller '%1' created successfully!\n\nYou can now add it to the 3D view.")
                                .arg(QString::fromStdString(virtual_ctrl->GetName())));
    }
}

void OpenRGB3DSpatialTab::on_export_custom_controller_clicked()
{
    if(virtual_controllers.empty())
    {
        QMessageBox::warning(this, "No Custom Controllers", "No custom controllers available to export");
        return;
    }

    QStringList controller_names;
    for(const VirtualController3D* ctrl : virtual_controllers)
    {
        controller_names.append(QString::fromStdString(ctrl->GetName()));
    }

    bool ok;
    QString selected = QInputDialog::getItem(this, "Export Custom Controller",
                                            "Select controller to export:",
                                            controller_names, 0, false, &ok);
    if(!ok || selected.isEmpty())
    {
        return;
    }

    int selected_idx = controller_names.indexOf(selected);
    VirtualController3D* ctrl = virtual_controllers[selected_idx];

    QString filename = QFileDialog::getSaveFileName(this, "Export Custom Controller",
                                                    QString::fromStdString(ctrl->GetName()) + ".3dctrl",
                                                    "3D Controller Files (*.3dctrl)");
    if(filename.isEmpty())
    {
        return;
    }

    json export_data = ctrl->ToJson();

    std::ofstream file(filename.toStdString());
    if(file.is_open())
    {
        file << export_data.dump(4);
        file.close();
        QMessageBox::information(this, "Export Successful",
                                QString("Custom controller '%1' exported successfully to:\n%2")
                                .arg(selected).arg(filename));
        LOG_INFO("[OpenRGB 3D Spatial] Exported custom controller '%s' to %s",
                 ctrl->GetName().c_str(), filename.toStdString().c_str());
    }
    else
    {
        QMessageBox::critical(this, "Export Failed",
                            QString("Failed to export custom controller to:\n%1").arg(filename));
    }
}

void OpenRGB3DSpatialTab::on_import_custom_controller_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Import Custom Controller",
                                                    "",
                                                    "3D Controller Files (*.3dctrl);;All Files (*)");
    if(filename.isEmpty())
    {
        return;
    }

    std::ifstream file(filename.toStdString());
    if(!file.is_open())
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("Failed to open file:\n%1").arg(filename));
        return;
    }

    try
    {
        json import_data;
        file >> import_data;
        file.close();

        std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
        VirtualController3D* virtual_ctrl = VirtualController3D::FromJson(import_data, controllers);

        if(virtual_ctrl)
        {
            for(const VirtualController3D* existing : virtual_controllers)
            {
                if(existing->GetName() == virtual_ctrl->GetName())
                {
                    QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Name",
                        QString("A custom controller named '%1' already exists.\n\nDo you want to replace it?")
                        .arg(QString::fromStdString(virtual_ctrl->GetName())),
                        QMessageBox::Yes | QMessageBox::No);

                    if(reply == QMessageBox::No)
                    {
                        delete virtual_ctrl;
                        return;
                    }
                    else
                    {
                        std::vector<VirtualController3D*>::iterator it = std::find(virtual_controllers.begin(), virtual_controllers.end(), existing);
                        if(it != virtual_controllers.end())
                        {
                            delete *it;
                            virtual_controllers.erase(it);
                        }
                        break;
                    }
                }
            }

            virtual_controllers.push_back(virtual_ctrl);
            SaveCustomControllers();
            UpdateAvailableControllersList();

            LOG_INFO("[OpenRGB 3D Spatial] Imported custom controller: %s (%dx%dx%d)",
                     virtual_ctrl->GetName().c_str(),
                     virtual_ctrl->GetWidth(),
                     virtual_ctrl->GetHeight(),
                     virtual_ctrl->GetDepth());

            QMessageBox::information(this, "Import Successful",
                                    QString("Custom controller '%1' imported successfully!\n\n"
                                           "Grid: %2x%3x%4\n"
                                           "LEDs: %5\n\n"
                                           "You can now add it to the 3D view.")
                                    .arg(QString::fromStdString(virtual_ctrl->GetName()))
                                    .arg(virtual_ctrl->GetWidth())
                                    .arg(virtual_ctrl->GetHeight())
                                    .arg(virtual_ctrl->GetDepth())
                                    .arg(virtual_ctrl->GetMappings().size()));
        }
        else
        {
            QMessageBox::warning(this, "Import Warning",
                                "Failed to import custom controller.\n\n"
                                "The required physical controllers may not be connected.");
        }
    }
    catch(const std::exception& e)
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("Failed to parse custom controller file:\n\n%1").arg(e.what()));
        LOG_ERROR("[OpenRGB 3D Spatial] Failed to import custom controller: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::on_edit_custom_controller_clicked()
{
    int list_row = available_controllers_list->currentRow();
    if(list_row < 0)
    {
        QMessageBox::warning(this, "No Selection", "Please select a custom controller from the available controllers list");
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    int visible_physical_count = 0;
    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        if(GetUnassignedLEDCount(controllers[i]) > 0)
        {
            visible_physical_count++;
        }
    }

    int virtual_idx = list_row - visible_physical_count;

    if(virtual_idx < 0 || virtual_idx >= (int)virtual_controllers.size())
    {
        QMessageBox::warning(this, "Not a Custom Controller", "Selected item is not a custom controller.\n\nOnly custom controllers can be edited.");
        return;
    }

    VirtualController3D* virtual_ctrl = virtual_controllers[virtual_idx];

    CustomControllerDialog dialog(resource_manager, this);
    dialog.setWindowTitle("Edit Custom 3D Controller");
    dialog.LoadExistingController(virtual_ctrl->GetName(),
                                  virtual_ctrl->GetWidth(),
                                  virtual_ctrl->GetHeight(),
                                  virtual_ctrl->GetDepth(),
                                  virtual_ctrl->GetMappings());

    if(dialog.exec() == QDialog::Accepted)
    {
        std::string old_name = virtual_ctrl->GetName();
        std::string new_name = dialog.GetControllerName().toStdString();

        if(old_name != new_name)
        {
            std::string config_dir = resource_manager->GetConfigurationDirectory().string();
            std::string custom_dir = config_dir + "/plugins/3d_spatial_custom_controllers";

            std::string safe_old_name = old_name;
            for(char& c : safe_old_name)
            {
                if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                {
                    c = '_';
                }
            }

            std::string old_filepath = custom_dir + "/" + safe_old_name + ".json";
            if(filesystem::exists(old_filepath))
            {
                filesystem::remove(old_filepath);
                LOG_INFO("[OpenRGB 3D Spatial] Removed old custom controller file: %s", old_filepath.c_str());
            }
        }

        delete virtual_ctrl;
        virtual_controllers[virtual_idx] = new VirtualController3D(
            new_name,
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings()
        );

        SaveCustomControllers();
        UpdateAvailableControllersList();

        LOG_INFO("[OpenRGB 3D Spatial] Edited custom controller: %s (%dx%dx%d)",
                 virtual_controllers[virtual_idx]->GetName().c_str(),
                 virtual_controllers[virtual_idx]->GetWidth(),
                 virtual_controllers[virtual_idx]->GetHeight(),
                 virtual_controllers[virtual_idx]->GetDepth());

        QMessageBox::information(this, "Custom Controller Updated",
                                QString("Custom controller '%1' updated successfully!")
                                .arg(QString::fromStdString(virtual_controllers[virtual_idx]->GetName())));
    }
}

void OpenRGB3DSpatialTab::SaveLayout(const std::string& filename)
{
    QFile file(QString::fromStdString(filename));

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG_ERROR("[OpenRGB 3D Spatial] Failed to save layout to %s", filename.c_str());
        return;
    }

    QTextStream out(&file);

    out << "OpenRGB3DSpatialLayout\n";
    out << "Version 1\n";
    out << controller_transforms.size() << "\n";

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i];

        if(ct->controller == nullptr)
        {
            QListWidgetItem* item = controller_list->item(i);
            QString display_name = item ? item->text() : "Unknown Custom Controller";

            out << display_name.toStdString().c_str() << "\n";
            out << "VIRTUAL_CONTROLLER\n";
        }
        else
        {
            out << ct->controller->name.c_str() << "\n";
            out << ct->controller->location.c_str() << "\n";
        }
        out << ct->led_positions.size() << "\n";

        for(unsigned int j = 0; j < ct->led_positions.size(); j++)
        {
            out << ct->led_positions[j].zone_idx << " " << ct->led_positions[j].led_idx << "\n";
        }

        out << ct->transform.position.x << " " << ct->transform.position.y << " " << ct->transform.position.z << "\n";
        out << ct->transform.rotation.x << " " << ct->transform.rotation.y << " " << ct->transform.rotation.z << "\n";
        out << ct->transform.scale.x << " " << ct->transform.scale.y << " " << ct->transform.scale.z << "\n";
        out << ct->display_color << "\n";
    }

    file.close();

    LOG_INFO("[OpenRGB 3D Spatial] Saved layout to %s", filename.c_str());
}

void OpenRGB3DSpatialTab::LoadLayout(const std::string& filename)
{
    QFile file(QString::fromStdString(filename));

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LOG_ERROR("[OpenRGB 3D Spatial] Failed to load layout from %s", filename.c_str());
        return;
    }

    QTextStream in(&file);

    QString header = in.readLine();
    if(header != "OpenRGB3DSpatialLayout")
    {
        LOG_ERROR("[OpenRGB 3D Spatial] Invalid layout file format");
        file.close();
        return;
    }

    QString version_line = in.readLine();
    int count = in.readLine().toInt();

    on_clear_all_clicked();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    for(int i = 0; i < count; i++)
    {
        QString ctrl_name = in.readLine();
        QString ctrl_location = in.readLine();
        int led_count = in.readLine().toInt();

        RGBController* controller = nullptr;
        bool is_virtual = (ctrl_location == "VIRTUAL_CONTROLLER");

        if(!is_virtual)
        {
            for(unsigned int j = 0; j < controllers.size(); j++)
            {
                if(controllers[j]->name == ctrl_name.toStdString() &&
                   controllers[j]->location == ctrl_location.toStdString())
                {
                    controller = controllers[j];
                    break;
                }
            }

            if(!controller)
            {
                LOG_WARNING("[OpenRGB 3D Spatial] Controller '%s' at '%s' not found, skipping",
                           ctrl_name.toStdString().c_str(), ctrl_location.toStdString().c_str());
                for(int j = 0; j < led_count; j++)
                {
                    in.readLine();
                }
                in.readLine();
                in.readLine();
                in.readLine();
                in.readLine();
                continue;
            }
        }

        ControllerTransform* ctrl_transform = new ControllerTransform();
        ctrl_transform->controller = controller;

        if(is_virtual)
        {
            QString virtual_name = ctrl_name;
            if(virtual_name.startsWith("[Custom] "))
            {
                virtual_name = virtual_name.mid(9);
            }

            VirtualController3D* virtual_ctrl = nullptr;
            for(VirtualController3D* vc : virtual_controllers)
            {
                if(QString::fromStdString(vc->GetName()) == virtual_name)
                {
                    virtual_ctrl = vc;
                    break;
                }
            }

            if(virtual_ctrl)
            {
                ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions();
                LOG_INFO("[OpenRGB 3D Spatial] Restored virtual controller '%s' with %d LEDs",
                         virtual_name.toStdString().c_str(), (int)ctrl_transform->led_positions.size());
            }
            else
            {
                LOG_WARNING("[OpenRGB 3D Spatial] Virtual controller '%s' not found, creating placeholder",
                           virtual_name.toStdString().c_str());
            }

            for(int j = 0; j < led_count; j++)
            {
                in.readLine();
            }
        }
        else
        {
            for(int j = 0; j < led_count; j++)
            {
                QStringList parts = in.readLine().split(" ");
                unsigned int zone_idx = parts[0].toUInt();
                unsigned int led_idx = parts[1].toUInt();

                std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateLEDPositions(controller);

                for(unsigned int k = 0; k < all_positions.size(); k++)
                {
                    if(all_positions[k].zone_idx == zone_idx && all_positions[k].led_idx == led_idx)
                    {
                        ctrl_transform->led_positions.push_back(all_positions[k]);
                        break;
                    }
                }
            }
        }

        QStringList pos_parts = in.readLine().split(" ");
        ctrl_transform->transform.position.x = pos_parts[0].toFloat();
        ctrl_transform->transform.position.y = pos_parts[1].toFloat();
        ctrl_transform->transform.position.z = pos_parts[2].toFloat();

        QStringList rot_parts = in.readLine().split(" ");
        ctrl_transform->transform.rotation.x = rot_parts[0].toFloat();
        ctrl_transform->transform.rotation.y = rot_parts[1].toFloat();
        ctrl_transform->transform.rotation.z = rot_parts[2].toFloat();

        QStringList scale_parts = in.readLine().split(" ");
        ctrl_transform->transform.scale.x = scale_parts[0].toFloat();
        ctrl_transform->transform.scale.y = scale_parts[1].toFloat();
        ctrl_transform->transform.scale.z = scale_parts[2].toFloat();

        ctrl_transform->display_color = in.readLine().toUInt();

        controller_transforms.push_back(ctrl_transform);

        QColor color;
        color.setRgb(ctrl_transform->display_color & 0xFF,
                     (ctrl_transform->display_color >> 8) & 0xFF,
                     (ctrl_transform->display_color >> 16) & 0xFF);

        QString name;
        if(is_virtual)
        {
            name = ctrl_name;
        }
        else
        {
            name = QString::fromStdString(controller->name);
            if(ctrl_transform->led_positions.size() < controller->leds.size())
            {
                if(ctrl_transform->led_positions.size() == 1)
                {
                    unsigned int led_global_idx = controller->zones[ctrl_transform->led_positions[0].zone_idx].start_idx +
                                                  ctrl_transform->led_positions[0].led_idx;
                    name += " - " + QString::fromStdString(controller->leds[led_global_idx].name);
                }
                else
                {
                    name += " - " + QString::fromStdString(controller->zones[ctrl_transform->led_positions[0].zone_idx].name);
                }
            }
        }

        QListWidgetItem* item = new QListWidgetItem(name);
        item->setBackground(QBrush(color));
        item->setForeground(QBrush(color.value() > 128 ? Qt::black : Qt::white));
        controller_list->addItem(item);
    }

    file.close();

    effects->SetControllerTransforms(&controller_transforms);
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();

    LOG_INFO("[OpenRGB 3D Spatial] Loaded layout from %s (%d items)", filename.c_str(), (int)controller_transforms.size());
}

std::string OpenRGB3DSpatialTab::GetLayoutPath(const std::string& layout_name)
{
    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path plugins_dir = config_dir / "plugins" / "3d_spatial_layouts";

    QDir dir;
    dir.mkpath(QString::fromStdString(plugins_dir.string()));

    std::string filename = layout_name + ".3dlayout";
    filesystem::path layout_file = plugins_dir / filename;

    return layout_file.string();
}

void OpenRGB3DSpatialTab::PopulateLayoutDropdown()
{
    QString current_text = layout_profiles_combo->currentText();

    layout_profiles_combo->blockSignals(true);
    layout_profiles_combo->clear();

    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path layouts_dir = config_dir / "plugins" / "3d_spatial_layouts";

    QDir dir(QString::fromStdString(layouts_dir.string()));
    QStringList filters;
    filters << "*.3dlayout";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for(const QFileInfo& file_info : files)
    {
        QString base_name = file_info.baseName();
        layout_profiles_combo->addItem(base_name);
    }

    json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    QString saved_profile = "";
    if(settings.contains("SelectedProfile"))
    {
        saved_profile = QString::fromStdString(settings["SelectedProfile"].get<std::string>());
    }

    if(!saved_profile.isEmpty())
    {
        int index = layout_profiles_combo->findText(saved_profile);
        if(index >= 0)
        {
            layout_profiles_combo->setCurrentIndex(index);
        }
    }
    else if(!current_text.isEmpty())
    {
        int index = layout_profiles_combo->findText(current_text);
        if(index >= 0)
        {
            layout_profiles_combo->setCurrentIndex(index);
        }
    }

    layout_profiles_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::SaveCurrentLayoutName()
{
    json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["SelectedProfile"] = layout_profiles_combo->currentText().toStdString();
    resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
    resource_manager->GetSettingsManager()->SaveSettings();
}

void OpenRGB3DSpatialTab::TryAutoLoadLayout()
{
    LOG_INFO("[OpenRGB 3D Spatial] TryAutoLoadLayout called, first_load=%s", first_load ? "true" : "false");

    if(!first_load)
    {
        return;
    }

    first_load = false;

    bool is_checked = auto_load_checkbox->isChecked();
    LOG_INFO("[OpenRGB 3D Spatial] Auto-load checkbox is: %s", is_checked ? "checked" : "unchecked");

    if(is_checked)
    {
        QString profile_name = layout_profiles_combo->currentText();
        LOG_INFO("[OpenRGB 3D Spatial] Selected profile name: '%s'", profile_name.toStdString().c_str());

        if(!profile_name.isEmpty())
        {
            std::string layout_path = GetLayoutPath(profile_name.toStdString());
            QFileInfo check_file(QString::fromStdString(layout_path));
            LOG_INFO("[OpenRGB 3D Spatial] Checking for layout file: %s", layout_path.c_str());

            if(check_file.exists())
            {
                LoadLayout(layout_path);
                LOG_INFO("[OpenRGB 3D Spatial] Auto-loaded profile '%s' on startup", profile_name.toStdString().c_str());
            }
            else
            {
                LOG_WARNING("[OpenRGB 3D Spatial] Auto-load profile '%s' not found at: %s",
                           profile_name.toStdString().c_str(), layout_path.c_str());
            }
        }
        else
        {
            LOG_WARNING("[OpenRGB 3D Spatial] Auto-load enabled but no profile selected");
        }
    }
    else
    {
        LOG_INFO("[OpenRGB 3D Spatial] Auto-load is disabled, skipping");
    }
}

void OpenRGB3DSpatialTab::SaveCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/3d_spatial_custom_controllers";

#ifdef _WIN32
    CreateDirectoryA(custom_dir.c_str(), NULL);
#else
    mkdir(custom_dir.c_str(), 0755);
#endif

    for(const VirtualController3D* ctrl : virtual_controllers)
    {
        std::string safe_name = ctrl->GetName();
        for(char& c : safe_name)
        {
            if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            {
                c = '_';
            }
        }

        std::string filepath = custom_dir + "/" + safe_name + ".json";
        std::ofstream file(filepath);
        if(file.is_open())
        {
            json ctrl_json = ctrl->ToJson();
            file << ctrl_json.dump(4);
            file.close();
            LOG_INFO("[OpenRGB 3D Spatial] Saved custom controller '%s' to %s", ctrl->GetName().c_str(), filepath.c_str());
        }
        else
        {
            LOG_ERROR("[OpenRGB 3D Spatial] Failed to save custom controller '%s' to %s", ctrl->GetName().c_str(), filepath.c_str());
        }
    }
}

void OpenRGB3DSpatialTab::LoadCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/3d_spatial_custom_controllers";

    filesystem::path dir_path(custom_dir);
    if(!filesystem::exists(dir_path))
    {
        LOG_INFO("[OpenRGB 3D Spatial] Custom controllers directory does not exist: %s", custom_dir.c_str());
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    int loaded_count = 0;

    try
    {
        for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir_path))
        {
            if(entry.path().extension() == ".json")
            {
                std::ifstream file(entry.path().string());
                if(file.is_open())
                {
                    try
                    {
                        json ctrl_json;
                        file >> ctrl_json;
                        file.close();

                        VirtualController3D* virtual_ctrl = VirtualController3D::FromJson(ctrl_json, controllers);
                        if(virtual_ctrl)
                        {
                            virtual_controllers.push_back(virtual_ctrl);
                            available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));
                            LOG_INFO("[OpenRGB 3D Spatial] Loaded custom controller: %s (%dx%dx%d) from %s",
                                     virtual_ctrl->GetName().c_str(),
                                     virtual_ctrl->GetWidth(),
                                     virtual_ctrl->GetHeight(),
                                     virtual_ctrl->GetDepth(),
                                     entry.path().filename().string().c_str());
                            loaded_count++;
                        }
                    }
                    catch(const std::exception& e)
                    {
                        LOG_ERROR("[OpenRGB 3D Spatial] Failed to parse custom controller file %s: %s",
                                 entry.path().filename().string().c_str(), e.what());
                    }
                }
            }
        }

        LOG_INFO("[OpenRGB 3D Spatial] Loaded %d custom controllers from %s", loaded_count, custom_dir.c_str());
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB 3D Spatial] Failed to load custom controllers directory: %s", e.what());
    }
}

bool OpenRGB3DSpatialTab::IsItemInScene(RGBController* controller, int granularity, int item_idx)
{
    for(ControllerTransform* ct : controller_transforms)
    {
        if(ct->controller == nullptr) continue;

        if(granularity == 0)
        {
            if(ct->controller == controller)
            {
                bool is_whole_device = true;
                std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateLEDPositions(controller);
                if(ct->led_positions.size() != all_positions.size())
                {
                    is_whole_device = false;
                }
                if(is_whole_device)
                {
                    return true;
                }
            }
        }
        else if(granularity == 1)
        {
            if(ct->controller == controller)
            {
                for(const LEDPosition3D& pos : ct->led_positions)
                {
                    if(pos.zone_idx == (unsigned int)item_idx)
                    {
                        return true;
                    }
                }
            }
        }
        else if(granularity == 2)
        {
            if(ct->controller == controller)
            {
                for(const LEDPosition3D& pos : ct->led_positions)
                {
                    unsigned int global_led_idx = controller->zones[pos.zone_idx].start_idx + pos.led_idx;
                    if(global_led_idx == (unsigned int)item_idx)
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

int OpenRGB3DSpatialTab::GetUnassignedZoneCount(RGBController* controller)
{
    int unassigned_count = 0;
    for(unsigned int i = 0; i < controller->zones.size(); i++)
    {
        if(!IsItemInScene(controller, 1, i))
        {
            unassigned_count++;
        }
    }
    return unassigned_count;
}

int OpenRGB3DSpatialTab::GetUnassignedLEDCount(RGBController* controller)
{
    int total_leds = (int)controller->leds.size();
    int assigned_leds = 0;

    for(ControllerTransform* ct : controller_transforms)
    {
        if(ct->controller == controller)
        {
            assigned_leds += (int)ct->led_positions.size();
        }
    }

    return total_leds - assigned_leds;
}