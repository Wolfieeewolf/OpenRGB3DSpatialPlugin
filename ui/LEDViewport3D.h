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

/*---------------------------------------------------------*\
| Qt Includes                                              |
\*---------------------------------------------------------*/
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>

/*---------------------------------------------------------*\
| System Includes                                          |
\*---------------------------------------------------------*/
#include <vector>
#include <memory>

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "LEDPosition3D.h"
#include "ControllerLayout3D.h"
#include "Gizmo3D.h"
#include "SpatialEffectTypes.h"
#include "VirtualController3D.h"

class LEDViewport3D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit LEDViewport3D(QWidget *parent = nullptr);
    ~LEDViewport3D();

    void SetControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    void SelectController(int index);
    void SelectReferencePoint(int index);
    void UpdateColors();
    void SetGridDimensions(int x, int y, int z);
    void SetGridSnapEnabled(bool enabled);
    bool IsGridSnapEnabled() const { return grid_snap_enabled; }
    void SetUserPosition(const UserPosition3D& position);
    void SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points);

    void EnforceFloorConstraint(ControllerTransform* ctrl);
    void UpdateGizmoPosition();
    void NotifyControllerTransformChanged();

    void AddControllerToSelection(int index);
    void RemoveControllerFromSelection(int index);
    void ClearSelection();
    bool IsControllerSelected(int index) const;
    const std::vector<int>& GetSelectedControllers() const { return selected_controller_indices; }

signals:
    void ControllerSelected(int index);
    void ControllerPositionChanged(int index, float x, float y, float z);
    void ControllerRotationChanged(int index, float x, float y, float z);
    void ControllerDeleteRequested(int index);
    void ReferencePointSelected(int index);
    void ReferencePointPositionChanged(int index, float x, float y, float z);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void DrawGrid();
    void DrawReferencePoints();
    void DrawAxes();
    void DrawAxisLabels();
    void DrawControllers();
    void DrawLEDs(ControllerTransform* ctrl);
    void DrawUserFigure();

    int PickController(int mouse_x, int mouse_y);
    int PickReferencePoint(int mouse_x, int mouse_y);
    bool RayBoxIntersect(float ray_origin[3], float ray_direction[3],
                        const Vector3D& box_min, const Vector3D& box_max, float& distance);
    bool RaySphereIntersect(float ray_origin[3], float ray_direction[3],
                           const Vector3D& sphere_center, float sphere_radius, float& distance);

    void CalculateControllerBounds(ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds);
    Vector3D GetControllerCenter(ControllerTransform* ctrl);
    Vector3D GetControllerSize(ControllerTransform* ctrl);

    bool IsControllerAboveFloor(ControllerTransform* ctrl);
    float GetControllerMinY(ControllerTransform* ctrl);

    Vector3D TransformLocalToWorld(const Vector3D& local_pos, const Transform3D& transform);

    std::vector<std::unique_ptr<ControllerTransform>>*  controller_transforms;
    int                                                  selected_controller_idx;
    std::vector<int>                                     selected_controller_indices;

    // Grid dimensions for proper bounds and visualization
    int     grid_x;
    int     grid_y;
    int     grid_z;
    bool    grid_snap_enabled;

    /*---------------------------------------------------------*\
    | User Position Reference Point                            |
    \*---------------------------------------------------------*/
    UserPosition3D  user_position;

    /*---------------------------------------------------------*\
    | Reference Points                                         |
    \*---------------------------------------------------------*/
    std::vector<std::unique_ptr<VirtualReferencePoint3D>>* reference_points;
    int                                     selected_ref_point_idx;

    /*---------------------------------------------------------*\
    | Camera Controls                                          |
    \*---------------------------------------------------------*/
    float   camera_distance;
    float   camera_yaw;
    float   camera_pitch;
    float   camera_target_x;
    float   camera_target_y;
    float   camera_target_z;

    /*---------------------------------------------------------*\
    | Mouse Interaction                                        |
    \*---------------------------------------------------------*/
    bool    dragging_rotate;
    bool    dragging_pan;
    bool    dragging_grab;
    QPoint  last_mouse_pos;
    QPoint  click_start_pos;

    /*---------------------------------------------------------*\
    | 3D Manipulation Gizmo                                   |
    \*---------------------------------------------------------*/
    Gizmo3D gizmo;
};

#endif