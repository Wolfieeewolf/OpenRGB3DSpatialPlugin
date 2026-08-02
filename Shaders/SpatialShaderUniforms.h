// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERUNIFORMS_H
#define SPATIALSHADERUNIFORMS_H

struct SpatialShaderUniforms
{
    float time_sec = 0.0f;
    /** Optional extras for Shader Field presets: [0]=zoom [1]=contrast [2]=hue [3]=detail */
    float params[8] = {};
    int param_count = 0;
    /** Kept zeroed for user .fs files that still declare u_audio; no longer fed live audio. */
    float audio_bins[128] = {};
    const float* audio_ptr = nullptr;
    int audio_count = 0;
};

#endif
