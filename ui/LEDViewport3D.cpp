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
#include <cmath>
#include <QtGlobal>

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

#define GIZMO_MODE_MOVE   0
#define GIZMO_MODE_ROTATE 1
#define GIZMO_MODE_SCALE  2

#ifdef _WIN32
// Manual GLU implementations for Windows to avoid header conflicts
void gluPerspective(double fovy, double aspect, double zNear, double zFar) {
    double fH = tan(fovy / 360 * M_PI) * zNear;
    double fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
}

void gluLookAt(double eyeX, double eyeY, double eyeZ,
               double centerX, double centerY, double centerZ,
               double upX, double upY, double upZ) {
    double forward[3], side[3], up[3];
    double m[4][4];

    forward[0] = centerX - eyeX;
    forward[1] = centerY - eyeY;
    forward[2] = centerZ - eyeZ;

    double len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
    if (len != 0.0) {
        forward[0] /= len;
        forward[1] /= len;
        forward[2] /= len;
    }

    side[0] = forward[1] * upZ - forward[2] * upY;
    side[1] = forward[2] * upX - forward[0] * upZ;
    side[2] = forward[0] * upY - forward[1] * upX;

    len = sqrt(side[0]*side[0] + side[1]*side[1] + side[2]*side[2]);
    if (len != 0.0) {
        side[0] /= len;
        side[1] /= len;
        side[2] /= len;
    }

    up[0] = side[1] * forward[2] - side[2] * forward[1];
    up[1] = side[2] * forward[0] - side[0] * forward[2];
    up[2] = side[0] * forward[1] - side[1] * forward[0];

    m[0][0] = side[0];
    m[1][0] = side[1];
    m[2][0] = side[2];
    m[3][0] = 0.0;

    m[0][1] = up[0];
    m[1][1] = up[1];
    m[2][1] = up[2];
    m[3][1] = 0.0;

    m[0][2] = -forward[0];
    m[1][2] = -forward[1];
    m[2][2] = -forward[2];
    m[3][2] = 0.0;

    m[0][3] = 0.0;
    m[1][3] = 0.0;
    m[2][3] = 0.0;
    m[3][3] = 1.0;

    glMultMatrixd(&m[0][0]);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}

int gluProject(double objX, double objY, double objZ,
               const double* model, const double* proj, const int* view,
               double* win_x, double* win_y, double* winZ) {
    double in[4];
    double out[4];

    in[0] = objX;
    in[1] = objY;
    in[2] = objZ;
    in[3] = 1.0;

    out[0] = model[0]*in[0] + model[4]*in[1] + model[8]*in[2] + model[12]*in[3];
    out[1] = model[1]*in[0] + model[5]*in[1] + model[9]*in[2] + model[13]*in[3];
    out[2] = model[2]*in[0] + model[6]*in[1] + model[10]*in[2] + model[14]*in[3];
    out[3] = model[3]*in[0] + model[7]*in[1] + model[11]*in[2] + model[15]*in[3];

    in[0] = proj[0]*out[0] + proj[4]*out[1] + proj[8]*out[2] + proj[12]*out[3];
    in[1] = proj[1]*out[0] + proj[5]*out[1] + proj[9]*out[2] + proj[13]*out[3];
    in[2] = proj[2]*out[0] + proj[6]*out[1] + proj[10]*out[2] + proj[14]*out[3];
    in[3] = proj[3]*out[0] + proj[7]*out[1] + proj[11]*out[2] + proj[15]*out[3];

    if (in[3] == 0.0) return 0;

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    in[0] = in[0] * 0.5 + 0.5;
    in[1] = in[1] * 0.5 + 0.5;
    in[2] = in[2] * 0.5 + 0.5;

    *win_x = in[0] * view[2] + view[0];
    *win_y = in[1] * view[3] + view[1];
    *winZ = in[2];

    return 1;
}
#endif

// Qt version compatibility helpers
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6+ uses position()
    #define MOUSE_X(event) ((int)(event)->position().x())
    #define MOUSE_Y(event) ((int)(event)->position().y())
