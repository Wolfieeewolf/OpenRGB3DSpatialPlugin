// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"

#include <cstdint>
#include <string>

namespace Action
{
namespace builtin_gpu
{

struct SampleRgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

/** Pixel-style tools with a built-in OpenGL volume body (no toolbox `.fs` required). */
bool Uses(const std::string& effect_id);

void Reset();

bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);

bool IsAvailable();

/** True effect: volumeMain at each room-normalized XYZ (interleaved in/out). */
bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb);

/** Preview/fallback atlas sample. */
SampleRgb Sample01(float nx, float ny, float nz);

} // namespace builtin_gpu
} // namespace Action
