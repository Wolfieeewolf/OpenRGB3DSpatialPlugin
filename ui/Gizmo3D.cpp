/*---------------------------------------------------------*\
| Gizmo3D.cpp                                               |
|                                                           |
|   3D manipulation gizmo with ray casting interaction     |
|                                                           |
|   Date: 2025-09-29                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "Gizmo3D.h"

/*---------------------------------------------------------*\
| System Includes                                          |
\*---------------------------------------------------------*/
#include <cmath>

/*---------------------------------------------------------*\
| OpenGL Includes                                          |
\*---------------------------------------------------------*/
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*---------------------------------------------------------*\
| Gizmo Constants                                          |
\*---------------------------------------------------------*/
#define GIZMO_SIZE                  1.5f
#define AXIS_THICKNESS              0.1f
#define CENTER_SPHERE_RADIUS        0.2f
#define RAY_INTERSECTION_TOLERANCE  0.15f

Gizmo3D::Gizmo3D()
{
    active = false;
    dragging = false;
    mode = GIZMO_MODE_MOVE;
    selected_axis = GIZMO_AXIS_NONE;
    target_transform = nullptr;

    gizmo_x = 0.0f;
    gizmo_y = 0.0f;
    gizmo_z = 0.0f;

    viewport_width = 800;
    viewport_height = 600;

    gizmo_size = GIZMO_SIZE;
    axis_thickness = AXIS_THICKNESS;
    center_sphere_radius = CENTER_SPHERE_RADIUS;

    /*---------------------------------------------------------*\
    | Initialize Colors                                        |
    \*---------------------------------------------------------*/
    color_x_axis[0] = 1.0f; color_x_axis[1] = 0.0f; color_x_axis[2] = 0.0f;  // Red
    color_y_axis[0] = 0.0f; color_y_axis[1] = 1.0f; color_y_axis[2] = 0.0f;  // Green
    color_z_axis[0] = 0.0f; color_z_axis[1] = 0.0f; color_z_axis[2] = 1.0f;  // Blue
    color_center[0] = 1.0f; color_center[1] = 1.0f; color_center[2] = 0.0f;  // Yellow
    color_highlight[0] = 1.0f; color_highlight[1] = 1.0f; color_highlight[2] = 1.0f;  // White

    /*---------------------------------------------------------*\
    | Initialize Grid Snapping                                 |
    \*---------------------------------------------------------*/
    grid_snap_enabled = false;
    grid_size = 1.0f;
}

Gizmo3D::~Gizmo3D()
{
}

void Gizmo3D::SetMode(GizmoMode new_mode)
{
    mode = new_mode;
}

void Gizmo3D::CycleMode()
{
    // Cycle through: MOVE → ROTATE → FREEROAM → MOVE (skip scale)
    switch(mode)
    {
        case GIZMO_MODE_MOVE:
            mode = GIZMO_MODE_ROTATE;
            break;
        case GIZMO_MODE_ROTATE:
            mode = GIZMO_MODE_FREEROAM;
            break;
        case GIZMO_MODE_FREEROAM:
            mode = GIZMO_MODE_MOVE;
            break;
    }

    // Ensure gizmo stays centered on target after mode change
    // Note: The actual centering will be done by LEDViewport3D::UpdateGizmoPosition()
    // which calls GetControllerCenter() for proper bounds-based centering
}

void Gizmo3D::SetPosition(float x, float y, float z)
{
    gizmo_x = x;
    gizmo_y = y;
    gizmo_z = z;
}

void Gizmo3D::SetTarget(ControllerTransform* target)
{
    target_transform = target;
    active = (target != nullptr);

    if(target)
    {
        SetPosition(target->transform.position.x, target->transform.position.y, target->transform.position.z);
    }
}

void Gizmo3D::SetViewportSize(int width, int height)
{
    viewport_width = width;
    viewport_height = height;
}

void Gizmo3D::SetGridSnap(bool enabled, float size)
{
    grid_snap_enabled = enabled;
    grid_size = size;
}

