// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"

#include <cstdint>
#include <string>

namespace Action
{
namespace volume_field
{

struct SampleRgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

/** Same root as Action toolbox `.fx` (used to find sibling `.fs` bodies). */
void SetToolboxDirectory(const filesystem::path& dir);

/** Drop cached GL body / assist state (call when toolbox reloads). */
void Reset();

/**
 * Prepare the GPU volume atlas for this clip's field shader (once per pack frame
 * / render_sequence). No-op if the effect has no `shader:` / `field:` header.
 */
bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);

bool IsAvailable();

/** Sample prepared field at normalized room coords (0..1). */
SampleRgb Sample01(float nx, float ny, float nz);
float SampleScalar01(float nx, float ny, float nz);

/** Phase 1: atlas RGB only — returns 1 when any channel is lit, else 0. */
float SampleA01(float nx, float ny, float nz);

} // namespace volume_field
} // namespace Action
