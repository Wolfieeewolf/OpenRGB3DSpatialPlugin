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

    /**
     * Compile + set params without atlas bake (true per-LED / point-eval path).
     * Use prepare() only when a dense atlas preview is also required.
     */
    bool prepareProgram(std::uint64_t render_sequence, float time_sec, const float* params, int param_count);

    /** True effect: volumeMain at each point (interleaved xyz → interleaved rgb). */
    bool evaluateAtPoints(const float* xyz01, int count, float* out_rgb);

    /** True single-point sample (volumeMain at exact p01). Prefer evaluateAtPoints for batches. */
    QVector3D sampleTrue01(float x, float y, float z);

private:
    void applyEngineSize(SpatialVolumeFieldEngine& engine) override;
    bool engineNeedsRefresh() const override;

    int resolution_ = 18;
    QImage pending_media_;
    bool pending_wrap_ = false;
    bool has_pending_media_ = false;
};