bool Gizmo3D::HandleMousePress(QMouseEvent* event, const float* modelview, const float* projection, const int* viewport)
{
    if(!active || !target_transform)
        return false;

    last_mouse_pos = event->pos();
    drag_start_pos = event->pos();

    /*---------------------------------------------------------*\
    | Check for gizmo interaction                             |
    \*---------------------------------------------------------*/
    selected_axis = PickGizmoAxis(event->x(), event->y(), modelview, projection, viewport);

    if(selected_axis == GIZMO_AXIS_CENTER)
    {
        // Center sphere clicked - cycle mode instead of dragging
        CycleMode();
        // Note: The viewport will call UpdateGizmoPosition() to recenter the gizmo
        return true; // Mode switched, don't start dragging
    }
    else if(selected_axis != GIZMO_AXIS_NONE)
    {
        dragging = true;
        return true; // Gizmo captured the mouse
    }

    return false; // No gizmo interaction
}

bool Gizmo3D::HandleMouseMove(QMouseEvent* event, const float* modelview, const float* projection, const int* viewport)
{
    if(!active || !dragging || !target_transform)
        return false;

    UpdateTransform(event->x(), event->y(), modelview, projection, viewport);
    last_mouse_pos = event->pos();

    // Force gizmo to stay locked to controller center
    // The viewport will call UpdateGizmoPosition() which recalculates the center

    return true;
}

bool Gizmo3D::HandleMouseRelease(QMouseEvent* event)
{
    (void)event;

    if(!active || !dragging)
        return false;

    dragging = false;
    selected_axis = GIZMO_AXIS_NONE;

    return true;
}

void Gizmo3D::Render(const float* modelview, const float* projection, const int* viewport)
{
    (void)modelview;
    (void)projection;
    (void)viewport;

    if(!active)
        return;

    glPushMatrix();
    glTranslatef(gizmo_x, gizmo_y, gizmo_z);

    switch(mode)
    {
        case GIZMO_MODE_MOVE:
            DrawMoveGizmo();
            break;
        case GIZMO_MODE_ROTATE:
            DrawRotateGizmo();
            break;
        case GIZMO_MODE_FREEROAM:
            DrawFreeroamGizmo();
            break;
    }

    glPopMatrix();
}

Ray3D Gizmo3D::GenerateRay(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    Ray3D ray;

    /*---------------------------------------------------------*\
    | Convert mouse coordinates to normalized device coords   |
    \*---------------------------------------------------------*/
    float x = (2.0f * mouse_x) / viewport[2] - 1.0f;
    float y = 1.0f - (2.0f * mouse_y) / viewport[3];

    /*---------------------------------------------------------*\
    | Create ray in clip space                                |
    \*---------------------------------------------------------*/
    float ray_clip[4] = { x, y, -1.0f, 1.0f };

    /*---------------------------------------------------------*\
    | Convert to eye coordinates                              |
    \*---------------------------------------------------------*/
    float inv_projection[16];
    // Simple inverse for perspective projection
    for(int i = 0; i < 16; i++) inv_projection[i] = 0.0f;
    inv_projection[0] = 1.0f / projection[0];
    inv_projection[5] = 1.0f / projection[5];
    inv_projection[10] = 0.0f;
    inv_projection[11] = -1.0f;
    inv_projection[14] = 1.0f / projection[14];
    inv_projection[15] = 0.0f;

    float ray_eye[4];
    for(int i = 0; i < 4; i++)
    {
        ray_eye[i] = 0.0f;
        for(int j = 0; j < 4; j++)
        {
            ray_eye[i] += inv_projection[i * 4 + j] * ray_clip[j];
        }
    }
    ray_eye[2] = -1.0f;
    ray_eye[3] = 0.0f;

    /*---------------------------------------------------------*\
    | Convert to world coordinates                            |
    \*---------------------------------------------------------*/
    float inv_modelview[16];
    // Simple inverse for basic transformations (this is simplified)
    for(int i = 0; i < 16; i++) inv_modelview[i] = modelview[i];

    float ray_world[4];
    for(int i = 0; i < 4; i++)
    {
        ray_world[i] = 0.0f;
        for(int j = 0; j < 4; j++)
        {
            ray_world[i] += inv_modelview[i * 4 + j] * ray_eye[j];
        }
    }

    /*---------------------------------------------------------*\
    | Set ray origin (camera position)                       |
    \*---------------------------------------------------------*/
    ray.origin[0] = modelview[12];
    ray.origin[1] = modelview[13];
    ray.origin[2] = modelview[14];

    /*---------------------------------------------------------*\
    | Set ray direction (normalized)                          |
    \*---------------------------------------------------------*/
    float length = sqrtf(ray_world[0] * ray_world[0] + ray_world[1] * ray_world[1] + ray_world[2] * ray_world[2]);
    if(length > 0.0f)
    {
        ray.direction[0] = ray_world[0] / length;
        ray.direction[1] = ray_world[1] / length;
        ray.direction[2] = ray_world[2] / length;
    }
    else
    {
        ray.direction[0] = 0.0f;
        ray.direction[1] = 0.0f;
        ray.direction[2] = -1.0f;
    }

    return ray;
}

