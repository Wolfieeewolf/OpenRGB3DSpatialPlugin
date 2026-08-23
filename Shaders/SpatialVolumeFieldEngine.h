// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QImage>
#include <QString>
#include <QVector3D>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

/**
 * Shared offscreen OpenGL assist: evaluate a volumetric GLSL field on a unit-cube
 * atlas (Z slices stacked), then sample on the CPU.
 *
 * User fragment body must define:
 *   void volumeMain(out vec4 out_color, in vec3 p01);
 * where p01 is in [0,1]^3. Engine supplies u_time, u_params[kMaxParams], and optional
 * sampler2D u_media (media texture for TextureProjection / OmniShapeTexture).
 *
 * Sibling to SpatialShaderEngine (2D fullscreen). Does not use the viewport MeshBatch.
 * Call ensureReady() from one thread only (typically the effect render path).
 * sample01 / sampleScalar01 are lock-free — do not call concurrently with ensureReady.
 */
class SpatialVolumeFieldEngine
{
public:
    static constexpr int kMaxParams = 24;
    static constexpr int kMinResolution = 8;
    static constexpr int kMaxResolution = 32;
    static constexpr int kMaxMediaEdge = 512;

    struct Params
    {
        float time_sec = 0.0f;
        float values[kMaxParams] = {};
        int count = 0;
    };

    SpatialVolumeFieldEngine();
    ~SpatialVolumeFieldEngine();

    SpatialVolumeFieldEngine(const SpatialVolumeFieldEngine&) = delete;
    SpatialVolumeFieldEngine& operator=(const SpatialVolumeFieldEngine&) = delete;

    void setFragmentBody(const QString& glsl_body);
    void setResolution(int n);
    void setParams(const Params& params);

    /** Optional 2D media for volumeMain (sampler2D u_media). Empty clears to 1x1 black. */
    void setMediaTexture(const QImage& image, bool wrap);
    void clearMediaTexture();
    bool mediaDirty() const;

    /** Rebuild atlas if dirty. Returns false if GL unavailable or compile failed. */
    bool ensureReady();

    /** Trilinear sample of atlas RGB (0..1). Safe if ensureReady failed (returns 0). */
    QVector3D sample01(float x, float y, float z) const;

    /** Scalar convenience: red channel. */
    float sampleScalar01(float x, float y, float z) const;

    bool isAvailable() const { return available_.load(); }
    QString lastError() const;

private:
    bool initGl();
    void shutdownGl();
    bool compileProgram(const QString& body);
    bool ensureFbo(int n);
    bool renderAtlas();
    void readbackAtlas(int n);
    void destroyPbos();
    bool ensurePbos(int n);
    void destroyMediaTexture();
    bool uploadMediaTexture();

    QString fragment_body_;
    Params params_{};
    int resolution_ = 18;

    mutable std::mutex mutex_;
    std::vector<float> atlas_rgb_;
    int atlas_res_ = 0;
    bool body_dirty_ = true;
    bool params_dirty_ = true;
    bool size_dirty_ = true;
    bool media_dirty_ = false;
    std::atomic<bool> available_{false};
    QString last_error_;

    QImage media_image_;
    bool media_wrap_ = false;
    qint64 media_cache_key_ = 0;
    unsigned int media_tex_id_ = 0;
    int media_tex_w_ = 0;
    int media_tex_h_ = 0;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    int fbo_n_ = 0;
    bool gl_ok_ = false;

    unsigned int pbos_[2] = {0, 0};
    int pbo_write_idx_ = 0;
    int pbo_bytes_ = 0;
    bool pbo_has_pending_ = false;
    int pbo_pending_n_ = 0;
    std::vector<unsigned char> readback_staging_;
};
