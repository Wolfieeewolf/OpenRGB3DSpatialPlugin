// SPDX-License-Identifier: GPL-2.0-only

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QEvent>
#include <QShortcut>
#include <QMessageBox>

#include <cmath>
#include <cfloat>
#include <cstring>
#include <algorithm>

#include "QtCompat.h"
#include "LEDViewport3D.h"
#include "LEDViewport3D_Internal.h"
#include "ControllerDisplayUtils.h"
#include "viewport/ViewportGLIncludes.h"
#include "viewport/ViewportMath.h"
#include "VirtualReferencePoint3D.h"

#include <GL/glu.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace
{
constexpr float kRoomTurntableSpinSensitivity = 0.25f;
constexpr float kRoomTurntablePitchLimitDeg = 89.0f;
constexpr float kRoomVolumePickPadUnits = 0.02f;
constexpr float kViewportClickSlopGlPx = 3.0f;

void MapQtMouseToGluWindow(const QWidget* widget, const QMouseEvent* event, const GLint vp[4], int& out_win_x, int& out_win_y)
{
    const int qw = std::max(1, widget->width());
    const int qh = std::max(1, widget->height());
    const int vp_w = std::max(1, (int)vp[2]);
    const int vp_h = std::max(1, (int)vp[3]);
    const double scale_x = (double)vp_w / (double)qw;
    const double scale_y = (double)vp_h / (double)qh;
    out_win_x = vp[0] + (int)std::lround((double)MOUSE_EVENT_X(event) * scale_x);
    const int y_from_top = (int)std::lround((double)MOUSE_EVENT_Y(event) * scale_y);
    out_win_y = vp[1] + vp_h - y_from_top;
}

float ViewportQtToGlScaleX(const QWidget* widget, const GLint vp[4])
{
    return (float)vp[2] / (float)std::max(1, widget->width());
}

float ViewportQtToGlScaleY(const QWidget* widget, const GLint vp[4])
{
    return (float)vp[3] / (float)std::max(1, widget->height());
}

float ViewportQtDragDistanceGl(const QWidget* widget, const GLint vp[4], const QPoint& delta)
{
    const float gx = (float)delta.x() * ViewportQtToGlScaleX(widget, vp);
    const float gy = (float)delta.y() * ViewportQtToGlScaleY(widget, vp);
    return sqrtf(gx * gx + gy * gy);
}
} // namespace

void LEDViewport3D::capturePickMatricesFromGl()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, pick_scene_modelview_);
    glGetFloatv(GL_PROJECTION_MATRIX, pick_projection_);
    glGetIntegerv(GL_VIEWPORT, pick_viewport_);
    pick_matrices_valid_ = (pick_viewport_[2] > 0 && pick_viewport_[3] > 0);
}

void LEDViewport3D::computePickMatricesFallback()
{
    const int fb_w = viewportFramebufferWidth(width());
    const int fb_h = viewportFramebufferHeight(height());
    pick_viewport_[0] = 0;
    pick_viewport_[1] = 0;
    pick_viewport_[2] = fb_w;
    pick_viewport_[3] = fb_h;

    const float aspect = (float)fb_w / (float)std::max(1, fb_h);
    const ViewportMat4 proj = ViewportMath::Perspective(45.0f, aspect, 0.1f, 100000.0f);
    std::memcpy(pick_projection_, proj.m, sizeof(float) * 16);

    const float pitch_rad = camera_pitch * (float)M_PI / 180.0f;
    const float yaw_rad = camera_yaw * (float)M_PI / 180.0f;
    const float cos_pitch = std::cos(pitch_rad);
    const ViewportVec3 eye = {
        camera_target_x + camera_distance * cos_pitch * std::cos(yaw_rad),
        camera_target_y + camera_distance * std::sin(pitch_rad),
        camera_target_z + camera_distance * cos_pitch * std::sin(yaw_rad),
    };
    const ViewportVec3 center = {camera_target_x, camera_target_y, camera_target_z};
    const ViewportVec3 up = {0.0f, 1.0f, 0.0f};
    const ViewportMat4 view = ViewportMath::LookAt(eye, center, up);
    std::memcpy(pick_view_modelview_, view.m, sizeof(float) * 16);

    ViewportMat4 scene = view;
    if(hasRoomPreviewRotation())
    {
        scene = ViewportMath::Multiply(view, roomTurntableMatrix());
    }
    std::memcpy(pick_scene_modelview_, scene.m, sizeof(float) * 16);
    pick_matrices_valid_ = true;
}

