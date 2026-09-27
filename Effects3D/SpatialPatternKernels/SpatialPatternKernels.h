// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIAL_PATTERN_KERNELS_H
#define SPATIAL_PATTERN_KERNELS_H

#include <QString>
#include <string>

int SpatialPatternKernelCount();
int SpatialPatternKernelClamp(int id);
/** Stable id = patterns/<id>.kernel stem (survives reorder / display-name edits). */
const char* SpatialPatternKernelId(int kernel_id);
const char* SpatialPatternKernelDisplayName(int kernel_id);
const char* SpatialPatternKernelPaletteName(int kernel_id);
/** Resolve runtime index from stable id (or display name). Returns -1 if unknown. */
int SpatialPatternKernelFindById(const std::string& id);
float EvalSpatialPatternKernel(int kernel_id, float s01, float phase01, float rep, float time_sec);
QString SpatialPatternKernelShader();

/** Clear cached kernels and load again from patterns/*.kernel. */
void SpatialPatternKernelsReload();

#endif