bool Gizmo3D::RayBoxIntersect(const Ray3D& ray, const Box3D& box, float& distance)
{
    float tmin = 0.0f;
    float tmax = 1000.0f;

    for(int i = 0; i < 3; i++)
    {
        if(fabsf(ray.direction[i]) < 1e-6f)
        {
            if(ray.origin[i] < box.min[i] || ray.origin[i] > box.max[i])
                return false;
        }
        else
        {
            float t1 = (box.min[i] - ray.origin[i]) / ray.direction[i];
            float t2 = (box.max[i] - ray.origin[i]) / ray.direction[i];

            if(t1 > t2) { float temp = t1; t1 = t2; t2 = temp; }

            tmin = fmaxf(tmin, t1);
            tmax = fminf(tmax, t2);

            if(tmin > tmax)
                return false;
        }
    }

    distance = tmin;
    return true;
}

bool Gizmo3D::RaySphereIntersect(const Ray3D& ray, float sphere_x, float sphere_y, float sphere_z, float radius, float& distance)
{
    float dx = ray.origin[0] - sphere_x;
    float dy = ray.origin[1] - sphere_y;
    float dz = ray.origin[2] - sphere_z;

    float a = ray.direction[0] * ray.direction[0] + ray.direction[1] * ray.direction[1] + ray.direction[2] * ray.direction[2];
    float b = 2.0f * (dx * ray.direction[0] + dy * ray.direction[1] + dz * ray.direction[2]);
    float c = dx * dx + dy * dy + dz * dz - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if(discriminant < 0.0f)
        return false;

    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

    if(t1 > 0.0f)
        distance = t1;
    else if(t2 > 0.0f)
        distance = t2;
    else
        return false;

    return true;
}

GizmoAxis Gizmo3D::PickGizmoAxis(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    Ray3D ray = GenerateRay(mouse_x, mouse_y, modelview, projection, viewport);

    float closest_distance = 1000.0f;
    GizmoAxis closest_axis = GIZMO_AXIS_NONE;

    /*---------------------------------------------------------*\
    | Check center sphere first                               |
    \*---------------------------------------------------------*/
    float distance;
    if(RaySphereIntersect(ray, gizmo_x, gizmo_y, gizmo_z, center_sphere_radius, distance))
    {
        if(distance < closest_distance)
        {
            closest_distance = distance;
            closest_axis = GIZMO_AXIS_CENTER;
        }
    }

    /*---------------------------------------------------------*\
    | Check X axis                                            |
    \*---------------------------------------------------------*/
    Box3D x_box;
    x_box.min[0] = gizmo_x; x_box.max[0] = gizmo_x + gizmo_size;
    x_box.min[1] = gizmo_y - axis_thickness; x_box.max[1] = gizmo_y + axis_thickness;
    x_box.min[2] = gizmo_z - axis_thickness; x_box.max[2] = gizmo_z + axis_thickness;

    if(RayBoxIntersect(ray, x_box, distance) && distance < closest_distance)
    {
        closest_distance = distance;
        closest_axis = GIZMO_AXIS_X;
    }

    /*---------------------------------------------------------*\
    | Check Y axis                                            |
    \*---------------------------------------------------------*/
    Box3D y_box;
    y_box.min[0] = gizmo_x - axis_thickness; y_box.max[0] = gizmo_x + axis_thickness;
    y_box.min[1] = gizmo_y; y_box.max[1] = gizmo_y + gizmo_size;
    y_box.min[2] = gizmo_z - axis_thickness; y_box.max[2] = gizmo_z + axis_thickness;

    if(RayBoxIntersect(ray, y_box, distance) && distance < closest_distance)
    {
        closest_distance = distance;
        closest_axis = GIZMO_AXIS_Y;
    }

    /*---------------------------------------------------------*\
    | Check Z axis                                            |
    \*---------------------------------------------------------*/
    Box3D z_box;
    z_box.min[0] = gizmo_x - axis_thickness; z_box.max[0] = gizmo_x + axis_thickness;
    z_box.min[1] = gizmo_y - axis_thickness; z_box.max[1] = gizmo_y + axis_thickness;
    z_box.min[2] = gizmo_z; z_box.max[2] = gizmo_z + gizmo_size;

    if(RayBoxIntersect(ray, z_box, distance) && distance < closest_distance)
    {
        closest_distance = distance;
        closest_axis = GIZMO_AXIS_Z;
    }

    return closest_axis;
}

