// SPDX-License-Identifier: GPL-2.0-only

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QFont>
#include <QTimer>

#include <cmath>
#include <cfloat>
#include <algorithm>

#include "QtCompat.h"

#include "Colors.h"

#include "LEDViewport3D.h"
#include "VirtualReferencePoint3D.h"
#include "ScreenCaptureManager.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

static inline Vector3D Subtract(const Vector3D& a, const Vector3D& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline Vector3D CrossVec(const Vector3D& a, const Vector3D& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float DotVec(const Vector3D& a, const Vector3D& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void ProjectPointToScreen(float x,
                                 float y,
                                 float z,
                                 const GLdouble modelview[16],
                                 const GLdouble projection[16],
                                 const GLint viewport[4],
                                 double& screen_x,
                                 double& screen_y)
{
    GLdouble winX;
    GLdouble winY;
    GLdouble winZ;
    gluProject(x, y, z, modelview, projection, viewport, &winX, &winY, &winZ);
    screen_x = winX;
    screen_y = viewport[3] - winY;
}

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
    , grid_scale_mm(10.0f)
    , room_width(1000.0f)
    , room_depth(1000.0f)
    , room_height(1000.0f)
    , use_manual_room_dimensions(false)
    , reference_points(nullptr)
    , display_planes(nullptr)
    , selected_display_plane_idx(-1)
    , selected_ref_point_idx(-1)
    , show_screen_preview(false)
    , show_test_pattern(false)
    , show_room_grid_overlay(false)
    , room_grid_brightness(0.35f)
    , room_grid_point_size(3.0f)
    , room_grid_step(4)
    , room_grid_overlay_last_nx(-1)
    , room_grid_overlay_last_ny(-1)
    , room_grid_overlay_last_nz(-1)
    , room_grid_overlay_use_bounds(false)
    , room_grid_overlay_min_x(0), room_grid_overlay_max_x(0)
    , room_grid_overlay_min_y(0), room_grid_overlay_max_y(0)
    , room_grid_overlay_min_z(0), room_grid_overlay_max_z(0)
    , room_grid_overlay_colors_dirty(true)
    , screen_preview_refresh_timer(nullptr)
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
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

LEDViewport3D::~LEDViewport3D()
{
    if(screen_preview_refresh_timer)
    {
        screen_preview_refresh_timer->stop();
        delete screen_preview_refresh_timer;
        screen_preview_refresh_timer = nullptr;
    }
    makeCurrent();
    for(std::map<std::string, GLuint>::iterator it = display_plane_textures.begin();
        it != display_plane_textures.end();
        ++it)
    {
        glDeleteTextures(1, &it->second);
    }
    display_plane_textures.clear();
    doneCurrent();
}

void LEDViewport3D::SetShowScreenPreview(bool show)
{
    if(show_screen_preview == show)
        return;
    show_screen_preview = show;
    if(show)
    {
        if(!screen_preview_refresh_timer)
        {
            screen_preview_refresh_timer = new QTimer(this);
            connect(screen_preview_refresh_timer, &QTimer::timeout, this, [this]() { update(); });
        }
        screen_preview_refresh_timer->start(33);
    }
    else
    {
        if(screen_preview_refresh_timer)
            screen_preview_refresh_timer->stop();
    }
    update();
}

void LEDViewport3D::SetControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    controller_transforms = transforms;

    update();
}

void LEDViewport3D::SelectController(int index)
{
    if(controller_transforms && index >= 0 && index < (int)controller_transforms->size())
    {
        selected_display_plane_idx = -1;
        selected_controller_idx = index;
        ControllerTransform* ctrl = (*controller_transforms)[index].get();
        if(ctrl && ctrl->hidden_by_virtual)
        {
            return;
        }

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
        selected_display_plane_idx = -1;
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
    if(display_planes && selected_display_plane_idx >= 0 &&
       selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            Transform3D& transform = plane->GetTransform();
            gizmo.SetTarget(plane);
            gizmo.SetPosition(transform.position.x,
                              transform.position.y,
                              transform.position.z);
            return;
        }
    }

    if(reference_points && selected_ref_point_idx >= 0 &&
       selected_ref_point_idx < (int)reference_points->size())
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[selected_ref_point_idx].get();
        if(ref_point)
        {
            Vector3D pos = ref_point->GetPosition();
            gizmo.SetTarget(ref_point);
            gizmo.SetPosition(pos.x, pos.y, pos.z);
            return;
        }
    }

    if(controller_transforms && selected_controller_idx >= 0 &&
       selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl && !ctrl->hidden_by_virtual)
        {
            gizmo.SetTarget(ctrl);
            gizmo.SetPosition(ctrl->transform.position.x,
                              ctrl->transform.position.y,
                              ctrl->transform.position.z);
            return;
        }
    }

    gizmo.SetTarget((DisplayPlane3D*)nullptr);
}

