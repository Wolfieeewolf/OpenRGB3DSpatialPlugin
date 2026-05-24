// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTCAMERA_H
#define VIEWPORTCAMERA_H

#include "ViewportMath.h"

struct ViewportCameraState
{
    float distance = 20.0f;
    float yaw_degrees = 45.0f;
    float pitch_degrees = 30.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;
};

namespace ViewportCamera
{
ViewportVec3 ComputeEyePosition(const ViewportCameraState& camera);
ViewportMat4 BuildViewMatrix(const ViewportCameraState& camera);
ViewportMat4 BuildProjectionMatrix(int viewport_width, int viewport_height, float fovy_degrees, float near_plane, float far_plane);
}

#endif