bool Gizmo3D::PickGizmoCenter(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    Ray3D ray = GenerateRay(mouse_x, mouse_y, modelview, projection, viewport);

    float distance;
    return RaySphereIntersect(ray, gizmo_x, gizmo_y, gizmo_z, center_sphere_radius, distance);
}

void Gizmo3D::UpdateTransform(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    (void)modelview;
    (void)projection;
    (void)viewport;

    if(!target_transform)
        return;

    float delta_x = mouse_x - last_mouse_pos.x();
    float delta_y = mouse_y - last_mouse_pos.y();

    float sensitivity = 0.01f;

    switch(mode)
    {
        case GIZMO_MODE_MOVE:
            {
                float move_delta[3] = { 0.0f, 0.0f, 0.0f };

                switch(selected_axis)
                {
                    case GIZMO_AXIS_X:
                        move_delta[0] = delta_x * sensitivity;
                        break;
                    case GIZMO_AXIS_Y:
                        move_delta[1] = -delta_y * sensitivity;
                        break;
                    case GIZMO_AXIS_Z:
                        move_delta[2] = delta_x * sensitivity;
                        break;
                    case GIZMO_AXIS_CENTER:
                        move_delta[0] = delta_x * sensitivity;
                        move_delta[1] = -delta_y * sensitivity;
                        break;
                }

                ApplyTranslation(move_delta[0], move_delta[1], move_delta[2]);
            }
            break;

        case GIZMO_MODE_ROTATE:
            {
                float rotate_delta[3] = { 0.0f, 0.0f, 0.0f };

                switch(selected_axis)
                {
                    case GIZMO_AXIS_X:
                        rotate_delta[0] = delta_y * sensitivity * 10.0f;
                        break;
                    case GIZMO_AXIS_Y:
                        rotate_delta[1] = delta_x * sensitivity * 10.0f;
                        break;
                    case GIZMO_AXIS_Z:
                        rotate_delta[2] = delta_x * sensitivity * 10.0f;
                        break;
                }

                ApplyRotation(rotate_delta[0], rotate_delta[1], rotate_delta[2]);
            }
            break;

        case GIZMO_MODE_FREEROAM:
            {
                // Freeroam movement in camera plane (screen space)
                ApplyFreeroamMovement(delta_x, delta_y, modelview, projection, viewport);
            }
            break;
    }
}

float Gizmo3D::SnapToGrid(float value)
{
    if(!grid_snap_enabled || grid_size <= 0.0f)
        return value;

    return roundf(value / grid_size) * grid_size;
}

void Gizmo3D::ApplyTranslation(float delta_x, float delta_y, float delta_z)
{
    if(!target_transform)
        return;

    // Apply the movement delta
    target_transform->transform.position.x += delta_x;
    target_transform->transform.position.y += delta_y;
    target_transform->transform.position.z += delta_z;

    // Apply grid snapping if enabled
    if(grid_snap_enabled)
    {
        target_transform->transform.position.x = SnapToGrid(target_transform->transform.position.x);
        target_transform->transform.position.y = SnapToGrid(target_transform->transform.position.y);
        target_transform->transform.position.z = SnapToGrid(target_transform->transform.position.z);
    }

    // Update gizmo position to follow target
    // Note: The actual centering will be done by LEDViewport3D::UpdateGizmoPosition()
    // which calls GetControllerCenter() for proper bounds-based centering
}