void LEDViewport3D::loadPickMatrices(float modelview[16], float projection[16], int viewport[4])
{
    if(!pick_matrices_valid_)
    {
        computePickMatricesFallback();
    }
    std::memcpy(modelview, pick_view_modelview_, sizeof(float) * 16);
    std::memcpy(projection, pick_projection_, sizeof(float) * 16);
    std::memcpy(viewport, pick_viewport_, sizeof(int) * 4);
}

void LEDViewport3D::loadScenePickMatrices(float modelview[16], float projection[16], int viewport[4])
{
    if(!pick_matrices_valid_)
    {
        computePickMatricesFallback();
    }
    std::memcpy(modelview, pick_scene_modelview_, sizeof(float) * 16);
    std::memcpy(projection, pick_projection_, sizeof(float) * 16);
    std::memcpy(viewport, pick_viewport_, sizeof(int) * 4);
}

void LEDViewport3D::DefaultCamera(float& distance, float& yaw, float& pitch,
                                  float& target_x, float& target_y, float& target_z)
{
    distance = 20.0f;
    yaw = 45.0f;
    pitch = 30.0f;
    target_x = 0.0f;
    target_y = 0.0f;
    target_z = 0.0f;
}

void LEDViewport3D::getRoomTurntablePivot(float& pivot_x, float& pivot_y, float& pivot_z) const
{
    const GridExtents extents = GetRoomExtents();
    pivot_x = extents.width_units * 0.5f;
    pivot_y = extents.height_units * 0.5f;
    pivot_z = extents.depth_units * 0.5f;
}

void LEDViewport3D::getRoomVolumeAabb(Vector3D& box_min, Vector3D& box_max) const
{
    const GridExtents extents = GetRoomExtents();
    box_min.x = -kRoomVolumePickPadUnits;
    box_min.y = -kRoomVolumePickPadUnits;
    box_min.z = -kRoomVolumePickPadUnits;
    box_max.x = extents.width_units + kRoomVolumePickPadUnits;
    box_max.y = extents.height_units + kRoomVolumePickPadUnits;
    box_max.z = extents.depth_units + kRoomVolumePickPadUnits;
}

bool LEDViewport3D::hasRoomPreviewRotation() const
{
    return std::fabs(room_turntable_yaw_deg) > 1e-4f || std::fabs(room_turntable_pitch_deg) > 1e-4f;
}

ViewportMat4 LEDViewport3D::roomTurntableMatrix() const
{
    if(!hasRoomPreviewRotation())
    {
        return ViewportMath::Identity();
    }

    float pivot_x = 0.0f;
    float pivot_y = 0.0f;
    float pivot_z = 0.0f;
    getRoomTurntablePivot(pivot_x, pivot_y, pivot_z);
    using namespace ViewportMath;
    return Multiply(Translation(pivot_x, pivot_y, pivot_z),
                    Multiply(RotationY(room_turntable_yaw_deg),
                             Multiply(RotationX(room_turntable_pitch_deg), Translation(-pivot_x, -pivot_y, -pivot_z))));
}

void LEDViewport3D::transformPickRay(float ray_origin[3], float ray_direction[3]) const
{
    (void)ray_origin;
    (void)ray_direction;
    // Picking unprojects with view*turntable (same as draw), so the ray is already
    // in layout space. Kept as a no-op for ABI/call-site stability.
}

