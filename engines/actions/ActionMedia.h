// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"

#include <string>

namespace Action
{

bool EvaluateMediaAtUv(const Clip& clip,
                       int local_ms,
                       float nx,
                       float ny,
                       RGBColor* out_color,
                       float* out_intensity,
                       bool collapse_v = false,
                       float uv_aspect = 1.0f);

/**
 * Batch GPU UV sample for media clips (NEAREST + alpha, optional column-max).
 * uvs: interleaved device UVs before MapSampleUv (nx,ny per LED).
 * collapse_flags: 1 = column reduce. Writes RGB 0..1 + lit flags.
 */
bool EvaluateMediaAtUvsGpu(const Clip& clip,
                           int local_ms,
                           const float* uvs,
                           const unsigned char* collapse_flags,
                           int count,
                           float uv_aspect,
                           float* out_rgb,
                           unsigned char* out_lit);

/**
 * Pin the current image/GIF/text frame once per pack frame for true UV sampling at LEDs.
 * Avoids reloading the source on every LED sample.
 */
bool PrepareMediaForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);
void ResetMediaGpu();

void InvalidateMediaCache(const std::string& path = {});
/** Drop rasterized marquee text only (leave image/GIF frames cached). */
void InvalidateMediaTextCache();

} // namespace Action
