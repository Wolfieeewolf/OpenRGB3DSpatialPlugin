// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"

namespace EffectPack
{
namespace block_eval
{

/** Mutable axis-eval scratch (LED / sequence / 1D). Return false = LED off. */
struct AxisCtx
{
    const Block* block = nullptr;
    int local_ms = 0;
    float axis = 0.0f;
    float progress = 0.0f;
    int twinkle_seed = 0;
    float intensity = 1.0f;
    RGBColor color = ToRGBColor(0, 0, 0);
};

using AxisFn = bool (*)(AxisCtx& ctx);

/** Normalized world sample (device or room AABB). */
struct WorldNorm
{
    float nx = 0.5f;
    float ny = 0.5f;
    float nz = 0.5f;
    float radius = 0.0f;
    float height = 0.5f;
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
};

struct WorldCtx
{
    const Block* block = nullptr;
    int local_ms = 0;
    float progress = 0.0f;
    int twinkle_seed = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    WorldNorm s;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float intensity = 1.0f;
    RGBColor color = ToRGBColor(0, 0, 0);
};

using WorldFn = bool (*)(WorldCtx& ctx);

} // namespace block_eval
} // namespace EffectPack
