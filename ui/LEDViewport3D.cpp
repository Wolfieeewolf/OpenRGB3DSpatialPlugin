/*---------------------------------------------------------*\
| LEDViewport3D.cpp                                        |
|                                                           |
|   OpenGL 3D viewport for LED visualization and control  |
|                                                           |
|   Date: 2025-09-29                                        |
|                                                           |
|   This file is part of the OpenRGB project               |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

/*---------------------------------------------------------*\
| Qt Includes                                              |
\*---------------------------------------------------------*/
#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>

/*---------------------------------------------------------*\
| System Includes                                          |
\*---------------------------------------------------------*/
#include <cmath>
#include <cfloat>
#include <algorithm>

/*---------------------------------------------------------*\
| OpenRGB Includes                                         |
\*---------------------------------------------------------*/
#include "Colors.h"

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "LEDViewport3D.h"

/*---------------------------------------------------------*\
| OpenGL Platform Includes                                 |
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

LEDViewport3D::LEDViewport3D(QWidget *parent)
    : QOpenGLWidget(parent)
    , controller_transforms(nullptr)
    , selected_controller_idx(-1)
    , grid_x(10)
    , grid_y(10)
    , grid_z(10)
    , grid_snap_enabled(false)
    , camera_distance(20.0f)
    , camera_yaw(45.0f)
    , camera_pitch(30.0f)
    , camera_target_x(0.0f)
    , camera_target_y(0.0f)
    , camera_target_z(0.0f)
    , dragging_rotate(false)
    , dragging_pan(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

LEDViewport3D::~LEDViewport3D()
{
}

void LEDViewport3D::SetControllerTransforms(std::vector<ControllerTransform*>* transforms)
{
    controller_transforms = transforms;

    if(controller_transforms)
    {
        for(unsigned int i = 0; i < controller_transforms->size(); i++)
        {
            if((*controller_transforms)[i])
            {
                EnforceFloorConstraint((*controller_transforms)[i]);
            }
        }
    }

    update();
}

void LEDViewport3D::SelectController(int index)
{
    if(controller_transforms && index >= 0 && index < (int)controller_transforms->size())
    {
        selected_controller_idx = index;
        ControllerTransform* ctrl = (*controller_transforms)[index];

        EnforceFloorConstraint(ctrl);

        if(!IsControllerSelected(index))
        {
            AddControllerToSelection(index);
        }

        gizmo.SetTarget(ctrl);
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();

        update();
    }
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

void LEDViewport3D::SetGridSnapEnabled(bool enabled)
{
    grid_snap_enabled = enabled;
}

void LEDViewport3D::UpdateGizmoPosition()
{
    if(selected_controller_idx >= 0 && controller_transforms &&
        selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];
        if(ctrl)
        {
            gizmo.SetPosition(ctrl->transform.position.x,
                            ctrl->transform.position.y,
                            ctrl->transform.position.z);
        }
    }
}

void LEDViewport3D::NotifyControllerTransformChanged()
{
    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void LEDViewport3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float camera_x = camera_target_x + camera_distance * cosf(camera_pitch * M_PI / 180.0f) * cosf(camera_yaw * M_PI / 180.0f);
    float camera_y = camera_target_y + camera_distance * sinf(camera_pitch * M_PI / 180.0f);
    float camera_z = camera_target_z + camera_distance * cosf(camera_pitch * M_PI / 180.0f) * sinf(camera_yaw * M_PI / 180.0f);

    gluLookAt(camera_x, camera_y, camera_z,
              camera_target_x, camera_target_y, camera_target_z,
              0.0f, 1.0f, 0.0f);

    DrawGrid();
    DrawAxes();
    DrawControllers();
    DrawUserFigure();
    DrawAxisLabels();

    UpdateGizmoPosition();

    float modelview[16];
    float projection[16];
    int viewport[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    if(selected_controller_idx >= 0 && controller_transforms &&
        selected_controller_idx < (int)controller_transforms->size())
    {
        gizmo.Render(modelview, projection, viewport);
    }
}

void LEDViewport3D::resizeGL(int w, int h)
{
    if(h == 0) h = 1;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)w / (float)h;
    gluPerspective(45.0, aspect, 0.1, 1000.0);

    glMatrixMode(GL_MODELVIEW);
}

void LEDViewport3D::mousePressEvent(QMouseEvent *event)
{
    last_mouse_pos = event->pos();

    float modelview[16];
    float projection[16];
    int viewport[4];

    makeCurrent();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    if(event->button() == Qt::LeftButton)
    {
        if(selected_controller_idx >= 0 && controller_transforms &&
            selected_controller_idx < (int)controller_transforms->size())
        {
            gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
            if(gizmo.HandleMousePress(event, modelview, projection, viewport))
            {
                update();
                return;
            }
        }

        int picked_controller = PickController(event->x(), event->y());
        if(picked_controller >= 0)
        {
            if(event->modifiers() & Qt::ControlModifier)
            {
                if(IsControllerSelected(picked_controller))
                {
                    RemoveControllerFromSelection(picked_controller);
                }
                else
                {
                    AddControllerToSelection(picked_controller);
                    if(controller_transforms && picked_controller < (int)controller_transforms->size())
                    {
                        ControllerTransform* ctrl = (*controller_transforms)[picked_controller];
                        EnforceFloorConstraint(ctrl);
                        gizmo.SetTarget(ctrl);
                        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
                        UpdateGizmoPosition();
                    }
                }
                emit ControllerSelected(selected_controller_idx);
                update();
                return;
            }
            else
            {
                ClearSelection();
                AddControllerToSelection(picked_controller);
                SelectController(picked_controller);
                emit ControllerSelected(picked_controller);
                update();
                return;
            }
        }

        if(!selected_controller_indices.empty())
        {
            ClearSelection();
            emit ControllerSelected(-1);
            update();
            return;
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

    float modelview[16];
    float projection[16];
    int viewport[4];

    makeCurrent();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    gizmo.SetGridSnap(grid_snap_enabled, 1.0f);

    if(gizmo.HandleMouseMove(event, modelview, projection, viewport))
    {
        if(selected_controller_idx >= 0 && controller_transforms &&
            selected_controller_idx < (int)controller_transforms->size())
        {
            ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx];
            EnforceFloorConstraint(ctrl);
            UpdateGizmoPosition();

            emit ControllerPositionChanged(selected_controller_idx,
                                         ctrl->transform.position.x,
                                         ctrl->transform.position.y,
                                         ctrl->transform.position.z);
            emit ControllerRotationChanged(selected_controller_idx,
                                         ctrl->transform.rotation.x,
                                         ctrl->transform.rotation.y,
                                         ctrl->transform.rotation.z);
        }

        update();
        last_mouse_pos = event->pos();
        return;
    }

    if(dragging_rotate)
    {
        float orbit_sensitivity = 0.5f;
        camera_yaw += delta.x() * orbit_sensitivity;
        camera_pitch -= delta.y() * orbit_sensitivity;

        if(camera_pitch > 89.0f) camera_pitch = 89.0f;
        if(camera_pitch < -89.0f) camera_pitch = -89.0f;

        update();
    }
    else if(dragging_pan)
    {
        float pan_sensitivity = 0.01f * camera_distance * 0.1f; // Scale with zoom level

        float right_x = cosf(camera_yaw * M_PI / 180.0f);
        float right_z = sinf(camera_yaw * M_PI / 180.0f);
        float up_x = -sinf(camera_yaw * M_PI / 180.0f) * sinf(camera_pitch * M_PI / 180.0f);
        float up_y = cosf(camera_pitch * M_PI / 180.0f);
        float up_z = cosf(camera_yaw * M_PI / 180.0f) * sinf(camera_pitch * M_PI / 180.0f);

        camera_target_x -= right_x * delta.x() * pan_sensitivity;
        camera_target_z -= right_z * delta.x() * pan_sensitivity;
        camera_target_x += up_x * delta.y() * pan_sensitivity;
        camera_target_y += up_y * delta.y() * pan_sensitivity;
        camera_target_z += up_z * delta.y() * pan_sensitivity;

        update();
    }

    last_mouse_pos = event->pos();
}

void LEDViewport3D::mouseReleaseEvent(QMouseEvent *event)
{
    (void)event;

    gizmo.HandleMouseRelease(event);
    dragging_rotate = false;
    dragging_pan = false;
    update();
}

void LEDViewport3D::wheelEvent(QWheelEvent *event)
{
    float zoom_speed = 0.001f * camera_distance; // Zoom speed scales with distance
    float min_zoom_speed = 0.1f;
    float max_zoom_speed = 2.0f;

    if(zoom_speed < min_zoom_speed) zoom_speed = min_zoom_speed;
    if(zoom_speed > max_zoom_speed) zoom_speed = max_zoom_speed;

    float delta = event->angleDelta().y() / 120.0f;

    if(delta > 0)
    {
        camera_distance -= zoom_speed * delta;
    }
    else
    {
        camera_distance -= zoom_speed * delta;
    }

    if(camera_distance < 0.5f) camera_distance = 0.5f;
    if(camera_distance > 200.0f) camera_distance = 200.0f;

    update();
}

void LEDViewport3D::DrawGrid()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glLineWidth(1.0f);

    glBegin(GL_LINES);

    for(int i = -grid_x; i <= grid_x; i++)
    {
        if(i == 0)
        {
            glColor3f(0.8f, 0.4f, 0.4f); // Red center line (X axis)
        }
        else if(i % 5 == 0)
        {
            glColor3f(0.5f, 0.5f, 0.5f); // Brighter every 5th line
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f); // Regular floor grid
        }

        glVertex3f((float)i, 0.0f, -(float)grid_z);
        glVertex3f((float)i, 0.0f, (float)grid_z);
    }

    for(int i = -grid_z; i <= grid_z; i++)
    {
        if(i == 0)
        {
            glColor3f(0.4f, 0.4f, 0.8f); // Blue center line (Z axis)
        }
        else if(i % 5 == 0)
        {
            glColor3f(0.5f, 0.5f, 0.5f); // Brighter every 5th line
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f); // Regular floor grid
        }

        glVertex3f(-(float)grid_x, 0.0f, (float)i);
        glVertex3f((float)grid_x, 0.0f, (float)i);
    }

    glEnd();

    glLineWidth(2.0f);
    glColor3f(0.6f, 0.8f, 0.6f); // Green floor boundary
    glBegin(GL_LINE_LOOP);
    glVertex3f(-(float)grid_x, 0.0f, -(float)grid_z);
    glVertex3f((float)grid_x, 0.0f, -(float)grid_z);
    glVertex3f((float)grid_x, 0.0f, (float)grid_z);
    glVertex3f(-(float)grid_x, 0.0f, (float)grid_z);
    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawAxes()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(3.0f);

    /*---------------------------------------------------------*\
    | Draw main axis lines from origin                         |
    \*---------------------------------------------------------*/
    glBegin(GL_LINES);

    // X Axis (Red) - Left/Right
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(2.0f, 0.0f, 0.0f);

    // Y Axis (Green) - Up/Down
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 2.0f, 0.0f);

    // Z Axis (Blue) - Front/Back
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 2.0f);

    glEnd();

    /*---------------------------------------------------------*\
    | Draw arrow indicators at axis endpoints                  |
    \*---------------------------------------------------------*/
    glLineWidth(2.0f);

    // X Axis arrows (Red - Left/Right Walls)
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    // Right Wall (+X) arrow
    glVertex3f(2.0f, 0.0f, 0.0f);
    glVertex3f(1.7f, 0.15f, 0.0f);
    glVertex3f(1.7f, -0.15f, 0.0f);
    // Left Wall (-X) arrow
    glVertex3f(-2.0f, 0.0f, 0.0f);
    glVertex3f(-1.7f, 0.15f, 0.0f);
    glVertex3f(-1.7f, -0.15f, 0.0f);
    glEnd();

    // Y Axis arrows (Green - Floor/Ceiling)
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    // Ceiling (+Y) arrow
    glVertex3f(0.0f, 2.0f, 0.0f);
    glVertex3f(0.15f, 1.7f, 0.0f);
    glVertex3f(-0.15f, 1.7f, 0.0f);
    glEnd();

    // Z Axis arrows (Blue - Front/Rear Walls)
    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    // Rear Wall (+Z) arrow
    glVertex3f(0.0f, 0.0f, 2.0f);
    glVertex3f(0.15f, 0.0f, 1.7f);
    glVertex3f(-0.15f, 0.0f, 1.7f);
    // Front Wall (-Z) arrow
    glVertex3f(0.0f, 0.0f, -2.0f);
    glVertex3f(0.15f, 0.0f, -1.7f);
    glVertex3f(-0.15f, 0.0f, -1.7f);
    glEnd();

    glLineWidth(1.0f);
}

