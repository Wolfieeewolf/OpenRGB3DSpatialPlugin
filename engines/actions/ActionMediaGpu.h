// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QImage>

namespace Action
{
namespace media_gpu
{

/**
 * Upload a prepared media frame (NEAREST + alpha) and optional column-max 1D strip
 * for collapse_v. Fail closed if the offscreen GL pool is unavailable.
 */
bool UploadPreparedFrame(const QImage& frame);

/** Drop GL textures (call from ResetMediaGpu). */
void Reset();

/**
 * Sample at UV01 pairs (interleaved u,v). collapse_flags[i]!=0 → column-max strip.
 * Writes RGB 0..1 and lit flags (1 if alpha passed). count samples.
 */
bool SampleAtUvs(const float* uv01,
                 const unsigned char* collapse_flags,
                 int count,
                 float* out_rgb,
                 unsigned char* out_lit);

bool IsAvailable();

} // namespace media_gpu
} // namespace Action
