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

/** Room-normalized origin + size for Sample* remap (default center / 1). */
void SetSampleOrigin01(float ox, float oy, float oz, float size);

/**
 * Compile field shader + params (GPU required). No-op if the effect has no
 * `shader:` / `field:` header.
 */
bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);

bool IsAvailable();

/**
 * True effect: run volumeMain at each room-normalized XYZ (interleaved in/out).
 * Applies sample-origin remap. Requires PrepareForClip first.
 */
bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb);

/** Single-point true GPU sample (for .fx field_* helpers). */
SampleRgb Sample01(float nx, float ny, float nz);

/** 1 when any channel is lit, else 0. */
float SampleA01(float nx, float ny, float nz);

} // namespace volume_field
} // namespace Action
