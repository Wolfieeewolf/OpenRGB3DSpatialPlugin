// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialFieldAssistBase.h"
#include "SpatialStripFieldEngine.h"

/**
 * Thin per-effect owner for SpatialStripFieldEngine (see SpatialFieldAssistBase).
 */
class SpatialStripFieldAssist : public SpatialFieldAssistBase<SpatialStripFieldEngine>
{
public:
    void setWidth(int w);

    float sample01(float s01) const;
    float sampleKernelSigned(float s01) const;

private:
    void applyEngineSize(SpatialStripFieldEngine& engine) override;

    int width_ = 256;
};
