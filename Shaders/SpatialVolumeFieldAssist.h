// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialFieldAssistBase.h"
#include "SpatialVolumeFieldEngine.h"

#include <QImage>
#include <QVector3D>

/**
 * Thin per-effect owner for SpatialVolumeFieldEngine (see SpatialFieldAssistBase).
 * Kept (vs bare Engine) so fail-latch / lazy create stay out of every effect class.
 */
class SpatialVolumeFieldAssist : public SpatialFieldAssistBase<SpatialVolumeFieldEngine>
{
public:
    void setResolution(int n);

    /** Optional media for sampler2D u_media. Applied on next prepare/ensureReady. */
    void setMediaTexture(const QImage& image, bool wrap);
    void clearMediaTexture();

    float sampleScalar01(float x, float y, float z) const;
    QVector3D sample01(float x, float y, float z) const;

private:
    void applyEngineSize(SpatialVolumeFieldEngine& engine) override;
    bool engineNeedsRefresh() const override;

    int resolution_ = 18;
    QImage pending_media_;
    bool pending_wrap_ = false;
    bool has_pending_media_ = false;
};
