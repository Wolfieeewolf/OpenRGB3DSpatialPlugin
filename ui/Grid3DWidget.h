/*---------------------------------------------------------*\
| Grid3DWidget.h                                            |
|                                                           |
|   3D grid visualization widget                           |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef GRID3DWIDGET_H
#define GRID3DWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include "SpatialGrid3D.h"

class Grid3DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Grid3DWidget(SpatialGrid3D* g, QWidget *parent = nullptr);

    void setGrid(SpatialGrid3D* g);
    void setRotation(float x, float y, float z);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Point3D
    {
        float x;
        float y;
        float z;
    };

    struct Point2D
    {
        int x;
        int y;
    };

    Point2D Project3DTo2D(Point3D point);
    void DrawGrid(QPainter& painter);
    void DrawDevices(QPainter& painter);

    SpatialGrid3D*  grid;

    float           rotation_x;
    float           rotation_y;
    float           rotation_z;

    float           zoom;

    QPoint          last_mouse_pos;
    bool            mouse_dragging;
};

#endif