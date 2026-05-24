// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportMath.h"
#include "LEDPosition3D.h"

#include <QMatrix4x4>
#include <QVector4D>

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
ViewportVec3 Normalize(const ViewportVec3& v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if(len < 1e-8f)
    {
        return {0.0f, 0.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

ViewportVec3 Cross(const ViewportVec3& a, const ViewportVec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Dot(const ViewportVec3& a, const ViewportVec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
} // namespace

namespace ViewportMath
{

ViewportMat4 Identity()
{
    return ViewportMat4{};
}

ViewportMat4 Multiply(const ViewportMat4& a, const ViewportMat4& b)
{
    ViewportMat4 out;
    for(int col = 0; col < 4; col++)
    {
        for(int row = 0; row < 4; row++)
        {
            float sum = 0.0f;
            for(int k = 0; k < 4; k++)
            {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            out.m[col * 4 + row] = sum;
        }
    }
    return out;
}

ViewportMat4 Perspective(float fovy_degrees, float aspect, float near_plane, float far_plane)
{
    ViewportMat4 out;
    std::memset(out.m, 0, sizeof(out.m));

    const float fovy_rad = fovy_degrees * (float)M_PI / 180.0f;
    const float f = 1.0f / std::tan(fovy_rad * 0.5f);
    const float range = near_plane - far_plane;

    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (far_plane + near_plane) / range;
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * far_plane * near_plane) / range;
    return out;
}

ViewportMat4 LookAt(const ViewportVec3& eye, const ViewportVec3& center, const ViewportVec3& up)
{
    const ViewportVec3 f = Normalize({center.x - eye.x, center.y - eye.y, center.z - eye.z});
    ViewportVec3 s = Normalize(Cross(f, up));
    ViewportVec3 u = Cross(s, f);

    ViewportMat4 out = Identity();
    out.m[0] = s.x;
    out.m[4] = s.y;
    out.m[8] = s.z;
    out.m[1] = u.x;
    out.m[5] = u.y;
    out.m[9] = u.z;
    out.m[2] = -f.x;
    out.m[6] = -f.y;
    out.m[10] = -f.z;
    out.m[12] = -Dot(s, eye);
    out.m[13] = -Dot(u, eye);
    out.m[14] = Dot(f, eye);
    return out;
}

ViewportMat4 Translation(float x, float y, float z)
{
    ViewportMat4 out = Identity();
    out.m[12] = x;
    out.m[13] = y;
    out.m[14] = z;
    return out;
}

ViewportMat4 Scale(float x, float y, float z)
{
    ViewportMat4 out = Identity();
    out.m[0] = x;
    out.m[5] = y;
    out.m[10] = z;
    return out;
}

static ViewportMat4 RotationAxis(float degrees, float ax, float ay, float az)
{
    const float rad = degrees * (float)M_PI / 180.0f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const float t = 1.0f - c;

    ViewportMat4 out = Identity();
    out.m[0] = t * ax * ax + c;
    out.m[4] = t * ax * ay + s * az;
    out.m[8] = t * ax * az - s * ay;
    out.m[1] = t * ax * ay - s * az;
    out.m[5] = t * ay * ay + c;
    out.m[9] = t * ay * az + s * ax;
    out.m[2] = t * ax * az + s * ay;
    out.m[6] = t * ay * az - s * ax;
    out.m[10] = t * az * az + c;
    return out;
}

ViewportMat4 RotationX(float degrees)
{
    return RotationAxis(degrees, 1.0f, 0.0f, 0.0f);
}

ViewportMat4 RotationY(float degrees)
{
    return RotationAxis(degrees, 0.0f, 1.0f, 0.0f);
}

ViewportMat4 RotationZ(float degrees)
{
    return RotationAxis(degrees, 0.0f, 0.0f, 1.0f);
}

ViewportMat4 FromTransform3D(const Transform3D& transform)
{
    const ViewportMat4 t = Translation(transform.position.x, transform.position.y, transform.position.z);
    const ViewportMat4 rx = RotationX(transform.rotation.x);
    const ViewportMat4 ry = RotationY(transform.rotation.y);
    const ViewportMat4 rz = RotationZ(transform.rotation.z);
    const ViewportMat4 s = Scale(transform.scale.x, transform.scale.y, transform.scale.z);
    return Multiply(t, Multiply(rx, Multiply(ry, Multiply(rz, s))));
}

ViewportMat4 ModelViewProjection(const ViewportMat4& projection, const ViewportMat4& view, const ViewportMat4& model)
{
    return Multiply(projection, Multiply(view, model));
}

bool ProjectWorldToWindow(const ViewportMat4& modelview,
                          const ViewportMat4& projection,
                          const int viewport[4],
                          float world_x,
                          float world_y,
                          float world_z,
                          float& out_win_x,
                          float& out_win_y,
                          float& out_win_z)
{
    const ViewportMat4 mvp = Multiply(projection, modelview);
    const float in[4] = {world_x, world_y, world_z, 1.0f};
    float clip[4];
    for(int row = 0; row < 4; row++)
    {
        clip[row] = mvp.m[row] * in[0] + mvp.m[4 + row] * in[1] + mvp.m[8 + row] * in[2] + mvp.m[12 + row] * in[3];
    }

    if(std::fabs(clip[3]) < 1e-8f)
    {
        return false;
    }

    const float ndc_x = clip[0] / clip[3];
    const float ndc_y = clip[1] / clip[3];
    const float ndc_z = clip[2] / clip[3];

    out_win_x = viewport[0] + (ndc_x + 1.0f) * 0.5f * (float)viewport[2];
    out_win_y = viewport[1] + (ndc_y + 1.0f) * 0.5f * (float)viewport[3];
    out_win_z = (ndc_z + 1.0f) * 0.5f;
    return true;
}

bool ProjectWorldToScreen(const ViewportMat4& modelview,
                          const ViewportMat4& projection,
                          const int viewport[4],
                          float world_x,
                          float world_y,
                          float world_z,
                          float& out_screen_x,
                          float& out_screen_y)
{
    float win_x = 0.0f;
    float win_y = 0.0f;
    float win_z = 0.0f;
    if(!ProjectWorldToWindow(modelview, projection, viewport, world_x, world_y, world_z, win_x, win_y, win_z))
    {
        return false;
    }
    out_screen_x = win_x;
    out_screen_y = (float)viewport[3] - win_y;
    return true;
}

bool UnprojectWindow(const ViewportMat4& modelview,
                     const ViewportMat4& projection,
                     const int viewport[4],
                     float win_x,
                     float win_y,
                     float win_z,
                     float& out_world_x,
                     float& out_world_y,
                     float& out_world_z)
{
    if(viewport[2] <= 0 || viewport[3] <= 0)
    {
        return false;
    }

    QMatrix4x4 mv(modelview.m);
    QMatrix4x4 proj(projection.m);
    bool invertible = false;
    const QMatrix4x4 inv = (proj * mv).inverted(&invertible);
    if(!invertible)
    {
        return false;
    }

    const float x = (win_x - (float)viewport[0]) / (float)viewport[2] * 2.0f - 1.0f;
    const float y = (win_y - (float)viewport[1]) / (float)viewport[3] * 2.0f - 1.0f;
    const float z = 2.0f * win_z - 1.0f;
    const QVector4D obj = inv * QVector4D(x, y, z, 1.0f);
    if(std::fabs(obj.w()) < 1e-8f)
    {
        return false;
    }

    out_world_x = obj.x() / obj.w();
    out_world_y = obj.y() / obj.w();
    out_world_z = obj.z() / obj.w();
    return true;
}

} // namespace ViewportMath
