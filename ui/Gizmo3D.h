/*---------------------------------------------------------*\
| Gizmo3D.h                                                 |
|                                                           |
|   3D manipulation gizmo with ray casting interaction     |
|                                                           |
|   Date: 2025-09-29                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef GIZMO3D_H
#define GIZMO3D_H

/*---------------------------------------------------------*\
| Qt Includes                                              |
\*---------------------------------------------------------*/
#include <QMouseEvent>
#include <QPoint>

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "LEDPosition3D.h"
#include "ControllerLayout3D.h"
#include "VirtualReferencePoint3D.h"

/*---------------------------------------------------------*\
| Gizmo Operation Modes                                    |
\*---------------------------------------------------------*/
enum GizmoMode
{
    GIZMO_MODE_MOVE     = 0,
    GIZMO_MODE_ROTATE   = 1,
    GIZMO_MODE_FREEROAM = 2
};

/*---------------------------------------------------------*\
| Gizmo Axis Selection                                     |
\*---------------------------------------------------------*/
enum GizmoAxis
{
    GIZMO_AXIS_NONE     = -1,
    GIZMO_AXIS_X        = 0,
    GIZMO_AXIS_Y        = 1,
    GIZMO_AXIS_Z        = 2,
    GIZMO_AXIS_CENTER   = 3
};

/*---------------------------------------------------------*\
| 3D Ray Structure                                         |
\*---------------------------------------------------------*/
struct Ray3D
{
    float origin[3];
    float direction[3];
};

/*---------------------------------------------------------*\
| 3D Bounding Box Structure                                |
\*---------------------------------------------------------*/
struct Box3D
{
    float min[3];
    float max[3];
};

class Gizmo3D
{
public:
    Gizmo3D();
    ~Gizmo3D();

    /*---------------------------------------------------------*\
    | Gizmo Configuration                                      |
    \*---------------------------------------------------------*/
    void SetMode(GizmoMode mode);
    void CycleMode(); // Cycle through all modes when center is clicked
    void SetPosition(float x, float y, float z);
    void SetTarget(ControllerTransform* target);
    void SetTarget(VirtualReferencePoint3D* target);
    void SetViewportSize(int width, int height);
    void SetGridSnap(bool enabled, float grid_size = 1.0f);

    /*---------------------------------------------------------*\
    | Mouse Interaction                                        |
    \*---------------------------------------------------------*/
    bool HandleMousePress(QMouseEvent* event, const float* modelview, const float* projection, const int* viewport);
    bool HandleMouseMove(QMouseEvent* event, const float* modelview, const float* projection, const int* viewport);
    bool HandleMouseRelease(QMouseEvent* event);

    /*---------------------------------------------------------*\
    | Rendering                                                |
    \*---------------------------------------------------------*/
    void Render(const float* modelview, const float* projection, const int* viewport);

    /*---------------------------------------------------------*\
    | State Queries                                            |
    \*---------------------------------------------------------*/
    bool IsActive() const { return active; }
    bool IsDragging() const { return dragging; }
    GizmoAxis GetSelectedAxis() const { return selected_axis; }

private:
    /*---------------------------------------------------------*\
    | Ray Casting                                              |
    \*---------------------------------------------------------*/
    Ray3D GenerateRay(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);
    bool RayBoxIntersect(const Ray3D& ray, const Box3D& box, float& distance);
    bool RaySphereIntersect(const Ray3D& ray, float sphere_x, float sphere_y, float sphere_z, float radius, float& distance);

    /*---------------------------------------------------------*\
    | Gizmo Hit Testing                                        |
    \*---------------------------------------------------------*/
    GizmoAxis PickGizmoAxis(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);
    bool PickGizmoCenter(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);

    /*---------------------------------------------------------*\
    | Gizmo Manipulation                                       |
    \*---------------------------------------------------------*/
    void UpdateTransform(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);
    void ApplyTranslation(float delta_x, float delta_y, float delta_z);
    void ApplyRotation(float delta_x, float delta_y, float delta_z);
    void ApplyFreeroamMovement(float delta_x, float delta_y, const float* modelview, const float* projection, const int* viewport);

    /*---------------------------------------------------------*\
    | Rendering Helpers                                        |
    \*---------------------------------------------------------*/
    void DrawMoveGizmo();
    void DrawRotateGizmo();
    void DrawFreeroamGizmo();
    void DrawAxis(float start[3], float end[3], float color[3], bool highlighted);
    void DrawArrowHead(float pos[3], float dir[3], float color[3]);
    void DrawSphere(float pos[3], float radius, float color[3]);
    void DrawCube(float pos[3], float size, float color[3]);

    /*---------------------------------------------------------*\
    | Coordinate Conversion                                    |
    \*---------------------------------------------------------*/
    void WorldToScreen(float world_x, float world_y, float world_z, int& screen_x, int& screen_y,
                       const float* modelview, const float* projection, const int* viewport);
    void ScreenToWorld(int screen_x, int screen_y, float& world_x, float& world_y, float& world_z,
                       const float* modelview, const float* projection, const int* viewport);

    /*---------------------------------------------------------*\
    | Grid Snapping Helper                                     |
    \*---------------------------------------------------------*/
    float SnapToGrid(float value);

private:
    /*---------------------------------------------------------*\
    | Gizmo State                                              |
    \*---------------------------------------------------------*/
    bool                    active;
    bool                    dragging;
    GizmoMode              mode;
    GizmoAxis              selected_axis;

    /*---------------------------------------------------------*\
    | Target Transform                                         |
    \*---------------------------------------------------------*/
    ControllerTransform*        target_transform;
    VirtualReferencePoint3D*    target_ref_point;

    /*---------------------------------------------------------*\
    | Gizmo Position                                           |
    \*---------------------------------------------------------*/
    float                  gizmo_x;
    float                  gizmo_y;
    float                  gizmo_z;

    /*---------------------------------------------------------*\
    | Mouse State                                              |
    \*---------------------------------------------------------*/
    QPoint                 last_mouse_pos;
    QPoint                 drag_start_pos;

    /*---------------------------------------------------------*\
    | Viewport Info                                            |
    \*---------------------------------------------------------*/
    int                    viewport_width;
    int                    viewport_height;

    /*---------------------------------------------------------*\
    | Gizmo Appearance                                         |
    \*---------------------------------------------------------*/
    float                  gizmo_size;
    float                  axis_thickness;
    float                  center_sphere_radius;

    /*---------------------------------------------------------*\
    | Colors                                                   |
    \*---------------------------------------------------------*/
    float                  color_x_axis[3];     // Red
    float                  color_y_axis[3];     // Green
    float                  color_z_axis[3];     // Blue
    float                  color_center[3];     // Yellow
    float                  color_highlight[3];  // White

    /*---------------------------------------------------------*\
    | Grid Snapping                                            |
    \*---------------------------------------------------------*/
    bool                   grid_snap_enabled;
    float                  grid_size;
};

#endif