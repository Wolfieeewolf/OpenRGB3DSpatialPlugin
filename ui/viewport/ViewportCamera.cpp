// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportCamera.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ViewportCamera
{

ViewportVec3 ComputeEyePosition(const ViewportCameraState& camera)
{
    const float pitch_rad = camera.pitch_degrees * (float)M_PI / 180.0f;
    const float yaw_rad = camera.yaw_degrees * (float)M_PI / 180.0f;

    ViewportVec3 eye;
    eye.x = camera.target_x + camera.distance * std::cos(pitch_rad) * std::cos(yaw_rad);
    eye.y = camera.target_y + camera.distance * std::sin(pitch_rad);
    eye.z = camera.target_z + camera.distance * std::cos(pitch_rad) * std::sin(yaw_rad);
    return eye;
}

ViewportMat4 BuildViewMatrix(const ViewportCameraState& camera)
{
    const ViewportVec3 eye = ComputeEyePosition(camera);
    const ViewportVec3 center = {camera.target_x, camera.target_y, camera.target_z};
    const ViewportVec3 up = {0.0f, 1.0f, 0.0f};
    return ViewportMath::LookAt(eye, center, up);
}

ViewportMat4 BuildProjectionMatrix(int viewport_width, int viewport_height, float fovy_degrees, float near_plane, float far_plane)
{
    int w = viewport_width;
    int h = viewport_height;
    if(h <= 0)
    {
        h = 1;
    }
    if(w <= 0)
    {
        w = 1;
    }
    const float aspect = (float)w / (float)h;
    return ViewportMath::Perspective(fovy_degrees, aspect, near_plane, far_plane);
}

} // namespace ViewportCamera