#else
    // Qt 5 uses x() and y()
    #define MOUSE_X(event) (event)->x()
    #define MOUSE_Y(event) (event)->y()
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
    gizmo_mode = GIZMO_MODE_MOVE;

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
        glScalef(ctrl->transform.scale.x, ctrl->transform.scale.y, ctrl->transform.scale.z);

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

        RGBController* controller = ctrl->controller ? ctrl->controller : led.controller;

        if(controller == nullptr || led.zone_idx >= controller->zones.size())
        {
            continue;
        }

        unsigned int led_global_idx = controller->zones[led.zone_idx].start_idx + led.led_idx;

        if(led_global_idx >= controller->colors.size())
        {
            continue;
        }

        RGBColor color = controller->colors[led_global_idx];

        // OpenRGB uses BGR format: 0x00BBGGRR
        float r = (color & 0xFF) / 255.0f;          // Red is in bits 0-7
        float g = ((color >> 8) & 0xFF) / 255.0f;   // Green is in bits 8-15
        float b = ((color >> 16) & 0xFF) / 255.0f;  // Blue is in bits 16-23

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

    if(gizmo_mode == GIZMO_MODE_ROTATE)
    {
        float radius = 5.0f;

        glLineWidth(2.0f);

        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(0, cos(angle) * radius, sin(angle) * radius);
        }
        glEnd();

        glColor3f(0.0f, 1.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(cos(angle) * radius, 0, sin(angle) * radius);
        }
        glEnd();

        glColor3f(0.0f, 0.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(cos(angle) * radius, sin(angle) * radius, 0);
        }
        glEnd();

        float cube_size = 0.39f;

        glColor3f(1.0f, 0.0f, 0.0f);

        glPushMatrix();
        glTranslatef(0, radius*0.7f, radius*0.7f);
        glBegin(GL_QUADS);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0, -radius*0.7f, -radius*0.7f);
        glBegin(GL_QUADS);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        glColor3f(0.0f, 1.0f, 0.0f);

        glPushMatrix();
        glTranslatef(radius*0.7f, 0, radius*0.7f);
        glBegin(GL_QUADS);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-radius*0.7f, 0, -radius*0.7f);
        glBegin(GL_QUADS);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        glColor3f(0.0f, 0.0f, 1.0f);

        glPushMatrix();
        glTranslatef(radius*0.7f, radius*0.7f, 0);
        glBegin(GL_QUADS);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-radius*0.7f, -radius*0.7f, 0);
        glBegin(GL_QUADS);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        glColor3f(1.0f, 1.0f, 0.0f);
        float center_size = 0.6f;
        glBegin(GL_QUADS);

        glVertex3f(-center_size, -center_size, -center_size);
        glVertex3f(center_size, -center_size, -center_size);
        glVertex3f(center_size, center_size, -center_size);
        glVertex3f(-center_size, center_size, -center_size);

        glVertex3f(-center_size, -center_size, center_size);
        glVertex3f(center_size, -center_size, center_size);
        glVertex3f(center_size, center_size, center_size);
        glVertex3f(-center_size, center_size, center_size);

        glVertex3f(-center_size, -center_size, -center_size);
        glVertex3f(-center_size, -center_size, center_size);
        glVertex3f(-center_size, center_size, center_size);
        glVertex3f(-center_size, center_size, -center_size);

        glVertex3f(center_size, -center_size, -center_size);
        glVertex3f(center_size, -center_size, center_size);
        glVertex3f(center_size, center_size, center_size);
        glVertex3f(center_size, center_size, -center_size);

        glVertex3f(-center_size, -center_size, -center_size);
        glVertex3f(center_size, -center_size, -center_size);
        glVertex3f(center_size, -center_size, center_size);
        glVertex3f(-center_size, -center_size, center_size);

        glVertex3f(-center_size, center_size, -center_size);
        glVertex3f(center_size, center_size, -center_size);
        glVertex3f(center_size, center_size, center_size);
        glVertex3f(-center_size, center_size, center_size);

        glEnd();
    }
    else if(gizmo_mode == GIZMO_MODE_SCALE)
    {
        glLineWidth(5.0f);
        glBegin(GL_LINES);

        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(5, 0, 0);

        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 5, 0);

        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 0, 5);

        glEnd();

        float cube_size = 0.45f;
        glBegin(GL_QUADS);

        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(5-cube_size, -cube_size, -cube_size);
        glVertex3f(5+cube_size, -cube_size, -cube_size);
        glVertex3f(5+cube_size, +cube_size, -cube_size);
        glVertex3f(5-cube_size, +cube_size, -cube_size);

        glVertex3f(5-cube_size, -cube_size, +cube_size);
        glVertex3f(5+cube_size, -cube_size, +cube_size);
        glVertex3f(5+cube_size, +cube_size, +cube_size);
        glVertex3f(5-cube_size, +cube_size, +cube_size);

        glVertex3f(5-cube_size, -cube_size, -cube_size);
        glVertex3f(5-cube_size, +cube_size, -cube_size);
        glVertex3f(5-cube_size, +cube_size, +cube_size);
        glVertex3f(5-cube_size, -cube_size, +cube_size);

        glVertex3f(5+cube_size, -cube_size, -cube_size);
        glVertex3f(5+cube_size, +cube_size, -cube_size);
        glVertex3f(5+cube_size, +cube_size, +cube_size);
        glVertex3f(5+cube_size, -cube_size, +cube_size);

        glVertex3f(5-cube_size, -cube_size, -cube_size);
        glVertex3f(5+cube_size, -cube_size, -cube_size);
        glVertex3f(5+cube_size, -cube_size, +cube_size);
        glVertex3f(5-cube_size, -cube_size, +cube_size);

        glVertex3f(5-cube_size, +cube_size, -cube_size);
        glVertex3f(5+cube_size, +cube_size, -cube_size);
        glVertex3f(5+cube_size, +cube_size, +cube_size);
        glVertex3f(5-cube_size, +cube_size, +cube_size);

        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-cube_size, 5-cube_size, -cube_size);
        glVertex3f(+cube_size, 5-cube_size, -cube_size);
        glVertex3f(+cube_size, 5+cube_size, -cube_size);
        glVertex3f(-cube_size, 5+cube_size, -cube_size);

        glVertex3f(-cube_size, 5-cube_size, +cube_size);
        glVertex3f(+cube_size, 5-cube_size, +cube_size);
        glVertex3f(+cube_size, 5+cube_size, +cube_size);
        glVertex3f(-cube_size, 5+cube_size, +cube_size);

        glVertex3f(-cube_size, 5-cube_size, -cube_size);
        glVertex3f(-cube_size, 5+cube_size, -cube_size);
        glVertex3f(-cube_size, 5+cube_size, +cube_size);
        glVertex3f(-cube_size, 5-cube_size, +cube_size);

        glVertex3f(+cube_size, 5-cube_size, -cube_size);
        glVertex3f(+cube_size, 5+cube_size, -cube_size);
        glVertex3f(+cube_size, 5+cube_size, +cube_size);
        glVertex3f(+cube_size, 5-cube_size, +cube_size);

        glVertex3f(-cube_size, 5-cube_size, -cube_size);
        glVertex3f(+cube_size, 5-cube_size, -cube_size);
        glVertex3f(+cube_size, 5-cube_size, +cube_size);
        glVertex3f(-cube_size, 5-cube_size, +cube_size);

        glVertex3f(-cube_size, 5+cube_size, -cube_size);
        glVertex3f(+cube_size, 5+cube_size, -cube_size);
        glVertex3f(+cube_size, 5+cube_size, +cube_size);
        glVertex3f(-cube_size, 5+cube_size, +cube_size);

        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(-cube_size, -cube_size, 5-cube_size);
        glVertex3f(+cube_size, -cube_size, 5-cube_size);
        glVertex3f(+cube_size, +cube_size, 5-cube_size);
        glVertex3f(-cube_size, +cube_size, 5-cube_size);

        glVertex3f(-cube_size, -cube_size, 5+cube_size);
        glVertex3f(+cube_size, -cube_size, 5+cube_size);
        glVertex3f(+cube_size, +cube_size, 5+cube_size);
        glVertex3f(-cube_size, +cube_size, 5+cube_size);

        glVertex3f(-cube_size, -cube_size, 5-cube_size);
        glVertex3f(-cube_size, +cube_size, 5-cube_size);
        glVertex3f(-cube_size, +cube_size, 5+cube_size);
        glVertex3f(-cube_size, -cube_size, 5+cube_size);

        glVertex3f(+cube_size, -cube_size, 5-cube_size);
        glVertex3f(+cube_size, +cube_size, 5-cube_size);
        glVertex3f(+cube_size, +cube_size, 5+cube_size);
        glVertex3f(+cube_size, -cube_size, 5+cube_size);

        glVertex3f(-cube_size, -cube_size, 5-cube_size);
        glVertex3f(+cube_size, -cube_size, 5-cube_size);
        glVertex3f(+cube_size, -cube_size, 5+cube_size);
        glVertex3f(-cube_size, -cube_size, 5+cube_size);

        glVertex3f(-cube_size, +cube_size, 5-cube_size);
        glVertex3f(+cube_size, +cube_size, 5-cube_size);
        glVertex3f(+cube_size, +cube_size, 5+cube_size);
        glVertex3f(-cube_size, +cube_size, 5+cube_size);

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

    float cube_size = 0.4f;
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
            if(PickGizmoCenter(MOUSE_X(event), MOUSE_Y(event)))
            {
                gizmo_mode = (gizmo_mode + 1) % 3;
                update();
                return;
            }

            int axis = PickGizmoAxis3D(MOUSE_X(event), MOUSE_Y(event));
            if(axis >= 0)
            {
                dragging_gizmo = true;
                dragging_axis = axis;
                update();
                return;
            }
        }

        int picked = PickController(MOUSE_X(event), MOUSE_Y(event));
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

    GLfloat win_x = (float)mouse_x;
    GLfloat win_y = (float)(viewport[3] - mouse_y);

    int closest_idx = -1;
    float closest_dist = 1000000.0f;

    for(unsigned int i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i];

        GLdouble obj_x, obj_y, obj_z;
        gluProject(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z,
                   modelview, projection, viewport, &obj_x, &obj_y, &obj_z);

        float dx = win_x - obj_x;
        float dy = win_y - obj_y;
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

    GLfloat win_x = (float)mouse_x;
    GLfloat win_y = (float)(viewport[3] - mouse_y);

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];

    GLdouble obj_x, obj_y, obj_z;
    gluProject(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z,
               modelview, projection, viewport, &obj_x, &obj_y, &obj_z);

    float dx = win_x - obj_x;
    float dy = win_y - obj_y;
    float dist = sqrt(dx*dx + dy*dy);

    return dist < 20.0f;
}

