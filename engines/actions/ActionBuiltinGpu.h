// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"

#include <cstdint>
#include <string>
#include <vector>

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
/** Room-grid overlay: bake the prepared field over the 0..1 cube (same program the LEDs
    just sampled). n×n×n packed RGBA8, width n, height n*n — as FolderVolume exports. */
bool ExportAtlasRgba8(std::vector<unsigned char>& out_rgba, int& out_n);
bool ExportAtlasGpu(unsigned int& out_tex, int& out_n);

/** Single-point true GPU sample. */
SampleRgb Sample01(float nx, float ny, float nz);

} // namespace builtin_gpu
} // namespace Action
