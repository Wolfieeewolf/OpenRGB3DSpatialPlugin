// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERENGINE_H
#define SPATIALSHADERENGINE_H

#include "SpatialShaderUniforms.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <thread>

class SpatialShaderEngine : public QObject
{
    Q_OBJECT
public:
    explicit SpatialShaderEngine(QObject* parent = nullptr);
    ~SpatialShaderEngine() override;

    void start();
    void stop();
    bool isRunning() const { return running.load(); }

    void setTargetFps(int fps);
    void setRenderSize(int width, int height);
    void setFragmentBody(const QString& glsl_body);
    void setUniforms(const SpatialShaderUniforms& uniforms);

signals:
    void frameReady(const QImage& image);
    void compileMessage(const QString& message);

private:
    void renderThreadMain();

    QString fragment_body;
    SpatialShaderUniforms uniform_values;
    int target_fps = 30;
    int render_width = 128;
    int render_height = 72;

    std::thread render_thread;
    std::mutex state_mutex;
    std::atomic<bool> running{false};
    std::atomic<bool> source_dirty{true};
    std::atomic<bool> size_dirty{true};
};

#endif
