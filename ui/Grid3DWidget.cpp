/*---------------------------------------------------------*\
| Grid3DWidget.cpp                                          |
|                                                           |
|   3D grid visualization widget                           |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Grid3DWidget.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Grid3DWidget::Grid3DWidget(SpatialGrid3D* g, QWidget *parent) :
    QWidget(parent),
    grid(g)
{
    rotation_x = 30.0f;
    rotation_y = 45.0f;
    rotation_z = 0.0f;
    zoom = 15.0f;
    mouse_dragging = false;

    setMinimumSize(400, 400);
}

void Grid3DWidget::setGrid(SpatialGrid3D* g)
{
    grid = g;
    update();
}

void Grid3DWidget::setRotation(float x, float y, float z)
{
    rotation_x = x;
    rotation_y = y;
    rotation_z = z;
    update();
}

void Grid3DWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::black);

    if(grid)
    {
        DrawGrid(painter);
        DrawDevices(painter);
    }
}

void Grid3DWidget::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        mouse_dragging = true;
        last_mouse_pos = event->pos();
    }
}

void Grid3DWidget::mouseMoveEvent(QMouseEvent *event)
{
    if(mouse_dragging)
    {
        QPoint delta = event->pos() - last_mouse_pos;

        rotation_y += delta.x() * 0.5f;
        rotation_x += delta.y() * 0.5f;

        last_mouse_pos = event->pos();
        update();
    }
}

void Grid3DWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    zoom += delta * 2.0f;

    if(zoom < 5.0f) zoom = 5.0f;
    if(zoom > 50.0f) zoom = 50.0f;

    update();
}

Grid3DWidget::Point2D Grid3DWidget::Project3DTo2D(Point3D point)
{
    float rad_x = rotation_x * M_PI / 180.0f;
    float rad_y = rotation_y * M_PI / 180.0f;

    float cos_x = cos(rad_x);
    float sin_x = sin(rad_x);
    float cos_y = cos(rad_y);
    float sin_y = sin(rad_y);

    float x1 = point.x * cos_y - point.z * sin_y;
    float z1 = point.x * sin_y + point.z * cos_y;
    float y1 = point.y * cos_x - z1 * sin_x;

    Point2D result;
    result.x = (int)(width() / 2 + x1 * zoom);
    result.y = (int)(height() / 2 - y1 * zoom);

    return result;
}

void Grid3DWidget::DrawGrid(QPainter& painter)
{
    unsigned int grid_w, grid_h, grid_d;
    grid->GetGridDimensions(&grid_w, &grid_h, &grid_d);

    float center_x = grid_w / 2.0f;
    float center_y = grid_h / 2.0f;
    float center_z = grid_d / 2.0f;

    painter.setPen(QPen(QColor(80, 80, 80), 1));

    for(unsigned int x = 0; x <= grid_w; x++)
    {
        Point3D p1 = {(float)x - center_x, -center_y, -center_z};
        Point3D p2 = {(float)x - center_x, -center_y, grid_d - center_z};

        Point2D s1 = Project3DTo2D(p1);
        Point2D s2 = Project3DTo2D(p2);

        painter.drawLine(s1.x, s1.y, s2.x, s2.y);
    }

    for(unsigned int z = 0; z <= grid_d; z++)
    {
        Point3D p1 = {-center_x, -center_y, (float)z - center_z};
        Point3D p2 = {grid_w - center_x, -center_y, (float)z - center_z};

        Point2D s1 = Project3DTo2D(p1);
        Point2D s2 = Project3DTo2D(p2);

        painter.drawLine(s1.x, s1.y, s2.x, s2.y);
    }

    for(unsigned int x = 0; x <= grid_w; x++)
    {
        Point3D p1 = {(float)x - center_x, -center_y, -center_z};
        Point3D p2 = {(float)x - center_x, grid_h - center_y, -center_z};

        Point2D s1 = Project3DTo2D(p1);
        Point2D s2 = Project3DTo2D(p2);

        painter.drawLine(s1.x, s1.y, s2.x, s2.y);
    }
}

void Grid3DWidget::DrawDevices(QPainter& painter)
{
    unsigned int grid_w, grid_h, grid_d;
    grid->GetGridDimensions(&grid_w, &grid_h, &grid_d);

    float center_x = grid_w / 2.0f;
    float center_y = grid_h / 2.0f;
    float center_z = grid_d / 2.0f;

    std::vector<DeviceGridEntry*> devices = grid->GetAllDevices();

    for(unsigned int i = 0; i < devices.size(); i++)
    {
        if(!devices[i]->enabled)
        {
            continue;
        }

        Point3D p = {
            (float)devices[i]->position.x - center_x,
            (float)devices[i]->position.y - center_y,
            (float)devices[i]->position.z - center_z
        };

        Point2D screen = Project3DTo2D(p);

        RGBColor color = 0x00FF00;
        if(devices[i]->controller->colors.size() > 0)
        {
            color = devices[i]->controller->colors[0];
        }

        unsigned char r = (color >> 16) & 0xFF;
        unsigned char g = (color >> 8) & 0xFF;
        unsigned char b = color & 0xFF;

        painter.setBrush(QColor(r, g, b));
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(QPoint(screen.x, screen.y), 8, 8);
    }
}