// SPDX-License-Identifier: GPL-2.0-only

#ifndef STRIPPATTERNSAMPLING_H
#define STRIPPATTERNSAMPLING_H

#include <algorithm>
#include <cmath>

namespace StripPatternSampling
{

inline int CellIndex01(float u01, int cells)
{
    cells = std::max(1, cells);
    float u = std::clamp(u01, 0.0f, 1.0f);
    int idx = (int)std::floor(u * (float)cells);
    return std::clamp(idx, 0, cells - 1);
}

inline float MatrixSerpentineS01(float u01, float v01, int virtual_w, int virtual_h, bool serpentine = true)
{
    virtual_w = std::max(1, virtual_w);
    virtual_h = std::max(1, virtual_h);
    int ix = CellIndex01(u01, virtual_w);
    int iy = CellIndex01(v01, virtual_h);
    if(serpentine && (iy & 1))
        ix = (virtual_w - 1) - ix;
    int linear = iy * virtual_w + ix;
    int denom = virtual_w * virtual_h;
    if(denom <= 1)
        return 0.0f;
    return (float)linear / (float)(denom - 1);
}

inline float MatrixRowMajorS01(float u01, float v01, int virtual_w, int virtual_h)
{
    return MatrixSerpentineS01(u01, v01, virtual_w, virtual_h, false);
}

}

#endif