LEDViewport3D::Ray3D LEDViewport3D::GenerateRay(int mouse_x, int mouse_y)
{
    makeCurrent();

    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    float win_x = (float)mouse_x;
    float win_y = (float)(viewport[3] - mouse_y);

    GLdouble near_x, near_y, near_z;
    GLdouble far_x, far_y, far_z;

    gluUnProject(win_x, win_y, 0.0, modelview, projection, viewport, &near_x, &near_y, &near_z);
    gluUnProject(win_x, win_y, 1.0, modelview, projection, viewport, &far_x, &far_y, &far_z);

    Ray3D ray;
    ray.origin[0] = (float)near_x;
    ray.origin[1] = (float)near_y;
    ray.origin[2] = (float)near_z;

    float dx = (float)(far_x - near_x);
    float dy = (float)(far_y - near_y);
    float dz = (float)(far_z - near_z);
    float length = sqrt(dx*dx + dy*dy + dz*dz);

    ray.direction[0] = dx / length;
    ray.direction[1] = dy / length;
    ray.direction[2] = dz / length;

    return ray;
}

bool LEDViewport3D::RayBoxIntersect(const Ray3D& ray, const Box3D& box, float& distance)
{
    float tmin = 0.0f;
    float tmax = 10000.0f; // Large number for "infinity"

    for (int i = 0; i < 3; i++) {
        if (fabs(ray.direction[i]) < 0.0001f) {
            if (ray.origin[i] < box.min[i] || ray.origin[i] > box.max[i]) {
                return false;
            }
        } else {
            float t1 = (box.min[i] - ray.origin[i]) / ray.direction[i];
            float t2 = (box.max[i] - ray.origin[i]) / ray.direction[i];

            if (t1 > t2) {
                float temp = t1;
                t1 = t2;
                t2 = temp;
            }

            tmin = fmax(tmin, t1);
            tmax = fmin(tmax, t2);

            if (tmin > tmax) {
                return false;
            }
        }
    }

    distance = tmin > 0.0f ? tmin : tmax;
    return distance > 0.0f;
}

