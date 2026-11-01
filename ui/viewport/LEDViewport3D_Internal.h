// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEDVIEWPORT3D_INTERNAL_H
#define LEDVIEWPORT3D_INTERNAL_H

#include "LEDPosition3D.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline Vector3D Subtract(const Vector3D& a, const Vector3D& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector3D CrossVec(const Vector3D& a, const Vector3D& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float DotVec(const Vector3D& a, const Vector3D& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

#endif // LEDVIEWPORT3D_INTERNAL_H
