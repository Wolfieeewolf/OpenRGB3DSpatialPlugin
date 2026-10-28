// SPDX-License-Identifier: GPL-2.0-only

#include "ScenePropPreviewWidget.h"

#include "GridSpaceUtils.h"
#include "viewport/MeshGeometry.h"
#include "viewport/MeshImport.h"
#include "viewport/ScenePropMeshCache.h"
#include "viewport/ViewportGLFormat.h"
#include "viewport/ViewportMath.h"
#include "viewport/ViewportShaders.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
void UnpackRgb(unsigned int rgb, float* r, float* g, float* b)
{
    *r = ((rgb >> 16) & 0xFF) / 255.0f;
    *g = ((rgb >> 8) & 0xFF) / 255.0f;
    *b = (rgb & 0xFF) / 255.0f;
}
} // namespace

ScenePropPreviewWidget::ScenePropPreviewWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    ViewportGLFormat::ApplyToWidget(this);
    setFixedSize(220, 180);
    setFocusPolicy(Qt::StrongFocus);
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        faces_[i] = ScenePropFaceKind::Solid;
    }
}

ScenePropPreviewWidget::~ScenePropPreviewWidget()
{
    releaseGlResources();
}

void ScenePropPreviewWidget::releaseGlResources()
{
    if(!gl_ready_)
    {
        faces_batch_.Abandon();
        edges_batch_.Abandon();
        return;
    }
    if(context() && context()->isValid())
    {
        makeCurrent();
        faces_batch_.Destroy();
        edges_batch_.Destroy();
        unlit_.Destroy();
        doneCurrent();
    }
    else
    {
        faces_batch_.Abandon();
        edges_batch_.Abandon();
    }
    gl_ready_ = false;
}

void ScenePropPreviewWidget::setBoxPreview(float width_mm, float height_mm, float depth_mm,
                                           unsigned int solid_rgb, unsigned int glass_rgb,
                                           const ScenePropFaceKind faces[SceneProp3D::kFaceCount])
{
    mode_ = Mode::Box;
    width_mm_ = std::max(1.0f, width_mm);
    height_mm_ = std::max(1.0f, height_mm);
    depth_mm_ = std::max(1.0f, depth_mm);
    solid_rgb_ = solid_rgb & 0x00FFFFFFu;
    glass_rgb_ = glass_rgb & 0x00FFFFFFu;
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        faces_[i] = faces ? faces[i] : ScenePropFaceKind::Solid;
    }
    mesh_path_.clear();
    if(gl_ready_)
    {
        makeCurrent();
        rebuildGeometry();
        doneCurrent();
    }
    update();
}

void ScenePropPreviewWidget::setMeshPreview(float width_mm, float height_mm, float depth_mm,
                                            unsigned int solid_rgb, unsigned int glass_rgb,
                                            ScenePropFaceKind body_kind,
                                            const QString& mesh_absolute_path)
{
    mode_ = mesh_absolute_path.isEmpty() ? Mode::Empty : Mode::Mesh;
    width_mm_ = std::max(1.0f, width_mm);
    height_mm_ = std::max(1.0f, height_mm);
    depth_mm_ = std::max(1.0f, depth_mm);
    solid_rgb_ = solid_rgb & 0x00FFFFFFu;
    glass_rgb_ = glass_rgb & 0x00FFFFFFu;
    mesh_body_ = body_kind;
    mesh_path_ = mesh_absolute_path;
    if(gl_ready_)
    {
        makeCurrent();
        rebuildGeometry();
        doneCurrent();
    }
    update();
}

void ScenePropPreviewWidget::clearPreview()
{
    mode_ = Mode::Empty;
    mesh_path_.clear();
    faces_cpu_.clear();
    edges_cpu_.clear();
    if(gl_ready_)
    {
        makeCurrent();
        faces_batch_.Destroy();
        edges_batch_.Destroy();
        doneCurrent();
    }
    update();
}

void ScenePropPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();
    gl_ready_ = ViewportShaders::CompileUnlitColor(unlit_, nullptr);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
    rebuildGeometry();
}

void ScenePropPreviewWidget::resizeGL(int, int)
{
}

void ScenePropPreviewWidget::rebuildGeometry()
{
    if(!gl_ready_)
    {
        return;
    }

    faces_cpu_.clear();
    edges_cpu_.clear();

    const float scale = DEFAULT_GRID_SCALE_MM;
    const float w = MMToGridUnits(width_mm_, scale);
    const float h = MMToGridUnits(height_mm_, scale);
    const float d = MMToGridUnits(depth_mm_, scale);
    const float hw = w * 0.5f;
    const float hh = h * 0.5f;
    const float hd = d * 0.5f;
    const float span = std::max(w, std::max(h, d));
    distance_ = std::max(2.0f, span * 1.8f);

    float sr, sg, sb, gr, gg, gb;
    UnpackRgb(solid_rgb_, &sr, &sg, &sb);
    UnpackRgb(glass_rgb_, &gr, &gg, &gb);
    const float br = 0.12f, bg = 0.12f, bb = 0.14f;

    if(mode_ == Mode::Box)
    {
        for(int face_i = 0; face_i < SceneProp3D::kFaceCount; ++face_i)
        {
            Vector3D corners[4];
            SceneProp3D::FaceLocalCorners((ScenePropFace)face_i, hw, hh, hd, corners);
            float r = sr, g = sg, b = sb;
            if(faces_[face_i] == ScenePropFaceKind::Glass)
            {
                r = gr; g = gg; b = gb;
            }
            else if(faces_[face_i] == ScenePropFaceKind::Blocker)
            {
                r = br; g = bg; b = bb;
            }
            MeshGeometry::PushQuadAsTris(faces_cpu_,
                                         corners[0].x, corners[0].y, corners[0].z,
                                         corners[1].x, corners[1].y, corners[1].z,
                                         corners[2].x, corners[2].y, corners[2].z,
                                         corners[3].x, corners[3].y, corners[3].z,
                                         r, g, b);
        }
    }
    else if(mode_ == Mode::Mesh)
    {
        const MeshImport::TriangleMesh* mesh =
            ScenePropMeshCache::instance()->GetOrLoad(mesh_path_.toStdString());
        if(mesh && !mesh->empty())
        {
            float r = sr, g = sg, b = sb;
            if(mesh_body_ == ScenePropFaceKind::Glass)
            {
                r = gr; g = gg; b = gb;
            }
            else if(mesh_body_ == ScenePropFaceKind::Blocker)
            {
                r = br; g = bg; b = bb;
            }
            float sx = 0.0f, sy = 0.0f, sz = 0.0f;
            float cx = 0.0f, cy = 0.0f, cz = 0.0f;
            MeshImport::AabbExtents(*mesh, &sx, &sy, &sz);
            MeshImport::AabbCenter(*mesh, &cx, &cy, &cz);
            const float scale_x = (2.0f * hw) / std::max(sx, 1e-6f);
            const float scale_y = (2.0f * hh) / std::max(sy, 1e-6f);
            const float scale_z = (2.0f * hd) / std::max(sz, 1e-6f);

            constexpr size_t kMaxPreviewTris = 80000;
            const size_t tri_count = mesh->triangleCount();
            const size_t step = (tri_count > kMaxPreviewTris)
                                    ? std::max<size_t>(1, tri_count / kMaxPreviewTris)
                                    : 1;
            for(size_t t = 0; t < tri_count; t += step)
            {
                const size_t i = t * 9;
                if(i + 8 >= mesh->positions.size())
                {
                    break;
                }
                for(size_t v = 0; v < 3; ++v)
                {
                    const size_t vi = i + v * 3;
                    const float x = (mesh->positions[vi] - cx) * scale_x;
                    const float y = (mesh->positions[vi + 1] - cy) * scale_y;
                    const float z = (mesh->positions[vi + 2] - cz) * scale_z;
                    MeshGeometry::PushPosColor(faces_cpu_, x, y, z, r, g, b);
                }
            }
        }
    }

    MeshGeometry::AppendAxisAlignedBoxEdges(edges_cpu_, -hw, -hh, -hd, hw, hh, hd,
                                            0.85f, 0.85f, 0.55f);

    if(!faces_cpu_.empty())
    {
        faces_batch_.Upload(MeshBatch::Layout::PosColor, faces_cpu_.data(), faces_cpu_.size() / 6);
    }
    else
    {
        faces_batch_.Destroy();
    }
    if(!edges_cpu_.empty())
    {
        edges_batch_.Upload(MeshBatch::Layout::PosColor, edges_cpu_.data(), edges_cpu_.size() / 6);
    }
    else
    {
        edges_batch_.Destroy();
    }
}