int LEDViewport3D::PickGizmoAxis3D(int mouse_x, int mouse_y)
{
    if(selected_controller_idx < 0 || !controller_transforms) {
        return -1;
    }

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];
    Ray3D ray = GenerateRay(mouse_x, mouse_y);

    float closest_distance = 10000.0f;
    int closest_axis = -1;

    if(gizmo_mode == GIZMO_MODE_ROTATE)
    {
        float ring_radius = 5.0f;
        float cube_size = 0.39f;

        float handle_positions[3][2][3] = {
            {{0, ring_radius*0.7f, ring_radius*0.7f}, {0, -ring_radius*0.7f, -ring_radius*0.7f}},
            // Y-axis handles (green)
            {{ring_radius*0.7f, 0, ring_radius*0.7f}, {-ring_radius*0.7f, 0, -ring_radius*0.7f}},
            // Z-axis handles (blue)
            {{ring_radius*0.7f, ring_radius*0.7f, 0}, {-ring_radius*0.7f, -ring_radius*0.7f, 0}}
        };

        for(int axis = 0; axis < 3; axis++)
        {
            for(int handle = 0; handle < 2; handle++)
            {
                // Transform handle position through object rotation
                float local_x = handle_positions[axis][handle][0];
                float local_y = handle_positions[axis][handle][1];
                float local_z = handle_positions[axis][handle][2];

                // Apply rotations (same as in drawing code)
                float rx = ctrl->transform.rotation.x * M_PI / 180.0f;
                float ry = ctrl->transform.rotation.y * M_PI / 180.0f;
                float rz = ctrl->transform.rotation.z * M_PI / 180.0f;

                // Z rotation
                float temp_x = local_x * cos(rz) - local_y * sin(rz);
                float temp_y = local_x * sin(rz) + local_y * cos(rz);
                local_x = temp_x;
                local_y = temp_y;

                // Y rotation
                temp_x = local_x * cos(ry) + local_z * sin(ry);
                float temp_z = -local_x * sin(ry) + local_z * cos(ry);
                local_x = temp_x;
                local_z = temp_z;

                // X rotation
                temp_y = local_y * cos(rx) - local_z * sin(rx);
                temp_z = local_y * sin(rx) + local_z * cos(rx);
                local_y = temp_y;
                local_z = temp_z;

                // Create bounding box for this cube handle
                Box3D box;
                float world_x = ctrl->transform.position.x + local_x;
                float world_y = ctrl->transform.position.y + local_y;
                float world_z = ctrl->transform.position.z + local_z;

                box.min[0] = world_x - cube_size;
                box.min[1] = world_y - cube_size;
                box.min[2] = world_z - cube_size;
                box.max[0] = world_x + cube_size;
                box.max[1] = world_y + cube_size;
                box.max[2] = world_z + cube_size;

                float distance;
                if(RayBoxIntersect(ray, box, distance) && distance < closest_distance)
                {
                    closest_distance = distance;
                    closest_axis = axis;
                }
            }
        }

        // Test center cube for free rotation (axis = 3)
        Box3D center_box;
        float center_size = 0.6f;
        center_box.min[0] = ctrl->transform.position.x - center_size;
        center_box.min[1] = ctrl->transform.position.y - center_size;
        center_box.min[2] = ctrl->transform.position.z - center_size;
        center_box.max[0] = ctrl->transform.position.x + center_size;
        center_box.max[1] = ctrl->transform.position.y + center_size;
        center_box.max[2] = ctrl->transform.position.z + center_size;

        float distance;
        if(RayBoxIntersect(ray, center_box, distance) && distance < closest_distance)
        {
            closest_distance = distance;
            closest_axis = 3; // Free rotation
        }
    }
    else if(gizmo_mode == GIZMO_MODE_SCALE)
    {
        // Test scale cube handles - match drawing positions
        Vector3D axes[3] = {{5, 0, 0}, {0, 5, 0}, {0, 0, 5}};
        float cube_size = 0.45f;

        for(int i = 0; i < 3; i++)
        {
            // Transform scale handle position
            float local_x = axes[i].x;
            float local_y = axes[i].y;
            float local_z = axes[i].z;

            // Apply rotations
            float rx = ctrl->transform.rotation.x * M_PI / 180.0f;
            float ry = ctrl->transform.rotation.y * M_PI / 180.0f;
            float rz = ctrl->transform.rotation.z * M_PI / 180.0f;

            // Z rotation
            float temp_x = local_x * cos(rz) - local_y * sin(rz);
            float temp_y = local_x * sin(rz) + local_y * cos(rz);
            local_x = temp_x;
            local_y = temp_y;

            // Y rotation
            temp_x = local_x * cos(ry) + local_z * sin(ry);
            float temp_z = -local_x * sin(ry) + local_z * cos(ry);
            local_x = temp_x;
            local_z = temp_z;

            // X rotation
            temp_y = local_y * cos(rx) - local_z * sin(rx);
            temp_z = local_y * sin(rx) + local_z * cos(rx);
            local_y = temp_y;
            local_z = temp_z;

            // Create bounding box
            Box3D box;
            float world_x = ctrl->transform.position.x + local_x;
            float world_y = ctrl->transform.position.y + local_y;
            float world_z = ctrl->transform.position.z + local_z;

            box.min[0] = world_x - cube_size;
            box.min[1] = world_y - cube_size;
            box.min[2] = world_z - cube_size;
            box.max[0] = world_x + cube_size;
            box.max[1] = world_y + cube_size;
            box.max[2] = world_z + cube_size;

            float distance;
            if(RayBoxIntersect(ray, box, distance) && distance < closest_distance)
            {
                closest_distance = distance;
                closest_axis = i;
            }
        }
    }
    else // GIZMO_MODE_MOVE
    {
        // Test move arrow handles with simplified box intersection for now
        Vector3D axes[3] = {{7, 0, 0}, {0, 7, 0}, {0, 0, 7}};
        float handle_size = 1.0f; // Generous hit area for arrows

        for(int i = 0; i < 3; i++)
        {
            // Transform move handle position
            float local_x = axes[i].x;
            float local_y = axes[i].y;
            float local_z = axes[i].z;

            // Apply rotations (same as other gizmos)
            float rx = ctrl->transform.rotation.x * M_PI / 180.0f;
            float ry = ctrl->transform.rotation.y * M_PI / 180.0f;
            float rz = ctrl->transform.rotation.z * M_PI / 180.0f;

            // Z rotation
            float temp_x = local_x * cos(rz) - local_y * sin(rz);
            float temp_y = local_x * sin(rz) + local_y * cos(rz);
            local_x = temp_x;
            local_y = temp_y;

            // Y rotation
            temp_x = local_x * cos(ry) + local_z * sin(ry);
            float temp_z = -local_x * sin(ry) + local_z * cos(ry);
            local_x = temp_x;
            local_z = temp_z;

            // X rotation
            temp_y = local_y * cos(rx) - local_z * sin(rx);
            temp_z = local_y * sin(rx) + local_z * cos(rx);
            local_y = temp_y;
            local_z = temp_z;

            // Create bounding box for arrow handle
            Box3D box;
            float world_x = ctrl->transform.position.x + local_x;
            float world_y = ctrl->transform.position.y + local_y;
            float world_z = ctrl->transform.position.z + local_z;

            box.min[0] = world_x - handle_size;
            box.min[1] = world_y - handle_size;
            box.min[2] = world_z - handle_size;
            box.max[0] = world_x + handle_size;
            box.max[1] = world_y + handle_size;
            box.max[2] = world_z + handle_size;

            float distance;
            if(RayBoxIntersect(ray, box, distance) && distance < closest_distance)
            {
                closest_distance = distance;
                closest_axis = i;
            }
        }

        // Test orange center cube for free movement
        Box3D center_box;
        float center_size = 0.4f;
        center_box.min[0] = ctrl->transform.position.x - center_size;
        center_box.min[1] = ctrl->transform.position.y - center_size;
        center_box.min[2] = ctrl->transform.position.z - center_size;
        center_box.max[0] = ctrl->transform.position.x + center_size;
        center_box.max[1] = ctrl->transform.position.y + center_size;
        center_box.max[2] = ctrl->transform.position.z + center_size;

        float distance;
        if(RayBoxIntersect(ray, center_box, distance) && distance < closest_distance)
        {
            closest_distance = distance;
            closest_axis = -1; // Free movement (no specific axis)
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

    if(gizmo_mode == GIZMO_MODE_ROTATE)
    {
        // Get current OpenGL matrices to convert screen delta to world space
        makeCurrent();
        GLdouble modelview[16];
        GLdouble projection[16];
        GLint viewport[4];
        glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
        glGetDoublev(GL_PROJECTION_MATRIX, projection);
        glGetIntegerv(GL_VIEWPORT, viewport);

        float rot_scale = 1.0f;

        if(dragging_axis == 0) // X-axis rotation (red handles) - always rotate around world X
        {
            // For X-axis rotation, use vertical mouse movement (dy)
            ctrl->transform.rotation.x += dy * rot_scale;
        }
        else if(dragging_axis == 1) // Y-axis rotation (green handles) - always rotate around world Y
        {
            // For Y-axis rotation, use horizontal mouse movement (dx)
            ctrl->transform.rotation.y += dx * rot_scale;
        }
        else if(dragging_axis == 2) // Z-axis rotation (blue handles) - always rotate around world Z
        {
            // For Z-axis rotation, use combined movement for intuitive feel
            float combined_delta = (dx - dy) * 0.7f;
            ctrl->transform.rotation.z += combined_delta * rot_scale;
        }
        else if(dragging_axis == 3) // Free rotation (center yellow handle)
        {
            // Free rotation combines X and Y rotations based on mouse movement
            ctrl->transform.rotation.y += dx * rot_scale;
            ctrl->transform.rotation.x += dy * rot_scale;
        }

        // Keep rotations within reasonable bounds
        if(ctrl->transform.rotation.x > 360.0f) ctrl->transform.rotation.x -= 360.0f;
        if(ctrl->transform.rotation.x < -360.0f) ctrl->transform.rotation.x += 360.0f;
        if(ctrl->transform.rotation.y > 360.0f) ctrl->transform.rotation.y -= 360.0f;
        if(ctrl->transform.rotation.y < -360.0f) ctrl->transform.rotation.y += 360.0f;
        if(ctrl->transform.rotation.z > 360.0f) ctrl->transform.rotation.z -= 360.0f;
        if(ctrl->transform.rotation.z < -360.0f) ctrl->transform.rotation.z += 360.0f;
    }
    else if(gizmo_mode == GIZMO_MODE_SCALE)
    {
        float scale_speed = 0.01f;
        float delta_scale = (dx - dy) * scale_speed;

        if(dragging_axis == 0)
        {
            ctrl->transform.scale.x += delta_scale;
            if(ctrl->transform.scale.x < 0.1f) ctrl->transform.scale.x = 0.1f;
            if(ctrl->transform.scale.x > 10.0f) ctrl->transform.scale.x = 10.0f;
        }
        else if(dragging_axis == 1)
        {
            ctrl->transform.scale.y += delta_scale;
            if(ctrl->transform.scale.y < 0.1f) ctrl->transform.scale.y = 0.1f;
            if(ctrl->transform.scale.y > 10.0f) ctrl->transform.scale.y = 10.0f;
        }
        else if(dragging_axis == 2)
        {
            ctrl->transform.scale.z += delta_scale;
            if(ctrl->transform.scale.z < 0.1f) ctrl->transform.scale.z = 0.1f;
            if(ctrl->transform.scale.z > 10.0f) ctrl->transform.scale.z = 10.0f;
        }

        emit ControllerScaleChanged(selected_controller_idx,
                                    ctrl->transform.scale.x,
                                    ctrl->transform.scale.y,
                                    ctrl->transform.scale.z);
    }
    else // GIZMO_MODE_MOVE
    {
        float move_scale = 0.1f;

        if(dragging_axis == 0) // X-axis (blue) - always move along world X axis
        {
            ctrl->transform.position.x += dx * move_scale;
        }
        else if(dragging_axis == 1) // Y-axis (green) - always move along world Y axis
        {
            ctrl->transform.position.y -= dy * move_scale;
        }
        else if(dragging_axis == 2) // Z-axis (red) - always move along world Z axis
        {
            ctrl->transform.position.z -= dy * move_scale;
        }
        else // Free movement in camera plane
        {
            // Calculate camera-relative movement vectors for free movement only
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