/*---------------------------------------------------------*\
| LEDViewport3D.cpp                                         |
|                                                           |
|   OpenGL 3D viewport for LED visualization and control   |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "LEDViewport3D.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LEDViewport3D::LEDViewport3D(QWidget *parent) : QOpenGLWidget(parent)
{
    controller_transforms = nullptr;
    selected_controller_idx = -1;

    camera_distance = 50.0f;
    camera_yaw = 45.0f;
    camera_pitch = 30.0f;
    camera_target_x = 0.0f;
    camera_target_y = 0.0f;
    camera_target_z = 0.0f;

    dragging_rotate = false;
    dragging_pan = false;
    dragging_gizmo = false;
    dragging_axis = -1;
    gizmo_rotate_mode = false;

    setMinimumSize(800, 600);
}

LEDViewport3D::~LEDViewport3D()
{
}

void LEDViewport3D::SetControllerTransforms(std::vector<ControllerTransform*>* transforms)
{
    controller_transforms = transforms;
    selected_controller_idx = -1;
    update();
}

void LEDViewport3D::SelectController(int index)
{
    selected_controller_idx = index;
    update();
}

void LEDViewport3D::UpdateColors()
{
    update();
}

void LEDViewport3D::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
}

void LEDViewport3D::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 500.0);

    glMatrixMode(GL_MODELVIEW);
}

void LEDViewport3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float yaw_rad = camera_yaw * M_PI / 180.0f;
    float pitch_rad = camera_pitch * M_PI / 180.0f;

    float cam_x = camera_target_x + camera_distance * cos(pitch_rad) * sin(yaw_rad);
    float cam_y = camera_target_y + camera_distance * sin(pitch_rad);
    float cam_z = camera_target_z + camera_distance * cos(pitch_rad) * cos(yaw_rad);

    gluLookAt(cam_x, cam_y, cam_z,
              camera_target_x, camera_target_y, camera_target_z,
              0.0, 1.0, 0.0);

    DrawGrid();
    DrawAxes();
    DrawControllers();

    if(selected_controller_idx >= 0)
    {
        DrawGizmo();
    }
}

void LEDViewport3D::DrawGrid()
{
    glColor3f(0.2f, 0.2f, 0.25f);
    glBegin(GL_LINES);

    for(int i = -50; i <= 50; i += 5)
    {
        glVertex3f(i, 0, -50);
        glVertex3f(i, 0, 50);

        glVertex3f(-50, 0, i);
        glVertex3f(50, 0, i);
    }

    glEnd();
}

void LEDViewport3D::DrawAxes()
{
    glLineWidth(3.0f);
    glBegin(GL_LINES);

    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0, 0, 0);
    glVertex3f(10, 0, 0);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 10, 0);

    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 10);

    glEnd();
    glLineWidth(1.0f);
}

void LEDViewport3D::DrawControllers()
{
    if(!controller_transforms)
    {
        return;
    }

    for(unsigned int i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i];

        glPushMatrix();
        glTranslatef(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z);
        glRotatef(ctrl->transform.rotation.z, 0.0f, 0.0f, 1.0f);
        glRotatef(ctrl->transform.rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.x, 1.0f, 0.0f, 0.0f);

        DrawLEDs(ctrl);


        if((int)i == selected_controller_idx)
        {
            glLineWidth(2.0f);
            glColor3f(1.0f, 1.0f, 0.0f);

            float size = 5.0f;
            glBegin(GL_LINE_LOOP);
            glVertex3f(-size, -size, -size);
            glVertex3f(size, -size, -size);
            glVertex3f(size, size, -size);
            glVertex3f(-size, size, -size);
            glEnd();

            glBegin(GL_LINE_LOOP);
            glVertex3f(-size, -size, size);
            glVertex3f(size, -size, size);
            glVertex3f(size, size, size);
            glVertex3f(-size, size, size);
            glEnd();

            glBegin(GL_LINES);
            glVertex3f(-size, -size, -size); glVertex3f(-size, -size, size);
            glVertex3f(size, -size, -size); glVertex3f(size, -size, size);
            glVertex3f(size, size, -size); glVertex3f(size, size, size);
            glVertex3f(-size, size, -size); glVertex3f(-size, size, size);
            glEnd();

            glLineWidth(1.0f);
        }

        glPopMatrix();
    }
}

void LEDViewport3D::DrawLEDs(ControllerTransform* ctrl)
{
    glPointSize(8.0f);
    glBegin(GL_POINTS);

    for(unsigned int i = 0; i < ctrl->led_positions.size(); i++)
    {
        LEDPosition3D& led = ctrl->led_positions[i];

        unsigned int led_global_idx = ctrl->controller->zones[led.zone_idx].start_idx + led.led_idx;
        RGBColor color = ctrl->controller->colors[led_global_idx];

        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;

        if(r < 0.1f && g < 0.1f && b < 0.1f)
        {
            r = 0.5f;
            g = 0.5f;
            b = 0.5f;
        }

        glColor3f(r, g, b);
        glVertex3f(led.local_position.x, led.local_position.y, led.local_position.z);
    }

    glEnd();
}

void LEDViewport3D::DrawGizmo()
{
    if(selected_controller_idx < 0 || !controller_transforms)
    {
        return;
    }

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];

    glPushMatrix();
    glTranslatef(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z);
    glRotatef(ctrl->transform.rotation.z, 0.0f, 0.0f, 1.0f);
    glRotatef(ctrl->transform.rotation.y, 0.0f, 1.0f, 0.0f);
    glRotatef(ctrl->transform.rotation.x, 1.0f, 0.0f, 0.0f);

    if(gizmo_rotate_mode)
    {
        glLineWidth(3.0f);
        glBegin(GL_LINE_LOOP);
        glColor3f(0.0f, 0.0f, 1.0f);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(cos(angle) * 5.0f, sin(angle) * 5.0f, 0);
        }
        glEnd();

        glBegin(GL_LINE_LOOP);
        glColor3f(0.0f, 1.0f, 0.0f);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(cos(angle) * 5.0f, 0, sin(angle) * 5.0f);
        }
        glEnd();

        glBegin(GL_LINE_LOOP);
        glColor3f(1.0f, 0.0f, 0.0f);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(0, cos(angle) * 5.0f, sin(angle) * 5.0f);
        }
        glEnd();
    }
    else
    {
        glLineWidth(4.0f);
        glBegin(GL_LINES);

        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(7, 0, 0);

        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 7, 0);

        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 0, 7);

        glEnd();

        glBegin(GL_TRIANGLES);

        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(7, 0, 0);
        glVertex3f(6, 0.3f, 0);
        glVertex3f(6, -0.3f, 0);

        glVertex3f(7, 0, 0);
        glVertex3f(6, 0, 0.3f);
        glVertex3f(6, 0, -0.3f);

        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0, 7, 0);
        glVertex3f(0.3f, 6, 0);
        glVertex3f(-0.3f, 6, 0);

        glVertex3f(0, 7, 0);
        glVertex3f(0, 6, 0.3f);
        glVertex3f(0, 6, -0.3f);

        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0, 0, 7);
        glVertex3f(0.3f, 0, 6);
        glVertex3f(-0.3f, 0, 6);

        glVertex3f(0, 0, 7);
        glVertex3f(0, 0.3f, 6);
        glVertex3f(0, -0.3f, 6);

        glEnd();
    }

    float cube_size = 0.8f;
    glBegin(GL_QUADS);
    glColor3f(1.0f, 0.5f, 0.0f);

    glVertex3f(-cube_size, -cube_size, -cube_size);
    glVertex3f(cube_size, -cube_size, -cube_size);
    glVertex3f(cube_size, cube_size, -cube_size);
    glVertex3f(-cube_size, cube_size, -cube_size);

    glVertex3f(-cube_size, -cube_size, cube_size);
    glVertex3f(cube_size, -cube_size, cube_size);
    glVertex3f(cube_size, cube_size, cube_size);
    glVertex3f(-cube_size, cube_size, cube_size);

    glVertex3f(-cube_size, -cube_size, -cube_size);
    glVertex3f(-cube_size, -cube_size, cube_size);
    glVertex3f(-cube_size, cube_size, cube_size);
    glVertex3f(-cube_size, cube_size, -cube_size);

    glVertex3f(cube_size, -cube_size, -cube_size);
    glVertex3f(cube_size, -cube_size, cube_size);
    glVertex3f(cube_size, cube_size, cube_size);
    glVertex3f(cube_size, cube_size, -cube_size);

    glVertex3f(-cube_size, -cube_size, -cube_size);
    glVertex3f(cube_size, -cube_size, -cube_size);
    glVertex3f(cube_size, -cube_size, cube_size);
    glVertex3f(-cube_size, -cube_size, cube_size);

    glVertex3f(-cube_size, cube_size, -cube_size);
    glVertex3f(cube_size, cube_size, -cube_size);
    glVertex3f(cube_size, cube_size, cube_size);
    glVertex3f(-cube_size, cube_size, cube_size);

    glEnd();

    glLineWidth(1.0f);
    glPopMatrix();
}

void LEDViewport3D::mousePressEvent(QMouseEvent *event)
{
    last_mouse_pos = event->pos();

    if(event->button() == Qt::LeftButton)
    {
        if(selected_controller_idx >= 0)
        {
            if(PickGizmoCenter(event->x(), event->y()))
            {
                gizmo_rotate_mode = !gizmo_rotate_mode;
                update();
                return;
            }

            int axis = PickGizmoAxis(event->x(), event->y());
            if(axis >= 0)
            {
                dragging_gizmo = true;
                dragging_axis = axis;
                update();
                return;
            }
        }

        int picked = PickController(event->x(), event->y());
        if(picked >= 0)
        {
            selected_controller_idx = picked;
            emit ControllerSelected(picked);
            dragging_gizmo = true;
            dragging_axis = -1;
            update();
        }
        else
        {
            selected_controller_idx = -1;
            emit ControllerSelected(-1);
            update();
        }
    }
    else if(event->button() == Qt::MiddleButton)
    {
        if(event->modifiers() & Qt::ShiftModifier)
        {
            dragging_pan = true;
        }
        else
        {
            dragging_rotate = true;
        }
    }
    else if(event->button() == Qt::RightButton)
    {
        dragging_pan = true;
    }
}

void LEDViewport3D::mouseMoveEvent(QMouseEvent *event)
{
    QPoint delta = event->pos() - last_mouse_pos;

    if(dragging_gizmo && selected_controller_idx >= 0)
    {
        UpdateGizmo(delta.x(), delta.y());
        update();
    }
    else if(dragging_rotate)
    {
        camera_yaw += delta.x() * 0.5f;
        camera_pitch -= delta.y() * 0.5f;

        if(camera_pitch > 89.0f) camera_pitch = 89.0f;
        if(camera_pitch < -89.0f) camera_pitch = -89.0f;

        update();
    }
    else if(dragging_pan)
    {
        float yaw_rad = camera_yaw * M_PI / 180.0f;
        float pitch_rad = camera_pitch * M_PI / 180.0f;

        float right_x = cos(yaw_rad);
        float right_z = -sin(yaw_rad);

        float up_x = -sin(yaw_rad) * sin(pitch_rad);
        float up_y = cos(pitch_rad);
        float up_z = -cos(yaw_rad) * sin(pitch_rad);

        float pan_scale = 0.05f;

        camera_target_x += (right_x * -delta.x() + up_x * delta.y()) * pan_scale;
        camera_target_y += up_y * delta.y() * pan_scale;
        camera_target_z += (right_z * -delta.x() + up_z * delta.y()) * pan_scale;

        update();
    }

    last_mouse_pos = event->pos();
}

void LEDViewport3D::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    dragging_rotate = false;
    dragging_pan = false;
    dragging_gizmo = false;
}

void LEDViewport3D::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    camera_distance -= delta * 3.0f;

    if(camera_distance < 10.0f) camera_distance = 10.0f;
    if(camera_distance > 200.0f) camera_distance = 200.0f;

    update();
}

int LEDViewport3D::PickController(int mouse_x, int mouse_y)
{
    if(!controller_transforms || controller_transforms->empty())
    {
        return -1;
    }

    makeCurrent();

    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    GLfloat winX = (float)mouse_x;
    GLfloat winY = (float)(viewport[3] - mouse_y);

    int closest_idx = -1;
    float closest_dist = 1000000.0f;

    for(unsigned int i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i];

        GLdouble objX, objY, objZ;
        gluProject(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z,
                   modelview, projection, viewport, &objX, &objY, &objZ);

        float dx = winX - objX;
        float dy = winY - objY;
        float dist = sqrt(dx*dx + dy*dy);

        if(dist < 50.0f && dist < closest_dist)
        {
            closest_dist = dist;
            closest_idx = i;
        }
    }

    return closest_idx;
}

bool LEDViewport3D::PickGizmoCenter(int mouse_x, int mouse_y)
{
    if(selected_controller_idx < 0 || !controller_transforms)
    {
        return false;
    }

    makeCurrent();

    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    GLfloat winX = (float)mouse_x;
    GLfloat winY = (float)(viewport[3] - mouse_y);

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];

    GLdouble objX, objY, objZ;
    gluProject(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z,
               modelview, projection, viewport, &objX, &objY, &objZ);

    float dx = winX - objX;
    float dy = winY - objY;
    float dist = sqrt(dx*dx + dy*dy);

    return dist < 20.0f;
}

int LEDViewport3D::PickGizmoAxis(int mouse_x, int mouse_y)
{
    if(selected_controller_idx < 0 || !controller_transforms)
    {
        return -1;
    }

    makeCurrent();

    GLint vp[4];
    GLdouble mv[16];
    GLdouble proj[16];
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);

    GLfloat winX = (float)mouse_x;
    GLfloat winY = (float)(vp[3] - mouse_y);

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];

    GLdouble centerX, centerY, centerZ;
    gluProject(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z,
               mv, proj, vp, &centerX, &centerY, &centerZ);

    float axis_length = 7.0f;
    Vector3D axes[3] = {{axis_length, 0, 0}, {0, axis_length, 0}, {0, 0, axis_length}};

    float closest_dist = 1000000.0f;
    int closest_axis = -1;

    for(int i = 0; i < 3; i++)
    {
        GLdouble axisX, axisY, axisZ;
        gluProject(ctrl->transform.position.x + axes[i].x,
                   ctrl->transform.position.y + axes[i].y,
                   ctrl->transform.position.z + axes[i].z,
                   mv, proj, vp, &axisX, &axisY, &axisZ);

        float line_dx = axisX - centerX;
        float line_dy = axisY - centerY;
        float line_len = sqrt(line_dx*line_dx + line_dy*line_dy);

        if(line_len < 0.1f) continue;

        float u = ((winX - centerX) * line_dx + (winY - centerY) * line_dy) / (line_len * line_len);
        u = fmax(0.0f, fmin(1.0f, u));

        float proj_x = centerX + u * line_dx;
        float proj_y = centerY + u * line_dy;

        float dist = sqrt((winX - proj_x) * (winX - proj_x) + (winY - proj_y) * (winY - proj_y));

        if(dist < 15.0f && dist < closest_dist)
        {
            closest_dist = dist;
            closest_axis = i;
        }
    }

    return closest_axis;
}

void LEDViewport3D::UpdateGizmo(int dx, int dy)
{
    if(!controller_transforms || selected_controller_idx < 0)
    {
        return;
    }

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];

    if(gizmo_rotate_mode)
    {
        float rot_scale = 0.5f;

        if(dragging_axis == 0)
        {
            ctrl->transform.rotation.x += dx * rot_scale;
        }
        else if(dragging_axis == 1)
        {
            ctrl->transform.rotation.y += dx * rot_scale;
        }
        else if(dragging_axis == 2)
        {
            ctrl->transform.rotation.z += dx * rot_scale;
        }
        else
        {
            ctrl->transform.rotation.y += dx * rot_scale;
            ctrl->transform.rotation.x -= dy * rot_scale;
        }
    }
    else
    {
        float move_scale = 0.1f;

        if(dragging_axis == 0)
        {
            ctrl->transform.position.x += dx * move_scale;
        }
        else if(dragging_axis == 1)
        {
            ctrl->transform.position.y -= dy * move_scale;
        }
        else if(dragging_axis == 2)
        {
            ctrl->transform.position.z -= dy * move_scale;
        }
        else
        {
            float yaw_rad = camera_yaw * M_PI / 180.0f;
            float pitch_rad = camera_pitch * M_PI / 180.0f;

            float right_x = cos(yaw_rad);
            float right_z = -sin(yaw_rad);

            float up_x = -sin(yaw_rad) * sin(pitch_rad);
            float up_y = cos(pitch_rad);
            float up_z = -cos(yaw_rad) * sin(pitch_rad);

            ctrl->transform.position.x += (right_x * dx + up_x * -dy) * move_scale;
            ctrl->transform.position.y += up_y * -dy * move_scale;
            ctrl->transform.position.z += (right_z * dx + up_z * -dy) * move_scale;
        }
    }

    emit ControllerPositionChanged(selected_controller_idx,
                                   ctrl->transform.position.x,
                                   ctrl->transform.position.y,
                                   ctrl->transform.position.z);
}