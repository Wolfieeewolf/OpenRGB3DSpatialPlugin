// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <string>

namespace Action
{
namespace script_gpu
{

/** Per-clip values for one batch (layout must match the generated shader). */
struct Uniforms
{
    float scalars[17] = {};
    float axis_vec[3] = {1.0f, 0.0f, 0.0f};
    float color[3] = {};      /* 0..255 */
    float intensity_mod = 1.0f;
    int stop_count = 1;
    float stop_pos[32] = {};
    float stop_col[32 * 3] = {}; /* 0..255 */
};

/** True once the shared offscreen GL context is up (scripts may run on the GPU). */
bool Available();

/**
 * Evaluate a translated `.fx` script for `count` LEDs in one draw.
 * attrs: 20 floats per LED. rgba_out: 4 bytes per LED (alpha 255 = lit, 0 = off).
 * Returns false (nothing written) when GL or the shader is unavailable; callers then
 * run the CPU interpreter instead.
 */
bool Run(const std::shared_ptr<const std::string>& fragment_source,
         const Uniforms& uniforms,
         const float* attrs,
         int count,
         unsigned char* rgba_out);

} // namespace script_gpu
} // namespace Action