void ScenePropPreviewWidget::drawBatch(const MeshBatch& batch, MeshBatch::Primitive primitive, float alpha)
{
    if(!unlit_.IsValid() || !batch.IsValid())
    {
        return;
    }
    const float aspect = std::max(0.1f, (float)width() / std::max(1, height()));
    const ViewportMat4 projection = ViewportMath::Perspective(40.0f, aspect, 0.05f, 100000.0f);
    const float yaw = yaw_deg_ * 3.14159265f / 180.0f;
    const float pitch = pitch_deg_ * 3.14159265f / 180.0f;
    const ViewportVec3 eye = {
        distance_ * std::cos(pitch) * std::sin(yaw),
        distance_ * std::sin(pitch),
        distance_ * std::cos(pitch) * std::cos(yaw),
    };
    const ViewportMat4 view = ViewportMath::LookAt(eye, {0, 0, 0}, {0, 1, 0});
    const ViewportMat4 mvp = ViewportMath::Multiply(projection, view);

    if(primitive == MeshBatch::Primitive::Lines)
    {
        glLineWidth(1.5f);
    }
    unlit_.Bind();
    unlit_.SetUniformMat4("u_mvp", mvp.m);
    unlit_.SetUniform1f("u_alpha", alpha);
    batch.Draw(primitive);
    unlit_.Unbind();
    if(primitive == MeshBatch::Primitive::Lines)
    {
        glLineWidth(1.0f);
    }
}

void ScenePropPreviewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(mode_ == Mode::Empty || !gl_ready_)
    {
        return;
    }
    const bool mesh_glass = (mode_ == Mode::Mesh && mesh_body_ == ScenePropFaceKind::Glass);
    if(mesh_glass)
    {
        glDepthMask(GL_FALSE);
    }
    drawBatch(faces_batch_, MeshBatch::Primitive::Triangles, mesh_glass ? 0.28f : 0.72f);
    if(mesh_glass)
    {
        glDepthMask(GL_TRUE);
    }
    drawBatch(edges_batch_, MeshBatch::Primitive::Lines, 0.9f);
}

void ScenePropPreviewWidget::mousePressEvent(QMouseEvent* event)
{
    last_mouse_ = event->pos();
}

void ScenePropPreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(!(event->buttons() & Qt::LeftButton))
    {
        return;
    }
    const QPoint d = event->pos() - last_mouse_;
    last_mouse_ = event->pos();
    yaw_deg_ += d.x() * 0.4f;
    pitch_deg_ = std::clamp(pitch_deg_ - d.y() * 0.4f, -85.0f, 85.0f);
    update();
}

void ScenePropPreviewWidget::wheelEvent(QWheelEvent* event)
{
    const float steps = event->angleDelta().y() / 120.0f;
    distance_ = std::clamp(distance_ * std::pow(0.9f, steps), 0.5f, 5000.0f);
    update();
}
