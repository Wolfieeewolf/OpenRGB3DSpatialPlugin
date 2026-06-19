// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERUNIFORMS_H
#define SPATIALSHADERUNIFORMS_H

struct SpatialShaderUniforms
{
    float time_sec = 0.0f;
    float audio_bins[128] = {};
    const float* audio_ptr = nullptr;
    int audio_count = 0;
};

#endif
