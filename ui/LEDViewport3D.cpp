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

#define GIZMO_MODE_MOVE     0
#define GIZMO_MODE_ROTATE   1
#define GIZMO_MODE_FREEROAM 2

#define RAY_INTERSECTION_MAX_DISTANCE 10000.0f

#ifdef _WIN32
// Manual GLU implementations for Windows to avoid header conflicts
void gluPerspective(double fovy, double aspect, double zNear, double zFar)
{
    double fH = tan(fovy / 360 * M_PI) * zNear;
    double fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
}

void gluLookAt(double eyeX, double eyeY, double eyeZ,
               double centerX, double centerY, double centerZ,
               double upX, double upY, double upZ)
{
    double forward[3], side[3], up[3];
    double m[4][4];

    forward[0] = centerX - eyeX;
    forward[1] = centerY - eyeY;
    forward[2] = centerZ - eyeZ;

    double len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
    if(len != 0.0)
    {
        forward[0] /= len;
        forward[1] /= len;
        forward[2] /= len;
    }

    side[0] = forward[1] * upZ - forward[2] * upY;
    side[1] = forward[2] * upX - forward[0] * upZ;
    side[2] = forward[0] * upY - forward[1] * upX;

    len = sqrt(side[0]*side[0] + side[1]*side[1] + side[2]*side[2]);
    if(len != 0.0)
    {
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

    if(in[3] == 0.0) return 0;

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

    // Default grid dimensions
    grid_x = 10;
    grid_y = 10;
    grid_z = 10;

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

void LEDViewport3D::SetGridDimensions(int x, int y, int z)
{
    grid_x = x;
    grid_y = y;
    grid_z = z;
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
    // Calculate grid bounds based on LED coordinate system
    int half_x = grid_x / 2;
    int half_y = grid_y / 2;
    int half_z = grid_z / 2;

    // LED coordinates range from -half to (size - half - 1)
    int min_x = -half_x;
    int max_x = grid_x - half_x - 1;
    int min_z = -half_z;
    int max_z = grid_z - half_z - 1;
    int max_y = grid_y - half_y - 1; // Top of grid

    // Ground grid (Y=0)
    glColor3f(0.2f, 0.2f, 0.25f);
    glBegin(GL_LINES);

    // Draw X lines (parallel to X axis) - every 0.5 units for smaller grid
    for(float z = min_z - 0.5f; z <= max_z + 0.5f; z += 0.5f)
    {
        glVertex3f(min_x - 0.5f, 0, z);
        glVertex3f(max_x + 0.5f, 0, z);
    }

    // Draw Z lines (parallel to Z axis) - every 0.5 units for smaller grid
    for(float x = min_x - 0.5f; x <= max_x + 0.5f; x += 0.5f)
    {
        glVertex3f(x, 0, min_z - 0.5f);
        glVertex3f(x, 0, max_z + 0.5f);
    }

    glEnd();

    // Ceiling grid (at top of Y range)
    glColor3f(0.15f, 0.15f, 0.2f);
    glBegin(GL_LINES);

    // Draw X lines (parallel to X axis) - every 0.5 units for smaller grid
    for(float z = min_z - 0.5f; z <= max_z + 0.5f; z += 0.5f)
    {
        glVertex3f(min_x - 0.5f, max_y + 1, z);
        glVertex3f(max_x + 0.5f, max_y + 1, z);
    }

    // Draw Z lines (parallel to Z axis) - every 0.5 units for smaller grid
    for(float x = min_x - 0.5f; x <= max_x + 0.5f; x += 0.5f)
    {
        glVertex3f(x, max_y + 1, min_z - 0.5f);
        glVertex3f(x, max_y + 1, max_z + 0.5f);
    }

    glEnd();

    // Vertical boundary lines connecting floor to ceiling
    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_LINES);

    // Corner pillars
    glVertex3f(min_x - 0.5f, 0, min_z - 0.5f); glVertex3f(min_x - 0.5f, max_y + 1, min_z - 0.5f);
    glVertex3f(max_x + 0.5f, 0, min_z - 0.5f); glVertex3f(max_x + 0.5f, max_y + 1, min_z - 0.5f);
    glVertex3f(min_x - 0.5f, 0, max_z + 0.5f); glVertex3f(min_x - 0.5f, max_y + 1, max_z + 0.5f);
    glVertex3f(max_x + 0.5f, 0, max_z + 0.5f); glVertex3f(max_x + 0.5f, max_y + 1, max_z + 0.5f);

    // Some intermediate vertical lines
    int mid_x = (min_x + max_x) / 2;
    int mid_z = (min_z + max_z) / 2;
    glVertex3f(mid_x, 0, mid_z); glVertex3f(mid_x, max_y + 1, mid_z);

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
        // Scale removed - LEDs now positioned at discrete 1x1x1 grid positions

        DrawLEDs(ctrl);


        if((int)i == selected_controller_idx)
        {
            glLineWidth(2.0f);
            glColor3f(1.0f, 1.0f, 0.0f);

            // Calculate bounding box of all LEDs
            float min_x = 1000000.0f, max_x = -1000000.0f;
            float min_y = 1000000.0f, max_y = -1000000.0f;
            float min_z = 1000000.0f, max_z = -1000000.0f;

            for(unsigned int j = 0; j < ctrl->led_positions.size(); j++)
            {
                LEDPosition3D& led = ctrl->led_positions[j];
                Vector3D& pos = led.local_position;

                if(pos.x < min_x) min_x = pos.x;
                if(pos.x > max_x) max_x = pos.x;
                if(pos.y < min_y) min_y = pos.y;
                if(pos.y > max_y) max_y = pos.y;
                if(pos.z < min_z) min_z = pos.z;
                if(pos.z > max_z) max_z = pos.z;
            }

            // Add a small buffer around the LEDs
            float buffer = 0.5f;
            min_x -= buffer; max_x += buffer;
            min_y -= buffer; max_y += buffer;
            min_z -= buffer; max_z += buffer;

            // Draw wireframe bounding box
            glBegin(GL_LINE_LOOP);
            glVertex3f(min_x, min_y, min_z);
            glVertex3f(max_x, min_y, min_z);
            glVertex3f(max_x, max_y, min_z);
            glVertex3f(min_x, max_y, min_z);
            glEnd();

            glBegin(GL_LINE_LOOP);
            glVertex3f(min_x, min_y, max_z);
            glVertex3f(max_x, min_y, max_z);
            glVertex3f(max_x, max_y, max_z);
            glVertex3f(min_x, max_y, max_z);
            glEnd();

            glBegin(GL_LINES);
            glVertex3f(min_x, min_y, min_z); glVertex3f(min_x, min_y, max_z);
            glVertex3f(max_x, min_y, min_z); glVertex3f(max_x, min_y, max_z);
            glVertex3f(max_x, max_y, min_z); glVertex3f(max_x, max_y, max_z);
            glVertex3f(min_x, max_y, min_z); glVertex3f(min_x, max_y, max_z);
            glEnd();

            glLineWidth(1.0f);
        }

        glPopMatrix();
    }
}