void LEDViewport3D::DrawAxisLabels()
{
    /*---------------------------------------------------------*\
    | Draw 2D text overlay for axis labels                     |
    \*---------------------------------------------------------*/
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);

    // Get 3D positions and project to 2D screen coordinates
    GLdouble modelview[16], projection[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    auto project3Dto2D = [&](float x, float y, float z, double& screenX, double& screenY) {
        GLdouble winX, winY, winZ;
        gluProject(x, y, z, modelview, projection, viewport, &winX, &winY, &winZ);
        screenX = winX;
        screenY = viewport[3] - winY; // Flip Y for Qt coordinates
    };

    // Draw X axis labels (Red - Left/Right Walls)
    double x, y;
    painter.setPen(QColor(255, 100, 100));
    project3Dto2D((float)grid_x, 0.5f, 0.0f, x, y);
    painter.drawText(QPointF(x + 10, y), "Right Wall (+X)");
    project3Dto2D(-(float)grid_x, 0.5f, 0.0f, x, y);
    painter.drawText(QPointF(x - 100, y), "Left Wall (-X)");

    // Draw Y axis labels (Green - Floor/Ceiling)
    painter.setPen(QColor(100, 255, 100));
    project3Dto2D(0.0f, (float)grid_y, 0.0f, x, y);
    painter.drawText(QPointF(x + 10, y), "Ceiling (+Y)");
    project3Dto2D(0.0f, 0.2f, 0.0f, x, y);
    painter.drawText(QPointF(x + 10, y), "Floor (Y=0)");

    // Draw Z axis labels (Blue - Front/Rear Walls)
    painter.setPen(QColor(100, 100, 255));
    project3Dto2D(0.0f, 0.5f, (float)grid_z, x, y);
    painter.drawText(QPointF(x + 10, y), "Rear Wall (+Z)");
    project3Dto2D(0.0f, 0.5f, -(float)grid_z, x, y);
    painter.drawText(QPointF(x - 110, y), "Front Wall (-Z)");

    // Draw origin label
    painter.setPen(QColor(255, 255, 255));
    project3Dto2D(0.3f, 0.3f, 0.3f, x, y);
    painter.drawText(QPointF(x, y), "Origin (0,0,0)");

    painter.end();

    /*---------------------------------------------------------*\
    | Draw directional labels at grid edges                    |
    \*---------------------------------------------------------*/
    float half_x = grid_x / 2.0f;
    float half_y = grid_y / 2.0f;

    glLineWidth(2.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, half_y - 1.5f, 0.0f);
    glVertex3f(0.0f, half_y, 0.0f);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, half_y, 0.0f);
    glVertex3f(-0.2f, half_y - 0.3f, 0.0f);
    glVertex3f(0.2f, half_y - 0.3f, 0.0f);
    glEnd();

    glColor3f(0.0f, 0.5f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, -half_y + 1.5f, 0.0f);
    glVertex3f(0.0f, -half_y, 0.0f);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, -half_y, 0.0f);
    glVertex3f(-0.2f, -half_y + 0.3f, 0.0f);
    glVertex3f(0.2f, -half_y + 0.3f, 0.0f);
    glEnd();

    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(-half_x + 1.5f, 0.0f, 0.0f);
    glVertex3f(-half_x, 0.0f, 0.0f);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex3f(-half_x, 0.0f, 0.0f);
    glVertex3f(-half_x + 0.3f, -0.2f, 0.0f);
    glVertex3f(-half_x + 0.3f, 0.2f, 0.0f);
    glEnd();

    glColor3f(0.5f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(half_x - 1.5f, 0.0f, 0.0f);
    glVertex3f(half_x, 0.0f, 0.0f);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex3f(half_x, 0.0f, 0.0f);
    glVertex3f(half_x - 0.3f, -0.2f, 0.0f);
    glVertex3f(half_x - 0.3f, 0.2f, 0.0f);
    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawControllers()
{
    if(!controller_transforms) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i];
        if(!ctrl) continue;

        glPushMatrix();

        glTranslatef(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z);
        glRotatef(ctrl->transform.rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(ctrl->transform.scale.x, ctrl->transform.scale.y, ctrl->transform.scale.z);

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f
        };

        glTranslatef(center_offset.x, center_offset.y, center_offset.z);

        bool is_selected = IsControllerSelected((int)i);
        bool is_primary = ((int)i == selected_controller_idx);

        if(is_primary)
        {
            glColor3f(1.0f, 1.0f, 0.0f); // Bright yellow for primary selection
            glLineWidth(3.0f);
        }
        else if(is_selected)
        {
            glColor3f(1.0f, 0.8f, 0.0f); // Orange for secondary selections
            glLineWidth(2.0f);
        }
        else
        {
            glColor3f(0.7f, 0.7f, 0.7f); // Light gray for unselected
            glLineWidth(1.0f);
        }

        glBegin(GL_LINES);

        glVertex3f(min_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, min_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, min_bounds.y, max_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, min_bounds.y, max_bounds.z);
        glVertex3f(min_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, min_bounds.y, min_bounds.z);

        glVertex3f(min_bounds.x, max_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, max_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, max_bounds.z);
        glVertex3f(max_bounds.x, max_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, max_bounds.z);
        glVertex3f(min_bounds.x, max_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, min_bounds.z);

        glVertex3f(min_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, max_bounds.z);
        glVertex3f(min_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, max_bounds.z);

        glEnd();

        DrawLEDs(ctrl);

        glPopMatrix();
    }

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawLEDs(ControllerTransform* ctrl)
{
    if(!ctrl->controller && !ctrl->virtual_controller) return;

    glDisable(GL_LIGHTING);
    glPointSize(4.0f);
    glBegin(GL_POINTS);

    if(ctrl->virtual_controller)
    {
        for(size_t i = 0; i < ctrl->led_positions.size(); i++)
        {
            const LEDPosition3D& pos = ctrl->led_positions[i];

            RGBColor color = 0xFFFFFF; // Default white
            if(ctrl->virtual_controller)
            {
                const std::vector<GridLEDMapping>& mappings = ctrl->virtual_controller->GetMappings();
                if(i < mappings.size() && mappings[i].controller)
                {
                    unsigned int zone_idx = mappings[i].zone_idx;
                    unsigned int led_idx = mappings[i].led_idx;
                    if(zone_idx < mappings[i].controller->zones.size())
                    {
                        unsigned int global_led_idx = mappings[i].controller->zones[zone_idx].start_idx + led_idx;
                        if(global_led_idx < mappings[i].controller->colors.size())
                        {
                            color = mappings[i].controller->colors[global_led_idx];
                        }
                    }
                }
            }

            float r = (float)RGBGetRValue(color) / 255.0f;
            float g = (float)RGBGetGValue(color) / 255.0f;
            float b = (float)RGBGetBValue(color) / 255.0f;

            glColor3f(r, g, b);
            glVertex3f(pos.local_position.x, pos.local_position.y, pos.local_position.z);
        }
    }
    else
    {
        for(size_t led_idx = 0; led_idx < ctrl->controller->colors.size() &&
             led_idx < ctrl->led_positions.size(); led_idx++)
        {
            const LEDPosition3D& pos = ctrl->led_positions[led_idx];
            const RGBColor& color = ctrl->controller->colors[led_idx];

            float r = (float)RGBGetRValue(color) / 255.0f;
            float g = (float)RGBGetGValue(color) / 255.0f;
            float b = (float)RGBGetBValue(color) / 255.0f;

            glColor3f(r, g, b);
            glVertex3f(pos.local_position.x, pos.local_position.y, pos.local_position.z);
        }
    }

    glEnd();

    glPointSize(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

int LEDViewport3D::PickController(int mouse_x, int mouse_y)
{
    if(!controller_transforms) return -1;

    float modelview[16];
    float projection[16];
    int viewport[4];

    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    float x = (2.0f * mouse_x) / viewport[2] - 1.0f;
    float y = 1.0f - (2.0f * mouse_y) / viewport[3];

    float ray_clip[4] = { x, y, -1.0f, 1.0f };

    float inv_projection[16];
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

    float ray_origin[3] = { modelview[12], modelview[13], modelview[14] };
    float ray_direction[3] = { ray_eye[0], ray_eye[1], ray_eye[2] };

    float length = sqrtf(ray_direction[0] * ray_direction[0] +
                        ray_direction[1] * ray_direction[1] +
                        ray_direction[2] * ray_direction[2]);
    if(length > 0.0f)
    {
        ray_direction[0] /= length;
        ray_direction[1] /= length;
        ray_direction[2] /= length;
    }

    float closest_distance = FLT_MAX;
    int closest_controller = -1;

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i];
        if(!ctrl) continue;

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f
        };

        Vector3D world_min = {
            ctrl->transform.position.x + center_offset.x + min_bounds.x,
            ctrl->transform.position.y + center_offset.y + min_bounds.y,
            ctrl->transform.position.z + center_offset.z + min_bounds.z
        };
        Vector3D world_max = {
            ctrl->transform.position.x + center_offset.x + max_bounds.x,
            ctrl->transform.position.y + center_offset.y + max_bounds.y,
            ctrl->transform.position.z + center_offset.z + max_bounds.z
        };

        float distance;
        if(RayBoxIntersect(ray_origin, ray_direction, world_min, world_max, distance))
        {
            if(distance < closest_distance)
            {
                closest_distance = distance;
                closest_controller = (int)i;
            }
        }
    }

    return closest_controller;
}

