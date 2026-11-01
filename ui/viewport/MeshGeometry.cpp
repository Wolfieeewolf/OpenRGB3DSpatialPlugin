// SPDX-License-Identifier: GPL-2.0-only

#include "MeshGeometry.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MeshGeometry
{
void PushPosColor(std::vector<float>& out, float x, float y, float z, float r, float g, float b)
{
    out.push_back(x);
    out.push_back(y);
    out.push_back(z);
    out.push_back(r);
    out.push_back(g);
    out.push_back(b);
}

void PushTri(std::vector<float>& out,
             float x0, float y0, float z0,
             float x1, float y1, float z1,
             float x2, float y2, float z2,
             float r, float g, float b)
{
    PushPosColor(out, x0, y0, z0, r, g, b);
    PushPosColor(out, x1, y1, z1, r, g, b);
    PushPosColor(out, x2, y2, z2, r, g, b);
}

void PushQuadAsTris(std::vector<float>& out,
                    float x0, float y0, float z0,
                    float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    float r, float g, float b)
{
    PushTri(out, x0, y0, z0, x1, y1, z1, x2, y2, z2, r, g, b);
    PushTri(out, x0, y0, z0, x2, y2, z2, x3, y3, z3, r, g, b);
}

void AppendAxisAlignedBoxFaces(std::vector<float>& out,
                               float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               float r, float g, float b)
{
    out.reserve(out.size() + 36 * 6);
    PushQuadAsTris(out, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, r, g, b);
    PushQuadAsTris(out, x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1, r, g, b);
    PushQuadAsTris(out, x0, y0, z0, x0, y1, z0, x0, y1, z1, x0, y0, z1, r, g, b);
    PushQuadAsTris(out, x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1, r, g, b);
    PushQuadAsTris(out, x0, y0, z0, x0, y1, z0, x1, y1, z0, x1, y0, z0, r, g, b);
    PushQuadAsTris(out, x0, y0, z1, x0, y1, z1, x1, y1, z1, x1, y0, z1, r, g, b);
}

void AppendAxisAlignedBoxEdges(std::vector<float>& out,
                               float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               float r, float g, float b)
{
    auto push_line = [&](float ax, float ay, float az, float bx, float by, float bz) {
        PushPosColor(out, ax, ay, az, r, g, b);
        PushPosColor(out, bx, by, bz, r, g, b);
    };
    push_line(x0, y0, z0, x1, y0, z0);
    push_line(x1, y0, z0, x1, y0, z1);
    push_line(x1, y0, z1, x0, y0, z1);
    push_line(x0, y0, z1, x0, y0, z0);
    push_line(x0, y1, z0, x1, y1, z0);
    push_line(x1, y1, z0, x1, y1, z1);
    push_line(x1, y1, z1, x0, y1, z1);
    push_line(x0, y1, z1, x0, y1, z0);
    push_line(x0, y0, z0, x0, y1, z0);
    push_line(x1, y0, z0, x1, y1, z0);
    push_line(x1, y0, z1, x1, y1, z1);
    push_line(x0, y0, z1, x0, y1, z1);
}

void AppendControllerIndicatorSphere(std::vector<float>& out,
                                     float cx, float cy, float cz,
                                     float radius,
                                     int segments)
{
    for(int i = 0; i < segments; ++i)
    {
        const float lat0 = (float)M_PI * ((float)i / (float)segments);
        const float lat1 = (float)M_PI * ((float)(i + 1) / (float)segments);
        const float y0 = radius * std::sin(lat0);
        const float y1 = radius * std::sin(lat1);
        const float r0 = radius * std::cos(lat0);
        const float r1 = radius * std::cos(lat1);
        const float cr = (i < segments / 2) ? 0.0f : 1.0f;
        const float cg = (i < segments / 2) ? 1.0f : 0.0f;
        const float cb = 0.0f;

        for(int j = 0; j < segments; ++j)
        {
            const float lng0 = 2.0f * (float)M_PI * ((float)j / (float)segments);
            const float lng1 = 2.0f * (float)M_PI * ((float)(j + 1) / (float)segments);
            const float x0 = std::cos(lng0);
            const float z0 = std::sin(lng0);
            const float x1 = std::cos(lng1);
            const float z1 = std::sin(lng1);

            const float ax = cx + x0 * r0;
            const float ay = cy + y0;
            const float az = cz + z0 * r0;
            const float bx = cx + x0 * r1;
            const float by = cy + y1;
            const float bz = cz + z0 * r1;
            const float cxv = cx + x1 * r0;
            const float cyv = cy + y0;
            const float czv = cz + z1 * r0;
            const float dx = cx + x1 * r1;
            const float dy = cy + y1;
            const float dz = cz + z1 * r1;

            PushTri(out, ax, ay, az, bx, by, bz, cxv, cyv, czv, cr, cg, cb);
            PushTri(out, bx, by, bz, dx, dy, dz, cxv, cyv, czv, cr, cg, cb);
        }
    }
}

void AppendFlatQuadXY(std::vector<float>& out,
                      float x0, float y0, float x1, float y1, float z,
                      float r, float g, float b)
{
    PushQuadAsTris(out,
                   x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z,
                   r, g, b);
}

void AppendFlatQuadBorderXY(std::vector<float>& out,
                            float x0, float y0, float x1, float y1, float z,
                            float r, float g, float b)
{
    PushPosColor(out, x0, y0, z, r, g, b);
    PushPosColor(out, x1, y0, z, r, g, b);
    PushPosColor(out, x1, y0, z, r, g, b);
    PushPosColor(out, x1, y1, z, r, g, b);
    PushPosColor(out, x1, y1, z, r, g, b);
    PushPosColor(out, x0, y1, z, r, g, b);
    PushPosColor(out, x0, y1, z, r, g, b);
    PushPosColor(out, x0, y0, z, r, g, b);
}

void AppendCircleLineLoopXY(std::vector<float>& out,
                            float radius, int segments,
                            float r, float g, float b)
{
    for(int i = 0; i < segments; ++i)
    {
        const float a0 = 2.0f * (float)M_PI * ((float)i / (float)segments);
        const float a1 = 2.0f * (float)M_PI * ((float)(i + 1) / (float)segments);
        PushPosColor(out, radius * std::cos(a0), radius * std::sin(a0), 0.0f, r, g, b);
        PushPosColor(out, radius * std::cos(a1), radius * std::sin(a1), 0.0f, r, g, b);
    }
}

void AppendCircleLineLoopXZ(std::vector<float>& out,
                            float radius, int segments,
                            float r, float g, float b)
{
    for(int i = 0; i < segments; ++i)
    {
        const float a0 = 2.0f * (float)M_PI * ((float)i / (float)segments);
        const float a1 = 2.0f * (float)M_PI * ((float)(i + 1) / (float)segments);
        PushPosColor(out, radius * std::cos(a0), 0.0f, radius * std::sin(a0), r, g, b);
        PushPosColor(out, radius * std::cos(a1), 0.0f, radius * std::sin(a1), r, g, b);
    }
}

void AppendUVSphere(std::vector<float>& out,
                    float radius, int segments, int rings,
                    float r, float g, float b)
{
    for(int i = 0; i < rings; ++i)
    {
        const float lat0 = (float)M_PI * (-0.5f + (float)i / (float)rings);
        const float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / (float)rings);
        const float y0 = radius * std::sin(lat0);
        const float y1 = radius * std::sin(lat1);
        const float r0 = radius * std::cos(lat0);
        const float r1 = radius * std::cos(lat1);
        for(int j = 0; j < segments; ++j)
        {
            const float lng0 = 2.0f * (float)M_PI * ((float)j / (float)segments);
            const float lng1 = 2.0f * (float)M_PI * ((float)(j + 1) / (float)segments);
            const float x0 = std::cos(lng0);
            const float z0 = std::sin(lng0);
            const float x1 = std::cos(lng1);
            const float z1 = std::sin(lng1);
            PushTri(out,
                    x0 * r0, y0, z0 * r0,
                    x0 * r1, y1, z0 * r1,
                    x1 * r0, y0, z1 * r0,
                    r, g, b);
            PushTri(out,
                    x0 * r1, y1, z0 * r1,
                    x1 * r1, y1, z1 * r1,
                    x1 * r0, y0, z1 * r0,
                    r, g, b);
        }
    }
}
} // namespace MeshGeometry