bool LEDViewport3D::buildPickRay(int win_x, int win_y, float ray_origin[3], float ray_direction[3])
{
    float modelview[16];
    float projection[16];
    int viewport[4];
    // Unproject with the same V*T used to draw controllers/planes — ray is in layout space.
    loadScenePickMatrices(modelview, projection, viewport);

    GLdouble near_x = 0.0;
    GLdouble near_y = 0.0;
    GLdouble near_z = 0.0;
    GLdouble far_x = 0.0;
    GLdouble far_y = 0.0;
    GLdouble far_z = 0.0;

    GLdouble mv[16];
    GLdouble proj[16];
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

    if(gluUnProject((GLdouble)win_x, (GLdouble)win_y, 0.0, mv, proj, vp, &near_x, &near_y, &near_z) == GL_FALSE)
    {
        return false;
    }
    if(gluUnProject((GLdouble)win_x, (GLdouble)win_y, 1.0, mv, proj, vp, &far_x, &far_y, &far_z) == GL_FALSE)
    {
        return false;
    }

    ray_origin[0] = (float)near_x;
    ray_origin[1] = (float)near_y;
    ray_origin[2] = (float)near_z;
    ray_direction[0] = (float)(far_x - near_x);
    ray_direction[1] = (float)(far_y - near_y);
    ray_direction[2] = (float)(far_z - near_z);

    const float length = std::sqrt(ray_direction[0] * ray_direction[0] + ray_direction[1] * ray_direction[1] +
                                   ray_direction[2] * ray_direction[2]);
    if(length <= 1e-6f)
    {
        return false;
    }

    ray_direction[0] /= length;
    ray_direction[1] /= length;
    ray_direction[2] /= length;
    return true;
}

bool LEDViewport3D::pickRoomVolume(int win_x, int win_y)
{
    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return false;
    }

    Vector3D box_min{};
    Vector3D box_max{};
    getRoomVolumeAabb(box_min, box_max);

    float distance = 0.0f;
    return RayBoxIntersect(ray_origin, ray_direction, box_min, box_max, distance);
}

void LEDViewport3D::clearSceneObjectSelection()
{
    selected_controller_indices.clear();
    selected_controller_idx = -1;
    selected_display_plane_idx = -1;
    selected_ref_point_idx = -1;
    gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
}

void LEDViewport3D::selectRoomViewport()
{
    clearSceneObjectSelection();
    if(room_viewport_selected_)
    {
        return;
    }

    room_viewport_selected_ = true;
    emit RoomViewportSelected(true);
    emit ControllerSelected(-1);
    emit ReferencePointSelected(-1);
    emit DisplayPlaneSelected(-1);
    update();
}

void LEDViewport3D::deselectRoomViewport()
{
    if(!room_viewport_selected_)
    {
        return;
    }
    room_viewport_selected_ = false;
    emit RoomViewportSelected(false);
    update();
}

void LEDViewport3D::pushRoomTurntableGl() const
{
    if(!hasRoomPreviewRotation())
    {
        return;
    }

    float pivot_x = 0.0f;
    float pivot_y = 0.0f;
    float pivot_z = 0.0f;
    getRoomTurntablePivot(pivot_x, pivot_y, pivot_z);
    glTranslatef(pivot_x, pivot_y, pivot_z);
    glRotatef(room_turntable_yaw_deg, 0.0f, 1.0f, 0.0f);
    glRotatef(room_turntable_pitch_deg, 1.0f, 0.0f, 0.0f);
    glTranslatef(-pivot_x, -pivot_y, -pivot_z);
}

void LEDViewport3D::multiplyModelviewByRoomTurntable(float modelview[16]) const
{
    if(!hasRoomPreviewRotation())
    {
        return;
    }

    ViewportMat4 view{};
    std::memcpy(view.m, modelview, sizeof(view.m));
    const ViewportMat4 combined = ViewportMath::Multiply(view, roomTurntableMatrix());
    std::memcpy(modelview, combined.m, sizeof(combined.m));
}

