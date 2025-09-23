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
    viewport_bridge = nullptr;

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

    if(viewport_bridge)
    {
        delete viewport_bridge;
    }
}

void OpenRGB3DSpatialTab::SetupUI()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);

    viewport_widget = new QQuickWidget();
    viewport_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QUrl qml_url("qrc:/ui/Viewport3DSimple.qml");
    viewport_widget->setSource(qml_url);
    viewport_widget->setMinimumHeight(500);

    if(viewport_widget->status() == QQuickWidget::Error)
    {
        qDebug() << "QML Load Error:" << viewport_widget->errors();
    }

    QQuickItem* root_item = viewport_widget->rootObject();
    if(root_item)
    {
        viewport_bridge = new Viewport3DBridge(root_item, this);
    }
    else
    {
        qDebug() << "Failed to get root QML object";
    }

    main_layout->addWidget(viewport_widget);

    QGroupBox* gizmo_group = new QGroupBox("Transform Tools");
    QHBoxLayout* gizmo_layout = new QHBoxLayout();

    QComboBox* gizmo_mode_combo = new QComboBox();
    gizmo_mode_combo->addItem("Translate");
    gizmo_mode_combo->addItem("Rotate");
    gizmo_mode_combo->addItem("Scale");
    connect(gizmo_mode_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_gizmo_mode_changed(int)));
    gizmo_layout->addWidget(new QLabel("Gizmo Mode:"));
    gizmo_layout->addWidget(gizmo_mode_combo);
    gizmo_layout->addStretch();

    gizmo_group->setLayout(gizmo_layout);
    main_layout->addWidget(gizmo_group);

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
    main_layout->addWidget(effect_group);

    main_layout->addStretch();
    setLayout(main_layout);
}

void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager || !viewport_bridge)
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
        ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};
        ctrl_transform->led_positions = ControllerLayout3D::GenerateLEDPositions(controller);

        controller_transforms.push_back(ctrl_transform);

        viewport_bridge->addController(controller);
    }

    effects->SetControllerTransforms(&controller_transforms);
}

void OpenRGB3DSpatialTab::UpdateDeviceList()
{
    LoadDevices();
}

void OpenRGB3DSpatialTab::on_gizmo_mode_changed(int index)
{
    if(viewport_widget && viewport_widget->rootObject())
    {
        QMetaObject::invokeMethod(viewport_widget->rootObject(), "setProperty",
                                 Q_ARG(QVariant, "gizmoMode"),
                                 Q_ARG(QVariant, index));
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