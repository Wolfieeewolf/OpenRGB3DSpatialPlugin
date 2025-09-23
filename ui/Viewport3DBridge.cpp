/*---------------------------------------------------------*\
| Viewport3DBridge.cpp                                      |
|                                                           |
|   Bridge between C++ and Qt Quick 3D viewport            |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Viewport3DBridge.h"
#include <QMetaObject>
#include <QQmlProperty>
#include <cmath>

ControllerModel3D::ControllerModel3D(RGBController* ctrl, QObject *parent)
    : QObject(parent), controller(ctrl)
{
    led_positions = ControllerLayout3D::GenerateLEDPositions(controller);

    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    transform.scale = {1.0f, 1.0f, 1.0f};
}

QString ControllerModel3D::getName() const
{
    return QString::fromStdString(controller->name);
}

QVariantList ControllerModel3D::getLEDPositions()
{
    QVariantList list;

    for(unsigned int i = 0; i < led_positions.size(); i++)
    {
        list.append(LEDPosToVariant(led_positions[i]));
    }

    return list;
}

QVector3D ControllerModel3D::getPosition() const
{
    return QVector3D(transform.position.x, transform.position.y, transform.position.z);
}

QVector3D ControllerModel3D::getRotation() const
{
    float angle = 2.0f * acos(transform.rotation.w) * 180.0f / M_PI;
    float axis_len = sqrt(transform.rotation.x * transform.rotation.x +
                         transform.rotation.y * transform.rotation.y +
                         transform.rotation.z * transform.rotation.z);

    if(axis_len > 0.001f)
    {
        return QVector3D(
            transform.rotation.x / axis_len * angle,
            transform.rotation.y / axis_len * angle,
            transform.rotation.z / axis_len * angle
        );
    }

    return QVector3D(0, 0, 0);
}

QVector3D ControllerModel3D::getScale() const
{
    return QVector3D(transform.scale.x, transform.scale.y, transform.scale.z);
}

void ControllerModel3D::setPosition(const QVector3D& pos)
{
    transform.position.x = pos.x();
    transform.position.y = pos.y();
    transform.position.z = pos.z();
    emit positionChanged();
}

void ControllerModel3D::setRotation(const QVector3D& rot)
{
    float angle = rot.length() * M_PI / 180.0f;

    if(angle > 0.001f)
    {
        QVector3D axis = rot.normalized();
        float half_angle = angle / 2.0f;

        transform.rotation.x = axis.x() * sin(half_angle);
        transform.rotation.y = axis.y() * sin(half_angle);
        transform.rotation.z = axis.z() * sin(half_angle);
        transform.rotation.w = cos(half_angle);
    }
    else
    {
        transform.rotation.x = 0.0f;
        transform.rotation.y = 0.0f;
        transform.rotation.z = 0.0f;
        transform.rotation.w = 1.0f;
    }

    emit rotationChanged();
}

void ControllerModel3D::setScale(const QVector3D& scl)
{
    transform.scale.x = scl.x();
    transform.scale.y = scl.y();
    transform.scale.z = scl.z();
    emit scaleChanged();
}

void ControllerModel3D::updateLEDColors()
{
    emit ledPositionsChanged();
}

QVariantMap ControllerModel3D::LEDPosToVariant(const LEDPosition3D& led_pos)
{
    QVariantMap map;

    QVariantMap local_pos;
    local_pos["x"] = led_pos.local_position.x;
    local_pos["y"] = led_pos.local_position.y;
    local_pos["z"] = led_pos.local_position.z;
    map["localPosition"] = local_pos;

    unsigned int led_global_idx = controller->zones[led_pos.zone_idx].start_idx + led_pos.led_idx;
    RGBColor color = controller->colors[led_global_idx];

    QVariantMap color_map;
    color_map["r"] = ((color >> 16) & 0xFF) / 255.0f;
    color_map["g"] = ((color >> 8) & 0xFF) / 255.0f;
    color_map["b"] = (color & 0xFF) / 255.0f;
    map["color"] = color_map;

    return map;
}

Viewport3DBridge::Viewport3DBridge(QQuickItem* vp, QObject *parent)
    : QObject(parent), viewport(vp)
{
}

Viewport3DBridge::~Viewport3DBridge()
{
    clearControllers();
}

void Viewport3DBridge::addController(RGBController* controller)
{
    ControllerModel3D* model = new ControllerModel3D(controller, this);
    controller_models.push_back(model);

    QMetaObject::invokeMethod(viewport, "addController",
                             Q_ARG(QVariant, QVariant::fromValue(model)));

    emit controllerAdded(model);
}

void Viewport3DBridge::removeController(RGBController* controller)
{
    for(unsigned int i = 0; i < controller_models.size(); i++)
    {
        if(controller_models[i]->getController() == controller)
        {
            ControllerModel3D* model = controller_models[i];
            controller_models.erase(controller_models.begin() + i);

            emit controllerRemoved(model);
            delete model;
            break;
        }
    }
}

void Viewport3DBridge::clearControllers()
{
    for(unsigned int i = 0; i < controller_models.size(); i++)
    {
        delete controller_models[i];
    }
    controller_models.clear();

    QMetaObject::invokeMethod(viewport, "clearControllers");
    emit controllersCleared();
}

void Viewport3DBridge::setSelectedController(RGBController* controller)
{
    for(unsigned int i = 0; i < controller_models.size(); i++)
    {
        if(controller_models[i]->getController() == controller)
        {
            QQmlProperty::write(viewport, "selectedController",
                               QVariant::fromValue(controller_models[i]));
            return;
        }
    }

    QQmlProperty::write(viewport, "selectedController", QVariant());
}

void Viewport3DBridge::updateLEDColors()
{
    for(unsigned int i = 0; i < controller_models.size(); i++)
    {
        controller_models[i]->updateLEDColors();
    }
}