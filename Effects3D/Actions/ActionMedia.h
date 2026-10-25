// SPDX-License-Identifier: GPL-2.0-only
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
 * Pin the current image/GIF/text frame once per pack frame for true UV sampling at LEDs.
 * Avoids reloading the source on every LED sample.
 */
bool PrepareMediaForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence);
void ResetMediaGpu();

void InvalidateMediaCache(const std::string& path = {});
/** Drop rasterized marquee text only (leave image/GIF frames cached). */
void InvalidateMediaTextCache();

} // namespace Action
