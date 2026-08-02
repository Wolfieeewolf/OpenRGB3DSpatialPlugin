// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialVolumeFieldAssist.h"

#include <algorithm>

void SpatialVolumeFieldAssist::setResolution(int n)
{
    resolution_ = std::clamp(n, SpatialVolumeFieldEngine::kMinResolution, SpatialVolumeFieldEngine::kMaxResolution);
    if(engine_)
    {
        engine_->setResolution(resolution_);
    }
}

void SpatialVolumeFieldAssist::setMediaTexture(const QImage& image, bool wrap)
{
    pending_media_ = image;
    pending_wrap_ = wrap;
    has_pending_media_ = true;
    if(engine_)
    {
        engine_->setMediaTexture(pending_media_, pending_wrap_);
    }
}

void SpatialVolumeFieldAssist::clearMediaTexture()
{
    pending_media_ = QImage();
    pending_wrap_ = false;
    has_pending_media_ = true;
    if(engine_)
    {
        engine_->clearMediaTexture();
    }
}

void SpatialVolumeFieldAssist::applyEngineSize(SpatialVolumeFieldEngine& engine)
{
    engine.setResolution(resolution_);
    if(has_pending_media_)
    {
        engine.setMediaTexture(pending_media_, pending_wrap_);
    }
}

bool SpatialVolumeFieldAssist::engineNeedsRefresh() const
{
    return engine_ && engine_->mediaDirty();
}

float SpatialVolumeFieldAssist::sampleScalar01(float x, float y, float z) const
{
    if(!isAvailable())
    {
        return 0.0f;
    }
    return engine_->sampleScalar01(x, y, z);
}

QVector3D SpatialVolumeFieldAssist::sample01(float x, float y, float z) const
{
    if(!isAvailable())
    {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }
    return engine_->sample01(x, y, z);
}
