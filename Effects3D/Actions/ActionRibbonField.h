// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"

#include <cstdint>
#include <string>

namespace Action
{
namespace ribbon_field
{

struct SampleRgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

void Reset();

/** Room-normalized origin + size for ribbon centre / extent (default center / 1). */
void SetSampleOrigin01(float ox, float oy, float oz, float size);

/**
 * Rebuild ribbon params and compile the GPGPU volumeMain (OpenGL 4.1).
 * Preview may still bake an atlas; LED apply uses EvaluateAtPoints.
 * Falls back to CPU tube sampling if GL is unavailable.
 * No-op unless the effect declares `ribbons:` in its `.fx` header.
 */
bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);

bool IsAvailable();

/** True effect: volumeMain at each room-normalized XYZ (interleaved in/out). */
bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb);

/** Soft tube density at normalized room coords (0..1). */
float SampleDensity01(float nx, float ny, float nz);

/** Preview/fallback atlas or CPU sample. */
SampleRgb Sample01(float nx, float ny, float nz);

} // namespace ribbon_field
} // namespace Action