bool LEDViewport3D::RayBoxIntersect(float ray_origin[3], float ray_direction[3],
                                   const Vector3D& box_min, const Vector3D& box_max, float& distance)
{
    float tmin = 0.0f;
    float tmax = 1000.0f;

    for(int i = 0; i < 3; i++)
    {
        float box_min_val = (i == 0) ? box_min.x : (i == 1) ? box_min.y : box_min.z;
        float box_max_val = (i == 0) ? box_max.x : (i == 1) ? box_max.y : box_max.z;

        if(fabsf(ray_direction[i]) < 1e-6f)
        {
            if(ray_origin[i] < box_min_val || ray_origin[i] > box_max_val)
                return false;
        }
        else
        {
            float t1 = (box_min_val - ray_origin[i]) / ray_direction[i];
            float t2 = (box_max_val - ray_origin[i]) / ray_direction[i];

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


void LEDViewport3D::CalculateControllerBounds(ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds)
{
    if(!ctrl || ctrl->led_positions.empty())
    {
        min_bounds = {-0.5f, -0.5f, -0.5f};
        max_bounds = {0.5f, 0.5f, 0.5f};
        return;
    }

    Vector3D first_pos = ctrl->led_positions[0].local_position;
    min_bounds = first_pos;
    max_bounds = first_pos;

    for(unsigned int i = 0; i < ctrl->led_positions.size(); i++)
    {
        const Vector3D& pos = ctrl->led_positions[i].local_position;

        if(pos.x < min_bounds.x) min_bounds.x = pos.x;
        if(pos.y < min_bounds.y) min_bounds.y = pos.y;
        if(pos.z < min_bounds.z) min_bounds.z = pos.z;

        if(pos.x > max_bounds.x) max_bounds.x = pos.x;
        if(pos.y > max_bounds.y) max_bounds.y = pos.y;
        if(pos.z > max_bounds.z) max_bounds.z = pos.z;
    }

    float padding = 0.1f;
    min_bounds.x -= padding;
    min_bounds.y -= padding;
    min_bounds.z -= padding;
    max_bounds.x += padding;
    max_bounds.y += padding;
    max_bounds.z += padding;
}

Vector3D LEDViewport3D::GetControllerCenter(ControllerTransform* ctrl)
{
    Vector3D min_bounds, max_bounds;
    CalculateControllerBounds(ctrl, min_bounds, max_bounds);

    Vector3D local_center = {
        (min_bounds.x + max_bounds.x) * 0.5f,
        (min_bounds.y + max_bounds.y) * 0.5f,
        (min_bounds.z + max_bounds.z) * 0.5f
    };

    return TransformLocalToWorld(local_center, ctrl->transform);
}

Vector3D LEDViewport3D::GetControllerSize(ControllerTransform* ctrl)
{
    Vector3D min_bounds, max_bounds;
    CalculateControllerBounds(ctrl, min_bounds, max_bounds);

    return {
        max_bounds.x - min_bounds.x,
        max_bounds.y - min_bounds.y,
        max_bounds.z - min_bounds.z
    };
}

void LEDViewport3D::EnforceFloorConstraint(ControllerTransform* ctrl)
{
    if(!ctrl) return;

    float min_y = GetControllerMinY(ctrl);

    if(min_y < 0.0f)
    {
        ctrl->transform.position.y += (0.0f - min_y);
    }
}

bool LEDViewport3D::IsControllerAboveFloor(ControllerTransform* ctrl)
{
    if(!ctrl) return true;

    return GetControllerMinY(ctrl) >= 0.0f;
}

float LEDViewport3D::GetControllerMinY(ControllerTransform* ctrl)
{
    if(!ctrl) return 0.0f;

    Vector3D min_bounds, max_bounds;
    CalculateControllerBounds(ctrl, min_bounds, max_bounds);

    Vector3D corners[8] = {
        {min_bounds.x, min_bounds.y, min_bounds.z},
        {max_bounds.x, min_bounds.y, min_bounds.z},
        {min_bounds.x, max_bounds.y, min_bounds.z},
        {max_bounds.x, max_bounds.y, min_bounds.z},
        {min_bounds.x, min_bounds.y, max_bounds.z},
        {max_bounds.x, min_bounds.y, max_bounds.z},
        {min_bounds.x, max_bounds.y, max_bounds.z},
        {max_bounds.x, max_bounds.y, max_bounds.z}
    };

    float min_y = FLT_MAX;
    for(int i = 0; i < 8; i++)
    {
        Vector3D world_corner = TransformLocalToWorld(corners[i], ctrl->transform);
        if(world_corner.y < min_y)
        {
            min_y = world_corner.y;
        }
    }

    return min_y;
}

Vector3D LEDViewport3D::TransformLocalToWorld(const Vector3D& local_pos, const Transform3D& transform)
{
    Vector3D scaled = {
        local_pos.x * transform.scale.x,
        local_pos.y * transform.scale.y,
        local_pos.z * transform.scale.z
    };

    Vector3D rotated = scaled;

    float rx = transform.rotation.x * M_PI / 180.0f;
    float ry = transform.rotation.y * M_PI / 180.0f;
    float rz = transform.rotation.z * M_PI / 180.0f;

    float temp_y = rotated.y * cosf(rx) - rotated.z * sinf(rx);
    float temp_z = rotated.y * sinf(rx) + rotated.z * cosf(rx);
    rotated.y = temp_y;
    rotated.z = temp_z;

    float temp_x = rotated.x * cosf(ry) + rotated.z * sinf(ry);
    temp_z = -rotated.x * sinf(ry) + rotated.z * cosf(ry);
    rotated.x = temp_x;
    rotated.z = temp_z;

    temp_x = rotated.x * cosf(rz) - rotated.y * sinf(rz);
    temp_y = rotated.x * sinf(rz) + rotated.y * cosf(rz);
    rotated.x = temp_x;
    rotated.y = temp_y;

    Vector3D world = {
        rotated.x + transform.position.x,
        rotated.y + transform.position.y,
        rotated.z + transform.position.z
    };

    return world;
}

void LEDViewport3D::AddControllerToSelection(int index)
{
    if(index < 0 || !controller_transforms || index >= (int)controller_transforms->size())
        return;

    for(unsigned int i = 0; i < selected_controller_indices.size(); i++)
    {
        if(selected_controller_indices[i] == index)
            return;
    }

    selected_controller_indices.push_back(index);
    selected_controller_idx = index; // Keep primary selection for gizmo
}

void LEDViewport3D::RemoveControllerFromSelection(int index)
{
    std::vector<int>::iterator it = std::find(selected_controller_indices.begin(), selected_controller_indices.end(), index);
    if(it != selected_controller_indices.end())
    {
        selected_controller_indices.erase(it);

        if(selected_controller_idx == index)
        {
            selected_controller_idx = selected_controller_indices.empty() ? -1 : selected_controller_indices[0];
        }
    }
}

void LEDViewport3D::ClearSelection()
{
    selected_controller_indices.clear();
    selected_controller_idx = -1;
    gizmo.SetTarget(nullptr);
}

bool LEDViewport3D::IsControllerSelected(int index) const
{
    return std::find(selected_controller_indices.begin(), selected_controller_indices.end(), index) != selected_controller_indices.end();
}

/*---------------------------------------------------------*\
| User Position Functions                                   |
\*---------------------------------------------------------*/
void LEDViewport3D::SetUserPosition(const UserPosition3D& position)
{
    user_position = position;
}

void LEDViewport3D::DrawUserFigure()
{
    if(!user_position.visible) return;

    glPushMatrix();
    glTranslatef(user_position.x, user_position.y, user_position.z);

    glColor3f(0.0f, 1.0f, 0.0f); 
    glLineWidth(3.0f);

    glBegin(GL_LINES);

    glVertex3f(0.0f, -0.8f, 0.0f);  // Feet
    glVertex3f(0.0f, 0.8f, 0.0f);   // Head

    glVertex3f(-0.6f, 0.2f, 0.0f);  // Left arm
    glVertex3f(0.6f, 0.2f, 0.0f);   // Right arm

    glVertex3f(0.0f, -0.2f, 0.0f);  // Pelvis center
    glVertex3f(-0.4f, -0.8f, 0.0f); // Left foot

    glVertex3f(0.0f, -0.2f, 0.0f);  // Pelvis center
    glVertex3f(0.4f, -0.8f, 0.0f);  // Right foot

    glEnd();

    glBegin(GL_LINE_LOOP);
    for(int i = 0; i < 12; i++)
    {
        float angle = 2.0f * M_PI * i / 12.0f;
        float x = 0.15f * cosf(angle);
        float y = 0.9f + 0.15f * sinf(angle);  // Above body
        glVertex3f(x, y, 0.0f);
    }
    glEnd();

    glLineWidth(1.0f);  // Reset line width
    glPopMatrix();
}