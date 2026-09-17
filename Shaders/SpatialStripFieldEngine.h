// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QString>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

/**
 * Offscreen OpenGL assist for 1D strip kernels: render a row atlas, sample on CPU.
 * User body: void stripMain(out vec4 out_color, in float s01);
 * Engine supplies u_time and u_params[kMaxParams].
 */
class SpatialStripFieldEngine
{
public:
    static constexpr int kMaxParams = 24;
    static constexpr int kMinWidth = 64;
    static constexpr int kMaxWidth = 512;

    struct Params
    {
        float time_sec = 0.0f;
        float values[kMaxParams] = {};
        int count = 0;
    };

    SpatialStripFieldEngine();
    ~SpatialStripFieldEngine();

    SpatialStripFieldEngine(const SpatialStripFieldEngine&) = delete;
    SpatialStripFieldEngine& operator=(const SpatialStripFieldEngine&) = delete;

    void setFragmentBody(const QString& glsl_body);
    void setWidth(int w);
    void setParams(const Params& params);

    bool ensureReady();
    /** Linear sample of red channel as signed kernel (-1..1) encoded in R as (k+1)/2.
     *  Lock-free — do not call concurrently with ensureReady. */
    float sampleKernelSigned(float s01) const;
    float sample01(float s01) const;

    bool isAvailable() const { return available_.load(); }
    QString lastError() const;

private:
    bool initGl();
    void shutdownGl();
    bool compileProgram(const QString& body);
    bool ensureFbo(int w);
    bool renderStrip();
    void readbackStrip(int w);

    QString fragment_body_;
    Params params_{};
    int width_ = 256;

    mutable std::mutex mutex_;
    std::vector<float> strip_r_;
    int strip_w_ = 0;
    bool body_dirty_ = true;
    bool params_dirty_ = true;
    bool size_dirty_ = true;
    std::atomic<bool> available_{false};
    QString last_error_;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    int fbo_w_ = 0;
    bool gl_ok_ = false;
};
