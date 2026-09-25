// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIAL_PATTERN_KERNELS_H
#define SPATIAL_PATTERN_KERNELS_H

#include <QString>

int SpatialPatternKernelCount();
int SpatialPatternKernelClamp(int id);
const char* SpatialPatternKernelDisplayName(int kernel_id);
const char* SpatialPatternKernelPaletteName(int kernel_id);
float EvalSpatialPatternKernel(int kernel_id, float s01, float phase01, float rep, float time_sec);
QString SpatialPatternKernelShader();

#endif
