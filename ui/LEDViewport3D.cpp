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
#include <QKeyEvent>
#include <QMessageBox>

/*---------------------------------------------------------*\
| System Includes                                          |
\*---------------------------------------------------------*/
#include <cmath>
#include <cfloat>
#include <algorithm>

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "QtCompat.h"

/*---------------------------------------------------------*\
| OpenRGB Includes                                         |
\*---------------------------------------------------------*/
#include "Colors.h"

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "LEDViewport3D.h"
#include "VirtualReferencePoint3D.h"

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
    , room_width(3668.0f)          // Default: ~12 feet
    , room_depth(2423.0f)          // Default: ~8 feet
    , room_height(2723.0f)         // Default: ~9 feet
    , use_manual_room_dimensions(false)
    , reference_points(nullptr)
    , selected_ref_point_idx(-1)
    , camera_distance(20.0f)
    , camera_yaw(45.0f)
    , camera_pitch(30.0f)
    , camera_target_x(0.0f)
    , camera_target_y(0.0f)
    , camera_target_z(0.0f)
    , dragging_rotate(false)
    , dragging_pan(false)
    , dragging_grab(false)
{
    // Default grid scale: 10mm per unit
    grid_scale_mm = 10.0f;
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

LEDViewport3D::~LEDViewport3D()
{
}

void LEDViewport3D::SetControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    controller_transforms = transforms;

    if(controller_transforms)
    {
        for(unsigned int i = 0; i < controller_transforms->size(); i++)
        {
            if((*controller_transforms)[i])
            {
                EnforceFloorConstraint((*controller_transforms)[i].get());
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
        ControllerTransform* ctrl = (*controller_transforms)[index].get();

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

void LEDViewport3D::SelectReferencePoint(int index)
{
    if(reference_points && index >= 0 && index < (int)reference_points->size())
    {
        selected_controller_indices.clear();
        selected_controller_idx = -1;
        selected_ref_point_idx = index;

        VirtualReferencePoint3D* ref_point = (*reference_points)[index].get();
        gizmo.SetTarget(ref_point);
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

void LEDViewport3D::SetRoomDimensions(float width, float depth, float height, bool use_manual)
{
    room_width = width;
    room_depth = depth;
    room_height = height;
    use_manual_room_dimensions = use_manual;
    update();
}

void LEDViewport3D::UpdateGizmoPosition()
{
    if(selected_ref_point_idx >= 0 && reference_points &&
        selected_ref_point_idx < (int)reference_points->size())
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[selected_ref_point_idx].get();
        if(ref_point)
        {
            Vector3D pos = ref_point->GetPosition();
            gizmo.SetPosition(pos.x, pos.y, pos.z);
        }
    }
    else if(selected_controller_idx >= 0 && controller_transforms &&
        selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
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
    // Mark world positions as dirty for the selected controller
    if(selected_controller_idx >= 0 && selected_controller_idx < (int)controller_transforms->size())
    {
        (*controller_transforms)[selected_controller_idx]->world_positions_dirty = true;
    }

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
    DrawRoomBoundary();
    DrawControllers();
    DrawUserFigure();
    DrawReferencePoints();
    DrawAxisLabels();

    UpdateGizmoPosition();

    float modelview[16];
    float projection[16];
    int viewport[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                    selected_controller_idx < (int)controller_transforms->size());
    bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                   selected_ref_point_idx < (int)reference_points->size());

    if(has_controller_selected || has_ref_point_selected)
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
    click_start_pos = event->pos();

    float modelview[16];
    float projection[16];
    int viewport[4];

    makeCurrent();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    if(event->button() == Qt::LeftButton)
    {
        // Check gizmo first (if something is selected)
        bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                        selected_controller_idx < (int)controller_transforms->size());
        bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                       selected_ref_point_idx < (int)reference_points->size());

        if(has_controller_selected || has_ref_point_selected)
        {
            gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
            if(gizmo.HandleMousePress(event, modelview, projection, viewport))
            {
                update();
                return;
            }
        }

        // Check if clicking on a reference point first (priority over controllers)
        int picked_ref_point = PickReferencePoint(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
        if(picked_ref_point >= 0)
        {
            // Clear controller selection only
            selected_controller_indices.clear();
            selected_controller_idx = -1;

            // Set reference point selection
            selected_ref_point_idx = picked_ref_point;

            // Set gizmo to target the reference point
            if(reference_points && picked_ref_point < (int)reference_points->size())
            {
                VirtualReferencePoint3D* ref_point = (*reference_points)[picked_ref_point].get();
                gizmo.SetTarget(ref_point);
                gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
                UpdateGizmoPosition();
            }

            emit ReferencePointSelected(picked_ref_point);
            update();
            return;
        }

        // Check if clicking on a controller
        int picked_controller = PickController(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
        if(picked_controller >= 0)
        {
            selected_ref_point_idx = -1; // Deselect reference point
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
                        ControllerTransform* ctrl = (*controller_transforms)[picked_controller].get();
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

        // Start grab mode (will decide on release if it was a click or drag)
        dragging_grab = true;
        return;
    }
    else if(event->button() == Qt::MiddleButton)
    {
        // Middle mouse = Vertical pan (up/down camera movement)
        dragging_pan = true;
    }
    else if(event->button() == Qt::RightButton)
    {
        // Right mouse = Rotate
        dragging_rotate = true;
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
            ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
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
        else if(selected_ref_point_idx >= 0 && reference_points &&
                selected_ref_point_idx < (int)reference_points->size())
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[selected_ref_point_idx].get();
            Vector3D pos = ref_point->GetPosition();

            emit ReferencePointPositionChanged(selected_ref_point_idx,
                                              pos.x, pos.y, pos.z);
        }

        update();
        last_mouse_pos = event->pos();
        return;
    }

    if(dragging_grab)
    {
        // Grab mode: slide the grid like grabbing and dragging the floor
        float grab_sensitivity = 0.003f * camera_distance;

        // Calculate camera right and forward vectors (in XZ plane only - parallel to ground)
        float yaw_rad = camera_yaw * M_PI / 180.0f;

        // Right vector (perpendicular to view, in XZ plane)
        float right_x = -sinf(yaw_rad);
        float right_z = cosf(yaw_rad);

        // Forward vector (view direction projected onto XZ plane)
        float forward_x = cosf(yaw_rad);
        float forward_z = sinf(yaw_rad);

        // Horizontal drag = slide left/right (swapped for natural feel)
        camera_target_x += right_x * delta.x() * grab_sensitivity;
        camera_target_z += right_z * delta.x() * grab_sensitivity;

        // Vertical drag = slide forward/back on the ground plane (inverted)
        camera_target_x -= forward_x * delta.y() * grab_sensitivity;
        camera_target_z -= forward_z * delta.y() * grab_sensitivity;

        update();
    }
    else if(dragging_rotate)
    {
        // Unity-style rotation: smooth and responsive
        float orbit_sensitivity = 0.3f;
        camera_yaw += delta.x() * orbit_sensitivity;  // Swapped back
        camera_pitch += delta.y() * orbit_sensitivity;  // Inverted up/down

        // Clamp pitch to avoid gimbal lock
        if(camera_pitch > 89.0f) camera_pitch = 89.0f;
        if(camera_pitch < -89.0f) camera_pitch = -89.0f;

        update();
    }
    else if(dragging_pan)
    {
        // Blender-style panning: scales with distance for consistent feel
        float pan_sensitivity = 0.002f * camera_distance;

        // Calculate camera right and up vectors
        float yaw_rad = camera_yaw * M_PI / 180.0f;
        float pitch_rad = camera_pitch * M_PI / 180.0f;

        // Right vector (perpendicular to view direction, in XZ plane)
        float right_x = -sinf(yaw_rad);
        float right_z = cosf(yaw_rad);

        // Up vector (camera's up direction in world space)
        float up_x = cosf(yaw_rad) * sinf(pitch_rad);
        float up_y = cosf(pitch_rad);
        float up_z = sinf(yaw_rad) * sinf(pitch_rad);

        // Pan the camera target
        camera_target_x += right_x * delta.x() * pan_sensitivity;
        camera_target_z += right_z * delta.x() * pan_sensitivity;
        camera_target_x -= up_x * delta.y() * pan_sensitivity;  // Swapped back
        camera_target_y -= up_y * delta.y() * pan_sensitivity;  // Swapped back
        camera_target_z -= up_z * delta.y() * pan_sensitivity;  // Swapped back

        update();
    }

    last_mouse_pos = event->pos();
}

void LEDViewport3D::mouseReleaseEvent(QMouseEvent *event)
{
    // Handle grab mode release - check if it was a click or drag
    if(dragging_grab && event->button() == Qt::LeftButton)
    {
        // Calculate distance moved
        QPoint delta = event->pos() - click_start_pos;
        float distance = sqrtf(delta.x() * delta.x() + delta.y() * delta.y());

        // If barely moved (< 3 pixels), treat as click for selection
        if(distance < 3.0f)
        {
            int picked_controller = PickController(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
            if(picked_controller >= 0)
            {
                // Select the controller
                ClearSelection();
                AddControllerToSelection(picked_controller);
                SelectController(picked_controller);
                emit ControllerSelected(picked_controller);
            }
            else
            {
                // Clicked empty space - deselect
                if(!selected_controller_indices.empty())
                {
                    ClearSelection();
                    emit ControllerSelected(-1);
                }
            }
        }
        // else: was a drag, camera already moved
    }
    // Handle middle-click selection
    else if(event->button() == Qt::MiddleButton)
    {
        int picked_controller = PickController(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
        if(picked_controller >= 0)
        {
            // Select the controller
            ClearSelection();
            AddControllerToSelection(picked_controller);
            SelectController(picked_controller);
            emit ControllerSelected(picked_controller);
        }
        else
        {
            // Clicked empty space - deselect
            if(!selected_controller_indices.empty())
            {
                ClearSelection();
                emit ControllerSelected(-1);
            }
        }
    }

    gizmo.HandleMouseRelease(event);
    dragging_rotate = false;
    dragging_pan = false;
    dragging_grab = false;
    update();
}

void LEDViewport3D::wheelEvent(QWheelEvent *event)
{
    // Blender-style zoom: exponential scaling for smooth zoom at all distances
    float delta = event->angleDelta().y() / 120.0f;

    // Use percentage-based zoom for consistent feel
    // Each scroll step zooms by 10% of current distance
    float zoom_factor = 1.0f - (delta * 0.1f);

    camera_distance *= zoom_factor;

    // Clamp to reasonable bounds
    if(camera_distance < 1.0f) camera_distance = 1.0f;
    if(camera_distance > 500.0f) camera_distance = 500.0f;

    update();
}

void LEDViewport3D::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        // Delete the selected controller with confirmation
        if(selected_controller_idx >= 0 && controller_transforms &&
           selected_controller_idx < (int)controller_transforms->size())
        {
            ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
            QString controller_name = "Unknown";

            if(ctrl && ctrl->controller)
            {
                controller_name = QString::fromStdString(ctrl->controller->name);
            }
            else if(ctrl && ctrl->virtual_controller)
            {
                controller_name = QString::fromStdString(ctrl->virtual_controller->GetName());
            }

            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "Delete Controller",
                                         QString("Are you sure you want to remove '%1' from the 3D view?").arg(controller_name),
                                         QMessageBox::Yes | QMessageBox::No);

            if(reply == QMessageBox::Yes)
            {
                emit ControllerDeleteRequested(selected_controller_idx);
            }
        }
    }
    else
    {
        QOpenGLWidget::keyPressEvent(event);
    }
}

void LEDViewport3D::DrawGrid()
{
    /*---------------------------------------------------------*\
    | Draw floor grid from origin (0,0,0) at front-left corner |
    | Grid extends to room dimensions in positive directions   |
    \*---------------------------------------------------------*/
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glLineWidth(1.0f);

    // Calculate grid range based on room dimensions or grid size
    float max_x = use_manual_room_dimensions ? (room_width / grid_scale_mm) : (float)grid_x;
    float max_y = use_manual_room_dimensions ? (room_depth / grid_scale_mm) : (float)grid_y;

    glBegin(GL_LINES);

    // Draw vertical grid lines (parallel to Y-axis, extending front-to-back)
    for(int i = 0; i <= (int)max_x; i++)
    {
        if(i == 0)
        {
            glColor3f(0.8f, 0.4f, 0.4f); // Red origin line (left wall, X=0)
        }
        else if(i % 5 == 0)
        {
            glColor3f(0.5f, 0.5f, 0.5f); // Brighter every 5th line
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f); // Regular floor grid
        }

        glVertex3f((float)i, 0.0f, 0.0f);        // Front edge
        glVertex3f((float)i, 0.0f, max_y);       // Back edge
    }

    // Draw horizontal grid lines (parallel to X-axis, extending left-to-right)
    for(int i = 0; i <= (int)max_y; i++)
    {
        if(i == 0)
        {
            glColor3f(0.4f, 0.4f, 0.8f); // Blue origin line (front wall, Y=0)
        }
        else if(i % 5 == 0)
        {
            glColor3f(0.5f, 0.5f, 0.5f); // Brighter every 5th line
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f); // Regular floor grid
        }

        glVertex3f(0.0f, 0.0f, (float)i);        // Left edge
        glVertex3f(max_x, 0.0f, (float)i);       // Right edge
    }

    glEnd();

    // Draw floor boundary rectangle at origin corner
    glLineWidth(2.0f);
    glColor3f(0.6f, 0.8f, 0.6f); // Green floor boundary
    glBegin(GL_LINE_LOOP);
    glVertex3f(0.0f, 0.0f, 0.0f);           // Front-left corner (origin)
    glVertex3f(max_x, 0.0f, 0.0f);          // Front-right corner
    glVertex3f(max_x, 0.0f, max_y);         // Back-right corner
    glVertex3f(0.0f, 0.0f, max_y);          // Back-left corner
    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawAxes()
{
    /*---------------------------------------------------------*\
    | Draw coordinate axes from origin (0,0,0)                 |
    | Corner-Origin System: All axes point in positive         |
    | directions from front-left-floor corner                  |
    \*---------------------------------------------------------*/
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(3.0f);

    /*---------------------------------------------------------*\
    | Draw main axis lines from origin (positive directions)   |
    \*---------------------------------------------------------*/
    glBegin(GL_LINES);

    // X Axis (Red) - Left to Right (positive direction only)
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(3.0f, 0.0f, 0.0f);

    // Z Axis (Green) - Front to Back (positive Y in grid coords, but Z visual)
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 3.0f);

    // Y Axis (Blue) - Floor to Ceiling (vertical up)
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 3.0f, 0.0f);

    glEnd();

    /*---------------------------------------------------------*\
    | Draw arrow indicators at axis endpoints                  |
    \*---------------------------------------------------------*/
    glLineWidth(2.0f);

    // X Axis arrow (Red - Right Wall direction)
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(3.0f, 0.0f, 0.0f);
    glVertex3f(2.7f, 0.15f, 0.0f);
    glVertex3f(2.7f, -0.15f, 0.0f);
    glEnd();

    // Z Axis arrow (Green - Back Wall direction)
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, 3.0f);
    glVertex3f(0.15f, 0.0f, 2.7f);
    glVertex3f(-0.15f, 0.0f, 2.7f);
    glEnd();

    // Y Axis arrow (Blue - Ceiling direction)
    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 3.0f, 0.0f);
    glVertex3f(0.15f, 2.7f, 0.0f);
    glVertex3f(-0.15f, 2.7f, 0.0f);
    glEnd();

    glLineWidth(1.0f);
}

