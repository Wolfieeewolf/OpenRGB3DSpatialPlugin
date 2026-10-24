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

void InvalidateMediaCache(const std::string& path = {});

} // namespace Action
