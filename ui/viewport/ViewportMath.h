// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTMATH_H
#define VIEWPORTMATH_H

/** Column-major 4x4 matrices (OpenGL convention). */
struct ViewportMat4
{
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct ViewportVec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ViewportFrameMatrices
{
    ViewportMat4 projection;
    ViewportMat4 view;
    int viewport[4] = {0, 0, 0, 0};
};

struct Transform3D;

namespace ViewportMath
{
ViewportMat4 Identity();
ViewportMat4 Multiply(const ViewportMat4& a, const ViewportMat4& b);

/** Vertical FOV in degrees, OpenGL perspective (right-handed, clip Z). */
ViewportMat4 Perspective(float fovy_degrees, float aspect, float near_plane, float far_plane);
ViewportMat4 LookAt(const ViewportVec3& eye, const ViewportVec3& center, const ViewportVec3& up);
ViewportMat4 Translation(float x, float y, float z);
ViewportMat4 Scale(float x, float y, float z);
ViewportMat4 RotationX(float degrees);
ViewportMat4 RotationY(float degrees);
ViewportMat4 RotationZ(float degrees);
/** Matches LEDViewport3D glTranslate / glRotate(X,Y,Z) / glScale order. */
ViewportMat4 FromTransform3D(const Transform3D& transform);
ViewportMat4 ModelViewProjection(const ViewportMat4& projection, const ViewportMat4& view, const ViewportMat4& model);

/** Window coords: origin bottom-left (matches legacy gluProject). */
bool ProjectWorldToWindow(const ViewportMat4& modelview,
                          const ViewportMat4& projection,
                          const int viewport[4],
                          float world_x,
                          float world_y,
                          float world_z,
                          float& out_win_x,
                          float& out_win_y,
                          float& out_win_z);

/** screen_y = viewport[3] - win_y for top-down overlays (QPainter). */
bool ProjectWorldToScreen(const ViewportMat4& modelview,
                          const ViewportMat4& projection,
                          const int viewport[4],
                          float world_x,
                          float world_y,
                          float world_z,
                          float& out_screen_x,
                          float& out_screen_y);

/** Window coords: origin bottom-left (matches gluUnProject / picking). win_z: 0 = near, 1 = far. */
bool UnprojectWindow(const ViewportMat4& modelview,
                     const ViewportMat4& projection,
                     const int viewport[4],
                     float win_x,
                     float win_y,
                     float win_z,
                     float& out_world_x,
                     float& out_world_y,
                     float& out_world_z);
}

#endif
