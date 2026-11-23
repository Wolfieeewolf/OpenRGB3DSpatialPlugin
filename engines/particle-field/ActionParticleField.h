// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"

#include <cstdint>
#include <string>
#include <vector>

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
/** Room-grid overlay: bake the prepared field over the 0..1 cube (same program the LEDs
    just sampled). n×n×n packed RGBA8, width n, height n*n — as FolderVolume exports. */
bool ExportAtlasRgba8(std::vector<unsigned char>& out_rgba, int& out_n);

/** Soft density at normalized room coords (0..1) via true GPU sample. */
float SampleDensity01(float nx, float ny, float nz);

/** Single-point true GPU sample (for .fx particle_* helpers). */
SampleRgb Sample01(float nx, float ny, float nz);

} // namespace particle_field
} // namespace Action
