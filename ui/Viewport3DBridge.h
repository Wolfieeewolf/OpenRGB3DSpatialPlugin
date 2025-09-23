/*---------------------------------------------------------*\
| Viewport3DBridge.h                                        |
|                                                           |
|   Bridge between C++ and Qt Quick 3D viewport            |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef VIEWPORT3DBRIDGE_H
#define VIEWPORT3DBRIDGE_H

#include <QObject>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QVector3D>
#include <QVariantList>
#include <QVariantMap>

#include "RGBController.h"
#include "ControllerLayout3D.h"
#include "LEDPosition3D.h"

class ControllerModel3D : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ getName NOTIFY nameChanged)
    Q_PROPERTY(QVariantList ledPositions READ getLEDPositions NOTIFY ledPositionsChanged)
    Q_PROPERTY(QVector3D position READ getPosition WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D rotation READ getRotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(QVector3D scale READ getScale WRITE setScale NOTIFY scaleChanged)

public:
    explicit ControllerModel3D(RGBController* ctrl, QObject *parent = nullptr);

    QString getName() const;
    QVariantList getLEDPositions();
    QVector3D getPosition() const;
    QVector3D getRotation() const;
    QVector3D getScale() const;

    void setPosition(const QVector3D& pos);
    void setRotation(const QVector3D& rot);
    void setScale(const QVector3D& scl);

    RGBController* getController() { return controller; }
    void updateLEDColors();

signals:
    void nameChanged();
    void ledPositionsChanged();
    void positionChanged();
    void rotationChanged();
    void scaleChanged();

private:
    RGBController*              controller;
    std::vector<LEDPosition3D>  led_positions;
    Transform3D                 transform;

    QVariantMap LEDPosToVariant(const LEDPosition3D& led_pos);
};

class Viewport3DBridge : public QObject
{
    Q_OBJECT

public:
    explicit Viewport3DBridge(QQuickItem* viewport, QObject *parent = nullptr);
    ~Viewport3DBridge();

    void addController(RGBController* controller);
    void removeController(RGBController* controller);
    void clearControllers();
    void setSelectedController(RGBController* controller);
    void updateLEDColors();

signals:
    void controllerAdded(QObject* controller);
    void controllerRemoved(QObject* controller);
    void controllersCleared();

private:
    QQuickItem*                         viewport;
    std::vector<ControllerModel3D*>     controller_models;
};

#endif