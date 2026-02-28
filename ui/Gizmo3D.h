// SPDX-License-Identifier: GPL-2.0-only


#ifndef GIZMO3D_H
#define GIZMO3D_H

#include <QMouseEvent>
#include <QPoint>

#include "LEDPosition3D.h"
#include "ControllerLayout3D.h"
#include "VirtualReferencePoint3D.h"
#include "DisplayPlane3D.h"

enum GizmoMode
{
    GIZMO_MODE_MOVE     = 0,
    GIZMO_MODE_ROTATE   = 1,
    GIZMO_MODE_FREEROAM = 2
};

enum GizmoAxis
{
    GIZMO_AXIS_NONE     = -1,
    GIZMO_AXIS_X        = 0,
    GIZMO_AXIS_Y        = 1,
    GIZMO_AXIS_Z        = 2,
    GIZMO_AXIS_CENTER   = 3
};

struct Ray3D
{
    float origin[3];
    float direction[3];
};

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

    void SetMode(GizmoMode mode);
    void CycleMode();
    void SetPosition(float x, float y, float z);
    void SetTarget(ControllerTransform* target);
    void SetTarget(VirtualReferencePoint3D* target);
    void SetTarget(DisplayPlane3D* target);
    void SetViewportSize(int width, int height);
    void SetGridSnap(bool enabled, float grid_size = 1.0f);
    void SetCameraDistance(float distance);

    bool HandleMousePress(QMouseEvent* event, const float* modelview, const float* projection, const int* viewport);
    bool HandleMouseMove(QMouseEvent* event, const float* modelview, const float* projection, const int* viewport);
    bool HandleMouseRelease(QMouseEvent* event);

    void Render(const float* modelview, const float* projection, const int* viewport);

    bool IsActive() const { return active; }
    bool IsDragging() const { return dragging; }
    GizmoAxis GetSelectedAxis() const { return selected_axis; }

private:
    Ray3D GenerateRay(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);
    bool RayBoxIntersect(const Ray3D& ray, const Box3D& box, float& distance);
    bool RaySphereIntersect(const Ray3D& ray, float sphere_x, float sphere_y, float sphere_z, float radius, float& distance);

    GizmoAxis PickGizmoAxis(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);
    bool PickGizmoCenter(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);

    void UpdateTransform(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);
    void ApplyTranslation(float delta_x, float delta_y, float delta_z);
    void ApplyRotation(float delta_x, float delta_y, float delta_z);
    void ApplyFreeroamMovement(float delta_x, float delta_y, const float* modelview, const float* projection, const int* viewport);
    void ApplyFreeroamDragRayPlane(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport);

    void DrawMoveGizmo();
    void DrawRotateGizmo();
    void DrawFreeroamGizmo();
    void DrawAxis(float start[3], float end[3], float color[3], bool highlighted);
    void DrawArrowHead(float pos[3], float dir[3], float color[3]);
    void DrawSphere(float pos[3], float radius, float color[3]);
    void DrawCube(float pos[3], float size, float color[3]);

    void WorldToScreen(float world_x, float world_y, float world_z, int& screen_x, int& screen_y,
                       const float* modelview, const float* projection, const int* viewport);
    void ScreenToWorld(int screen_x, int screen_y, float& world_x, float& world_y, float& world_z,
                       const float* modelview, const float* projection, const int* viewport);

    float SnapToGrid(float value);
    float Dot3(const float a[3], const float b[3]);
    void Cross3(const float a[3], const float b[3], float out[3]);
    void Normalize3(float v[3]);
    float ClosestAxisParamToRay(const float axis_origin[3], const float axis_dir_unit[3], const Ray3D& ray);

private:
    bool                    active;
    bool                    dragging;
    GizmoMode              mode;
    GizmoAxis              selected_axis;
    GizmoAxis              hover_axis;

    ControllerTransform*        target_transform;
    VirtualReferencePoint3D*    target_ref_point;
    DisplayPlane3D*             target_display_plane;

    float                  gizmo_x;
    float                  gizmo_y;
    float                  gizmo_z;
    float                  base_gizmo_size;

    QPoint                 last_mouse_pos;
    QPoint                 drag_start_pos;

    int                    viewport_width;
    int                    viewport_height;
    float                  camera_distance;

    float                  gizmo_size;
    float                  axis_thickness;
    float                  center_sphere_radius;

    float                  color_x_axis[3];
    float                  color_y_axis[3];
    float                  color_z_axis[3];
    float                  color_center[3];
    float                  color_highlight[3];

    bool                   grid_snap_enabled;
    float                  grid_size;

    float                  drag_axis_t0;
    float                  drag_axis_dir[3];
    float                  drag_plane_normal[3];
    float                  drag_start_world[3];
    bool                   center_press_pending;
    float                  rot_plane_normal[3];
    float                  rot_u[3];
    float                  rot_v[3];
    float                  rot_angle0;
};

#endif
