// SPDX-License-Identifier: GPL-2.0-only

#ifndef MESHGEOMETRY_H
#define MESHGEOMETRY_H

#include <vector>

/** CPU PosColor mesh builders for MeshBatch uploads (6 floats/vert). */
namespace MeshGeometry
{
void PushPosColor(std::vector<float>& out, float x, float y, float z, float r, float g, float b);

void PushTri(std::vector<float>& out,
             float x0, float y0, float z0,
             float x1, float y1, float z1,
             float x2, float y2, float z2,
             float r, float g, float b);

void PushQuadAsTris(std::vector<float>& out,
                    float x0, float y0, float z0,
                    float x1, float y1, float z1,
                    float x2, float y2, float z2,
                    float x3, float y3, float z3,
                    float r, float g, float b);

void AppendAxisAlignedBoxFaces(std::vector<float>& out,
                               float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               float r, float g, float b);

void AppendAxisAlignedBoxEdges(std::vector<float>& out,
                               float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               float r, float g, float b);

void AppendControllerIndicatorSphere(std::vector<float>& out,
                                     float cx, float cy, float cz,
                                     float radius,
                                     int segments);

void AppendFlatQuadXY(std::vector<float>& out,
                      float x0, float y0, float x1, float y1, float z,
                      float r, float g, float b);

void AppendFlatQuadBorderXY(std::vector<float>& out,
                            float x0, float y0, float x1, float y1, float z,
                            float r, float g, float b);

void AppendCircleLineLoopXY(std::vector<float>& out,
                            float radius, int segments,
                            float r, float g, float b);

void AppendCircleLineLoopXZ(std::vector<float>& out,
                            float radius, int segments,
                            float r, float g, float b);

void AppendUVSphere(std::vector<float>& out,
                    float radius, int segments, int rings,
                    float r, float g, float b);
}

#endif
