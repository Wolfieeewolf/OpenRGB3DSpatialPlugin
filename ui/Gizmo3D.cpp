// SPDX-License-Identifier: GPL-2.0-only

#include "Gizmo3D.h"
#include "QtCompat.h"

#include <cmath>
#include <QMouseEvent>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

#define GIZMO_RING_SEGMENTS         72

#define GIZMO_SIZE                  1.5f
#define AXIS_THICKNESS              0.1f
#define AXIS_HIT_THICKNESS          0.25f
#define CENTER_SPHERE_RADIUS        0.30f
#define CENTER_SPHERE_HIT_RADIUS    0.40f
#define ROTATE_RING_HIT_THICKNESS   0.35f
#define GIZMO_DRAG_SLOP_GL_PX       3.0f

namespace
{

void BeginGizmoSmoothLines()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void EndGizmoSmoothLines()
{
    glDisable(GL_LINE_SMOOTH);
}

void DrawGizmoRingLoop(int plane_axis, float radius, float r, float g, float b, float alpha)
{
    BeginGizmoSmoothLines();
    glColor4f(r, g, b, alpha);
    glBegin(GL_LINE_LOOP);
    for(int i = 0; i < GIZMO_RING_SEGMENTS; i++)
    {
        const float angle = ((float)i / (float)GIZMO_RING_SEGMENTS) * 2.0f * (float)M_PI;
        const float c = cosf(angle);
        const float s = sinf(angle);
        if(plane_axis == 0)
        {
            glVertex3f(0.0f, c * radius, s * radius);
        }
        else if(plane_axis == 1)
        {
            glVertex3f(c * radius, 0.0f, s * radius);
        }
        else
        {
            glVertex3f(c * radius, s * radius, 0.0f);
        }
    }
    glEnd();
    EndGizmoSmoothLines();
}

void DrawGizmoLineSegment(float x0, float y0, float z0, float x1, float y1, float z1,
                        float r, float g, float b, float alpha)
{
    BeginGizmoSmoothLines();
    glColor4f(r, g, b, alpha);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y1, z1);
    glEnd();
    EndGizmoSmoothLines();
}

} // namespace

Gizmo3D::Gizmo3D()
{
    active = false;
    dragging = false;
    mode = GIZMO_MODE_MOVE;
    selected_axis = GIZMO_AXIS_NONE;
    hover_axis = GIZMO_AXIS_NONE;
    target_transform = nullptr;
    target_ref_point = nullptr;
    target_display_plane = nullptr;

    gizmo_x = 0.0f;
    gizmo_y = 0.0f;
    gizmo_z = 0.0f;

    viewport_width = 800;
    viewport_height = 600;

    base_gizmo_size = GIZMO_SIZE;
    gizmo_size = GIZMO_SIZE;
    axis_thickness = AXIS_THICKNESS;
    center_sphere_radius = CENTER_SPHERE_RADIUS;

    color_x_axis[0] = 1.0f; color_x_axis[1] = 0.0f; color_x_axis[2] = 0.0f;
    color_y_axis[0] = 0.0f; color_y_axis[1] = 1.0f; color_y_axis[2] = 0.0f;
    color_z_axis[0] = 0.0f; color_z_axis[1] = 0.0f; color_z_axis[2] = 1.0f;
    color_center[0] = 1.0f; color_center[1] = 1.0f; color_center[2] = 0.0f;
    color_highlight[0] = 1.0f; color_highlight[1] = 1.0f; color_highlight[2] = 1.0f;

    grid_snap_enabled = false;
    grid_size = 1.0f;

    camera_distance = 20.0f;

    drag_axis_t0 = 0.0f;
    drag_axis_t_begin = 0.0f;
    drag_start_position[0] = drag_start_position[1] = drag_start_position[2] = 0.0f;
    drag_axis_dir[0] = 1.0f; drag_axis_dir[1] = 0.0f; drag_axis_dir[2] = 0.0f;
    drag_plane_normal[0] = 0.0f; drag_plane_normal[1] = 1.0f; drag_plane_normal[2] = 0.0f;
    drag_start_world[0] = drag_start_world[1] = drag_start_world[2] = 0.0f;
    drag_grab_offset[0] = drag_grab_offset[1] = drag_grab_offset[2] = 0.0f;
    center_press_pending = false;
    rot_plane_normal[0] = 1.0f; rot_plane_normal[1] = 0.0f; rot_plane_normal[2] = 0.0f;
    rot_u[0]=0.0f; rot_u[1]=1.0f; rot_u[2]=0.0f;
    rot_v[0]=0.0f; rot_v[1]=0.0f; rot_v[2]=1.0f;
    rot_angle0 = 0.0f;
    rot_drag_start_angle = 0.0f;
    rot_drag_accum_degrees = 0.0f;
    rot_snap_remainder_degrees = 0.0f;
    rotate_snap_active = false;
    rotate_snap_step_degrees = 15.0f;
}

Gizmo3D::~Gizmo3D() = default;

void Gizmo3D::SetMode(GizmoMode new_mode)
{
    dragging = false;
    center_press_pending = false;
    selected_axis = GIZMO_AXIS_NONE;
    hover_axis = GIZMO_AXIS_NONE;
    mode = new_mode;
}

void Gizmo3D::CycleMode()
{
    dragging = false;
    center_press_pending = false;
    selected_axis = GIZMO_AXIS_NONE;
    hover_axis = GIZMO_AXIS_NONE;

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
    target_ref_point = nullptr;
    target_display_plane = nullptr;
    active = (target != nullptr);

    if(target)
    {
        SetPosition(target->transform.position.x, target->transform.position.y, target->transform.position.z);
    }
}

void Gizmo3D::SetTarget(VirtualReferencePoint3D* target)
{
    target_ref_point = target;
    target_transform = nullptr;
    target_display_plane = nullptr;
    active = (target != nullptr);

    if(target)
    {
        Vector3D pos = target->GetPosition();
        SetPosition(pos.x, pos.y, pos.z);
    }
}