void Gizmo3D::ApplyRotation(float delta_x, float delta_y, float delta_z)
{
    if(!target_transform)
        return;

    target_transform->transform.rotation.x += delta_x;
    target_transform->transform.rotation.y += delta_y;
    target_transform->transform.rotation.z += delta_z;

    // Clamp rotations to valid ranges
    while(target_transform->transform.rotation.x > 360.0f) target_transform->transform.rotation.x -= 360.0f;
    while(target_transform->transform.rotation.x < 0.0f) target_transform->transform.rotation.x += 360.0f;
    while(target_transform->transform.rotation.y > 360.0f) target_transform->transform.rotation.y -= 360.0f;
    while(target_transform->transform.rotation.y < 0.0f) target_transform->transform.rotation.y += 360.0f;
    while(target_transform->transform.rotation.z > 360.0f) target_transform->transform.rotation.z -= 360.0f;
    while(target_transform->transform.rotation.z < 0.0f) target_transform->transform.rotation.z += 360.0f;

    // Keep gizmo centered on the controller - rotation doesn't change position
    // Note: The actual centering will be done by LEDViewport3D::UpdateGizmoPosition()
    // which calls GetControllerCenter() for proper bounds-based centering
}

void Gizmo3D::ApplyFreeroamMovement(float delta_x, float delta_y, const float* modelview, const float* projection, const int* viewport)
{
    (void)modelview;
    (void)projection;
    (void)viewport;

    if(!target_transform)
        return;

    // Extract camera angles from modelview matrix - simplified approach
    // In a full implementation, we'd properly extract camera yaw/pitch
    // For now, use screen-space movement with reasonable sensitivity
    float move_scale = 0.01f;

    // Simple screen-space to world-space conversion
    // This provides intuitive movement where dragging moves the object
    // in the direction you drag on screen
    target_transform->transform.position.x += delta_x * move_scale;
    target_transform->transform.position.y -= delta_y * move_scale; // Invert Y for intuitive movement

    // Update gizmo position to follow target
    // Note: The actual centering will be done by LEDViewport3D::UpdateGizmoPosition()
    // which calls GetControllerCenter() for proper bounds-based centering
}


void Gizmo3D::DrawMoveGizmo()
{
    glDisable(GL_LIGHTING);
    glLineWidth(4.0f);

    /*---------------------------------------------------------*\
    | Draw 3D arrow axes with proper arrowheads               |
    \*---------------------------------------------------------*/

    // X axis (Red) - pointing right
    float* color = (selected_axis == GIZMO_AXIS_X) ? color_highlight : color_x_axis;
    glColor3f(color[0], color[1], color[2]);

    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(gizmo_size, 0.0f, 0.0f);
    glEnd();

    // X arrow head
    glBegin(GL_TRIANGLES);
    glVertex3f(gizmo_size, 0.0f, 0.0f);
    glVertex3f(gizmo_size - 0.3f, 0.15f, 0.0f);
    glVertex3f(gizmo_size - 0.3f, -0.15f, 0.0f);

    glVertex3f(gizmo_size, 0.0f, 0.0f);
    glVertex3f(gizmo_size - 0.3f, 0.0f, 0.15f);
    glVertex3f(gizmo_size - 0.3f, 0.0f, -0.15f);
    glEnd();

    // Y axis (Green) - pointing up
    color = (selected_axis == GIZMO_AXIS_Y) ? color_highlight : color_y_axis;
    glColor3f(color[0], color[1], color[2]);

    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, gizmo_size, 0.0f);
    glEnd();

    // Y arrow head
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, gizmo_size, 0.0f);
    glVertex3f(0.15f, gizmo_size - 0.3f, 0.0f);
    glVertex3f(-0.15f, gizmo_size - 0.3f, 0.0f);

    glVertex3f(0.0f, gizmo_size, 0.0f);
    glVertex3f(0.0f, gizmo_size - 0.3f, 0.15f);
    glVertex3f(0.0f, gizmo_size - 0.3f, -0.15f);
    glEnd();

    // Z axis (Blue) - pointing forward
    color = (selected_axis == GIZMO_AXIS_Z) ? color_highlight : color_z_axis;
    glColor3f(color[0], color[1], color[2]);

    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, gizmo_size);
    glEnd();

    // Z arrow head
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, gizmo_size);
    glVertex3f(0.15f, 0.0f, gizmo_size - 0.3f);
    glVertex3f(-0.15f, 0.0f, gizmo_size - 0.3f);

    glVertex3f(0.0f, 0.0f, gizmo_size);
    glVertex3f(0.0f, 0.15f, gizmo_size - 0.3f);
    glVertex3f(0.0f, -0.15f, gizmo_size - 0.3f);
    glEnd();

    /*---------------------------------------------------------*\
    | Draw orange center cube for mode switching             |
    \*---------------------------------------------------------*/
    float orange[3] = {1.0f, 0.5f, 0.0f}; // Orange color for mode switching
    color = (selected_axis == GIZMO_AXIS_CENTER) ? color_highlight : orange;
    float center[3] = { 0.0f, 0.0f, 0.0f };
    DrawCube(center, center_sphere_radius, color);

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

