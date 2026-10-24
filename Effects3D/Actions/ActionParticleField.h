// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"

#include <cstdint>
#include <string>

namespace Action
{
namespace particle_field
{

struct SampleRgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

void Reset();

/** Room-normalized origin + size for burst centre / Sample* remap (default center / 1). */
void SetSampleOrigin01(float ox, float oy, float oz, float size);

/**
 * Advance / rebuild the room-space particle set for this clip (once per pack frame).
 * No-op unless the effect declares `particles:` in its `.fx` header.
 */
bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);

bool IsAvailable();

/** Soft density at normalized room coords (0..1). */
float SampleDensity01(float nx, float ny, float nz);

/** Density-tinted RGB (uses clip colour via paint path separately). */
SampleRgb Sample01(float nx, float ny, float nz);

} // namespace particle_field
} // namespace Action
