// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROPPREVIEWWIDGET_H
#define SCENEPROPPREVIEWWIDGET_H

#include "SceneProp3D.h"
#include "viewport/GlProgram.h"
#include "viewport/MeshBatch.h"

#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <vector>

class ScenePropPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core
{
    Q_OBJECT

public:
    explicit ScenePropPreviewWidget(QWidget* parent = nullptr);
    ~ScenePropPreviewWidget() override;

    void setBoxPreview(float width_mm, float height_mm, float depth_mm,
                       unsigned int solid_rgb, unsigned int glass_rgb,
                       const ScenePropFaceKind faces[SceneProp3D::kFaceCount]);
    void setMeshPreview(float width_mm, float height_mm, float depth_mm,
                        unsigned int solid_rgb, unsigned int glass_rgb,
                        ScenePropFaceKind body_kind,
                        const QString& mesh_absolute_path);
    void clearPreview();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void releaseGlResources();
    void rebuildGeometry();
    void drawBatch(const MeshBatch& batch, MeshBatch::Primitive primitive, float alpha);

    enum class Mode { Empty, Box, Mesh };

    Mode mode_ = Mode::Empty;
    float width_mm_ = 200.0f;
    float height_mm_ = 450.0f;
    float depth_mm_ = 450.0f;
    unsigned int solid_rgb_ = 0x555555u;
    unsigned int glass_rgb_ = 0x4AA8C8u;
    ScenePropFaceKind faces_[SceneProp3D::kFaceCount] = {};
    ScenePropFaceKind mesh_body_ = ScenePropFaceKind::Solid;
    QString mesh_path_;

    float yaw_deg_ = 35.0f;
    float pitch_deg_ = 25.0f;
    float distance_ = 3.0f;
    QPoint last_mouse_;

    bool gl_ready_ = false;
    GlProgram unlit_;
    MeshBatch faces_batch_;
    MeshBatch edges_batch_;
    std::vector<float> faces_cpu_;
    std::vector<float> edges_cpu_;
};

#endif
