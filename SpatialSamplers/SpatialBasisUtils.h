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