void LEDViewport3D::DrawAxisLabels()
{
    /*---------------------------------------------------------*\
    | Draw 2D text overlay for axis labels                     |
    | Corner-Origin System: Labels at (0,0,0) and positive ends|
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

    // Calculate max positions based on room dimensions or grid
    float max_x = use_manual_room_dimensions ? (room_width / grid_scale_mm) : (float)grid_x;
    float max_y = use_manual_room_dimensions ? (room_depth / grid_scale_mm) : (float)grid_y;
    float max_z = use_manual_room_dimensions ? (room_height / grid_scale_mm) : (float)grid_z;

    double screen_x, screen_y;

    // Draw X axis labels (Red - Left/Right)
    painter.setPen(QColor(255, 100, 100));
    project3Dto2D(max_x, 0.5f, 0.0f, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), QString("Right Wall (X=%1)").arg((int)max_x));
    project3Dto2D(0.3f, 0.5f, 0.0f, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), "Left Wall (X=0)");

    // Draw Z axis labels (Green - Front/Back)
    painter.setPen(QColor(100, 255, 100));
    project3Dto2D(0.5f, 0.0f, max_y, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), QString("Back Wall (Y=%1)").arg((int)max_y));
    project3Dto2D(0.5f, 0.0f, 0.3f, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), "Front Wall (Y=0)");

    // Draw Y axis labels (Blue - Floor/Ceiling)
    painter.setPen(QColor(100, 100, 255));
    project3Dto2D(0.5f, max_z, 0.0f, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), QString("Ceiling (Z=%1)").arg((int)max_z));
    project3Dto2D(0.5f, 0.2f, 0.0f, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), "Floor (Z=0)");

    // Draw origin label (Front-Left-Floor Corner)
    painter.setPen(QColor(255, 255, 255));
    project3Dto2D(0.5f, 0.5f, 0.5f, screen_x, screen_y);
    painter.drawText(QPointF(screen_x, screen_y), "Origin (0,0,0)\nFront-Left-Floor");

    painter.end();
}

void LEDViewport3D::DrawRoomBoundary()
{
    /*---------------------------------------------------------*\
    | Draw 3D wireframe box showing room boundaries            |
    | Only drawn when manual room dimensions are enabled       |
    \*---------------------------------------------------------*/
    if(!use_manual_room_dimensions)
    {
        return; // Don't draw boundary when auto-detecting
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    // Calculate room boundary in grid units
    float max_x = room_width / grid_scale_mm;
    float max_y = room_depth / grid_scale_mm;
    float max_z = room_height / grid_scale_mm;

    // Use cyan color for room boundary (distinct from other elements)
    glColor3f(0.0f, 0.8f, 0.8f); // Bright cyan

    glBegin(GL_LINES);

    /*---------------------------------------------------------*\
    | Draw bottom rectangle (floor level, Z=0)                 |
    \*---------------------------------------------------------*/
    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(max_x, 0.0f, 0.0f);     // Front edge
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, 0.0f, max_y);    // Right edge
    glVertex3f(max_x, 0.0f, max_y);     glVertex3f(0.0f, 0.0f, max_y);     // Back edge
    glVertex3f(0.0f, 0.0f, max_y);      glVertex3f(0.0f, 0.0f, 0.0f);      // Left edge

    /*---------------------------------------------------------*\
    | Draw top rectangle (ceiling level, Z=max_z)             |
    \*---------------------------------------------------------*/
    glVertex3f(0.0f, max_z, 0.0f);      glVertex3f(max_x, max_z, 0.0f);    // Front edge
    glVertex3f(max_x, max_z, 0.0f);     glVertex3f(max_x, max_z, max_y);   // Right edge
    glVertex3f(max_x, max_z, max_y);    glVertex3f(0.0f, max_z, max_y);    // Back edge
    glVertex3f(0.0f, max_z, max_y);     glVertex3f(0.0f, max_z, 0.0f);     // Left edge

    /*---------------------------------------------------------*\
    | Draw vertical edges connecting floor to ceiling         |
    \*---------------------------------------------------------*/
    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(0.0f, max_z, 0.0f);     // Front-left corner
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, max_z, 0.0f);    // Front-right corner
    glVertex3f(max_x, 0.0f, max_y);     glVertex3f(max_x, max_z, max_y);   // Back-right corner
    glVertex3f(0.0f, 0.0f, max_y);      glVertex3f(0.0f, max_z, max_y);    // Back-left corner

    glEnd();

    // Reset line width and color
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
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
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
            glColor3f(0.3f, 0.3f, 0.3f); // Gray for unselected controllers
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

        /*---------------------------------------------------------*\
        | Draw TOP indicator sphere                                |
        | Shows which way is "up" on the controller                |
        \*---------------------------------------------------------*/
        float top_y = max_bounds.y;  // Top of bounding box
        float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        const float indicator_radius = 0.15f;
        const int sphere_segments = 8;

        glPushMatrix();
        glTranslatef(center_x, top_y, center_z);

        // Draw half sphere (top green, bottom red)
        for(int i = 0; i < sphere_segments / 2; i++)
        {
            float lat0 = M_PI * (0.0f + (float)i / sphere_segments);
            float lat1 = M_PI * (0.0f + (float)(i + 1) / sphere_segments);
            float y0 = indicator_radius * sinf(lat0);
            float y1 = indicator_radius * sinf(lat1);
            float r0 = indicator_radius * cosf(lat0);
            float r1 = indicator_radius * cosf(lat1);

            glBegin(GL_QUAD_STRIP);
            for(int j = 0; j <= sphere_segments; j++)
            {
                float lng = 2.0f * M_PI * (float)j / sphere_segments;
                float x = cosf(lng);
                float z = sinf(lng);

                // Top half = Green (correct orientation)
                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

        // Draw bottom half (red - wrong orientation indicator)
        for(int i = sphere_segments / 2; i < sphere_segments; i++)
        {
            float lat0 = M_PI * (0.0f + (float)i / sphere_segments);
            float lat1 = M_PI * (0.0f + (float)(i + 1) / sphere_segments);
            float y0 = indicator_radius * sinf(lat0);
            float y1 = indicator_radius * sinf(lat1);
            float r0 = indicator_radius * cosf(lat0);
            float r1 = indicator_radius * cosf(lat1);

            glBegin(GL_QUAD_STRIP);
            for(int j = 0; j <= sphere_segments; j++)
            {
                float lng = 2.0f * M_PI * (float)j / sphere_segments;
                float x = cosf(lng);
                float z = sinf(lng);

                // Bottom half = Red (needs rotation)
                glColor3f(1.0f, 0.0f, 0.0f);
                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

        glPopMatrix();

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

    /*---------------------------------------------------------*\
    | Use gluUnProject for proper ray generation             |
    \*---------------------------------------------------------*/
    GLdouble near_x, near_y, near_z;
    GLdouble far_x, far_y, far_z;

    // Convert mouse Y to OpenGL coordinates (flip Y axis)
    int gl_mouse_y = viewport[3] - mouse_y;

    // Convert to GLdouble arrays
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

    // Unproject near and far plane
    gluUnProject((GLdouble)mouse_x, (GLdouble)gl_mouse_y, 0.0,
                 mv, proj, vp,
                 &near_x, &near_y, &near_z);

    gluUnProject((GLdouble)mouse_x, (GLdouble)gl_mouse_y, 1.0,
                 mv, proj, vp,
                 &far_x, &far_y, &far_z);

    // Create ray
    float ray_origin[3] = { (float)near_x, (float)near_y, (float)near_z };
    float ray_direction[3] = {
        (float)(far_x - near_x),
        (float)(far_y - near_y),
        (float)(far_z - near_z)
    };

    // Normalize direction
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
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl) continue;

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        // Calculate center offset to match DrawControllers rendering
        Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f
        };

        // Transform local bounds to world space (matching draw transform order)
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

        // Ensure min < max (swap if needed)
        if(world_min.x > world_max.x) { float temp = world_min.x; world_min.x = world_max.x; world_max.x = temp; }
        if(world_min.y > world_max.y) { float temp = world_min.y; world_min.y = world_max.y; world_max.y = temp; }
        if(world_min.z > world_max.z) { float temp = world_min.z; world_min.z = world_max.z; world_max.z = temp; }

        float distance;
        if(RayBoxIntersect(ray_origin, ray_direction, world_min, world_max, distance))
        {
            // Prefer closer objects - distance-based priority
            // This helps when multiple boxes overlap or are aligned
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

bool LEDViewport3D::RaySphereIntersect(float ray_origin[3], float ray_direction[3],
                                       const Vector3D& sphere_center, float sphere_radius, float& distance)
{
    // Calculate vector from ray origin to sphere center
    float oc[3] = {
        ray_origin[0] - sphere_center.x,
        ray_origin[1] - sphere_center.y,
        ray_origin[2] - sphere_center.z
    };

    // Quadratic equation coefficients: at^2 + bt + c = 0
    float a = ray_direction[0] * ray_direction[0] +
              ray_direction[1] * ray_direction[1] +
              ray_direction[2] * ray_direction[2];

    float b = 2.0f * (oc[0] * ray_direction[0] +
                      oc[1] * ray_direction[1] +
                      oc[2] * ray_direction[2]);

    float c = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - sphere_radius * sphere_radius;

    // Calculate discriminant
    float discriminant = b * b - 4.0f * a * c;

    if(discriminant < 0.0f)
    {
        return false; // No intersection
    }

    // Calculate the two possible intersection distances
    float sqrt_discriminant = std::sqrt(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

    // Use the closest positive intersection
    if(t1 > 0.0f)
    {
        distance = t1;
        return true;
    }
    else if(t2 > 0.0f)
    {
        distance = t2;
        return true;
    }

    return false; // Both intersections behind the ray origin
}

int LEDViewport3D::PickReferencePoint(int mouse_x, int mouse_y)
{
    if(!reference_points) return -1;

    float modelview[16];
    float projection[16];
    int viewport[4];

    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Use gluUnProject for proper ray generation
    GLdouble near_x, near_y, near_z;
    GLdouble far_x, far_y, far_z;

    int gl_mouse_y = viewport[3] - mouse_y;

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

    gluUnProject((GLdouble)mouse_x, (GLdouble)gl_mouse_y, 0.0,
                 mv, proj, vp,
                 &near_x, &near_y, &near_z);

    gluUnProject((GLdouble)mouse_x, (GLdouble)gl_mouse_y, 1.0,
                 mv, proj, vp,
                 &far_x, &far_y, &far_z);

    // Create ray
    float ray_origin[3] = { (float)near_x, (float)near_y, (float)near_z };
    float ray_direction[3] = {
        (float)(far_x - near_x),
        (float)(far_y - near_y),
        (float)(far_z - near_z)
    };

    // Find closest reference point
    int closest_ref_point = -1;
    float closest_distance = FLT_MAX;
    const float sphere_radius = 0.3f; // Match the radius used in DrawReferencePoints

    for(unsigned int i = 0; i < reference_points->size(); i++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
        if(!ref_point->IsVisible()) continue;

        Vector3D pos = ref_point->GetPosition();
        float distance;

        if(RaySphereIntersect(ray_origin, ray_direction, pos, sphere_radius, distance))
        {
            if(distance < closest_distance)
            {
                closest_distance = distance;
                closest_ref_point = (int)i;
            }
        }
    }

    return closest_ref_point;
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

    // Check if this is a single point (1x1x1 or all LEDs at same position)
    float size_x = max_bounds.x - min_bounds.x;
    float size_y = max_bounds.y - min_bounds.y;
    float size_z = max_bounds.z - min_bounds.z;

    // For degenerate boxes (single point or very thin), create a minimum sized box
    float min_dimension = 0.2f;  // Minimum box dimension

    if(size_x < 0.001f)  // Essentially zero
    {
        float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        min_bounds.x = center_x - min_dimension;
        max_bounds.x = center_x + min_dimension;
    }
    if(size_y < 0.001f)
    {
        float center_y = (min_bounds.y + max_bounds.y) * 0.5f;
        min_bounds.y = center_y - min_dimension;
        max_bounds.y = center_y + min_dimension;
    }
    if(size_z < 0.001f)
    {
        float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        min_bounds.z = center_z - min_dimension;
        max_bounds.z = center_z + min_dimension;
    }

    // Small padding for visual clarity (tighter now)
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
    // Floor constraint disabled for corner-origin coordinate system
    // In this system, Y can range from 0 (front wall) to max (back wall)
    // and Z can range from 0 (floor) to max (ceiling)
    // No constraint is needed since all valid positions are positive
    (void)ctrl;  // Suppress unused parameter warning
    return;
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
    selected_ref_point_idx = -1;
    gizmo.SetTarget((ControllerTransform*)nullptr);
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

void LEDViewport3D::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
}

void LEDViewport3D::DrawUserFigure()
{
    // Find and draw User type reference point with smiley face
    if(!reference_points) return;

    for(size_t idx = 0; idx < reference_points->size(); idx++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
        if(ref_point->GetType() == REF_POINT_USER && ref_point->IsVisible())
        {
            bool is_selected = ((int)idx == selected_ref_point_idx);
            Vector3D pos = ref_point->GetPosition();
            Rotation3D rot = ref_point->GetRotation();
            RGBColor color = ref_point->GetDisplayColor();

            float r = (color & 0xFF) / 255.0f;
            float g = ((color >> 8) & 0xFF) / 255.0f;
            float b = ((color >> 16) & 0xFF) / 255.0f;

            glPushMatrix();
            glTranslatef(pos.x, pos.y, pos.z);
            glRotatef(rot.x, 1.0f, 0.0f, 0.0f);
            glRotatef(rot.y, 0.0f, 1.0f, 0.0f);
            glRotatef(rot.z, 0.0f, 0.0f, 1.0f);

            const float head_radius = 0.4f;
            const int segments = 20;

            glColor3f(r, g, b);
            glLineWidth(2.0f);

            // Draw head circle
            glBegin(GL_LINE_LOOP);
            for(int i = 0; i < segments; i++)
            {
                float angle = 2.0f * M_PI * i / segments;
                float x = head_radius * cosf(angle);
                float y = head_radius * sinf(angle);
                glVertex3f(x, y, 0.0f);
            }
            glEnd();

            // Draw eyes
            glPointSize(6.0f);
            glBegin(GL_POINTS);
            glVertex3f(-0.15f, 0.1f, 0.0f);
            glVertex3f(0.15f, 0.1f, 0.0f);
            glEnd();

            // Draw smile
            glBegin(GL_LINE_STRIP);
            for(int i = 0; i <= 10; i++)
            {
                float t = (float)i / 10.0f;
                float angle = M_PI + t * M_PI;
                float x = 0.25f * cosf(angle);
                float y = -0.05f + 0.25f * sinf(angle);
                glVertex3f(x, y, 0.0f);
            }
            glEnd();

            // Draw selection box when selected
            if(is_selected)
            {
                glDisable(GL_DEPTH_TEST); // Make sure selection box is always visible
                float box_size = head_radius * 1.5f;
                glColor3f(1.0f, 1.0f, 0.0f); // Bright yellow
                glLineWidth(3.0f);

                glBegin(GL_LINES);
                // Bottom square
                glVertex3f(-box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, -box_size);
                glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, box_size);
                glVertex3f(box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, box_size);
                glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, -box_size);

                // Top square
                glVertex3f(-box_size, box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
                glVertex3f(box_size, box_size, -box_size); glVertex3f(box_size, box_size, box_size);
                glVertex3f(box_size, box_size, box_size); glVertex3f(-box_size, box_size, box_size);
                glVertex3f(-box_size, box_size, box_size); glVertex3f(-box_size, box_size, -box_size);

                // Vertical edges
                glVertex3f(-box_size, -box_size, -box_size); glVertex3f(-box_size, box_size, -box_size);
                glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
                glVertex3f(box_size, -box_size, box_size); glVertex3f(box_size, box_size, box_size);
                glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, box_size, box_size);
                glEnd();

                glEnable(GL_DEPTH_TEST); // Re-enable depth test
            }

            glLineWidth(1.0f);
            glPointSize(1.0f);
            glPopMatrix();

            // Only draw the first User type reference point
            break;
        }
    }
}

void LEDViewport3D::DrawReferencePoints()
{
    if(!reference_points) return;

    const float sphere_radius = 0.3f;
    const int segments = 16;
    const int rings = 12;

    for(size_t idx = 0; idx < reference_points->size(); idx++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
        if(!ref_point->IsVisible()) continue;

        // Skip User type - drawn as smiley face in DrawUserFigure()
        if(ref_point->GetType() == REF_POINT_USER) continue;

        bool is_selected = ((int)idx == selected_ref_point_idx);

        Vector3D pos = ref_point->GetPosition();
        Rotation3D rot = ref_point->GetRotation();
        RGBColor color = ref_point->GetDisplayColor();

        float r = (color & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = ((color >> 16) & 0xFF) / 255.0f;

        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glRotatef(rot.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rot.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rot.z, 0.0f, 0.0f, 1.0f);

        // Draw filled sphere
        glColor3f(r, g, b);
        for(int i = 0; i < rings; i++)
        {
            float lat0 = M_PI * (-0.5f + (float)i / rings);
            float lat1 = M_PI * (-0.5f + (float)(i + 1) / rings);
            float y0 = sphere_radius * sinf(lat0);
            float y1 = sphere_radius * sinf(lat1);
            float r0 = sphere_radius * cosf(lat0);
            float r1 = sphere_radius * cosf(lat1);

            glBegin(GL_QUAD_STRIP);
            for(int j = 0; j <= segments; j++)
            {
                float lng = 2.0f * M_PI * (float)j / segments;
                float x = cosf(lng);
                float z = sinf(lng);

                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

        // Draw outline - brighter and thicker if selected
        if(is_selected)
        {
            glColor3f(1.0f, 1.0f, 0.0f); // Bright yellow for selection
            glLineWidth(4.0f);
        }
        else
        {
            glColor3f(r * 0.5f, g * 0.5f, b * 0.5f);
            glLineWidth(2.0f);
        }

        glBegin(GL_LINE_LOOP);
        for(int i = 0; i < segments; i++)
        {
            float angle = 2.0f * M_PI * i / segments;
            float x = sphere_radius * cosf(angle);
            float z = sphere_radius * sinf(angle);
            glVertex3f(x, 0.0f, z);
        }
        glEnd();

        // Draw selection box when selected
        if(is_selected)
        {
            glDisable(GL_DEPTH_TEST); // Make sure selection box is always visible
            float box_size = sphere_radius * 1.5f;
            glColor3f(1.0f, 1.0f, 0.0f); // Bright yellow
            glLineWidth(3.0f);

            glBegin(GL_LINES);
            // Bottom square
            glVertex3f(-box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, -box_size);
            glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, box_size);
            glVertex3f(box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, box_size);
            glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, -box_size);

            // Top square
            glVertex3f(-box_size, box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
            glVertex3f(box_size, box_size, -box_size); glVertex3f(box_size, box_size, box_size);
            glVertex3f(box_size, box_size, box_size); glVertex3f(-box_size, box_size, box_size);
            glVertex3f(-box_size, box_size, box_size); glVertex3f(-box_size, box_size, -box_size);

            // Vertical edges
            glVertex3f(-box_size, -box_size, -box_size); glVertex3f(-box_size, box_size, -box_size);
            glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
            glVertex3f(box_size, -box_size, box_size); glVertex3f(box_size, box_size, box_size);
            glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, box_size, box_size);
            glEnd();

            glEnable(GL_DEPTH_TEST); // Re-enable depth test
        }

        glLineWidth(1.0f);
        glPopMatrix();
    }
}