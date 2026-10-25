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

bool SpatialVolumeFieldAssist::prepareProgram(std::uint64_t render_sequence,
                                              float time_sec,
                                              const float* params,
                                              int param_count)
{
    return finishPrepare(render_sequence, time_sec, params, param_count, /*atlas=*/false);
}

bool SpatialVolumeFieldAssist::evaluateAtPoints(const float* xyz01, int count, float* out_rgb)
{
    if(!engine_ || !xyz01 || !out_rgb || count <= 0)
    {
        return false;
    }
    return engine_->evaluateAtPoints(xyz01, count, out_rgb);
}

QVector3D SpatialVolumeFieldAssist::sampleTrue01(float x, float y, float z)
{
    float xyz[3] = {
        std::clamp(x, 0.0f, 1.0f),
        std::clamp(y, 0.0f, 1.0f),
        std::clamp(z, 0.0f, 1.0f)
    };
    float rgb[3] = {};
    if(!evaluateAtPoints(xyz, 1, rgb))
    {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }
    return QVector3D(rgb[0], rgb[1], rgb[2]);
}