void LEDViewport3D::resetRoomPreviewSpin()
{
    room_turntable_yaw_deg = 0.0f;
    room_turntable_pitch_deg = 0.0f;
}

void LEDViewport3D::clampRoomTurntablePitch()
{
    if(room_turntable_pitch_deg > kRoomTurntablePitchLimitDeg)
    {
        room_turntable_pitch_deg = kRoomTurntablePitchLimitDeg;
    }
    if(room_turntable_pitch_deg < -kRoomTurntablePitchLimitDeg)
    {
        room_turntable_pitch_deg = -kRoomTurntablePitchLimitDeg;
    }
}

void LEDViewport3D::applyViewportClickPick(int gl_win_x, int gl_win_y)
{
    const int picked_controller = PickController(gl_win_x, gl_win_y);
    if(picked_controller >= 0)
    {
        ClearSelection();
        AddControllerToSelection(picked_controller);
        SelectController(picked_controller);
        emit ControllerSelected(picked_controller);
        return;
    }

    const int picked_ref = PickReferencePoint(gl_win_x, gl_win_y);
    if(picked_ref >= 0)
    {
        ClearSelection();
        SelectReferencePoint(picked_ref);
        emit ReferencePointSelected(picked_ref);
        return;
    }

    const int picked_plane = PickDisplayPlane(gl_win_x, gl_win_y);
    if(picked_plane >= 0)
    {
        ClearSelection();
        SelectDisplayPlane(picked_plane);
        emit DisplayPlaneSelected(picked_plane);
        return;
    }

    if(pickRoomVolume(gl_win_x, gl_win_y))
    {
        selectRoomViewport();
        return;
    }

    ClearSelection();
    emit ControllerSelected(-1);
    emit ReferencePointSelected(-1);
    emit DisplayPlaneSelected(-1);
}

void LEDViewport3D::ResetCameraToDefault()
{
    DefaultCamera(camera_distance, camera_yaw, camera_pitch,
                  camera_target_x, camera_target_y, camera_target_z);
    const GridExtents extents = GetRoomExtents();
    camera_target_x = extents.width_units * 0.5f;
    camera_target_y = extents.height_units * 0.5f;
    camera_target_z = extents.depth_units * 0.5f;
    resetRoomPreviewSpin();
    deselectRoomViewport();
    pick_matrices_valid_ = false;
    update();
}

void LEDViewport3D::clearCameraDragState()
{
    dragging_rotate = false;
    dragging_pan = false;
    dragging_grab = false;
    dragging_room_turntable = false;
}

void LEDViewport3D::retargetOrbitToRoomCenterPreservingEye()
{
    const float pitch_rad = camera_pitch * (float)M_PI / 180.0f;
    const float yaw_rad = camera_yaw * (float)M_PI / 180.0f;
    const float cos_pitch = std::cos(pitch_rad);
    const float eye_x = camera_target_x + camera_distance * cos_pitch * std::cos(yaw_rad);
    const float eye_y = camera_target_y + camera_distance * std::sin(pitch_rad);
    const float eye_z = camera_target_z + camera_distance * cos_pitch * std::sin(yaw_rad);

    const GridExtents extents = GetRoomExtents();
    camera_target_x = extents.width_units * 0.5f;
    camera_target_y = extents.height_units * 0.5f;
    camera_target_z = extents.depth_units * 0.5f;

    const float dx = eye_x - camera_target_x;
    const float dy = eye_y - camera_target_y;
    const float dz = eye_z - camera_target_z;
    camera_distance = std::max(1.0f, std::sqrt(dx * dx + dy * dy + dz * dz));
    camera_pitch = std::asin(std::clamp(dy / camera_distance, -1.0f, 1.0f)) * 180.0f / (float)M_PI;
    camera_yaw = std::atan2(dz, dx) * 180.0f / (float)M_PI;
    pick_matrices_valid_ = false;
}