void Gizmo3D::DrawCube(float pos[3], float size, float color[3])
{
    glColor3f(color[0], color[1], color[2]);

    glPushMatrix();
    glTranslatef(pos[0], pos[1], pos[2]);

    // Draw cube faces using quads (similar to original orange center cube)
    glBegin(GL_QUADS);

    // Front face
    glVertex3f(-size, -size, -size);
    glVertex3f(+size, -size, -size);
    glVertex3f(+size, +size, -size);
    glVertex3f(-size, +size, -size);

    // Back face
    glVertex3f(-size, -size, +size);
    glVertex3f(+size, -size, +size);
    glVertex3f(+size, +size, +size);
    glVertex3f(-size, +size, +size);

    // Left face
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, -size, +size);
    glVertex3f(-size, +size, +size);
    glVertex3f(-size, +size, -size);

    // Right face
    glVertex3f(+size, -size, -size);
    glVertex3f(+size, -size, +size);
    glVertex3f(+size, +size, +size);
    glVertex3f(+size, +size, -size);

    // Bottom face
    glVertex3f(-size, -size, -size);
    glVertex3f(+size, -size, -size);
    glVertex3f(+size, -size, +size);
    glVertex3f(-size, -size, +size);

    // Top face
    glVertex3f(-size, +size, -size);
    glVertex3f(+size, +size, -size);
    glVertex3f(+size, +size, +size);
    glVertex3f(-size, +size, +size);

    glEnd();
    glPopMatrix();
}

void Gizmo3D::DrawRotateGizmo()
{
    glDisable(GL_LIGHTING);
    glLineWidth(3.0f);

    /*---------------------------------------------------------*\
    | Draw rotation rings for each axis                       |
    \*---------------------------------------------------------*/

    // X axis rotation ring (Red) - rotation around X axis (YZ plane)
    float* color = (selected_axis == GIZMO_AXIS_X) ? color_highlight : color_x_axis;
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_LINE_LOOP);
    for(int i = 0; i <= 32; i++)
    {
        float angle = (i / 32.0f) * 2.0f * M_PI;
        glVertex3f(0.0f, cosf(angle) * gizmo_size, sinf(angle) * gizmo_size);
    }
    glEnd();

    // Y axis rotation ring (Green) - rotation around Y axis (XZ plane)
    color = (selected_axis == GIZMO_AXIS_Y) ? color_highlight : color_y_axis;
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_LINE_LOOP);
    for(int i = 0; i <= 32; i++)
    {
        float angle = (i / 32.0f) * 2.0f * M_PI;
        glVertex3f(cosf(angle) * gizmo_size, 0.0f, sinf(angle) * gizmo_size);
    }
    glEnd();

    // Z axis rotation ring (Blue) - rotation around Z axis (XY plane)
    color = (selected_axis == GIZMO_AXIS_Z) ? color_highlight : color_z_axis;
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_LINE_LOOP);
    for(int i = 0; i <= 32; i++)
    {
        float angle = (i / 32.0f) * 2.0f * M_PI;
        glVertex3f(cosf(angle) * gizmo_size, sinf(angle) * gizmo_size, 0.0f);
    }
    glEnd();

    /*---------------------------------------------------------*\
    | Draw orange center cube for mode switching             |
    \*---------------------------------------------------------*/
    float orange[3] = {1.0f, 0.5f, 0.0f}; // Orange color for mode switching
    color = (selected_axis == GIZMO_AXIS_CENTER) ? color_highlight : orange;
    float center[3] = { 0.0f, 0.0f, 0.0f };
    DrawCube(center, center_sphere_radius, color);

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

void Gizmo3D::DrawAxis(float start[3], float end[3], float color[3], bool highlighted)
{
    (void)highlighted;

    glColor3f(color[0], color[1], color[2]);

    glBegin(GL_LINES);
    glVertex3f(start[0], start[1], start[2]);
    glVertex3f(end[0], end[1], end[2]);
    glEnd();

    // Draw arrow head
    DrawArrowHead(end, end, color);
}