void LEDViewport3D::NotifyControllerTransformChanged()
{
    if(!controller_transforms) return;
    if(selected_controller_idx >= 0 && selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl && !ctrl->hidden_by_virtual)
        {
            ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
        }
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

    if(show_screen_preview && !show_test_pattern)
    {
        UpdateDisplayPlaneTextures();
    }
    else
    {
        ClearDisplayPlaneTextures();
    }

    DrawDisplayPlanes();
    if(show_room_grid_overlay)
    {
        DrawRoomGridOverlay();
    }
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
    bool has_display_plane_selected = (selected_display_plane_idx >= 0 && display_planes &&
                                       selected_display_plane_idx < (int)display_planes->size());

    if(has_controller_selected || has_ref_point_selected || has_display_plane_selected)
    {
        gizmo.SetCameraDistance(camera_distance);
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

    gizmo.SetViewportSize(w, h);
}

void LEDViewport3D::mousePressEvent(QMouseEvent *event)
{
    last_mouse_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    click_start_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));

    float modelview[16];
    float projection[16];
    int viewport[4];

    makeCurrent();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    if(event->button() == Qt::LeftButton)
    {
        bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                        selected_controller_idx < (int)controller_transforms->size());
        bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                       selected_ref_point_idx < (int)reference_points->size());
        bool has_display_plane_selected = (selected_display_plane_idx >= 0 && display_planes &&
                                           selected_display_plane_idx < (int)display_planes->size());

        if(has_controller_selected || has_ref_point_selected || has_display_plane_selected)
        {
            gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
            gizmo.SetCameraDistance(camera_distance);
            if(gizmo.HandleMousePress(event, modelview, projection, viewport))
            {
                update();
                return;
            }
        }

        int picked_ref_point = PickReferencePoint(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
        if(picked_ref_point >= 0)
        {
            selected_controller_indices.clear();
            selected_controller_idx = -1;

            selected_ref_point_idx = picked_ref_point;

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

        int picked_controller = PickController(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
        if(picked_controller >= 0)
        {
            selected_ref_point_idx = -1;
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

        dragging_grab = true;
        return;
    }
    else if(event->button() == Qt::MiddleButton)
    {
        dragging_pan = true;
    }
    else if(event->button() == Qt::RightButton)
    {
        dragging_rotate = true;
    }
}

void LEDViewport3D::mouseMoveEvent(QMouseEvent *event)
{
    QPoint current_pos((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    QPoint delta = current_pos - last_mouse_pos;

    float modelview[16];
    float projection[16];
    int viewport[4];

    makeCurrent();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
    gizmo.SetCameraDistance(camera_distance);

    if(gizmo.HandleMouseMove(event, modelview, projection, viewport))
    {
        if(selected_controller_idx >= 0 && controller_transforms &&
            selected_controller_idx < (int)controller_transforms->size())
        {
            ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
            if(ctrl)
            {
                ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
            }
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
        else if(display_planes && selected_display_plane_idx >= 0 &&
                selected_display_plane_idx < (int)display_planes->size())
        {
            DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                UpdateGizmoPosition();
                emit DisplayPlanePositionChanged(selected_display_plane_idx,
                                                 transform.position.x,
                                                 transform.position.y,
                                                 transform.position.z);
                emit DisplayPlaneRotationChanged(selected_display_plane_idx,
                                                 transform.rotation.x,
                                                 transform.rotation.y,
                                                 transform.rotation.z);
            }
        }

        update();
        last_mouse_pos = current_pos;
        return;
    }
    else
    {
        update();
    }

    if(dragging_grab)
    {
        float grab_sensitivity = 0.003f * camera_distance;

        float yaw_rad = camera_yaw * M_PI / 180.0f;

        float right_x = -sinf(yaw_rad);
        float right_z = cosf(yaw_rad);

        float forward_x = cosf(yaw_rad);
        float forward_z = sinf(yaw_rad);

        camera_target_x += right_x * delta.x() * grab_sensitivity;
        camera_target_z += right_z * delta.x() * grab_sensitivity;

        camera_target_x -= forward_x * delta.y() * grab_sensitivity;
        camera_target_z -= forward_z * delta.y() * grab_sensitivity;

        update();
    }
    else if(dragging_rotate)
    {
        float orbit_sensitivity = 0.3f;
        camera_yaw += delta.x() * orbit_sensitivity;
        camera_pitch += delta.y() * orbit_sensitivity;

        if(camera_pitch > 89.0f) camera_pitch = 89.0f;
        if(camera_pitch < -89.0f) camera_pitch = -89.0f;

        update();
    }
    else if(dragging_pan)
    {
        float pan_sensitivity = 0.002f * camera_distance;

        float yaw_rad = camera_yaw * M_PI / 180.0f;
        float pitch_rad = camera_pitch * M_PI / 180.0f;

        float right_x = -sinf(yaw_rad);
        float right_z = cosf(yaw_rad);

        float up_x = cosf(yaw_rad) * sinf(pitch_rad);
        float up_y = cosf(pitch_rad);
        float up_z = sinf(yaw_rad) * sinf(pitch_rad);

        camera_target_x += right_x * delta.x() * pan_sensitivity;
        camera_target_z += right_z * delta.x() * pan_sensitivity;
        camera_target_x -= up_x * delta.y() * pan_sensitivity;
        camera_target_y -= up_y * delta.y() * pan_sensitivity;
        camera_target_z -= up_z * delta.y() * pan_sensitivity;

        update();
    }

    last_mouse_pos = current_pos;
}

void LEDViewport3D::mouseReleaseEvent(QMouseEvent *event)
{
    if(dragging_grab && event->button() == Qt::LeftButton)
    {
        QPoint delta = event->pos() - click_start_pos;
        float distance = sqrtf(delta.x() * delta.x() + delta.y() * delta.y());

        if(distance < 3.0f)
        {
            int picked_controller = PickController(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
            if(picked_controller >= 0)
            {
                ClearSelection();
                AddControllerToSelection(picked_controller);
                SelectController(picked_controller);
                emit ControllerSelected(picked_controller);
            }
            else
            {
                int picked_plane = PickDisplayPlane(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
                if(picked_plane >= 0)
                {
                    ClearSelection();
                    SelectDisplayPlane(picked_plane);
                }
                else
                {
                    ClearSelection();
                    SelectDisplayPlane(-1);
                    emit ControllerSelected(-1);
                    emit ReferencePointSelected(-1);
                }
            }
        }
    }
    else if(event->button() == Qt::MiddleButton)
    {
        int picked_controller = PickController(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
        if(picked_controller >= 0)
        {
            ClearSelection();
            AddControllerToSelection(picked_controller);
            SelectController(picked_controller);
            emit ControllerSelected(picked_controller);
        }
        else
        {
            int picked_plane = PickDisplayPlane(MOUSE_EVENT_X(event), MOUSE_EVENT_Y(event));
            if(picked_plane >= 0)
            {
                ClearSelection();
                SelectDisplayPlane(picked_plane);
            }
            else
            {
                ClearSelection();
                SelectDisplayPlane(-1);
                emit ControllerSelected(-1);
                emit ReferencePointSelected(-1);
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
    float delta = event->angleDelta().y() / 120.0f;
    float zoom_factor = 1.0f - (delta * 0.1f);

    camera_distance *= zoom_factor;

    if(camera_distance < 1.0f) camera_distance = 1.0f;
    if(camera_distance > 500.0f) camera_distance = 500.0f;

    update();
}

void LEDViewport3D::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
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

GridExtents LEDViewport3D::GetRoomExtents() const
{
    ManualRoomSettings settings = MakeManualRoomSettings(use_manual_room_dimensions,
                                                         room_width,
                                                         room_height,
                                                         room_depth);
    GridDimensionDefaults defaults = MakeGridDefaults(grid_x, grid_y, grid_z);
    return ResolveGridExtents(settings, grid_scale_mm, defaults);
}

void LEDViewport3D::DrawGrid()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glLineWidth(1.0f);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;

    glBegin(GL_LINES);

    for(int i = 0; i <= (int)max_x; i++)
    {
        if(i == 0)
        {
            glColor3f(0.8f, 0.4f, 0.4f);
        }
        else if(i % 5 == 0)
        {
            glColor3f(0.5f, 0.5f, 0.5f);
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f);
        }

        glVertex3f((float)i, 0.0f, 0.0f);
        glVertex3f((float)i, 0.0f, max_z);
    }

    for(int i = 0; i <= (int)max_z; i++)
    {
        if(i == 0)
        {
            glColor3f(0.4f, 0.4f, 0.8f);
        }
        else if(i % 5 == 0)
        {
            glColor3f(0.5f, 0.5f, 0.5f);
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f);
        }

        glVertex3f(0.0f, 0.0f, (float)i);
        glVertex3f(max_x, 0.0f, (float)i);
    }

    glEnd();

    glLineWidth(2.0f);
    glColor3f(0.6f, 0.8f, 0.6f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, max_z);
    glVertex3f(0.0f, 0.0f, max_z);
    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawAxes()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(3.0f);

    glBegin(GL_LINES);

    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(3.0f, 0.0f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 3.0f, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 3.0f);

    glEnd();

    glLineWidth(2.0f);

    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(3.0f, 0.0f, 0.0f);
    glVertex3f(2.7f, 0.15f, 0.0f);
    glVertex3f(2.7f, -0.15f, 0.0f);
    glEnd();

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 3.0f, 0.0f);
    glVertex3f(0.15f, 2.7f, 0.0f);
    glVertex3f(-0.15f, 2.7f, 0.0f);
    glEnd();

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, 3.0f);
    glVertex3f(0.15f, 0.0f, 2.7f);
    glVertex3f(-0.15f, 0.0f, 2.7f);
    glEnd();

    glLineWidth(1.0f);
}

void LEDViewport3D::DrawAxisLabels()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);

    GLdouble modelview[16], projection[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_depth = extents.depth_units;
    const float max_z = extents.height_units;

    double screen_x, screen_y;

    painter.setPen(QColor(255, 100, 100));
    ProjectPointToScreen(max_x, 0.5f, 0.0f, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), QString("Right Wall (X=%1)").arg((int)max_x));
    ProjectPointToScreen(0.3f, 0.5f, 0.0f, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), "Left Wall (X=0)");

    painter.setPen(QColor(100, 255, 100));
    ProjectPointToScreen(0.5f, 0.0f, max_depth, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), QString("Back Wall (Y=%1)").arg((int)max_depth));
    ProjectPointToScreen(0.5f, 0.0f, 0.3f, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), "Front Wall (Y=0)");

    painter.setPen(QColor(100, 100, 255));
    ProjectPointToScreen(0.5f, max_z, 0.0f, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), QString("Ceiling (Z=%1)").arg((int)max_z));
    ProjectPointToScreen(0.5f, 0.2f, 0.0f, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x + 10, screen_y), "Floor (Z=0)");

    painter.setPen(QColor(255, 255, 255));
    ProjectPointToScreen(0.5f, 0.5f, 0.5f, modelview, projection, viewport, screen_x, screen_y);
    painter.drawText(QPointF(screen_x, screen_y), "Origin (0,0,0)\nFront-Left-Floor");

    painter.end();
}

