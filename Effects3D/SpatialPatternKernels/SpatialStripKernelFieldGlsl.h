// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialStripKernelEvalGlsl.h"
#include <QString>

/** 1D strip pattern kernels (signed encoded as (k+1)/2 in R).
 *  u_params: [0]=kernel_id [1]=phase01 [2]=repeats [3]=time_sec (also u_time)
 */
inline QString SpatialStripKernelFieldGlslBody()
{
    return QString::fromUtf8(SpatialStripKernelEvalGlsl()) + QString::fromUtf8(R"(
void stripMain(out vec4 out_color, in float s01)
{
    int kid = int(u_params[0] + 0.5);
    float phase01 = u_params[1];
    float repeats = max(u_params[2], 1.0);
    float time_sec = u_params[3];
    float k = evalStripKernelSigned(kid, s01, phase01, repeats, time_sec);
    float enc = clamp((k + 1.0) * 0.5, 0.0, 1.0);
    out_color = vec4(enc, enc, enc, 1.0);
}
)");
}

/** Full strip body for setFragmentBody. */
inline QString SpatialStripKernelFieldGlsl()
{
    return SpatialStripKernelFieldGlslBody();
}
