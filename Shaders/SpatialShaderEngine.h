// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialShaderUniforms.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

/**
 * 2D fullscreen spatialMain renderer on the shared offscreen GL pool.
 * No extra GL context or worker thread — those crash Windows drivers when
 * combined with the viewport.
 */
class SpatialShaderEngine : public QObject
{
    Q_OBJECT
public:
    explicit SpatialShaderEngine(QObject* parent = nullptr);
    ~SpatialShaderEngine() override;

    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    void setTargetFps(int fps);
    void setRenderSize(int width, int height);
    void setFragmentBody(const QString& glsl_body);
    void setUniforms(const SpatialShaderUniforms& uniforms);

    /** Render one frame on the shared pool. Call from the effect render thread. */
    bool ensureReady();

    QImage latestFrame() const;
    QString lastError() const;

signals:
    void frameReady(const QImage& image);
    void compileMessage(const QString& message);

private:
    bool compileProgram(const QString& body);
    bool renderFrame();

    QString fragment_body_;
    SpatialShaderUniforms uniform_values_;
    int target_fps_ = 30;
    int render_width_ = 128;
    int render_height_ = 72;

    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    bool body_dirty_ = true;
    bool size_dirty_ = true;
    bool params_dirty_ = true;
    QString last_error_;
    QImage latest_frame_;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    int fbo_w_ = 0;
    int fbo_h_ = 0;
};
