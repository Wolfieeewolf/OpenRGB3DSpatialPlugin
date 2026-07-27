// SPDX-License-Identifier: GPL-2.0-only

#include "LEDViewport3D.h"
#include "viewport/ViewportMath.h"
#include "viewport/ViewportGLFormat.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr float kRoomTurntablePitchLimitDeg = 89.0f;
constexpr float kRoomVolumePickPadUnits = 0.02f;
} // namespace

void LEDViewport3D::syncPickMatricesFromFrame(const ViewportFrame& frame)
{
    std::memcpy(pick_projection_, frame.projection.m, sizeof(float) * 16);
    std::memcpy(pick_view_modelview_, frame.view.m, sizeof(float) * 16);
    std::memcpy(pick_scene_modelview_, frame.scene.m, sizeof(float) * 16);
    for(int i = 0; i < 4; ++i)
    {
        pick_viewport_[i] = frame.viewport[i];
    }
    pick_matrices_valid_ = (pick_viewport_[2] > 0 && pick_viewport_[3] > 0);
}

ViewportFrame LEDViewport3D::BuildViewportFrame() const
{
    ViewportFrame frame;
    const int fb_w = viewportFramebufferWidth(width());
    const int fb_h = viewportFramebufferHeight(height());
    frame.viewport[0] = 0;
    frame.viewport[1] = 0;
    frame.viewport[2] = fb_w;
    frame.viewport[3] = fb_h;

    const float aspect = (float)fb_w / (float)std::max(1, fb_h);
    frame.projection = ViewportMath::Perspective(ViewportGLFormat::kDefaultFovyDegrees,
                                                 aspect,
                                                 ViewportGLFormat::kDefaultNearPlane,
                                                 ViewportGLFormat::kDefaultFarPlane);

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
    frame.view = ViewportMath::LookAt(eye, center, up);
    frame.scene = hasRoomPreviewRotation()
        ? ViewportMath::Multiply(frame.view, roomTurntableMatrix())
        : frame.view;
    return frame;
}

void LEDViewport3D::computePickMatricesFallback()
{
    syncPickMatricesFromFrame(BuildViewportFrame());
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

bool LEDViewport3D::buildPickRay(int win_x, int win_y, Ray3D& ray)
{
    float modelview[16];
    float projection[16];
    int viewport[4];
    // Unproject with the same V*T used to draw controllers/planes — ray is in layout space.
    loadScenePickMatrices(modelview, projection, viewport);

    ViewportMat4 mv;
    ViewportMat4 proj;
    std::memcpy(mv.m, modelview, sizeof(float) * 16);
    std::memcpy(proj.m, projection, sizeof(float) * 16);

    float near_x = 0.0f;
    float near_y = 0.0f;
    float near_z = 0.0f;
    float far_x = 0.0f;
    float far_y = 0.0f;
    float far_z = 0.0f;
    if(!ViewportMath::UnprojectWindow(mv, proj, viewport, (float)win_x, (float)win_y, 0.0f,
                                      near_x, near_y, near_z))
    {
        return false;
    }
    if(!ViewportMath::UnprojectWindow(mv, proj, viewport, (float)win_x, (float)win_y, 1.0f,
                                      far_x, far_y, far_z))
    {
        return false;
    }

    ray.origin[0] = near_x;
    ray.origin[1] = near_y;
    ray.origin[2] = near_z;
    ray.direction[0] = far_x - near_x;
    ray.direction[1] = far_y - near_y;
    ray.direction[2] = far_z - near_z;

    const float length = std::sqrt(ray.direction[0] * ray.direction[0] + ray.direction[1] * ray.direction[1] +
                                   ray.direction[2] * ray.direction[2]);
    if(length <= 1e-6f)
    {
        return false;
    }

    ray.direction[0] /= length;
    ray.direction[1] /= length;
    ray.direction[2] /= length;
    return true;
}

bool LEDViewport3D::pickRoomVolume(const Ray3D& ray)
{
    Vector3D box_min{};
    Vector3D box_max{};
    getRoomVolumeAabb(box_min, box_max);

    float distance = 0.0f;
    return RayBoxIntersect(ray, box_min, box_max, distance);
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
    Ray3D ray{};
    if(!buildPickRay(gl_win_x, gl_win_y, ray))
    {
        ClearSelection();
        emit ControllerSelected(-1);
        emit ReferencePointSelected(-1);
        emit DisplayPlaneSelected(-1);
        return;
    }

    const int picked_controller = PickController(ray);
    if(picked_controller >= 0)
    {
        ClearSelection();
        AddControllerToSelection(picked_controller);
        SelectController(picked_controller);
        emit ControllerSelected(picked_controller);
        return;
    }

    const int picked_ref = PickReferencePoint(ray);
    if(picked_ref >= 0)
    {
        ClearSelection();
        SelectReferencePoint(picked_ref);
        emit ReferencePointSelected(picked_ref);
        return;
    }

    const int picked_plane = PickDisplayPlane(ray);
    if(picked_plane >= 0)
    {
        ClearSelection();
        SelectDisplayPlane(picked_plane);
        emit DisplayPlaneSelected(picked_plane);
        return;
    }

    if(pickRoomVolume(ray))
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

    /* Fit the whole room in view instead of a fixed close-up distance. */
    const float span_units = std::max(1.5f,
        std::max(extents.width_units, std::max(extents.height_units, extents.depth_units)) * 1.15f);
    const float fov_rad = 45.0f * (float)M_PI / 180.0f;
    float distance = (span_units * 0.55f) / tanf(fov_rad * 0.5f);
    camera_distance = std::max(2.0f, std::min(50000.0f, distance));

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

void LEDViewport3D::showGizmoModeFeedback()
{
    gizmo_mode_feedback_timer_.start(1200);
    update();
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