void Gizmo3D::DrawArrowHead(float pos[3], float dir[3], float color[3])
{
    (void)dir;

    glColor3f(color[0], color[1], color[2]);

    // Simple arrow head implementation
    float arrow_size = 0.1f;

    glPushMatrix();
    glTranslatef(pos[0], pos[1], pos[2]);

    glBegin(GL_TRIANGLES);
    // Simple pyramid arrow head
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(-arrow_size, arrow_size, 0.0f);
    glVertex3f(-arrow_size, -arrow_size, 0.0f);
    glEnd();

    glPopMatrix();
}

void Gizmo3D::DrawSphere(float pos[3], float radius, float color[3])
{
    glColor3f(color[0], color[1], color[2]);

    glPushMatrix();
    glTranslatef(pos[0], pos[1], pos[2]);

    // Simple sphere using triangle strips
    const int slices = 16;
    const int stacks = 16;

    for(int i = 0; i < stacks; i++)
    {
        float lat0 = M_PI * (-0.5f + (float)i / stacks);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / stacks);
        float y0 = radius * sinf(lat0);
        float y1 = radius * sinf(lat1);
        float r0 = radius * cosf(lat0);
        float r1 = radius * cosf(lat1);

        glBegin(GL_TRIANGLE_STRIP);
        for(int j = 0; j <= slices; j++)
        {
            float lng = 2.0f * M_PI * j / slices;
            float x = cosf(lng);
            float z = sinf(lng);

            glVertex3f(x * r0, y0, z * r0);
            glVertex3f(x * r1, y1, z * r1);
        }
        glEnd();
    }

    glPopMatrix();
}

void Gizmo3D::WorldToScreen(float world_x, float world_y, float world_z, int& screen_x, int& screen_y,
                           const float* modelview, const float* projection, const int* viewport)
{
    (void)world_z;
    (void)modelview;
    (void)projection;
    (void)viewport;

    // Implementation would use OpenGL matrix math to convert world to screen coordinates
    // This is a simplified version
    screen_x = (int)(world_x * 100.0f + viewport_width / 2);
    screen_y = (int)(world_y * 100.0f + viewport_height / 2);
}

void Gizmo3D::ScreenToWorld(int screen_x, int screen_y, float& world_x, float& world_y, float& world_z,
                           const float* modelview, const float* projection, const int* viewport)
{
    (void)modelview;
    (void)projection;
    (void)viewport;

    // Implementation would use OpenGL matrix math to convert screen to world coordinates
    // This is a simplified version
    world_x = (screen_x - viewport_width / 2) / 100.0f;
    world_y = (screen_y - viewport_height / 2) / 100.0f;
    world_z = 0.0f;
}

void Gizmo3D::DrawFreeroamGizmo()
{
    glDisable(GL_LIGHTING);

    /*---------------------------------------------------------*\
    | Draw vertical stick for freeroam grab handle            |
    \*---------------------------------------------------------*/
    glLineWidth(5.0f);

    // Purple color for freeroam stick
    float purple[3] = {0.5f, 0.0f, 1.0f};
    float* stick_color = (selected_axis == GIZMO_AXIS_CENTER) ? color_highlight : purple;
    glColor3f(stick_color[0], stick_color[1], stick_color[2]);

    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, gizmo_size, 0.0f);
    glEnd();

    /*---------------------------------------------------------*\
    | Draw purple grab cube at top of stick                   |
    \*---------------------------------------------------------*/
    float cube_size = 0.3f;
    float stick_height = gizmo_size;

    glBegin(GL_QUADS);

    // Front face
    glVertex3f(-cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, -cube_size);

    // Back face
    glVertex3f(-cube_size, stick_height - cube_size, +cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, +cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, +cube_size);

    // Left face
    glVertex3f(-cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height - cube_size, +cube_size);

    // Right face
    glVertex3f(+cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, +cube_size);

    // Bottom face
    glVertex3f(-cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height - cube_size, +cube_size);

    // Top face
    glVertex3f(-cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, +cube_size);

    glEnd();

    /*---------------------------------------------------------*\
    | Draw orange center cube for mode switching             |
    \*---------------------------------------------------------*/
    float orange[3] = {1.0f, 0.5f, 0.0f}; // Orange color for mode switching
    float* center_color = (selected_axis == GIZMO_AXIS_CENTER) ? color_highlight : orange;
    float center[3] = { 0.0f, 0.0f, 0.0f };
    DrawCube(center, center_sphere_radius, center_color);

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}