void LEDViewport3D::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    last_mouse_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    click_start_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));

    float modelview[16];
    float projection[16];
    int viewport[4];
    loadScenePickMatrices(modelview, projection, viewport);

    int gl_win_x = 0;
    int gl_win_y = 0;
    MapQtMouseToGluWindow(this, event, reinterpret_cast<const GLint*>(viewport), gl_win_x, gl_win_y);

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
            if(gizmo.HandleMousePress(event, gl_win_x, gl_win_y, modelview, projection, viewport))
            {
                update();
                return;
            }
        }

        int picked_ref_point = PickReferencePoint(gl_win_x, gl_win_y);
        if(picked_ref_point >= 0)
        {
            ClearSelection();
            SelectReferencePoint(picked_ref_point);
            emit ReferencePointSelected(picked_ref_point);
            update();
            return;
        }

        int picked_controller = PickController(gl_win_x, gl_win_y);
        if(picked_controller >= 0)
        {
            if(event->modifiers() & Qt::ControlModifier)
            {
                deselectRoomViewport();
                selected_ref_point_idx = -1;
                selected_display_plane_idx = -1;
                if(IsControllerSelected(picked_controller))
                {
                    RemoveControllerFromSelection(picked_controller);
                    if(selected_controller_idx >= 0 && controller_transforms &&
                       selected_controller_idx < (int)controller_transforms->size())
                    {
                        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
                        gizmo.SetTarget(ctrl);
                        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
                        UpdateGizmoPosition();
                    }
                    else
                    {
                        gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
                    }
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

            ClearSelection();
            AddControllerToSelection(picked_controller);
            SelectController(picked_controller);
            emit ControllerSelected(picked_controller);
            update();
            return;
        }

        // Display planes must be picked on press (same as controllers). Otherwise a miss on the
        // thin gizmo falls through to the room AABB, clears the plane, and starts turntable spin.
        const int picked_plane = PickDisplayPlane(gl_win_x, gl_win_y);
        if(picked_plane >= 0)
        {
            ClearSelection();
            SelectDisplayPlane(picked_plane);
            emit DisplayPlaneSelected(picked_plane);
            update();
            return;
        }

        dragging_grab = false;
        dragging_pan = false;
        dragging_rotate = false;
        if(room_viewport_selected_)
        {
            // Only spin when the click is on the room volume; otherwise treat as grab so
            // release can still click-pick controllers/planes consistently.
            if(pickRoomVolume(gl_win_x, gl_win_y))
            {
                dragging_room_turntable = true;
            }
            else
            {
                dragging_grab = true;
            }
            return;
        }
        if(pickRoomVolume(gl_win_x, gl_win_y))
        {
            selectRoomViewport();
            dragging_room_turntable = true;
            return;
        }

        dragging_grab = true;
        return;
    }
    else if(event->button() == Qt::MiddleButton)
    {
        dragging_grab = false;
        dragging_pan = true;
        dragging_rotate = false;
    }
    else if(event->button() == Qt::RightButton)
    {
        // Orbit around the live room center. Preserve the current eye position so
        // beginning a right-drag does not cause a visible camera jump.
        retargetOrbitToRoomCenterPreservingEye();
        dragging_grab = false;
        dragging_pan = false;
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
    // Camera view for pan axes; scene (V*T) for gizmo hover/drag.
    loadPickMatrices(modelview, projection, viewport);

    const float delta_x_gl = (float)delta.x() * ViewportQtToGlScaleX(this, reinterpret_cast<const GLint*>(viewport));
    const float delta_y_gl = (float)delta.y() * ViewportQtToGlScaleY(this, reinterpret_cast<const GLint*>(viewport));

    int gl_win_x = 0;
    int gl_win_y = 0;
    MapQtMouseToGluWindow(this, event, reinterpret_cast<const GLint*>(viewport), gl_win_x, gl_win_y);

    gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
    gizmo.SetCameraDistance(camera_distance);

    const bool left_held = (event->buttons() & Qt::LeftButton) != 0;
    float gizmo_modelview[16];
    float gizmo_projection[16];
    int gizmo_viewport[4];
    loadScenePickMatrices(gizmo_modelview, gizmo_projection, gizmo_viewport);
    if(gizmo.IsActive() && gizmo.HandleMouseMove(event, gl_win_x, gl_win_y, gizmo_modelview, gizmo_projection, gizmo_viewport))
    {
        if(gizmo.IsDragging())
        {
            if(selected_controller_idx >= 0 && controller_transforms &&
               selected_controller_idx < (int)controller_transforms->size())
            {
                ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
                if(ctrl)
                {
                    ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
                    ControllerLayout3D::UpdateWorldPositions(ctrl);
                }
            }
        }

        last_gizmo_hover_axis = gizmo.GetHoverAxis();
        update();
        last_mouse_pos = current_pos;
        return;
    }

    if(gizmo.IsActive())
    {
        const GizmoAxis hover = gizmo.GetHoverAxis();
        if(hover != last_gizmo_hover_axis)
        {
            last_gizmo_hover_axis = hover;
            update();
            last_mouse_pos = current_pos;
            return;
        }
    }

    if(dragging_room_turntable && left_held)
    {
        room_turntable_yaw_deg += delta_x_gl * kRoomTurntableSpinSensitivity;
        room_turntable_pitch_deg += delta_y_gl * kRoomTurntableSpinSensitivity;
        clampRoomTurntablePitch();
        pick_matrices_valid_ = false;
        update();
    }
    else if(dragging_grab && left_held)
    {
        float grab_sensitivity = 0.003f * camera_distance;

        float yaw_rad = camera_yaw * M_PI / 180.0f;

        float right_x = -sinf(yaw_rad);
        float right_z = cosf(yaw_rad);

        float forward_x = cosf(yaw_rad);
        float forward_z = sinf(yaw_rad);

        camera_target_x -= right_x * delta_x_gl * grab_sensitivity;
        camera_target_z -= right_z * delta_x_gl * grab_sensitivity;

        camera_target_x += forward_x * delta_y_gl * grab_sensitivity;
        camera_target_z += forward_z * delta_y_gl * grab_sensitivity;

        pick_matrices_valid_ = false;
        update();
    }
    else if(dragging_rotate && (event->buttons() & Qt::RightButton))
    {
        float orbit_sensitivity = 0.3f;
        camera_yaw += delta_x_gl * orbit_sensitivity;
        camera_pitch += delta_y_gl * orbit_sensitivity;

        if(camera_pitch > 89.0f) camera_pitch = 89.0f;
        if(camera_pitch < -89.0f) camera_pitch = -89.0f;

        pick_matrices_valid_ = false;
        update();
    }
    else if(dragging_pan && (event->buttons() & Qt::MiddleButton))
    {
        float pan_sensitivity = 0.002f * camera_distance;

        const float right_x = modelview[0];
        const float right_y = modelview[4];
        const float right_z = modelview[8];
        const float up_x = modelview[1];
        const float up_y = modelview[5];
        const float up_z = modelview[9];

        camera_target_x += right_x * delta_x_gl * pan_sensitivity;
        camera_target_y += right_y * delta_x_gl * pan_sensitivity;
        camera_target_z += right_z * delta_x_gl * pan_sensitivity;
        camera_target_x += up_x * delta_y_gl * pan_sensitivity;
        camera_target_y += up_y * delta_y_gl * pan_sensitivity;
        camera_target_z += up_z * delta_y_gl * pan_sensitivity;

        pick_matrices_valid_ = false;
        update();
    }
    else
    {
        clearCameraDragState();
    }

    last_mouse_pos = current_pos;
}

void LEDViewport3D::mouseReleaseEvent(QMouseEvent *event)
{
    float modelview[16];
    float projection[16];
    int viewport[4];
    loadScenePickMatrices(modelview, projection, viewport);
    (void)modelview;
    (void)projection;
    int gl_win_x = 0;
    int gl_win_y = 0;
    MapQtMouseToGluWindow(this, event, reinterpret_cast<const GLint*>(viewport), gl_win_x, gl_win_y);

    if(dragging_grab && event->button() == Qt::LeftButton)
    {
        const QPoint delta = MOUSE_EVENT_POS(event) - click_start_pos;
        const float distance = ViewportQtDragDistanceGl(this, reinterpret_cast<const GLint*>(viewport), delta);

        if(distance < kViewportClickSlopGlPx)
        {
            applyViewportClickPick(gl_win_x, gl_win_y);
        }
    }
    // Middle button is pan-only (no click-pick); left click owns selection.

    const bool gizmo_was_dragging = gizmo.IsDragging();
    gizmo.HandleMouseRelease(event);
    if(gizmo_was_dragging)
    {
        emitGizmoDragCompleted();
    }
    if(dragging_room_turntable && event->button() == Qt::LeftButton)
    {
        const QPoint delta = MOUSE_EVENT_POS(event) - click_start_pos;
        const float distance = ViewportQtDragDistanceGl(this, reinterpret_cast<const GLint*>(viewport), delta);
        if(distance < kViewportClickSlopGlPx)
        {
            // Click (not drag): full pick so controllers inside the spun room still win.
            applyViewportClickPick(gl_win_x, gl_win_y);
        }
    }
    if(event->button() == Qt::LeftButton)
    {
        dragging_grab = false;
        dragging_room_turntable = false;
    }
    else if(event->button() == Qt::MiddleButton)
    {
        dragging_pan = false;
    }
    else if(event->button() == Qt::RightButton)
    {
        dragging_rotate = false;
    }
    update();
}

void LEDViewport3D::leaveEvent(QEvent *event)
{
    QOpenGLWidget::leaveEvent(event);
    clearCameraDragState();
}

void LEDViewport3D::wheelEvent(QWheelEvent *event)
{
    float delta = 0.0f;
    const QPoint pixel_delta = event->pixelDelta();
    if(pixel_delta.y() != 0)
    {
        delta = (float)pixel_delta.y() / 120.0f;
    }
    else
    {
        delta = event->angleDelta().y() / 120.0f;
    }

    float zoom_factor = 1.0f - (delta * 0.1f);

    camera_distance *= zoom_factor;

    if(camera_distance < 1.0f) camera_distance = 1.0f;
    if(camera_distance > 50000.0f) camera_distance = 50000.0f;

    pick_matrices_valid_ = false;
    update();
}

void LEDViewport3D::applyGizmoMoveMode()
{
    gizmo.SetMode(GIZMO_MODE_MOVE);
    update();
}

void LEDViewport3D::applyGizmoRotateMode()
{
    gizmo.SetMode(GIZMO_MODE_ROTATE);
    update();
}

void LEDViewport3D::applyGizmoFreeroamMode()
{
    gizmo.SetMode(GIZMO_MODE_FREEROAM);
    update();
}

void LEDViewport3D::toggleGridSnapFromKeyboard()
{
    grid_snap_enabled = !grid_snap_enabled;
    gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
    update();
}

void LEDViewport3D::tryDeleteSelectedFromKeyboard()
{
    if(selected_controller_idx < 0 || !controller_transforms ||
       selected_controller_idx >= (int)controller_transforms->size())
    {
        return;
    }

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
    QString controller_name = QStringLiteral("Unknown");

    if(ctrl && ctrl->controller)
    {
        controller_name = ControllerDisplay::FormatRgbControllerTitle(ctrl->controller);
    }
    else if(ctrl && ctrl->virtual_controller)
    {
        controller_name = QString::fromStdString(ctrl->virtual_controller->GetName());
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Delete Controller"),
        QStringLiteral("Are you sure you want to remove '%1' from the 3D view?").arg(controller_name),
        QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        emit ControllerDeleteRequested(selected_controller_idx);
    }
}

void LEDViewport3D::installViewportKeyboardShortcuts(QWidget* shortcut_scope)
{
    if(viewport_keyboard_shortcuts_installed_)
    {
        return;
    }

    QWidget* scope = shortcut_scope ? shortcut_scope : parentWidget();
    if(!scope)
    {
        return;
    }

    const auto add_key = [&](Qt::Key key, void (LEDViewport3D::*handler)()) {
        QShortcut* shortcut = new QShortcut(QKeySequence(key), scope);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        shortcut->setAutoRepeat(false);
        connect(shortcut, &QShortcut::activated, this, handler);
    };

    add_key(Qt::Key_W, &LEDViewport3D::applyGizmoMoveMode);
    add_key(Qt::Key_E, &LEDViewport3D::applyGizmoRotateMode);
    add_key(Qt::Key_R, &LEDViewport3D::applyGizmoFreeroamMode);
    add_key(Qt::Key_Q, &LEDViewport3D::toggleGridSnapFromKeyboard);
    add_key(Qt::Key_Delete, &LEDViewport3D::tryDeleteSelectedFromKeyboard);
    add_key(Qt::Key_Backspace, &LEDViewport3D::tryDeleteSelectedFromKeyboard);
    add_key(Qt::Key_F, &LEDViewport3D::focusSelectionFromKeyboard);
    add_key(Qt::Key_Home, &LEDViewport3D::resetCameraFromKeyboard);

    viewport_keyboard_shortcuts_installed_ = true;
}

void LEDViewport3D::focusSelectionFromKeyboard()
{
    FocusSelectionInView();
}

void LEDViewport3D::resetCameraFromKeyboard()
{
    ResetCameraToDefault();
}

void LEDViewport3D::keyPressEvent(QKeyEvent *event)
{
    // W/E/R/Q/F/Home/Delete/Backspace are handled by installViewportKeyboardShortcuts
    // (WidgetWithChildrenShortcut) — do not duplicate them here.
    if(event->key() == Qt::Key_Escape)
    {
        ClearSelection();
        emit ControllerSelected(-1);
        emit ReferencePointSelected(-1);
        emit DisplayPlaneSelected(-1);
        event->accept();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

void LEDViewport3D::focusInEvent(QFocusEvent *event)
{
    QOpenGLWidget::focusInEvent(event);
}

int LEDViewport3D::PickController(int win_x, int win_y)
{
    if(!controller_transforms) return -1;

    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return -1;
    }

    float closest_distance = FLT_MAX;
    int closest_controller = -1;

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl) continue;
        if(ctrl->hidden_by_virtual) continue;

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f
        };

        Vector3D local_corners[8] = {
            {min_bounds.x + center_offset.x, min_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, min_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {min_bounds.x + center_offset.x, max_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, max_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {min_bounds.x + center_offset.x, min_bounds.y + center_offset.y, max_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, min_bounds.y + center_offset.y, max_bounds.z + center_offset.z},
            {min_bounds.x + center_offset.x, max_bounds.y + center_offset.y, max_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, max_bounds.y + center_offset.y, max_bounds.z + center_offset.z}
        };

        Vector3D world_min = TransformLocalToWorld(local_corners[0], ctrl->transform);
        Vector3D world_max = world_min;
        for(int corner = 1; corner < 8; corner++)
        {
            Vector3D world_corner = TransformLocalToWorld(local_corners[corner], ctrl->transform);
            if(world_corner.x < world_min.x) world_min.x = world_corner.x;
            if(world_corner.y < world_min.y) world_min.y = world_corner.y;
            if(world_corner.z < world_min.z) world_min.z = world_corner.z;
            if(world_corner.x > world_max.x) world_max.x = world_corner.x;
            if(world_corner.y > world_max.y) world_max.y = world_corner.y;
            if(world_corner.z > world_max.z) world_max.z = world_corner.z;
        }

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
    float tmax = FLT_MAX;

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

int LEDViewport3D::PickReferencePoint(int win_x, int win_y)
{
    if(!reference_points) return -1;

    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return -1;
    }

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

int LEDViewport3D::PickDisplayPlane(int win_x, int win_y)
{
    if(!display_planes) return -1;

    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return -1;
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

