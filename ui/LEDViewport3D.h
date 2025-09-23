/*---------------------------------------------------------*\
| LEDViewport3D.h                                           |
|                                                           |
|   OpenGL 3D viewport for LED visualization and control   |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef LEDVIEWPORT3D_H
#define LEDVIEWPORT3D_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <vector>
#include "LEDPosition3D.h"

class LEDViewport3D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit LEDViewport3D(QWidget *parent = nullptr);
    ~LEDViewport3D();

    void SetControllerTransforms(std::vector<ControllerTransform*>* transforms);
    void SelectController(int index);
    void UpdateColors();

signals:
    void ControllerSelected(int index);
    void ControllerPositionChanged(int index, float x, float y, float z);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void DrawGrid();
    void DrawAxes();
    void DrawControllers();
    void DrawLEDs(ControllerTransform* ctrl);
    void DrawGizmo();

    int PickController(int mouse_x, int mouse_y);
    void UpdateGizmo(int dx, int dy);

    std::vector<ControllerTransform*>*  controller_transforms;
    int                                 selected_controller_idx;

    float   camera_distance;
    float   camera_yaw;
    float   camera_pitch;
    float   camera_target_x;
    float   camera_target_y;
    float   camera_target_z;

    bool    dragging_rotate;
    bool    dragging_pan;
    bool    dragging_gizmo;
    QPoint  last_mouse_pos;
};

#endif