// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALBASISUTILS_H
#define SPATIALBASISUTILS_H

#include <cmath>

namespace SpatialBasisUtils
{

struct BasisVectors
{
    float right_x = 1.0f;
    float right_y = 0.0f;
    float right_z = 0.0f;
    float up_x = 0.0f;
    float up_y = 1.0f;
    float up_z = 0.0f;
    float forward_x = 0.0f;
    float forward_y = 0.0f;
    float forward_z = 1.0f;
};

static inline void Normalize3(float& x, float& y, float& z, float fallback_x, float fallback_y, float fallback_z)
{
    const float l = std::sqrt(x * x + y * y + z * z);
    if(l <= 1e-6f)
    {
        x = fallback_x;
        y = fallback_y;
        z = fallback_z;
    }
    else
    {
        x /= l;
        y /= l;
        z /= l;
    }
}

/** Yaw-only room basis (pitch stripped). Matches Minecraft mod RoomSampleWorldMapper. */
static inline BasisVectors BuildHorizontalBasis(float look_x,
                                                float look_y,
                                                float look_z,
                                                float heading_offset_deg)
{
    float lx = look_x;
    float ly = look_y;
    float lz = look_z;
    Normalize3(lx, ly, lz, 0.0f, 0.0f, 1.0f);

    constexpr float ux = 0.0f;
    constexpr float uy = 1.0f;
    constexpr float uz = 0.0f;
    const float horiz = lx * ux + ly * uy + lz * uz;
    float fx = lx - horiz * ux;
    float fy = ly - horiz * uy;
    float fz = lz - horiz * uz;
    Normalize3(fx, fy, fz, 0.0f, 0.0f, 1.0f);

    float rx = fy * uz - fz * uy;
    float ry = fz * ux - fx * uz;
    float rz = fx * uy - fy * ux;
    Normalize3(rx, ry, rz, 1.0f, 0.0f, 0.0f);

    const float yaw = heading_offset_deg * 0.01745329251f;
    if(std::fabs(yaw) > 1e-5f)
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        const float fx2 = fx * c + rx * s;
        const float fy2 = fy * c + ry * s;
        const float fz2 = fz * c + rz * s;
        const float rx2 = rx * c - fx * s;
        const float ry2 = ry * c - fy * s;
        const float rz2 = rz * c - fz * s;
        fx = fx2;
        fy = fy2;
        fz = fz2;
        rx = rx2;
        ry = ry2;
        rz = rz2;
    }

    BasisVectors out{};
    out.forward_x = fx;
    out.forward_y = fy;
    out.forward_z = fz;
    out.up_x = ux;
    out.up_y = uy;
    out.up_z = uz;
    out.right_x = rx;
    out.right_y = ry;
    out.right_z = rz;
    return out;
}

static inline BasisVectors BuildOrthonormalBasis(float forward_x,
                                                 float forward_y,
                                                 float forward_z,
                                                 float up_x,
                                                 float up_y,
                                                 float up_z)
{
    BasisVectors out;
    out.up_x = up_x;
    out.up_y = up_y;
    out.up_z = up_z;
    Normalize3(out.up_x, out.up_y, out.up_z, 0.0f, 1.0f, 0.0f);

    out.forward_x = forward_x;
    out.forward_y = forward_y;
    out.forward_z = forward_z;
    Normalize3(out.forward_x, out.forward_y, out.forward_z, 0.0f, 0.0f, 1.0f);

    const float fup = out.forward_x * out.up_x + out.forward_y * out.up_y + out.forward_z * out.up_z;
    out.forward_x -= fup * out.up_x;
    out.forward_y -= fup * out.up_y;
    out.forward_z -= fup * out.up_z;
    Normalize3(out.forward_x, out.forward_y, out.forward_z, 0.0f, 0.0f, 1.0f);

    out.right_x = out.forward_y * out.up_z - out.forward_z * out.up_y;
    out.right_y = out.forward_z * out.up_x - out.forward_x * out.up_z;
    out.right_z = out.forward_x * out.up_y - out.forward_y * out.up_x;
    Normalize3(out.right_x, out.right_y, out.right_z, 1.0f, 0.0f, 0.0f);

    return out;
}

static inline bool NormalizeDirection(float x, float y, float z, float& out_x, float& out_y, float& out_z)
{
    const float l = std::sqrt(x * x + y * y + z * z);
    if(l <= 1e-6f)
    {
        out_x = 0.0f;
        out_y = 0.0f;
        out_z = 0.0f;
        return false;
    }
    out_x = x / l;
    out_y = y / l;
    out_z = z / l;
    return true;
}

static inline void ToLocal(const BasisVectors& basis,
                           float world_x,
                           float world_y,
                           float world_z,
                           float& out_lx,
                           float& out_ly,
                           float& out_lz)
{
    out_lx = world_x * basis.right_x + world_y * basis.right_y + world_z * basis.right_z;
    out_ly = world_x * basis.up_x + world_y * basis.up_y + world_z * basis.up_z;
    out_lz = world_x * basis.forward_x + world_y * basis.forward_y + world_z * basis.forward_z;
}

}

#endif
