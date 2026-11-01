// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialStripFieldAssist.h"

#include <algorithm>

void SpatialStripFieldAssist::setWidth(int w)
{
    width_ = std::clamp(w, SpatialStripFieldEngine::kMinWidth, SpatialStripFieldEngine::kMaxWidth);
    if(engine_)
    {
        engine_->setWidth(width_);
    }
}

void SpatialStripFieldAssist::applyEngineSize(SpatialStripFieldEngine& engine)
{
    engine.setWidth(width_);
}

float SpatialStripFieldAssist::sample01(float s01) const
{
    if(!isAvailable())
    {
        return 0.0f;
    }
    return engine_->sample01(s01);
}

float SpatialStripFieldAssist::sampleKernelSigned(float s01) const
{
    if(!isAvailable())
    {
        return 0.0f;
    }
    return engine_->sampleKernelSigned(s01);
}