void LEDViewport3D::DrawLEDs(ControllerTransform* ctrl)
{
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

        // Draw LED as 1x1x1 cube at exact grid position
        float x = led.local_position.x;
        float y = led.local_position.y;
        float z = led.local_position.z;

        float size = 0.25f; // Cube size matches 0.5 grid spacing (0.5f total cube size)

        glBegin(GL_QUADS);

        // Front face
        glVertex3f(x - size, y - size, z + size);
        glVertex3f(x + size, y - size, z + size);
        glVertex3f(x + size, y + size, z + size);
        glVertex3f(x - size, y + size, z + size);

        // Back face
        glVertex3f(x - size, y - size, z - size);
        glVertex3f(x - size, y + size, z - size);
        glVertex3f(x + size, y + size, z - size);
        glVertex3f(x + size, y - size, z - size);

        // Top face
        glVertex3f(x - size, y + size, z - size);
        glVertex3f(x - size, y + size, z + size);
        glVertex3f(x + size, y + size, z + size);
        glVertex3f(x + size, y + size, z - size);

        // Bottom face
        glVertex3f(x - size, y - size, z - size);
        glVertex3f(x + size, y - size, z - size);
        glVertex3f(x + size, y - size, z + size);
        glVertex3f(x - size, y - size, z + size);

        // Right face
        glVertex3f(x + size, y - size, z - size);
        glVertex3f(x + size, y + size, z - size);
        glVertex3f(x + size, y + size, z + size);
        glVertex3f(x + size, y - size, z + size);

        // Left face
        glVertex3f(x - size, y - size, z - size);
        glVertex3f(x - size, y - size, z + size);
        glVertex3f(x - size, y + size, z + size);
        glVertex3f(x - size, y + size, z - size);

        glEnd();
    }
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

        float cube_size = 0.48f;

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
    }
    else if(gizmo_mode == GIZMO_MODE_FREEROAM)
    {
        // Draw line/stick for freeroam grab handle
        glLineWidth(5.0f);
        glBegin(GL_LINES);

        glColor3f(0.5f, 0.0f, 1.0f);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 5, 0);

        glEnd();

        float cube_size = 0.48f;
        glBegin(GL_QUADS);

        // Purple grab cube on stick (like scale Y)
        glColor3f(0.5f, 0.0f, 1.0f);

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

        // Orange center cube for mode switching
        glColor3f(1.0f, 0.5f, 0.0f);
        float center_size = 0.48f;

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

