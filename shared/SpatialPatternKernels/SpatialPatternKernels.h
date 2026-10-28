// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIAL_PATTERN_KERNELS_H
#define SPATIAL_PATTERN_KERNELS_H

#include <QString>
#include <string>

int SpatialPatternKernelCount();
int SpatialPatternKernelClamp(int id);
/** Stable id = library/patterns/<id>.kernel stem (survives reorder / display-name edits). */
const char* SpatialPatternKernelId(int kernel_id);
const char* SpatialPatternKernelDisplayName(int kernel_id);
const char* SpatialPatternKernelPaletteName(int kernel_id);
/** Resolve runtime index from stable id (or display name). Returns -1 if unknown. */
int SpatialPatternKernelFindById(const std::string& id);
QString SpatialPatternKernelShader();

#endif
