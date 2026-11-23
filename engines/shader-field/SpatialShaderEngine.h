// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SpatialOverlayHandoff.h"
#include "SpatialShaderUniforms.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

/**
 * 2D spatialMain renderer on the shared offscreen GL pool.
 * No extra GL context or worker thread — those crash Windows drivers when
 * combined with the viewport.
 *
 * LED colours: evaluate spatialMain at exact UVs (point FBO + small readback).
 * Full atlas CPU readback only for overlay via exportAtlasRgba8.
 */
class SpatialShaderEngine : public QObject
{
    Q_OBJECT
public:
    static constexpr int kMinEdge = 32;
    static constexpr int kMaxEdge = 1024;
    static constexpr int kMaxPointEdge = 2048;

    explicit SpatialShaderEngine(QObject* parent = nullptr);
    ~SpatialShaderEngine() override;

    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    void setRenderSize(int width, int height);
    void setFragmentBody(const QString& glsl_body);
    void setUniforms(const SpatialShaderUniforms& uniforms);

    /** Compile programs (and keep them warm). Call from the effect render thread. */
    bool ensureReady();

    /**
     * Evaluate spatialMain at UV01 pairs (interleaved u,v). Writes RGB 0..1 into out_rgb
     * (count * 3). Fail closed if GL/compile fails.
     */
    bool sampleAtUvs(const float* uv01, int count, float* out_rgb);

    /** Bake atlas + upright overlay handoff (no CPU readback). */
    bool ensureAtlasTextureReady();
    /** Durable top-left handoff texture (share-group) for the room-grid overlay. */
    unsigned int atlasTextureId() const;
    int atlasTextureWidth() const;
    int atlasTextureHeight() const;

    /** Full atlas bake + RGBA8 readback for room-grid overlay (top-left origin). */
    bool exportAtlasRgba8(std::vector<unsigned char>& out_rgba, int& out_w, int& out_h);

    QImage latestFrame() const;
    QString lastError() const;
    int renderWidth() const;
    int renderHeight() const;

signals:
    void compileMessage(const QString& message);

private:
    bool compilePrograms(const QString& body);
    bool ensurePrograms();
    bool renderAtlas(bool readback_cpu);
    void destroyPointResources();
    void destroyPointPbos();
    bool ensurePointPbos(int bytes);
    bool ensurePointFbo(int w, int h);
    bool uploadUvTexture(const float* uv01, int count, int w, int h);
    void unpackPointReadback(const unsigned char* rgba, int w, int count, float* out_rgb) const;

    QString fragment_body_;
    SpatialShaderUniforms uniform_values_;
    int render_width_ = 1024;
    int render_height_ = 576;

    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    bool body_dirty_ = true;
    bool size_dirty_ = true;
    bool params_dirty_ = true;
    QString last_error_;
    QImage latest_frame_;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLShaderProgram> point_program_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    std::unique_ptr<QOpenGLFramebufferObject> point_fbo_;
    SpatialOverlayHandoff overlay_handoff_;
    int fbo_w_ = 0;
    int fbo_h_ = 0;
    int point_fbo_w_ = 0;
    int point_fbo_h_ = 0;
    unsigned int uv_tex_id_ = 0;
    int uv_tex_w_ = 0;
    int uv_tex_h_ = 0;
    std::vector<float> uv_upload_;
    std::vector<unsigned char> readback_staging_;

    /** PBO double-buffer for sampleAtUvs (same idea as Volume evaluateAtPoints). */
    unsigned int point_pbos_[2] = {0, 0};
    int point_pbo_write_idx_ = 0;
    int point_pbo_bytes_ = 0;
    bool point_pbo_has_pending_ = false;
    int point_pbo_pending_w_ = 0;
    int point_pbo_pending_h_ = 0;
    int point_pbo_pending_count_ = 0;
};
