// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTHELPERS_H
#define EFFECTHELPERS_H

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI 6.28318530718f
#define RAD_TO_DEG 57.2957795131f
#define DEG_TO_RAD 0.01745329251f

static inline float smoothstep(float edge0, float edge1, float x)
{
    float t = fmax(0.0f, fmin(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

static inline float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

static inline float clamp(float value, float min_val, float max_val)
{
    return fmax(min_val, fmin(max_val, value));
}

#endif