void Gizmo3D::SetTarget(DisplayPlane3D* target)
{
    target_display_plane = target;
    target_transform = nullptr;
    target_ref_point = nullptr;
    active = (target != nullptr);

    if(target)
    {
        Transform3D& t = target->GetTransform();
        SetPosition(t.position.x, t.position.y, t.position.z);
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

void Gizmo3D::SetCameraDistance(float distance)
{
    if(distance < 0.01f) distance = 0.01f;
    camera_distance = distance;
    float scale = camera_distance * 0.05f;
    if(scale < 0.25f) scale = 0.25f;
    if(scale > 10.0f) scale = 10.0f;
    gizmo_size = base_gizmo_size * scale;
}

bool Gizmo3D::HandleMousePress(QMouseEvent* event, int gl_win_x, int gl_win_y,
                               const float* modelview, const float* projection, const int* viewport)
{
    if(!active || (!target_transform && !target_ref_point && !target_display_plane))
        return false;

    last_mouse_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    drag_start_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    last_gl_mouse_pos = QPoint(gl_win_x, gl_win_y);

    selected_axis = PickGizmoAxis(gl_win_x, gl_win_y, modelview, projection, viewport);

    if(selected_axis == GIZMO_AXIS_CENTER)
    {
        if(mode == GIZMO_MODE_FREEROAM)
        {
            center_press_pending = true;
            dragging = false;

            float right[3] = { modelview[0], modelview[4], modelview[8] };
            float up[3]    = { modelview[1], modelview[5], modelview[9] };
            drag_plane_normal[0] = right[1]*up[2] - right[2]*up[1];
            drag_plane_normal[1] = right[2]*up[0] - right[0]*up[2];
            drag_plane_normal[2] = right[0]*up[1] - right[1]*up[0];
            float len = sqrtf(drag_plane_normal[0]*drag_plane_normal[0] + drag_plane_normal[1]*drag_plane_normal[1] + drag_plane_normal[2]*drag_plane_normal[2]);
            if(len > 1e-6f) { drag_plane_normal[0]/=len; drag_plane_normal[1]/=len; drag_plane_normal[2]/=len; }
            drag_start_world[0] = gizmo_x;
            drag_start_world[1] = gizmo_y;
            drag_start_world[2] = gizmo_z;
            captureDragStartPosition();
            drag_grab_offset[0] = 0.0f;
            drag_grab_offset[1] = 0.0f;
            drag_grab_offset[2] = 0.0f;

            Ray3D ray = GenerateRay(gl_win_x, gl_win_y, modelview, projection, viewport);
            float n_dot_d = drag_plane_normal[0]*ray.direction[0] + drag_plane_normal[1]*ray.direction[1] + drag_plane_normal[2]*ray.direction[2];
            if(fabsf(n_dot_d) > 1e-6f)
            {
                float w0x = drag_start_world[0] - ray.origin[0];
                float w0y = drag_start_world[1] - ray.origin[1];
                float w0z = drag_start_world[2] - ray.origin[2];
                float t = (drag_plane_normal[0]*w0x + drag_plane_normal[1]*w0y + drag_plane_normal[2]*w0z) / n_dot_d;
                if(t >= 0.0f)
                {
                    float hitx = ray.origin[0] + t*ray.direction[0];
                    float hity = ray.origin[1] + t*ray.direction[1];
                    float hitz = ray.origin[2] + t*ray.direction[2];
                    drag_grab_offset[0] = hitx - gizmo_x;
                    drag_grab_offset[1] = hity - gizmo_y;
                    drag_grab_offset[2] = hitz - gizmo_z;
                }
            }
            return true;
        }
        CycleMode();
        return true;
    }
    else if(selected_axis != GIZMO_AXIS_NONE)
    {
        dragging = true;
        if(mode == GIZMO_MODE_ROTATE && (selected_axis == GIZMO_AXIS_X || selected_axis == GIZMO_AXIS_Y || selected_axis == GIZMO_AXIS_Z))
        {
            rot_plane_normal[0] = (selected_axis == GIZMO_AXIS_X) ? 1.0f : 0.0f;
            rot_plane_normal[1] = (selected_axis == GIZMO_AXIS_Y) ? 1.0f : 0.0f;
            rot_plane_normal[2] = (selected_axis == GIZMO_AXIS_Z) ? 1.0f : 0.0f;
            if(selected_axis == GIZMO_AXIS_X) { rot_u[0]=0.0f; rot_u[1]=1.0f; rot_u[2]=0.0f; rot_v[0]=0.0f; rot_v[1]=0.0f; rot_v[2]=1.0f; }
            if(selected_axis == GIZMO_AXIS_Y) { rot_u[0]=1.0f; rot_u[1]=0.0f; rot_u[2]=0.0f; rot_v[0]=0.0f; rot_v[1]=0.0f; rot_v[2]=1.0f; }
            if(selected_axis == GIZMO_AXIS_Z) { rot_u[0]=1.0f; rot_u[1]=0.0f; rot_u[2]=0.0f; rot_v[0]=0.0f; rot_v[1]=1.0f; rot_v[2]=0.0f; }

            Ray3D ray = GenerateRay(gl_win_x, gl_win_y, modelview, projection, viewport);
            float center[3] = { gizmo_x, gizmo_y, gizmo_z };
            float denom = rot_plane_normal[0]*ray.direction[0] + rot_plane_normal[1]*ray.direction[1] + rot_plane_normal[2]*ray.direction[2];
            float angle = 0.0f;
            if(fabsf(denom) > 1e-6f)
            {
                float w0x = center[0] - ray.origin[0];
                float w0y = center[1] - ray.origin[1];
                float w0z = center[2] - ray.origin[2];
                float t = (rot_plane_normal[0]*w0x + rot_plane_normal[1]*w0y + rot_plane_normal[2]*w0z) / denom;
                float hx = ray.origin[0] + t*ray.direction[0] - center[0];
                float hy = ray.origin[1] + t*ray.direction[1] - center[1];
                float hz = ray.origin[2] + t*ray.direction[2] - center[2];
                float x = hx*rot_u[0] + hy*rot_u[1] + hz*rot_u[2];
                float y = hx*rot_v[0] + hy*rot_v[1] + hz*rot_v[2];
                angle = atan2f(y, x);
            }
            rot_angle0 = angle;
            rot_drag_start_angle = angle;
            rot_drag_accum_degrees = 0.0f;
            rot_snap_remainder_degrees = 0.0f;
            rotate_snap_active = ((event->modifiers() & Qt::ShiftModifier) != 0);
        }
        else
        {
            Ray3D ray = GenerateRay(gl_win_x, gl_win_y, modelview, projection, viewport);
            float origin[3] = { gizmo_x, gizmo_y, gizmo_z };
            if(selected_axis == GIZMO_AXIS_X) { drag_axis_dir[0] = 1.0f; drag_axis_dir[1] = 0.0f; drag_axis_dir[2] = 0.0f; }
            if(selected_axis == GIZMO_AXIS_Y) { drag_axis_dir[0] = 0.0f; drag_axis_dir[1] = 1.0f; drag_axis_dir[2] = 0.0f; }
            if(selected_axis == GIZMO_AXIS_Z) { drag_axis_dir[0] = 0.0f; drag_axis_dir[1] = 0.0f; drag_axis_dir[2] = 1.0f; }
            float a[3] = { drag_axis_dir[0], drag_axis_dir[1], drag_axis_dir[2] };
            float d[3] = { ray.direction[0], ray.direction[1], ray.direction[2] };
            float w0[3] = { origin[0] - ray.origin[0], origin[1] - ray.origin[1], origin[2] - ray.origin[2] };
            float A = a[0]*a[0] + a[1]*a[1] + a[2]*a[2];
            float B = a[0]*d[0] + a[1]*d[1] + a[2]*d[2];
            float C = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
            float D = a[0]*w0[0] + a[1]*w0[1] + a[2]*w0[2];
            float E = d[0]*w0[0] + d[1]*w0[1] + d[2]*w0[2];
            float denom = A*C - B*B;
            drag_axis_t0 = (fabsf(denom) < 1e-6f) ? D : ((B*E - C*D) / denom);
            drag_axis_t_begin = drag_axis_t0;
            captureDragStartPosition();
        }
        return true;
    }

    return false;
}

bool Gizmo3D::HandleMouseMove(QMouseEvent* event, int gl_win_x, int gl_win_y,
                              const float* modelview, const float* projection, const int* viewport)
{
    if(!active || (!target_transform && !target_ref_point && !target_display_plane))
        return false;

    if(center_press_pending && !dragging)
    {
        QPoint cur((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
        const float scale_x = (float)viewport[2] / (float)std::max(1, viewport_width);
        const float scale_y = (float)viewport[3] / (float)std::max(1, viewport_height);
        const float dx = (float)(cur.x() - drag_start_pos.x()) * scale_x;
        const float dy = (float)(cur.y() - drag_start_pos.y()) * scale_y;
        const float dist_gl = sqrtf(dx * dx + dy * dy);
        if(dist_gl >= GIZMO_DRAG_SLOP_GL_PX)
        {
            dragging = true;
            last_mouse_pos = cur;
            last_gl_mouse_pos = QPoint(gl_win_x, gl_win_y);
            return true;
        }
        return false;
    }
    else if(dragging)
    {
        rotate_snap_active = ((event->modifiers() & Qt::ShiftModifier) != 0);
        UpdateTransform(gl_win_x, gl_win_y, modelview, projection, viewport);
        last_mouse_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
        last_gl_mouse_pos = QPoint(gl_win_x, gl_win_y);
        return true;
    }
    hover_axis = PickGizmoAxis(gl_win_x, gl_win_y, modelview, projection, viewport);
    return false;
}

bool Gizmo3D::HandleMouseRelease(QMouseEvent* event)
{
    (void)event;

    if(!active)
        return false;

    if(center_press_pending && !dragging)
    {
        center_press_pending = false;
        CycleMode();
        return true;
    }

    if(dragging)
    {
        dragging = false;
        selected_axis = GIZMO_AXIS_NONE;
        hover_axis = GIZMO_AXIS_NONE;
        center_press_pending = false;
        rot_drag_accum_degrees = 0.0f;
        rot_snap_remainder_degrees = 0.0f;
        rotate_snap_active = false;
        return true;
    }

    return false;
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

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

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

    glDisable(GL_DEPTH_TEST);
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

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

Ray3D Gizmo3D::GenerateRay(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    Ray3D ray;

    GLdouble near_x, near_y, near_z;
    GLdouble far_x, far_y, far_z;

    /* mouse_x / mouse_y are OpenGL window coords (same as gluProject / GL_VIEWPORT), from LEDViewport3D::MapQtMouseToGluWindow. */
    const GLdouble gl_win_y = (GLdouble)mouse_y;

    GLdouble mv[16], proj[16];
    GLint vp[4];
    for(int i = 0; i < 16; i++)
    {
        mv[i] = (GLdouble)modelview[i];
        proj[i] = (GLdouble)projection[i];
    }
    for(int i = 0; i < 4; i++)
    {
        vp[i] = (GLint)viewport[i];
    }

    gluUnProject((GLdouble)mouse_x, gl_win_y, 0.0,
                 mv, proj, vp,
                 &near_x, &near_y, &near_z);

    gluUnProject((GLdouble)mouse_x, gl_win_y, 1.0,
                 mv, proj, vp,
                 &far_x, &far_y, &far_z);

    ray.origin[0] = (float)near_x;
    ray.origin[1] = (float)near_y;
    ray.origin[2] = (float)near_z;

    float dx = (float)(far_x - near_x);
    float dy = (float)(far_y - near_y);
    float dz = (float)(far_z - near_z);
    float length = sqrtf(dx * dx + dy * dy + dz * dz);

    if(length > 0.0f)
    {
        ray.direction[0] = dx / length;
        ray.direction[1] = dy / length;
        ray.direction[2] = dz / length;
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
    float tmax = 1.0e30f;

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

    float distance;
    if(RaySphereIntersect(ray, gizmo_x, gizmo_y, gizmo_z, CENTER_SPHERE_HIT_RADIUS, distance))
    {
        return GIZMO_AXIS_CENTER;
    }

    if(mode == GIZMO_MODE_ROTATE)
    {
        const float ring_radius = gizmo_size;
        const float ring_thickness = ROTATE_RING_HIT_THICKNESS;
        for(int axis = 0; axis < 3; axis++)
        {
            float n[3] = { 0.0f, 0.0f, 0.0f };
            if(axis == 0) n[0] = 1.0f;
            if(axis == 1) n[1] = 1.0f;
            if(axis == 2) n[2] = 1.0f;
            float denom = Dot3(n, ray.direction);
            if(fabsf(denom) < 1e-6f)
            {
                continue;
            }
            float w0[3] = { gizmo_x - ray.origin[0], gizmo_y - ray.origin[1], gizmo_z - ray.origin[2] };
            float t = Dot3(n, w0) / denom;
            if(t <= 0.0f)
            {
                continue;
            }

            float hx = ray.origin[0] + t * ray.direction[0] - gizmo_x;
            float hy = ray.origin[1] + t * ray.direction[1] - gizmo_y;
            float hz = ray.origin[2] + t * ray.direction[2] - gizmo_z;

            float radial_sq = 0.0f;
            if(axis == 0) radial_sq = hy*hy + hz*hz;
            if(axis == 1) radial_sq = hx*hx + hz*hz;
            if(axis == 2) radial_sq = hx*hx + hy*hy;
            float radial = sqrtf(radial_sq);
            float ring_err = fabsf(radial - ring_radius);

            if(ring_err <= ring_thickness && t < closest_distance)
            {
                closest_distance = t;
                closest_axis = (axis == 0) ? GIZMO_AXIS_X : ((axis == 1) ? GIZMO_AXIS_Y : GIZMO_AXIS_Z);
            }
        }

        if(closest_axis != GIZMO_AXIS_NONE)
            return closest_axis;
    }

    if(mode == GIZMO_MODE_FREEROAM)
    {
        float cube_center[3] = { gizmo_x, gizmo_y + gizmo_size, gizmo_z };
        float s = 0.3f;
        Box3D cube_box;
        cube_box.min[0] = cube_center[0] - s; cube_box.max[0] = cube_center[0] + s;
        cube_box.min[1] = cube_center[1] - s; cube_box.max[1] = cube_center[1] + s;
        cube_box.min[2] = cube_center[2] - s; cube_box.max[2] = cube_center[2] + s;
        float dist;
        if(RayBoxIntersect(ray, cube_box, dist))
        {
            return GIZMO_AXIS_CENTER;
        }
    }

    Box3D x_box;
    x_box.min[0] = gizmo_x; x_box.max[0] = gizmo_x + gizmo_size;
    x_box.min[1] = gizmo_y - AXIS_HIT_THICKNESS; x_box.max[1] = gizmo_y + AXIS_HIT_THICKNESS;
    x_box.min[2] = gizmo_z - AXIS_HIT_THICKNESS; x_box.max[2] = gizmo_z + AXIS_HIT_THICKNESS;

    if(RayBoxIntersect(ray, x_box, distance) && distance < closest_distance)
    {
        closest_distance = distance;
        closest_axis = GIZMO_AXIS_X;
    }

    Box3D y_box;
    y_box.min[0] = gizmo_x - AXIS_HIT_THICKNESS; y_box.max[0] = gizmo_x + AXIS_HIT_THICKNESS;
    y_box.min[1] = gizmo_y; y_box.max[1] = gizmo_y + gizmo_size;
    y_box.min[2] = gizmo_z - AXIS_HIT_THICKNESS; y_box.max[2] = gizmo_z + AXIS_HIT_THICKNESS;

    if(RayBoxIntersect(ray, y_box, distance) && distance < closest_distance)
    {
        closest_distance = distance;
        closest_axis = GIZMO_AXIS_Y;
    }

    Box3D z_box;
    z_box.min[0] = gizmo_x - AXIS_HIT_THICKNESS; z_box.max[0] = gizmo_x + AXIS_HIT_THICKNESS;
    z_box.min[1] = gizmo_y - AXIS_HIT_THICKNESS; z_box.max[1] = gizmo_y + AXIS_HIT_THICKNESS;
    z_box.min[2] = gizmo_z; z_box.max[2] = gizmo_z + gizmo_size;

    if(RayBoxIntersect(ray, z_box, distance) && distance < closest_distance)
    {
        closest_axis = GIZMO_AXIS_Z;
    }

    return closest_axis;
}

bool Gizmo3D::PickGizmoCenter(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    Ray3D ray = GenerateRay(mouse_x, mouse_y, modelview, projection, viewport);

    float distance;
    return RaySphereIntersect(ray, gizmo_x, gizmo_y, gizmo_z, CENTER_SPHERE_HIT_RADIUS, distance);
}

void Gizmo3D::UpdateTransform(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    if(!target_transform && !target_ref_point && !target_display_plane)
        return;

    switch(mode)
    {
        case GIZMO_MODE_MOVE:
            {
                if(selected_axis == GIZMO_AXIS_X || selected_axis == GIZMO_AXIS_Y || selected_axis == GIZMO_AXIS_Z)
                {
                    Ray3D ray = GenerateRay(mouse_x, mouse_y, modelview, projection, viewport);
                    const float origin[3] = {drag_start_position[0], drag_start_position[1], drag_start_position[2]};
                    float a[3] = {drag_axis_dir[0], drag_axis_dir[1], drag_axis_dir[2]};
                    const float alen = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
                    if(alen > 1e-6f)
                    {
                        a[0] /= alen;
                        a[1] /= alen;
                        a[2] /= alen;
                    }
                    const float d[3] = {ray.direction[0], ray.direction[1], ray.direction[2]};
                    const float w0[3] = {origin[0] - ray.origin[0], origin[1] - ray.origin[1], origin[2] - ray.origin[2]};
                    const float A = a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
                    const float B = a[0] * d[0] + a[1] * d[1] + a[2] * d[2];
                    const float C = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
                    const float D = a[0] * w0[0] + a[1] * w0[1] + a[2] * w0[2];
                    const float E = d[0] * w0[0] + d[1] * w0[1] + d[2] * w0[2];
                    const float denom = A * C - B * B;
                    const float t_now = (fabsf(denom) < 1e-6f) ? D : ((B * E - C * D) / denom);
                    const float along = t_now - drag_axis_t_begin;
                    setTargetWorldPosition(drag_start_position[0] + a[0] * along,
                                           drag_start_position[1] + a[1] * along,
                                           drag_start_position[2] + a[2] * along);
                    drag_axis_t0 = t_now;
                }
            }
            break;

        case GIZMO_MODE_ROTATE:
            {
                if(selected_axis == GIZMO_AXIS_X || selected_axis == GIZMO_AXIS_Y || selected_axis == GIZMO_AXIS_Z)
                {
                    Ray3D ray = GenerateRay(mouse_x, mouse_y, modelview, projection, viewport);
                    float center[3] = { gizmo_x, gizmo_y, gizmo_z };
                    float denom = rot_plane_normal[0]*ray.direction[0] + rot_plane_normal[1]*ray.direction[1] + rot_plane_normal[2]*ray.direction[2];
                    if(fabsf(denom) > 1e-6f)
                    {
                        float w0x = center[0] - ray.origin[0];
                        float w0y = center[1] - ray.origin[1];
                        float w0z = center[2] - ray.origin[2];
                        float t = (rot_plane_normal[0]*w0x + rot_plane_normal[1]*w0y + rot_plane_normal[2]*w0z) / denom;
                        float hx = ray.origin[0] + t*ray.direction[0] - center[0];
                        float hy = ray.origin[1] + t*ray.direction[1] - center[1];
                        float hz = ray.origin[2] + t*ray.direction[2] - center[2];
                        float x = hx*rot_u[0] + hy*rot_u[1] + hz*rot_u[2];
                        float y = hx*rot_v[0] + hy*rot_v[1] + hz*rot_v[2];
                        float angle_now = atan2f(y, x);
                        float dtheta = angle_now - rot_angle0;
                        while(dtheta > (float)M_PI) dtheta -= (float)(2.0 * M_PI);
                        while(dtheta < (float)-M_PI) dtheta += (float)(2.0 * M_PI);
                        float raw_deg = dtheta * (180.0f / (float)M_PI);
                        float deg = raw_deg;
                        if(rotate_snap_active && rotate_snap_step_degrees > 0.0f)
                        {
                            float total = rot_snap_remainder_degrees + raw_deg;
                            float sign = (total >= 0.0f) ? 1.0f : -1.0f;
                            float whole_steps = floorf(fabsf(total) / rotate_snap_step_degrees);
                            deg = sign * whole_steps * rotate_snap_step_degrees;
                            rot_snap_remainder_degrees = total - deg;
                        }
                        else
                        {
                            rot_snap_remainder_degrees = 0.0f;
                        }
                        float rx=0, ry=0, rz=0;
                        if(selected_axis == GIZMO_AXIS_X) rx = deg;
                        if(selected_axis == GIZMO_AXIS_Y) ry = deg;
                        if(selected_axis == GIZMO_AXIS_Z) rz = deg;
                        ApplyRotation(rx, ry, rz);
                        rot_angle0 = angle_now;
                        rot_drag_accum_degrees += deg;
                    }
                    else
                    {
                        float delta_x = (float)(mouse_x - last_gl_mouse_pos.x());
                        float delta_y = (float)(mouse_y - last_gl_mouse_pos.y());
                        float sensitivity = 0.05f;
                        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
                        if(selected_axis == GIZMO_AXIS_X) rx = delta_y * sensitivity * 10.0f;
                        if(selected_axis == GIZMO_AXIS_Y) ry = delta_x * sensitivity * 10.0f;
                        if(selected_axis == GIZMO_AXIS_Z) rz = delta_x * sensitivity * 10.0f;
                        ApplyRotation(rx, ry, rz);
                    }
                }
            }
            break;

        case GIZMO_MODE_FREEROAM:
            {
                ApplyFreeroamDragRayPlane(mouse_x, mouse_y, modelview, projection, viewport);
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

void Gizmo3D::captureDragStartPosition()
{
    drag_start_position[0] = gizmo_x;
    drag_start_position[1] = gizmo_y;
    drag_start_position[2] = gizmo_z;

    if(target_transform)
    {
        drag_start_position[0] = target_transform->transform.position.x;
        drag_start_position[1] = target_transform->transform.position.y;
        drag_start_position[2] = target_transform->transform.position.z;
    }
    else if(target_ref_point)
    {
        const Vector3D pos = target_ref_point->GetPosition();
        drag_start_position[0] = pos.x;
        drag_start_position[1] = pos.y;
        drag_start_position[2] = pos.z;
    }
    else if(target_display_plane)
    {
        const Transform3D& transform = target_display_plane->GetTransform();
        drag_start_position[0] = transform.position.x;
        drag_start_position[1] = transform.position.y;
        drag_start_position[2] = transform.position.z;
    }
}

void Gizmo3D::setTargetWorldPosition(float x, float y, float z)
{
    if(grid_snap_enabled)
    {
        x = SnapToGrid(x);
        y = SnapToGrid(y);
        z = SnapToGrid(z);
    }

    if(target_transform)
    {
        target_transform->transform.position.x = x;
        target_transform->transform.position.y = y;
        target_transform->transform.position.z = z;
    }
    else if(target_ref_point)
    {
        target_ref_point->SetPosition({x, y, z});
    }
    else if(target_display_plane)
    {
        Transform3D& transform = target_display_plane->GetTransform();
        transform.position.x = x;
        transform.position.y = y;
        transform.position.z = z;
    }

    gizmo_x = x;
    gizmo_y = y;
    gizmo_z = z;
}

float Gizmo3D::Dot3(const float a[3], const float b[3])
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void Gizmo3D::Cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

float Gizmo3D::ClosestAxisParamToRay(const float axis_origin[3], const float axis_dir_unit[3], const Ray3D& ray)
{
    float a[3] = { axis_dir_unit[0], axis_dir_unit[1], axis_dir_unit[2] };
    float d[3] = { ray.direction[0], ray.direction[1], ray.direction[2] };
    float w0[3] = { axis_origin[0] - ray.origin[0], axis_origin[1] - ray.origin[1], axis_origin[2] - ray.origin[2] };
    float A = Dot3(a,a);
    float B = Dot3(a,d);
    float C = Dot3(d,d);
    float D = Dot3(a,w0);
    float E = Dot3(d,w0);
    float denom = A*C - B*B;
    if(fabsf(denom) < 1e-6f) return D;
    return (B*E - C*D) / denom;
}

void Gizmo3D::ApplyTranslation(float delta_x, float delta_y, float delta_z)
{
    setTargetWorldPosition(gizmo_x + delta_x, gizmo_y + delta_y, gizmo_z + delta_z);
}

void Gizmo3D::ApplyRotation(float delta_x, float delta_y, float delta_z)
{
    if(target_ref_point)
    {
        Rotation3D rot = target_ref_point->GetRotation();
        rot.x += delta_x;
        rot.y += delta_y;
        rot.z += delta_z;

        while(rot.x > 360.0f) rot.x -= 360.0f;
        while(rot.x < 0.0f) rot.x += 360.0f;
        while(rot.y > 360.0f) rot.y -= 360.0f;
        while(rot.y < 0.0f) rot.y += 360.0f;
        while(rot.z > 360.0f) rot.z -= 360.0f;
        while(rot.z < 0.0f) rot.z += 360.0f;

        target_ref_point->SetRotation(rot);
    }
    else if(target_transform)
    {
        target_transform->transform.rotation.x += delta_x;
        target_transform->transform.rotation.y += delta_y;
        target_transform->transform.rotation.z += delta_z;

        while(target_transform->transform.rotation.x > 360.0f) target_transform->transform.rotation.x -= 360.0f;
        while(target_transform->transform.rotation.x < 0.0f) target_transform->transform.rotation.x += 360.0f;
        while(target_transform->transform.rotation.y > 360.0f) target_transform->transform.rotation.y -= 360.0f;
        while(target_transform->transform.rotation.y < 0.0f) target_transform->transform.rotation.y += 360.0f;
        while(target_transform->transform.rotation.z > 360.0f) target_transform->transform.rotation.z -= 360.0f;
        while(target_transform->transform.rotation.z < 0.0f) target_transform->transform.rotation.z += 360.0f;

    }
    else if(target_display_plane)
    {
        Transform3D& transform = target_display_plane->GetTransform();
        transform.rotation.x += delta_x;
        transform.rotation.y += delta_y;
        transform.rotation.z += delta_z;

        while(transform.rotation.x > 360.0f) transform.rotation.x -= 360.0f;
        while(transform.rotation.x < 0.0f) transform.rotation.x += 360.0f;
        while(transform.rotation.y > 360.0f) transform.rotation.y -= 360.0f;
        while(transform.rotation.y < 0.0f) transform.rotation.y += 360.0f;
        while(transform.rotation.z > 360.0f) transform.rotation.z -= 360.0f;
        while(transform.rotation.z < 0.0f) transform.rotation.z += 360.0f;
    }
}

void Gizmo3D::ApplyFreeroamMovement(float delta_x, float delta_y, const float* modelview, const float* projection, const int* viewport)
{
    (void)projection;
    (void)viewport;

    float right_x = modelview[0];
    float right_y = modelview[4];
    float right_z = modelview[8];

    float up_x = modelview[1];
    float up_y = modelview[5];
    float up_z = modelview[9];

    float move_scale = 0.05f;

    if(target_ref_point)
    {
        Vector3D pos = target_ref_point->GetPosition();
        pos.x += (right_x * delta_x - up_x * delta_y) * move_scale;
        pos.y += (right_y * delta_x - up_y * delta_y) * move_scale;
        pos.z += (right_z * delta_x - up_z * delta_y) * move_scale;
        setTargetWorldPosition(pos.x, pos.y, pos.z);
    }
    else if(target_transform)
    {
        const float x = target_transform->transform.position.x + (right_x * delta_x - up_x * delta_y) * move_scale;
        const float y = target_transform->transform.position.y + (right_y * delta_x - up_y * delta_y) * move_scale;
        const float z = target_transform->transform.position.z + (right_z * delta_x - up_z * delta_y) * move_scale;
        setTargetWorldPosition(x, y, z);
    }
    else if(target_display_plane)
    {
        Transform3D& transform = target_display_plane->GetTransform();
        const float x = transform.position.x + (right_x * delta_x - up_x * delta_y) * move_scale;
        const float y = transform.position.y + (right_y * delta_x - up_y * delta_y) * move_scale;
        const float z = transform.position.z + (right_z * delta_x - up_z * delta_y) * move_scale;
        setTargetWorldPosition(x, y, z);
    }
}

void Gizmo3D::ApplyFreeroamDragRayPlane(int mouse_x, int mouse_y, const float* modelview, const float* projection, const int* viewport)
{
    (void)projection;
    (void)viewport;

    Ray3D ray = GenerateRay(mouse_x, mouse_y, modelview, projection, viewport);
    float n_dot_d = drag_plane_normal[0]*ray.direction[0] + drag_plane_normal[1]*ray.direction[1] + drag_plane_normal[2]*ray.direction[2];
    if(fabsf(n_dot_d) < 1e-6f)
    {
        float dx = (float)(mouse_x - last_gl_mouse_pos.x());
        float dy = (float)(mouse_y - last_gl_mouse_pos.y());
        ApplyFreeroamMovement(dx, dy, modelview, projection, viewport);
        return;
    }
    float w0x = drag_start_world[0] - ray.origin[0];
    float w0y = drag_start_world[1] - ray.origin[1];
    float w0z = drag_start_world[2] - ray.origin[2];
    float t = (drag_plane_normal[0]*w0x + drag_plane_normal[1]*w0y + drag_plane_normal[2]*w0z) / n_dot_d;
    if(t < 0.0f)
    {
        float dx = (float)(mouse_x - last_gl_mouse_pos.x());
        float dy = (float)(mouse_y - last_gl_mouse_pos.y());
        ApplyFreeroamMovement(dx, dy, modelview, projection, viewport);
        return;
    }
    float hitx = ray.origin[0] + t*ray.direction[0];
    float hity = ray.origin[1] + t*ray.direction[1];
    float hitz = ray.origin[2] + t*ray.direction[2];

    const float target_x = hitx - drag_grab_offset[0];
    const float target_y = hity - drag_grab_offset[1];
    const float target_z = hitz - drag_grab_offset[2];
    setTargetWorldPosition(target_x, target_y, target_z);
}

void Gizmo3D::DrawMoveGizmo()
{
    glDisable(GL_LIGHTING);

    GizmoAxis hl = dragging ? selected_axis : hover_axis;
    float* color = (hl == GIZMO_AXIS_X) ? color_highlight : color_x_axis;
    const float x_alpha = (hl == GIZMO_AXIS_X) ? 1.0f : 0.92f;
    glLineWidth((hl == GIZMO_AXIS_X) ? 4.5f : 3.5f);
    DrawGizmoLineSegment(0.0f, 0.0f, 0.0f, gizmo_size, 0.0f, 0.0f, color[0], color[1], color[2], x_alpha);

    glBegin(GL_TRIANGLES);
    glVertex3f(gizmo_size, 0.0f, 0.0f);
    glVertex3f(gizmo_size - 0.3f, 0.15f, 0.0f);
    glVertex3f(gizmo_size - 0.3f, -0.15f, 0.0f);

    glVertex3f(gizmo_size, 0.0f, 0.0f);
    glVertex3f(gizmo_size - 0.3f, 0.0f, 0.15f);
    glVertex3f(gizmo_size - 0.3f, 0.0f, -0.15f);
    glEnd();

    color = (hl == GIZMO_AXIS_Y) ? color_highlight : color_y_axis;
    const float y_alpha = (hl == GIZMO_AXIS_Y) ? 1.0f : 0.92f;
    glLineWidth((hl == GIZMO_AXIS_Y) ? 4.5f : 3.5f);
    DrawGizmoLineSegment(0.0f, 0.0f, 0.0f, 0.0f, gizmo_size, 0.0f, color[0], color[1], color[2], y_alpha);

    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, gizmo_size, 0.0f);
    glVertex3f(0.15f, gizmo_size - 0.3f, 0.0f);
    glVertex3f(-0.15f, gizmo_size - 0.3f, 0.0f);

    glVertex3f(0.0f, gizmo_size, 0.0f);
    glVertex3f(0.0f, gizmo_size - 0.3f, 0.15f);
    glVertex3f(0.0f, gizmo_size - 0.3f, -0.15f);
    glEnd();

    color = (hl == GIZMO_AXIS_Z) ? color_highlight : color_z_axis;
    const float z_alpha = (hl == GIZMO_AXIS_Z) ? 1.0f : 0.92f;
    glLineWidth((hl == GIZMO_AXIS_Z) ? 4.5f : 3.5f);
    DrawGizmoLineSegment(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_size, color[0], color[1], color[2], z_alpha);

    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, gizmo_size);
    glVertex3f(0.15f, 0.0f, gizmo_size - 0.3f);
    glVertex3f(-0.15f, 0.0f, gizmo_size - 0.3f);

    glVertex3f(0.0f, 0.0f, gizmo_size);
    glVertex3f(0.0f, 0.15f, gizmo_size - 0.3f);
    glVertex3f(0.0f, -0.15f, gizmo_size - 0.3f);
    glEnd();

    float orange[3] = {1.0f, 0.5f, 0.0f};
    color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : orange;
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

    glBegin(GL_QUADS);

    glVertex3f(-size, -size, -size);
    glVertex3f(+size, -size, -size);
    glVertex3f(+size, +size, -size);
    glVertex3f(-size, +size, -size);

    glVertex3f(-size, -size, +size);
    glVertex3f(+size, -size, +size);
    glVertex3f(+size, +size, +size);
    glVertex3f(-size, +size, +size);

    glVertex3f(-size, -size, -size);
    glVertex3f(-size, -size, +size);
    glVertex3f(-size, +size, +size);
    glVertex3f(-size, +size, -size);

    glVertex3f(+size, -size, -size);
    glVertex3f(+size, -size, +size);
    glVertex3f(+size, +size, +size);
    glVertex3f(+size, +size, -size);

    glVertex3f(-size, -size, -size);
    glVertex3f(+size, -size, -size);
    glVertex3f(+size, -size, +size);
    glVertex3f(-size, -size, +size);

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

    float handle_radius = 0.15f;

    GizmoAxis hl = dragging ? selected_axis : hover_axis;
    float* color = (hl == GIZMO_AXIS_X) ? color_highlight : color_x_axis;
    glLineWidth((hl == GIZMO_AXIS_X) ? 5.0f : 3.0f);
    DrawGizmoRingLoop(0, gizmo_size, color[0], color[1], color[2], (hl == GIZMO_AXIS_X) ? 1.0f : 0.88f);
    for(int i = 0; i < 4; i++)
    {
        float angle = (i / 4.0f) * 2.0f * M_PI;
        float handle_pos[3] = {0.0f, cosf(angle) * gizmo_size, sinf(angle) * gizmo_size};
        DrawSphere(handle_pos, handle_radius, color);
    }

    color = (hl == GIZMO_AXIS_Y) ? color_highlight : color_y_axis;
    glLineWidth((hl == GIZMO_AXIS_Y) ? 5.0f : 3.0f);
    DrawGizmoRingLoop(1, gizmo_size, color[0], color[1], color[2], (hl == GIZMO_AXIS_Y) ? 1.0f : 0.88f);
    for(int i = 0; i < 4; i++)
    {
        float angle = (i / 4.0f) * 2.0f * M_PI;
        float handle_pos[3] = {cosf(angle) * gizmo_size, 0.0f, sinf(angle) * gizmo_size};
        DrawSphere(handle_pos, handle_radius, color);
    }

    color = (hl == GIZMO_AXIS_Z) ? color_highlight : color_z_axis;
    glLineWidth((hl == GIZMO_AXIS_Z) ? 5.0f : 3.0f);
    DrawGizmoRingLoop(2, gizmo_size, color[0], color[1], color[2], (hl == GIZMO_AXIS_Z) ? 1.0f : 0.88f);
    for(int i = 0; i < 4; i++)
    {
        float angle = (i / 4.0f) * 2.0f * M_PI;
        float handle_pos[3] = {cosf(angle) * gizmo_size, sinf(angle) * gizmo_size, 0.0f};
        DrawSphere(handle_pos, handle_radius, color);
    }

    if(dragging && mode == GIZMO_MODE_ROTATE &&
       (selected_axis == GIZMO_AXIS_X || selected_axis == GIZMO_AXIS_Y || selected_axis == GIZMO_AXIS_Z))
    {
        float arc_radius = gizmo_size * 1.12f;
        float start = rot_drag_start_angle;
        float sweep = rot_drag_accum_degrees * ((float)M_PI / 180.0f);
        const int segments = 72;

        glLineWidth(4.5f);
        BeginGizmoSmoothLines();
        glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
        glBegin(GL_LINE_STRIP);
        for(int i = 0; i <= segments; i++)
        {
            float t = (float)i / (float)segments;
            float a = start + sweep * t;
            if(selected_axis == GIZMO_AXIS_X) glVertex3f(0.0f, cosf(a) * arc_radius, sinf(a) * arc_radius);
            if(selected_axis == GIZMO_AXIS_Y) glVertex3f(cosf(a) * arc_radius, 0.0f, sinf(a) * arc_radius);
            if(selected_axis == GIZMO_AXIS_Z) glVertex3f(cosf(a) * arc_radius, sinf(a) * arc_radius, 0.0f);
        }
        glEnd();
        EndGizmoSmoothLines();
        glLineWidth(3.0f);
    }

    float orange[3] = {1.0f, 0.5f, 0.0f};
    color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : orange;
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

    DrawArrowHead(end, end, color);
}

void Gizmo3D::DrawArrowHead(float pos[3], float dir[3], float color[3])
{
    (void)dir;

    glColor3f(color[0], color[1], color[2]);

    float arrow_size = 0.1f;

    glPushMatrix();
    glTranslatef(pos[0], pos[1], pos[2]);

    glBegin(GL_TRIANGLES);
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

    screen_x = (int)(world_x * 100.0f + viewport_width / 2);
    screen_y = (int)(world_y * 100.0f + viewport_height / 2);
}

void Gizmo3D::ScreenToWorld(int screen_x, int screen_y, float& world_x, float& world_y, float& world_z,
                           const float* modelview, const float* projection, const int* viewport)
{
    (void)modelview;
    (void)projection;
    (void)viewport;

    world_x = (screen_x - viewport_width / 2) / 100.0f;
    world_y = (screen_y - viewport_height / 2) / 100.0f;
    world_z = 0.0f;
}

void Gizmo3D::DrawFreeroamGizmo()
{
    glDisable(GL_LIGHTING);

    float purple[3] = {0.5f, 0.0f, 1.0f};
    GizmoAxis hl = dragging ? selected_axis : hover_axis;
    float* stick_color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : purple;
    glLineWidth((hl == GIZMO_AXIS_CENTER) ? 5.0f : 4.0f);
    DrawGizmoLineSegment(0.0f, 0.0f, 0.0f, 0.0f, gizmo_size, 0.0f,
                         stick_color[0], stick_color[1], stick_color[2],
                         (hl == GIZMO_AXIS_CENTER) ? 1.0f : 0.9f);

    float cube_size = 0.3f;
    float stick_height = gizmo_size;

    glBegin(GL_QUADS);

    glVertex3f(-cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, -cube_size);

    glVertex3f(-cube_size, stick_height - cube_size, +cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, +cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, +cube_size);

    glVertex3f(-cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height - cube_size, +cube_size);

    glVertex3f(+cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, +cube_size);

    glVertex3f(-cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height - cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height - cube_size, +cube_size);

    glVertex3f(-cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, -cube_size);
    glVertex3f(+cube_size, stick_height + cube_size, +cube_size);
    glVertex3f(-cube_size, stick_height + cube_size, +cube_size);

    glEnd();

    float orange[3] = {1.0f, 0.5f, 0.0f};
    float* center_color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : orange;
    float center[3] = { 0.0f, 0.0f, 0.0f };
    DrawCube(center, center_sphere_radius, center_color);

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}
