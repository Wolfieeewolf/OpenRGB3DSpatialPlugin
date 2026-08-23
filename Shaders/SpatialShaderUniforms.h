// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERUNIFORMS_H
#define SPATIALSHADERUNIFORMS_H

struct SpatialShaderUniforms
{
    float time_sec = 0.0f;
    /** Optional extras for Shader Field presets: [0]=zoom [1]=contrast [2]=hue [3]=detail */
    float params[8] = {};
    int param_count = 0;
};

#endif