void LEDViewport3D::DrawRoomBoundary()
{
    if(!use_manual_room_dimensions)
    {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    glColor3f(0.0f, 0.8f, 0.8f);

    glBegin(GL_LINES);

    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(max_x, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, 0.0f, max_z);
    glVertex3f(max_x, 0.0f, max_z);     glVertex3f(0.0f, 0.0f, max_z);
    glVertex3f(0.0f, 0.0f, max_z);      glVertex3f(0.0f, 0.0f, 0.0f);

    glVertex3f(0.0f, max_y, 0.0f);      glVertex3f(max_x, max_y, 0.0f);
    glVertex3f(max_x, max_y, 0.0f);     glVertex3f(max_x, max_y, max_z);
    glVertex3f(max_x, max_y, max_z);    glVertex3f(0.0f, max_y, max_z);
    glVertex3f(0.0f, max_y, max_z);     glVertex3f(0.0f, max_y, 0.0f);

    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(0.0f, max_y, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, max_y, 0.0f);
    glVertex3f(max_x, 0.0f, max_z);     glVertex3f(max_x, max_y, max_z);
    glVertex3f(0.0f, 0.0f, max_z);      glVertex3f(0.0f, max_y, max_z);

    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::SetRoomGridOverlayBounds(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z)
{
    room_grid_overlay_use_bounds = true;
    room_grid_overlay_min_x = min_x;
    room_grid_overlay_max_x = max_x;
    room_grid_overlay_min_y = min_y;
    room_grid_overlay_max_y = max_y;
    room_grid_overlay_min_z = min_z;
    room_grid_overlay_max_z = max_z;
}

void LEDViewport3D::ClearRoomGridOverlayBounds()
{
    room_grid_overlay_use_bounds = false;
}

void LEDViewport3D::GetRoomGridOverlayDimensions(int* out_nx, int* out_ny, int* out_nz) const
{
    const int step = std::max(1, room_grid_step);
    int nx = 1, ny = 1, nz = 1;
    if(room_grid_overlay_use_bounds)
    {
        float w = room_grid_overlay_max_x - room_grid_overlay_min_x;
        float h = room_grid_overlay_max_y - room_grid_overlay_min_y;
        float d = room_grid_overlay_max_z - room_grid_overlay_min_z;
        nx = std::max(1, (int)(w / (float)step) + 1);
        ny = std::max(1, (int)(h / (float)step) + 1);
        nz = std::max(1, (int)(d / (float)step) + 1);
    }
    else
    {
        const GridExtents extents = GetRoomExtents();
        nx = (int)(extents.width_units / (float)step) + 1;
        ny = (int)(extents.height_units / (float)step) + 1;
        nz = (int)(extents.depth_units / (float)step) + 1;
    }
    const int max_overlay_points = 35000;
    int count = nx * ny * nz;
    if(count > max_overlay_points)
    {
        float f = powf((float)max_overlay_points / (float)count, 1.0f / 3.0f);
        nx = std::max(1, (int)(nx * f));
        ny = std::max(1, (int)(ny * f));
        nz = std::max(1, (int)(nz * f));
    }
    if(out_nx) *out_nx = nx;
    if(out_ny) *out_ny = ny;
    if(out_nz) *out_nz = nz;
}

void LEDViewport3D::SetRoomGridColorBuffer(const std::vector<RGBColor>& buf)
{
    room_grid_color_buffer = buf;
    room_grid_overlay_colors_dirty = true;
    update();
}

void LEDViewport3D::DrawRoomGridOverlay()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    int nx = 0, ny = 0, nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);
    const int count = nx * ny * nz;
    if(count <= 0) return;

    float min_x, max_x, min_y, max_y, min_z, max_z;
    if(room_grid_overlay_use_bounds)
    {
        min_x = room_grid_overlay_min_x;
        max_x = room_grid_overlay_max_x;
        min_y = room_grid_overlay_min_y;
        max_y = room_grid_overlay_max_y;
        min_z = room_grid_overlay_min_z;
        max_z = room_grid_overlay_max_z;
    }
    else
    {
        const GridExtents extents = GetRoomExtents();
        min_x = 0;
        max_x = extents.width_units;
        min_y = 0;
        max_y = extents.height_units;
        min_z = 0;
        max_z = extents.depth_units;
    }

    const bool use_buffer = ((int)room_grid_color_buffer.size() == count);
    const bool use_callback = (!use_buffer && room_grid_color_callback != nullptr);
    const float default_r = 0.4f * room_grid_brightness;
    const float default_g = 0.6f * room_grid_brightness;
    const float default_b = 0.7f * room_grid_brightness;

    const float span_x = (nx > 1) ? (max_x - min_x) : 0.0f;
    const float span_y = (ny > 1) ? (max_y - min_y) : 0.0f;
    const float span_z = (nz > 1) ? (max_z - min_z) : 0.0f;

    if(room_grid_overlay_positions.size() != (size_t)(3 * count) ||
       room_grid_overlay_last_nx != nx || room_grid_overlay_last_ny != ny || room_grid_overlay_last_nz != nz)
    {
        room_grid_overlay_last_nx = nx;
        room_grid_overlay_last_ny = ny;
        room_grid_overlay_last_nz = nz;
        room_grid_overlay_colors_dirty = true;
        room_grid_overlay_positions.resize(3 * (size_t)count);
        float* pos = room_grid_overlay_positions.data();
        for(int ix = 0; ix < nx; ix++)
        {
            const float x = (nx > 1) ? (min_x + (float)ix / (float)(nx - 1) * span_x) : min_x;
            for(int iy = 0; iy < ny; iy++)
            {
                const float y = (ny > 1) ? (min_y + (float)iy / (float)(ny - 1) * span_y) : min_y;
                for(int iz = 0; iz < nz; iz++)
                {
                    const float z = (nz > 1) ? (min_z + (float)iz / (float)(nz - 1) * span_z) : min_z;
                    *pos++ = x;
                    *pos++ = y;
                    *pos++ = z;
                }
            }
        }
    }

    const bool need_color_refill = room_grid_overlay_colors_dirty || room_grid_overlay_colors.size() != (size_t)(3 * count);
    if(need_color_refill)
    {
        room_grid_overlay_colors_dirty = false;
        room_grid_overlay_colors.resize(3 * (size_t)count);
        float* col = room_grid_overlay_colors.data();
        for(int ix = 0; ix < nx; ix++)
        {
            const float x = (nx > 1) ? (min_x + (float)ix / (float)(nx - 1) * span_x) : min_x;
            for(int iy = 0; iy < ny; iy++)
            {
                const float y = (ny > 1) ? (min_y + (float)iy / (float)(ny - 1) * span_y) : min_y;
                for(int iz = 0; iz < nz; iz++)
                {
                    const float z = (nz > 1) ? (min_z + (float)iz / (float)(nz - 1) * span_z) : min_z;
                    float r, g, b;
                    if(use_buffer)
                    {
                        const size_t idx = (size_t)(ix * ny * nz + iy * nz + iz);
                        RGBColor c = room_grid_color_buffer[idx];
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else if(use_callback)
                    {
                        RGBColor c = room_grid_color_callback(x, y, z);
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else
                    {
                        r = default_r;
                        g = default_g;
                        b = default_b;
                    }
                    *col++ = r;
                    *col++ = g;
                    *col++ = b;
                }
            }
        }
    }

    glPointSize(room_grid_point_size);
    glVertexPointer(3, GL_FLOAT, 0, room_grid_overlay_positions.data());
    glColorPointer(3, GL_FLOAT, 0, room_grid_overlay_colors.data());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDrawArrays(GL_POINTS, 0, count);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawDisplayPlanes()
{
    if(!display_planes) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(size_t plane_index = 0; plane_index < display_planes->size(); plane_index++)
    {
        DisplayPlane3D* plane_ptr = (*display_planes)[plane_index].get();
        if(!plane_ptr || !plane_ptr->IsVisible()) continue;

        float width_units = MMToGridUnits(plane_ptr->GetWidthMM(), grid_scale_mm);
        float height_units = MMToGridUnits(plane_ptr->GetHeightMM(), grid_scale_mm);

        if(width_units <= 0.0f || height_units <= 0.0f) continue;

        float half_w = width_units * 0.5f;
        float half_h = height_units * 0.5f;

        Vector3D local_corners[4] = {
            { -half_w, -half_h, 0.0f },
            {  half_w, -half_h, 0.0f },
            {  half_w,  half_h, 0.0f },
            { -half_w,  half_h, 0.0f }
        };

        Vector3D world_corners[4];
        for(int i = 0; i < 4; ++i)
        {
            world_corners[i] = TransformLocalToWorld(local_corners[i], plane_ptr->GetTransform());
        }

        bool selected = ((int)plane_index == selected_display_plane_idx);
        float fill_color[4]   = { selected ? 0.35f : 0.2f,  selected ? 0.80f : 0.60f, 1.0f, selected ? 0.30f : 0.18f };
        float border_color[4] = { selected ? 0.65f : 0.35f, selected ? 0.90f : 0.70f, 1.0f, selected ? 1.00f : 0.85f };

        bool plane_test_pattern = show_test_pattern;
        
        if(plane_test_pattern)
        {
            Vector3D center;
            center.x = (world_corners[0].x + world_corners[2].x) * 0.5f;
            center.y = (world_corners[0].y + world_corners[2].y) * 0.5f;
            center.z = (world_corners[0].z + world_corners[2].z) * 0.5f;

            Vector3D mid_bottom, mid_top, mid_left, mid_right;
            mid_bottom.x = (world_corners[0].x + world_corners[1].x) * 0.5f;
            mid_bottom.y = (world_corners[0].y + world_corners[1].y) * 0.5f;
            mid_bottom.z = (world_corners[0].z + world_corners[1].z) * 0.5f;

            mid_top.x = (world_corners[2].x + world_corners[3].x) * 0.5f;
            mid_top.y = (world_corners[2].y + world_corners[3].y) * 0.5f;
            mid_top.z = (world_corners[2].z + world_corners[3].z) * 0.5f;

            mid_left.x = (world_corners[0].x + world_corners[3].x) * 0.5f;
            mid_left.y = (world_corners[0].y + world_corners[3].y) * 0.5f;
            mid_left.z = (world_corners[0].z + world_corners[3].z) * 0.5f;

            mid_right.x = (world_corners[1].x + world_corners[2].x) * 0.5f;
            mid_right.y = (world_corners[1].y + world_corners[2].y) * 0.5f;
            mid_right.z = (world_corners[1].z + world_corners[2].z) * 0.5f;

            glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
            glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_left.x, mid_left.y, mid_left.z);
            glEnd();

            glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
            glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
            glVertex3f(mid_right.x, mid_right.y, mid_right.z);
            glVertex3f(center.x, center.y, center.z);
            glEnd();

            glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_right.x, mid_right.y, mid_right.z);
            glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
            glVertex3f(mid_top.x, mid_top.y, mid_top.z);
            glEnd();

            glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(mid_left.x, mid_left.y, mid_left.z);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_top.x, mid_top.y, mid_top.z);
            glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
            glEnd();
        }
        bool plane_screen_preview = show_screen_preview;
        
        if(plane_screen_preview)
        {
            std::string source_id = plane_ptr->GetCaptureSourceId();
            GLuint texture_id = 0;
            bool has_texture = false;

            if(!source_id.empty())
            {
                std::map<std::string, GLuint>::iterator texture_it = display_plane_textures.find(source_id);
                if(texture_it != display_plane_textures.end())
                {
                    texture_id = texture_it->second;
                    has_texture = true;
                }
            }

            if(has_texture)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texture_id);
                glColor4f(1.0f, 1.0f, 1.0f, 0.85f);

                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 1.0f);
                glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
                glTexCoord2f(1.0f, 1.0f);
                glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
                glTexCoord2f(1.0f, 0.0f);
                glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
                glTexCoord2f(0.0f, 0.0f);
                glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
                glEnd();

                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
            }
            else
            {
                Vector3D center;
                center.x = (world_corners[0].x + world_corners[2].x) * 0.5f;
                center.y = (world_corners[0].y + world_corners[2].y) * 0.5f;
                center.z = (world_corners[0].z + world_corners[2].z) * 0.5f;

                Vector3D mid_bottom, mid_top, mid_left, mid_right;
                mid_bottom.x = (world_corners[0].x + world_corners[1].x) * 0.5f;
                mid_bottom.y = (world_corners[0].y + world_corners[1].y) * 0.5f;
                mid_bottom.z = (world_corners[0].z + world_corners[1].z) * 0.5f;

                mid_top.x = (world_corners[2].x + world_corners[3].x) * 0.5f;
                mid_top.y = (world_corners[2].y + world_corners[3].y) * 0.5f;
                mid_top.z = (world_corners[2].z + world_corners[3].z) * 0.5f;

                mid_left.x = (world_corners[0].x + world_corners[3].x) * 0.5f;
                mid_left.y = (world_corners[0].y + world_corners[3].y) * 0.5f;
                mid_left.z = (world_corners[0].z + world_corners[3].z) * 0.5f;

                mid_right.x = (world_corners[1].x + world_corners[2].x) * 0.5f;
                mid_right.y = (world_corners[1].y + world_corners[2].y) * 0.5f;
                mid_right.z = (world_corners[1].z + world_corners[2].z) * 0.5f;

                glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glEnd();

                glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(center.x, center.y, center.z);
                glEnd();

                glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glEnd();

                glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
                glEnd();
            }
        }
        else
        {
            glColor4fv(fill_color);
            glBegin(GL_QUADS);
            for(int i = 0; i < 4; ++i)
            {
                glVertex3f(world_corners[i].x, world_corners[i].y, world_corners[i].z);
            }
            glEnd();
        }

        glColor4fv(border_color);
        glLineWidth(selected ? 3.0f : 2.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i < 4; ++i)
        {
            glVertex3f(world_corners[i].x, world_corners[i].y, world_corners[i].z);
        }
        glEnd();
        glLineWidth(1.0f);

    }

    glDisable(GL_BLEND);
}

