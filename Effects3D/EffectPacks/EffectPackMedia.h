// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"

#include <string>

namespace EffectPack
{

bool EvaluateMediaAtUv(const Block& block,
                       int local_ms,
                       float nx,
                       float ny,
                       RGBColor* out_color,
                       float* out_intensity,
                       bool collapse_v = false);

void InvalidateMediaCache(const std::string& path = {});

} // namespace EffectPack