void LEDViewport3D::mouseReleaseEvent(QMouseEvent*)
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
    float tmax = RAY_INTERSECTION_MAX_DISTANCE;

    for(int i = 0; i < 3; i++)
    {
        if(fabs(ray.direction[i]) < 0.0001f)
        {
            if(ray.origin[i] < box.min[i] || ray.origin[i] > box.max[i])
            {
                return false;
            }
        }
        else
        {
            float t1 = (box.min[i] - ray.origin[i]) / ray.direction[i];
            float t2 = (box.max[i] - ray.origin[i]) / ray.direction[i];

            if(t1 > t2)
            {
                float temp = t1;
                t1 = t2;
                t2 = temp;
            }

            tmin = fmax(tmin, t1);
            tmax = fmin(tmax, t2);

            if(tmin > tmax)
            {
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
        float cube_size = 0.48f;

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
                // Apply rotation transform
                ApplyRotationToPoint(local_x, local_y, local_z,
                                   ctrl->transform.rotation.x,
                                   ctrl->transform.rotation.y,
                                   ctrl->transform.rotation.z);

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
        float center_size = 0.48f;
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
    else if(gizmo_mode == GIZMO_MODE_MOVE)
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

            // Apply rotation transform
            ApplyRotationToPoint(local_x, local_y, local_z,
                               ctrl->transform.rotation.x,
                               ctrl->transform.rotation.y,
                               ctrl->transform.rotation.z);

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
        float center_size = 0.48f;
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
    else if(gizmo_mode == GIZMO_MODE_FREEROAM)
    {
        // Test purple grab cube on stick for freeroam movement
        Box3D grab_box;
        float grab_cube_size = 0.48f;
        float stick_height = 5.0f;

        // Purple cube position (on stick at Y=5)
        grab_box.min[0] = ctrl->transform.position.x - grab_cube_size;
        grab_box.min[1] = ctrl->transform.position.y + stick_height - grab_cube_size;
        grab_box.min[2] = ctrl->transform.position.z - grab_cube_size;
        grab_box.max[0] = ctrl->transform.position.x + grab_cube_size;
        grab_box.max[1] = ctrl->transform.position.y + stick_height + grab_cube_size;
        grab_box.max[2] = ctrl->transform.position.z + grab_cube_size;

        float distance;
        if(RayBoxIntersect(ray, grab_box, distance))
        {
            closest_distance = distance;
            closest_axis = 0; // Use axis 0 for freeroam grab cube
        }

        // Test orange center cube for mode switching
        Box3D center_box;
        float center_size = 0.48f;
        center_box.min[0] = ctrl->transform.position.x - center_size;
        center_box.min[1] = ctrl->transform.position.y - center_size;
        center_box.min[2] = ctrl->transform.position.z - center_size;
        center_box.max[0] = ctrl->transform.position.x + center_size;
        center_box.max[1] = ctrl->transform.position.y + center_size;
        center_box.max[2] = ctrl->transform.position.z + center_size;

        float center_distance;
        if(RayBoxIntersect(ray, center_box, center_distance) && center_distance < closest_distance)
        {
            closest_distance = center_distance;
            closest_axis = -1; // Mode switching
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
    else if(gizmo_mode == GIZMO_MODE_MOVE)
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
            ctrl->transform.position.z += dy * move_scale;
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
    else if(gizmo_mode == GIZMO_MODE_FREEROAM)
    {
        float move_scale = 0.1f;

        // Freeroam movement in camera plane
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

    // Snap to grid (0.5 unit increments)
    ctrl->transform.position.x = round(ctrl->transform.position.x / 0.5f) * 0.5f;
    ctrl->transform.position.y = round(ctrl->transform.position.y / 0.5f) * 0.5f;
    ctrl->transform.position.z = round(ctrl->transform.position.z / 0.5f) * 0.5f;

    // Grid bounds collision
    int half_x = grid_x / 2;
    int half_y = grid_y / 2;
    int half_z = grid_z / 2;

    // Calculate grid bounds
    float min_x = -half_x - 0.5f;
    float max_x = grid_x - half_x - 1 + 0.5f;
    float min_y = 0.0f; // Floor
    float max_y = grid_y - half_y; // Ceiling (LEDs can go up to max_y - 1)
    float min_z = -half_z - 0.5f;
    float max_z = grid_z - half_z - 1 + 0.5f;

    // Apply collision bounds
    if(ctrl->transform.position.x < min_x) ctrl->transform.position.x = min_x;
    if(ctrl->transform.position.x > max_x) ctrl->transform.position.x = max_x;
    if(ctrl->transform.position.y < min_y) ctrl->transform.position.y = min_y;
    if(ctrl->transform.position.y > max_y) ctrl->transform.position.y = max_y;
    if(ctrl->transform.position.z < min_z) ctrl->transform.position.z = min_z;
    if(ctrl->transform.position.z > max_z) ctrl->transform.position.z = max_z;

    emit ControllerPositionChanged(selected_controller_idx,
                                   ctrl->transform.position.x,
                                   ctrl->transform.position.y,
                                   ctrl->transform.position.z);
}

void LEDViewport3D::ApplyRotationToPoint(float& x, float& y, float& z, float rx, float ry, float rz)
{
    // Convert angles to radians
    rx = rx * M_PI / 180.0f;
    ry = ry * M_PI / 180.0f;
    rz = rz * M_PI / 180.0f;

    // Z rotation
    float new_x = x * cos(rz) - y * sin(rz);
    float new_y = x * sin(rz) + y * cos(rz);
    x = new_x;
    y = new_y;

    // Y rotation
    new_x = x * cos(ry) + z * sin(ry);
    float new_z = -x * sin(ry) + z * cos(ry);
    x = new_x;
    z = new_z;

    // X rotation
    new_y = y * cos(rx) - z * sin(rx);
    new_z = y * sin(rx) + z * cos(rx);
    y = new_y;
    z = new_z;
}