void LEDViewport3D::UpdateDisplayPlaneTextures()
{
    if(!display_planes) return;

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
        if(!capture_mgr.IsInitialized()) return;
    }

    for(size_t i = 0; i < display_planes->size(); i++)
    {
        DisplayPlane3D* plane = (*display_planes)[i].get();
        if(!plane || !plane->IsVisible()) continue;

        std::string source_id = plane->GetCaptureSourceId();
        if(source_id.empty()) continue;

        if(!capture_mgr.IsCapturing(source_id))
        {
            capture_mgr.StartCapture(source_id);
        }

        std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(source_id);
        if(!frame || !frame->valid || frame->data.empty()) continue;

        GLuint texture_id = 0;
        std::map<std::string, GLuint>::iterator texture_it = display_plane_textures.find(source_id);
        if(texture_it != display_plane_textures.end())
        {
            texture_id = texture_it->second;
        }
        else
        {
            glGenTextures(1, &texture_id);
            display_plane_textures[source_id] = texture_id;
        }

        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void LEDViewport3D::ClearDisplayPlaneTextures()
{
    for(std::map<std::string, GLuint>::iterator it = display_plane_textures.begin();
        it != display_plane_textures.end(); ++it)
    {
        if(it->second != 0)
        {
            glDeleteTextures(1, &(it->second));
        }
    }
    display_plane_textures.clear();
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
        if(!ctrl || ctrl->hidden_by_virtual) continue;

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
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(3.0f);
        }
        else if(is_selected)
        {
            glColor3f(1.0f, 0.8f, 0.0f);
            glLineWidth(2.0f);
        }
        else
        {
            glColor3f(0.3f, 0.3f, 0.3f);
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

        float top_y = max_bounds.y;
        float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        const float indicator_radius = 0.15f;
        const int sphere_segments = 8;

        glPushMatrix();
        glTranslatef(center_x, top_y, center_z);

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

                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

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
    if(!ctrl || ctrl->hidden_by_virtual) return;
    if(!ctrl->controller && !ctrl->virtual_controller) return;

    glDisable(GL_LIGHTING);
    glPointSize(4.0f);
    glBegin(GL_POINTS);

    if(ctrl->virtual_controller)
    {
        const std::vector<GridLEDMapping>& mappings = ctrl->virtual_controller->GetMappings();
        for(size_t i = 0; i < ctrl->led_positions.size(); i++)
        {
            const LEDPosition3D& pos = ctrl->led_positions[i];
            RGBColor color = pos.preview_color;

            if(i < mappings.size() && mappings[i].controller)
            {
                unsigned int zone_idx = mappings[i].zone_idx;
                unsigned int led_idx = mappings[i].led_idx;
                RGBController* mapping_ctrl = mappings[i].controller;
                if(mapping_ctrl && zone_idx < mapping_ctrl->zones.size())
                {
                    unsigned int global_led_idx = mapping_ctrl->zones[zone_idx].start_idx + led_idx;
                    if(global_led_idx < mapping_ctrl->colors.size())
                    {
                        color = mapping_ctrl->colors[global_led_idx];
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
        for(size_t i = 0; i < ctrl->led_positions.size(); i++)
        {
            const LEDPosition3D& pos = ctrl->led_positions[i];
            RGBColor color = pos.preview_color;

            if(ctrl->controller && pos.zone_idx < ctrl->controller->zones.size())
            {
                unsigned int global_led_idx = ctrl->controller->zones[pos.zone_idx].start_idx + pos.led_idx;
                if(global_led_idx < ctrl->controller->colors.size())
                {
                    color = ctrl->controller->colors[global_led_idx];
                }
            }

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

    float ray_origin[3] = { (float)near_x, (float)near_y, (float)near_z };
    float ray_direction[3] = {
        (float)(far_x - near_x),
        (float)(far_y - near_y),
        (float)(far_z - near_z)
    };

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

        if(world_min.x > world_max.x) { float temp = world_min.x; world_min.x = world_max.x; world_max.x = temp; }
        if(world_min.y > world_max.y) { float temp = world_min.y; world_min.y = world_max.y; world_max.y = temp; }
        if(world_min.z > world_max.z) { float temp = world_min.z; world_min.z = world_max.z; world_max.z = temp; }

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

bool LEDViewport3D::RaySphereIntersect(float ray_origin[3], float ray_direction[3],
                                       const Vector3D& sphere_center, float sphere_radius, float& distance)
{
    float oc[3] = {
        ray_origin[0] - sphere_center.x,
        ray_origin[1] - sphere_center.y,
        ray_origin[2] - sphere_center.z
    };

    float a = ray_direction[0] * ray_direction[0] +
              ray_direction[1] * ray_direction[1] +
              ray_direction[2] * ray_direction[2];

    float b = 2.0f * (oc[0] * ray_direction[0] +
                      oc[1] * ray_direction[1] +
                      oc[2] * ray_direction[2]);

    float c = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - sphere_radius * sphere_radius;

    float discriminant = b * b - 4.0f * a * c;

    if(discriminant < 0.0f)
    {
        return false;
    }

    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

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

    return false;
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

    float ray_origin[3] = { (float)near_x, (float)near_y, (float)near_z };
    float ray_direction[3] = {
        (float)(far_x - near_x),
        (float)(far_y - near_y),
        (float)(far_z - near_z)
    };

    int closest_ref_point = -1;
    float closest_distance = FLT_MAX;
    const float sphere_radius = 0.3f;

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

    float size_x = max_bounds.x - min_bounds.x;
    float size_y = max_bounds.y - min_bounds.y;
    float size_z = max_bounds.z - min_bounds.z;

    float min_dimension = 0.2f;

    if(size_x < 0.001f)
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


int LEDViewport3D::PickDisplayPlane(int mouse_x, int mouse_y)
{
    if(!display_planes) return -1;

    float modelview[16];
    float projection[16];
    int viewport[4];

    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

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

    float ray_origin[3] = { (float)near_x, (float)near_y, (float)near_z };
    float ray_direction[3] = {
        (float)(far_x - near_x),
        (float)(far_y - near_y),
        (float)(far_z - near_z)
    };

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
    int closest_plane = -1;

    for(size_t i = 0; i < display_planes->size(); i++)
    {
        DisplayPlane3D* plane = (*display_planes)[i].get();
        if(!plane || !plane->IsVisible()) continue;

        float width_units = MMToGridUnits(plane->GetWidthMM(), grid_scale_mm);
        float height_units = MMToGridUnits(plane->GetHeightMM(), grid_scale_mm);
        if(width_units <= 0.0f || height_units <= 0.0f) continue;

        float half_w = width_units * 0.5f;
        float half_h = height_units * 0.5f;

        Vector3D local_corners[4] = {
            { -half_w, -half_h, 0.0f },
            {  half_w, -half_h, 0.0f },
            {  half_w,  half_h, 0.0f },
            { -half_w,  half_h, 0.0f }
        };

        Vector3D world_corners[4];
        for(int corner = 0; corner < 4; ++corner)
        {
            world_corners[corner] = TransformLocalToWorld(local_corners[corner], plane->GetTransform());
        }

        Vector3D edge_u = Subtract(world_corners[1], world_corners[0]);
        Vector3D edge_v = Subtract(world_corners[3], world_corners[0]);
        Vector3D normal = CrossVec(edge_u, edge_v);
        float normal_len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if(normal_len < 1e-6f)
        {
            continue;
        }
        normal.x /= normal_len;
        normal.y /= normal_len;
        normal.z /= normal_len;

        float denom = normal.x * ray_direction[0] + normal.y * ray_direction[1] + normal.z * ray_direction[2];
        if(fabsf(denom) < 1e-6f)
        {
            continue;
        }

        Vector3D diff0 = { world_corners[0].x - ray_origin[0],
                           world_corners[0].y - ray_origin[1],
                           world_corners[0].z - ray_origin[2] };
        float t = (diff0.x * normal.x + diff0.y * normal.y + diff0.z * normal.z) / denom;
        if(t < 0.0f)
        {
            continue;
        }

        Vector3D hit = { ray_origin[0] + ray_direction[0] * t,
                         ray_origin[1] + ray_direction[1] * t,
                         ray_origin[2] + ray_direction[2] * t };

        Vector3D hit_vec = Subtract(hit, world_corners[0]);
        float dot00 = DotVec(edge_u, edge_u);
        float dot01 = DotVec(edge_u, edge_v);
        float dot11 = DotVec(edge_v, edge_v);
        float dot20 = DotVec(hit_vec, edge_u);
        float dot21 = DotVec(hit_vec, edge_v);
        float denom_uv = dot00 * dot11 - dot01 * dot01;
        if(fabsf(denom_uv) < 1e-6f)
        {
            continue;
        }

        float u = (dot11 * dot20 - dot01 * dot21) / denom_uv;
        float v = (dot00 * dot21 - dot01 * dot20) / denom_uv;

        if(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
        {
            if(t < closest_distance)
            {
                closest_distance = t;
                closest_plane = (int)i;
            }
        }
    }

    return closest_plane;
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
    selected_controller_idx = index;
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
    selected_display_plane_idx = -1;
    selected_ref_point_idx = -1;
    gizmo.SetTarget((DisplayPlane3D*)nullptr);
}

bool LEDViewport3D::IsControllerSelected(int index) const
{
    return std::find(selected_controller_indices.begin(), selected_controller_indices.end(), index) != selected_controller_indices.end();
}

void LEDViewport3D::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
}

void LEDViewport3D::SetDisplayPlanes(std::vector<std::unique_ptr<DisplayPlane3D>>* planes)
{
    display_planes = planes;
    if(!display_planes)
    {
        selected_display_plane_idx = -1;
    }
    else if(selected_display_plane_idx >= (int)display_planes->size())
    {
        selected_display_plane_idx = -1;
    }

    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::SelectDisplayPlane(int index)
{
    if(display_planes && index >= 0 && index < (int)display_planes->size())
    {
        selected_controller_indices.clear();
        selected_controller_idx = -1;
        selected_ref_point_idx = -1;
        selected_display_plane_idx = index;
        gizmo.SetTarget((*display_planes)[index].get());
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();
    }
    else
    {
        selected_display_plane_idx = -1;

        if(selected_controller_idx < 0 && selected_ref_point_idx < 0)
        {
            gizmo.SetTarget((DisplayPlane3D*)nullptr);
        }
    }

    NotifyDisplayPlaneChanged();
    update();
}

void LEDViewport3D::NotifyDisplayPlaneChanged()
{
    if(display_planes && selected_display_plane_idx >= 0 &&
       selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            Transform3D& transform = plane->GetTransform();
            emit DisplayPlanePositionChanged(selected_display_plane_idx,
                                             transform.position.x,
                                             transform.position.y,
                                             transform.position.z);
            emit DisplayPlaneRotationChanged(selected_display_plane_idx,
                                             transform.rotation.x,
                                             transform.rotation.y,
                                             transform.rotation.z);
        }
    }
    else
    {
        emit DisplayPlanePositionChanged(-1, 0.0f, 0.0f, 0.0f);
        emit DisplayPlaneRotationChanged(-1, 0.0f, 0.0f, 0.0f);
    }

    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::DrawUserFigure()
{
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

            glBegin(GL_LINE_LOOP);
            for(int i = 0; i < segments; i++)
            {
                float angle = 2.0f * M_PI * i / segments;
                float x = head_radius * cosf(angle);
                float y = head_radius * sinf(angle);
                glVertex3f(x, y, 0.0f);
            }
            glEnd();

            glPointSize(6.0f);
            glBegin(GL_POINTS);
            glVertex3f(-0.15f, 0.1f, 0.0f);
            glVertex3f(0.15f, 0.1f, 0.0f);
            glEnd();

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

            if(is_selected)
            {
                glDisable(GL_DEPTH_TEST);
                float box_size = head_radius * 1.5f;
                glColor3f(1.0f, 1.0f, 0.0f);
                glLineWidth(3.0f);

                glBegin(GL_LINES);
                glVertex3f(-box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, -box_size);
                glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, box_size);
                glVertex3f(box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, box_size);
                glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, -box_size);

                glVertex3f(-box_size, box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
                glVertex3f(box_size, box_size, -box_size); glVertex3f(box_size, box_size, box_size);
                glVertex3f(box_size, box_size, box_size); glVertex3f(-box_size, box_size, box_size);
                glVertex3f(-box_size, box_size, box_size); glVertex3f(-box_size, box_size, -box_size);

                glVertex3f(-box_size, -box_size, -box_size); glVertex3f(-box_size, box_size, -box_size);
                glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
                glVertex3f(box_size, -box_size, box_size); glVertex3f(box_size, box_size, box_size);
                glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, box_size, box_size);
                glEnd();

                glEnable(GL_DEPTH_TEST);
            }

            glLineWidth(1.0f);
            glPointSize(1.0f);
            glPopMatrix();

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

        if(is_selected)
        {
            glColor3f(1.0f, 1.0f, 0.0f);
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

        if(is_selected)
        {
            glDisable(GL_DEPTH_TEST);
            float box_size = sphere_radius * 1.5f;
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(3.0f);

            glBegin(GL_LINES);
            glVertex3f(-box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, -box_size);
            glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, box_size);
            glVertex3f(box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, box_size);
            glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, -box_size);

            glVertex3f(-box_size, box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
            glVertex3f(box_size, box_size, -box_size); glVertex3f(box_size, box_size, box_size);
            glVertex3f(box_size, box_size, box_size); glVertex3f(-box_size, box_size, box_size);
            glVertex3f(-box_size, box_size, box_size); glVertex3f(-box_size, box_size, -box_size);

            glVertex3f(-box_size, -box_size, -box_size); glVertex3f(-box_size, box_size, -box_size);
            glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
            glVertex3f(box_size, -box_size, box_size); glVertex3f(box_size, box_size, box_size);
            glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, box_size, box_size);
            glEnd();

            glEnable(GL_DEPTH_TEST);
        }

        glLineWidth(1.0f);
        glPopMatrix();
    }
}


