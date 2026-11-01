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

/** Room-normalized origin + size for burst centre (default center / 1). */
void SetSampleOrigin01(float ox, float oy, float oz, float size);

/**
 * Compile GPGPU volumeMain + params (OpenGL 4.1 required — no CPU fallback).
 * No-op unless the effect declares `particles:` in its `.fx` header.
 */
bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);

bool IsAvailable();

/** True effect: volumeMain at each room-normalized XYZ (interleaved in/out). */
bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb);

/** Soft density at normalized room coords (0..1) via true GPU sample. */
float SampleDensity01(float nx, float ny, float nz);

/** Single-point true GPU sample (for .fx particle_* helpers). */
SampleRgb Sample01(float nx, float ny, float nz);

} // namespace particle_field
} // namespace Action
