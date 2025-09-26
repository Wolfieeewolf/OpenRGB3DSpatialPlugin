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

    // Normalize forward
    double len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
    if (len != 0.0) {
        forward[0] /= len;
        forward[1] /= len;
        forward[2] /= len;
    }

    // Calculate side = forward x up
    side[0] = forward[1] * upZ - forward[2] * upY;
    side[1] = forward[2] * upX - forward[0] * upZ;
    side[2] = forward[0] * upY - forward[1] * upX;

    // Normalize side
    len = sqrt(side[0]*side[0] + side[1]*side[1] + side[2]*side[2]);
    if (len != 0.0) {
        side[0] /= len;
        side[1] /= len;
        side[2] /= len;
    }

    // Calculate up = side x forward
    up[0] = side[1] * forward[2] - side[2] * forward[1];
    up[1] = side[2] * forward[0] - side[0] * forward[2];
    up[2] = side[0] * forward[1] - side[1] * forward[0];

    // Build matrix
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
               double* winX, double* winY, double* winZ) {
    double in[4];
    double out[4];

    in[0] = objX;
    in[1] = objY;
    in[2] = objZ;
    in[3] = 1.0;

    // Transform by modelview matrix
    out[0] = model[0]*in[0] + model[4]*in[1] + model[8]*in[2] + model[12]*in[3];
    out[1] = model[1]*in[0] + model[5]*in[1] + model[9]*in[2] + model[13]*in[3];
    out[2] = model[2]*in[0] + model[6]*in[1] + model[10]*in[2] + model[14]*in[3];
    out[3] = model[3]*in[0] + model[7]*in[1] + model[11]*in[2] + model[15]*in[3];

    // Transform by projection matrix
    in[0] = proj[0]*out[0] + proj[4]*out[1] + proj[8]*out[2] + proj[12]*out[3];
    in[1] = proj[1]*out[0] + proj[5]*out[1] + proj[9]*out[2] + proj[13]*out[3];
    in[2] = proj[2]*out[0] + proj[6]*out[1] + proj[10]*out[2] + proj[14]*out[3];
    in[3] = proj[3]*out[0] + proj[7]*out[1] + proj[11]*out[2] + proj[15]*out[3];

    if (in[3] == 0.0) return 0;

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    // Map to range 0-1
    in[0] = in[0] * 0.5 + 0.5;
    in[1] = in[1] * 0.5 + 0.5;
    in[2] = in[2] * 0.5 + 0.5;

    // Map to viewport
    *winX = in[0] * view[2] + view[0];
    *winY = in[1] * view[3] + view[1];
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

        // Draw thin rings as visual guides
        glLineWidth(2.0f);

        // X-axis rotation ring (red) - YZ plane
        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(0, cos(angle) * radius, sin(angle) * radius);
        }
        glEnd();

        // Y-axis rotation ring (green) - XZ plane
        glColor3f(0.0f, 1.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(cos(angle) * radius, 0, sin(angle) * radius);
        }
        glEnd();

        // Z-axis rotation ring (blue) - XY plane
        glColor3f(0.0f, 0.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i <= 32; i++)
        {
            float angle = (i / 32.0f) * 2.0f * M_PI;
            glVertex3f(cos(angle) * radius, sin(angle) * radius, 0);
        }
        glEnd();

        // Draw cube-shaped grab handles away from ring intersections
        float cube_size = 0.39f; // 20% smaller than previous (0.49 * 0.8 = 0.39)

        // X-axis handles (red) - positioned away from Y/Z ring intersections
        glColor3f(1.0f, 0.0f, 0.0f);

        // Handle 1: at (0, radius*0.7, radius*0.7)
        glPushMatrix();
        glTranslatef(0, radius*0.7f, radius*0.7f);
        // Draw cube
        glBegin(GL_QUADS);
        // Front face
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        // Back face
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        // Left face
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(-cube_size, cube_size, -cube_size);
        // Right face
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        // Top face
        glVertex3f(-cube_size, cube_size, -cube_size);
        glVertex3f(-cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, cube_size);
        glVertex3f(cube_size, cube_size, -cube_size);
        // Bottom face
        glVertex3f(-cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, -cube_size);
        glVertex3f(cube_size, -cube_size, cube_size);
        glVertex3f(-cube_size, -cube_size, cube_size);
        glEnd();
        glPopMatrix();

        // Handle 2: at (0, -radius*0.7, -radius*0.7)
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

        // Y-axis handles (green) - positioned away from X/Z ring intersections
        glColor3f(0.0f, 1.0f, 0.0f);

        // Handle 1: at (radius*0.7, 0, radius*0.7)
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

        // Handle 2: at (-radius*0.7, 0, -radius*0.7)
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

        // Z-axis handles (blue) - positioned away from X/Y ring intersections
        glColor3f(0.0f, 0.0f, 1.0f);

        // Handle 1: at (radius*0.7, radius*0.7, 0)
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

        // Handle 2: at (-radius*0.7, -radius*0.7, 0)
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

        // Center sphere for free rotation in all directions
        glColor3f(1.0f, 1.0f, 0.0f);
        float center_size = 0.6f;
        glBegin(GL_QUADS);

        // Draw a simple cube as center handle
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

        // X-axis line (blue)
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(5, 0, 0);

        // Y-axis line (green)
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 5, 0);

        // Z-axis line (red)
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 0, 5);

        glEnd();

        // Reasonably sized cube handles for grabbing
        float cube_size = 0.45f; // 10% smaller (0.5 * 0.9 = 0.45)
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

            int axis = PickGizmoAxis(MOUSE_X(event), MOUSE_Y(event));
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

    if(gizmo_mode == GIZMO_MODE_ROTATE)
    {
        float ring_radius = 5.0f;

        // Define handle positions for each axis (2 handles per axis) - away from intersections
        float handle_positions[3][2][3] = {
            // X-axis handles (red) - positioned away from Y/Z ring intersections
            {{0, ring_radius*0.7f, ring_radius*0.7f}, {0, -ring_radius*0.7f, -ring_radius*0.7f}},
            // Y-axis handles (green) - positioned away from X/Z ring intersections
            {{ring_radius*0.7f, 0, ring_radius*0.7f}, {-ring_radius*0.7f, 0, -ring_radius*0.7f}},
            // Z-axis handles (blue) - positioned away from X/Y ring intersections
            {{ring_radius*0.7f, ring_radius*0.7f, 0}, {-ring_radius*0.7f, -ring_radius*0.7f, 0}}
        };

        // Check each handle - need to transform through object's rotation matrix
        for(int axis = 0; axis < 3; axis++)
        {
            for(int handle = 0; handle < 2; handle++)
            {
                // Apply the same transformations as in DrawGizmo
                float local_x = handle_positions[axis][handle][0];
                float local_y = handle_positions[axis][handle][1];
                float local_z = handle_positions[axis][handle][2];

                // Apply object rotations (same order as in DrawGizmo)
                float rx = ctrl->transform.rotation.x * M_PI / 180.0f;
                float ry = ctrl->transform.rotation.y * M_PI / 180.0f;
                float rz = ctrl->transform.rotation.z * M_PI / 180.0f;

                // Apply Z rotation
                float temp_x = local_x * cos(rz) - local_y * sin(rz);
                float temp_y = local_x * sin(rz) + local_y * cos(rz);
                local_x = temp_x;
                local_y = temp_y;

                // Apply Y rotation
                temp_x = local_x * cos(ry) + local_z * sin(ry);
                float temp_z = -local_x * sin(ry) + local_z * cos(ry);
                local_x = temp_x;
                local_z = temp_z;

                // Apply X rotation
                temp_y = local_y * cos(rx) - local_z * sin(rx);
                temp_z = local_y * sin(rx) + local_z * cos(rx);
                local_y = temp_y;
                local_z = temp_z;

                // Transform to world position
                float world_x = ctrl->transform.position.x + local_x;
                float world_y = ctrl->transform.position.y + local_y;
                float world_z = ctrl->transform.position.z + local_z;

                GLdouble handleX, handleY, handleZ;
                gluProject(world_x, world_y, world_z, mv, proj, vp, &handleX, &handleY, &handleZ);

                float dx = winX - handleX;
                float dy = winY - handleY;
                float dist = sqrt(dx*dx + dy*dy);

                if(dist < 35.0f && dist < closest_dist) // Slightly larger hit area for easier clicking
                {
                    closest_dist = dist;
                    closest_axis = axis;
                }
            }
        }

        // Check center handle for free rotation (axis = 3)
        GLdouble centerX_proj, centerY_proj, centerZ_proj;
        gluProject(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z,
                   mv, proj, vp, &centerX_proj, &centerY_proj, &centerZ_proj);

        float center_dx = winX - centerX_proj;
        float center_dy = winY - centerY_proj;
        float center_dist = sqrt(center_dx*center_dx + center_dy*center_dy);

        if(center_dist < 25.0f && center_dist < closest_dist)
        {
            closest_dist = center_dist;
            closest_axis = 3; // Special value for free rotation
        }
    }
    else if(gizmo_mode == GIZMO_MODE_SCALE)
    {
        for(int i = 0; i < 3; i++)
        {
            // Apply the same transformations as in DrawGizmo for scale handles
            float local_x = axes[i].x;
            float local_y = axes[i].y;
            float local_z = axes[i].z;

            // Apply object rotations (same order as in DrawGizmo)
            float rx = ctrl->transform.rotation.x * M_PI / 180.0f;
            float ry = ctrl->transform.rotation.y * M_PI / 180.0f;
            float rz = ctrl->transform.rotation.z * M_PI / 180.0f;

            // Apply Z rotation
            float temp_x = local_x * cos(rz) - local_y * sin(rz);
            float temp_y = local_x * sin(rz) + local_y * cos(rz);
            local_x = temp_x;
            local_y = temp_y;

            // Apply Y rotation
            temp_x = local_x * cos(ry) + local_z * sin(ry);
            float temp_z = -local_x * sin(ry) + local_z * cos(ry);
            local_x = temp_x;
            local_z = temp_z;

            // Apply X rotation
            temp_y = local_y * cos(rx) - local_z * sin(rx);
            temp_z = local_y * sin(rx) + local_z * cos(rx);
            local_y = temp_y;
            local_z = temp_z;

            // Transform to world position
            float world_x = ctrl->transform.position.x + local_x;
            float world_y = ctrl->transform.position.y + local_y;
            float world_z = ctrl->transform.position.z + local_z;

            GLdouble cubeX, cubeY, cubeZ;
            gluProject(world_x, world_y, world_z, mv, proj, vp, &cubeX, &cubeY, &cubeZ);

            float dx = winX - cubeX;
            float dy = winY - cubeY;
            float dist = sqrt(dx*dx + dy*dy);

            // Hit detection for scale cube handles
            if(dist < 40.0f && dist < closest_dist) // Adjusted for smaller handles
            {
                closest_dist = dist;
                closest_axis = i;
            }
        }
    }
    else
    {
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