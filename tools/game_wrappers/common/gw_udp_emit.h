// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstdint>

namespace GwUdp
{

bool EmitUniformRgb(float r, float g, float b, const char* source);
void EmitFromColorRef(std::uint32_t color1, std::uint32_t color2, const char* source);

} // namespace GwUdp
