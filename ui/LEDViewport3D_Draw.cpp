// SPDX-License-Identifier: GPL-2.0-only

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <cmath>
#include <algorithm>
#include <cstring>

#include "LEDViewport3D.h"
#include "viewport/ViewportMath.h"
#include "viewport/ViewportGLFormat.h"
#include "viewport/MeshGeometry.h"

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
using MeshGeometry::PushPosColor;
void ProjectPointToScreen(float x,
                                 float y,
                                 float z,
                                 const float modelview[16],
                                 const float projection[16],
                                 const int viewport[4],
                                 double& screen_x,
                                 double& screen_y)
{
    ViewportMat4 mv;
    ViewportMat4 proj;
    std::memcpy(mv.m, modelview, sizeof(float) * 16);
    std::memcpy(proj.m, projection, sizeof(float) * 16);
    float sx = 0.0f;
    float sy = 0.0f;
    if(!ViewportMath::ProjectWorldToScreen(mv, proj, viewport, x, y, z, sx, sy))
    {
        screen_x = 0.0;
        screen_y = 0.0;
        return;
    }
    screen_x = (double)sx;
    screen_y = (double)sy;
}

/* Map framebuffer top-down coords (from ProjectWorldToScreen) into Qt logical widget space. */
void GlWindowPointToQtLogical(const QWidget* widget, const GLint vp[4], double gl_x, double gl_y, double& qt_x, double& qt_y)
{
    qt_x = (gl_x - (double)vp[0]) * (double)std::max(1, widget->width()) / (double)std::max(1, vp[2]);
    qt_y = (gl_y - (double)vp[1]) * (double)std::max(1, widget->height()) / (double)std::max(1, vp[3]);
}

const char* GizmoModeLabel(GizmoMode mode)
{
    switch(mode)
    {
        case GIZMO_MODE_MOVE:     return "Move";
        case GIZMO_MODE_ROTATE:   return "Rotate";
        case GIZMO_MODE_FREEROAM: return "Freeroam";
    }
    return "Unknown";
}
} // namespace


void LEDViewport3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(!viewport_paint_enabled_ || width() < 2 || height() < 2)
    {
        return;
    }

    paintGlScene();
    paintViewportText2D();
}

void LEDViewport3D::paintGlScene()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    const ViewportFrame frame = BuildViewportFrame();
    syncPickMatricesFromFrame(frame);

    DrawGrid();
    DrawAxes();
    DrawRoomBoundary();
    if(room_viewport_selected_)
    {
        DrawRoomViewportSelection();
    }

    DrawDisplayPlanes();
    if(show_room_grid_overlay)
    {
        DrawRoomGridOverlay();
    }
    DrawControllers();
    DrawLightBlockerLayers();
    DrawUserFigure();
    DrawReferencePoints();

    const bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                          selected_controller_idx < (int)controller_transforms->size());
    const bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                         selected_ref_point_idx < (int)reference_points->size());
    const bool has_display_plane_selected = (selected_display_plane_idx >= 0 && display_planes &&
                                             selected_display_plane_idx < (int)display_planes->size());

    if(has_controller_selected || has_ref_point_selected || has_display_plane_selected)
    {
        syncGizmoScreenScale();
        drawGizmo();
    }
}

void LEDViewport3D::syncGizmoScreenScale()
{
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    gizmo.GetPosition(gx, gy, gz);

    /* View-space |z| from the scene matrix used to draw the gizmo. */
    float depth = camera_distance;
    if(pick_matrices_valid_)
    {
        const float* mv = pick_scene_modelview_;
        const float vz = mv[2] * gx + mv[6] * gy + mv[10] * gz + mv[14];
        depth = std::fabs(vz);
    }

    if(depth < 0.01f)
    {
        depth = 0.01f;
    }
    gizmo.SetScreenScale(depth, ViewportGLFormat::kDefaultFovyDegrees);
}

void LEDViewport3D::drawGizmoMeshPass(const GizmoDrawMesh& mesh, const ViewportMat4& model)
{
    if(!mesh.triangle_interleaved.empty() && (mesh.triangle_interleaved.size() % 6) == 0)
    {
        const size_t vert_count = mesh.triangle_interleaved.size() / 6;
        gizmo_tris_batch_.Upload(MeshBatch::Layout::PosColor,
                                 mesh.triangle_interleaved.data(),
                                 vert_count);
        drawUnlitBatch(gizmo_tris_batch_, MeshBatch::Primitive::Triangles, 1.0f, 1.0f, &model);
    }

    for(const GizmoDrawMesh::LineBatch& line_batch : mesh.line_batches)
    {
        if(line_batch.interleaved.empty() || (line_batch.interleaved.size() % 6) != 0)
        {
            continue;
        }
        const size_t vert_count = line_batch.interleaved.size() / 6;
        gizmo_lines_batch_.Upload(MeshBatch::Layout::PosColor,
                                  line_batch.interleaved.data(),
                                  vert_count);
        drawUnlitBatch(gizmo_lines_batch_, MeshBatch::Primitive::Lines, line_batch.width, 1.0f, &model);
    }
}

void LEDViewport3D::drawGizmo()
{
    if(!gizmo.IsActive() || !viewport_shader_programs_ok_)
    {
        return;
    }

    GizmoDrawMesh mesh;
    gizmo.buildDrawMesh(mesh);
    if(mesh.triangle_interleaved.empty() && mesh.line_batches.empty())
    {
        return;
    }

    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    gizmo.GetPosition(gx, gy, gz);
    const ViewportMat4 model = ViewportMath::Translation(gx, gy, gz);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* Pass 1: depth-tested, no depth write (under overlay). */
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    drawGizmoMeshPass(mesh, model);

    /* Pass 2: overlay without depth so handles stay visible. */
    glDisable(GL_DEPTH_TEST);
    drawGizmoMeshPass(mesh, model);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void LEDViewport3D::paintViewportText2D()
{
    if(width() < 2 || height() < 2)
    {
        return;
    }

    const bool has_selection = (selected_controller_idx >= 0 && controller_transforms &&
                                selected_controller_idx < (int)controller_transforms->size()) ||
                               (selected_ref_point_idx >= 0 && reference_points &&
                                selected_ref_point_idx < (int)reference_points->size()) ||
                               (selected_display_plane_idx >= 0 && display_planes &&
                                selected_display_plane_idx < (int)display_planes->size());
    const bool need_rotate_readout =
        has_selection && gizmo.GetMode() == GIZMO_MODE_ROTATE && gizmo.IsDragging();
    const bool need_gizmo_toast = gizmo_mode_feedback_timer_.isActive();
    if(!show_room_guide_labels_ && !need_rotate_readout && !need_gizmo_toast)
    {
        return;
    }

    // Pick matrices were snapshotted in paintGlScene before any QPainter work.
    float modelview[16];
    float projection[16];
    int viewport[4];
    loadScenePickMatrices(modelview, projection, viewport);

    prepareForQtPainterInPaintGl();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    paintRoomGuideLabels(painter, modelview, projection, viewport);
    if(need_rotate_readout)
    {
        paintRotateReadout(painter, modelview, projection, viewport);
    }
    if(need_gizmo_toast)
    {
        paintGizmoModeToast(painter);
    }

    painter.end();
}


void LEDViewport3D::prepareForQtPainterInPaintGl()
{
    if(!isValid())
    {
        return;
    }

    makeCurrent();

    /* Qt 6 has no QOpenGLWidget::resetOpenGLState(); manual Core-safe reset before QPainter. */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if(QOpenGLContext* ctx = context())
    {
        if(QOpenGLExtraFunctions* xf = ctx->extraFunctions())
        {
            xf->glBindVertexArray(0);
            xf->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        }
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void LEDViewport3D::paintRotateReadout(QPainter& painter,
                                       const float modelview[16],
                                       const float projection[16],
                                       const int viewport[4]) const
{
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    gizmo.GetPosition(gx, gy, gz);
    double screen_x = 0.0;
    double screen_y = 0.0;
    ProjectPointToScreen(gx, gy, gz, modelview, projection, viewport, screen_x, screen_y);
    double label_x = 0.0;
    double label_y = 0.0;
    const GLint viewport_i[4] = {
        (GLint)viewport[0], (GLint)viewport[1], (GLint)viewport[2], (GLint)viewport[3]
    };
    GlWindowPointToQtLogical(this, viewport_i, screen_x, screen_y, label_x, label_y);

    painter.setPen(QColor(255, 255, 255));
    painter.setFont(QFont("Arial", 10, QFont::Bold));

    const char axis_char = (gizmo.GetSelectedAxis() == GIZMO_AXIS_X) ? 'X' :
                           (gizmo.GetSelectedAxis() == GIZMO_AXIS_Y) ? 'Y' : 'Z';
    const QString snap_text = gizmo.IsRotateSnapActive() ? " | Snap 15 deg" : " | Hold Shift to snap 15 deg";
    const QString text = QString("Rotate %1: %2 deg%3")
                           .arg(QChar(axis_char),
                                QString::number(gizmo.GetRotateAccumDegrees(), 'f', 1),
                                snap_text);
    painter.drawText(QPointF(label_x + 12.0, label_y - 12.0), text);
}

void LEDViewport3D::paintGizmoModeToast(QPainter& painter) const
{
    const QString text = QString("Gizmo: %1").arg(QString::fromLatin1(GizmoModeLabel(gizmo.GetMode())));
    const QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);
    const QRect text_bounds = QFontMetrics(font).boundingRect(text);
    const QRectF panel(12.0, 12.0, text_bounds.width() + 24.0, text_bounds.height() + 14.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 24, 30, 205));
    painter.drawRoundedRect(panel, 5.0, 5.0);
    painter.setPen(QColor(245, 245, 245));
    painter.drawText(panel, Qt::AlignCenter, text);
}

float LEDViewport3D::ledPreviewPointSizeGl() const
{
    float size = 220.0f / std::max(camera_distance, 4.0f);
    return std::max(4.0f, std::min(12.0f, size));
}

void LEDViewport3D::drawUnlitBatch(const MeshBatch& batch,
                                   MeshBatch::Primitive primitive,
                                   float line_width,
                                   float alpha,
                                   const ViewportMat4* model)
{
    if(!viewport_shader_programs_ok_ || !batch.IsValid() || !gl_prog_unlit_color_.IsValid())
    {
        return;
    }

    ViewportMat4 projection;
    ViewportMat4 view;
    std::memcpy(projection.m, pick_projection_, sizeof(float) * 16);
    std::memcpy(view.m, pick_scene_modelview_, sizeof(float) * 16);
    const ViewportMat4 mvp = model
        ? ViewportMath::ModelViewProjection(projection, view, *model)
        : ViewportMath::Multiply(projection, view);

    if(primitive == MeshBatch::Primitive::Lines)
    {
        glLineWidth(line_width);
    }

    gl_prog_unlit_color_.Bind();
    gl_prog_unlit_color_.SetUniformMat4("u_mvp", mvp.m);
    gl_prog_unlit_color_.SetUniform1f("u_alpha", alpha);
    batch.Draw(primitive);
    gl_prog_unlit_color_.Unbind();

    if(primitive == MeshBatch::Primitive::Lines)
    {
        glLineWidth(1.0f);
    }
}

void LEDViewport3D::drawUnlitPoints(const MeshBatch& batch,
                                    float point_size,
                                    float alpha,
                                    const ViewportMat4& model)
{
    if(!viewport_shader_programs_ok_ || !batch.IsValid() || !gl_prog_unlit_point_.IsValid())
    {
        return;
    }

    ViewportMat4 projection;
    ViewportMat4 view;
    std::memcpy(projection.m, pick_projection_, sizeof(float) * 16);
    std::memcpy(view.m, pick_scene_modelview_, sizeof(float) * 16);
    const ViewportMat4 mvp = ViewportMath::ModelViewProjection(projection, view, model);

    glEnable(GL_PROGRAM_POINT_SIZE);
    gl_prog_unlit_point_.Bind();
    gl_prog_unlit_point_.SetUniformMat4("u_mvp", mvp.m);
    gl_prog_unlit_point_.SetUniform1f("u_point_size", point_size);
    gl_prog_unlit_point_.SetUniform1f("u_alpha", alpha);
    batch.Draw(MeshBatch::Primitive::Points);
    gl_prog_unlit_point_.Unbind();
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void LEDViewport3D::drawTexturedBatch(const MeshBatch& batch,
                                      unsigned int texture_id,
                                      float alpha,
                                      const ViewportMat4* model)
{
    if(!viewport_shader_programs_ok_ || !batch.IsValid() || !gl_prog_textured_unlit_.IsValid() || texture_id == 0)
    {
        return;
    }

    ViewportMat4 projection;
    ViewportMat4 view;
    std::memcpy(projection.m, pick_projection_, sizeof(float) * 16);
    std::memcpy(view.m, pick_scene_modelview_, sizeof(float) * 16);
    const ViewportMat4 mvp = model
        ? ViewportMath::ModelViewProjection(projection, view, *model)
        : ViewportMath::Multiply(projection, view);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    gl_prog_textured_unlit_.Bind();
    gl_prog_textured_unlit_.SetUniformMat4("u_mvp", mvp.m);
    gl_prog_textured_unlit_.SetUniform1i("u_texture", 0);
    gl_prog_textured_unlit_.SetUniform1f("u_alpha", alpha);
    batch.Draw(MeshBatch::Primitive::Triangles);
    gl_prog_textured_unlit_.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void LEDViewport3D::RebuildFloorGridCache(const GridExtents& extents)
{
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;
    cached_floor_grid_max_x = max_x;
    cached_floor_grid_max_z = max_z;

    cached_floor_grid_interleaved_.clear();
    const int line_count = ((int)max_x + 1) + ((int)max_z + 1);
    cached_floor_grid_interleaved_.reserve((size_t)line_count * 2 * 6);

    auto push_line = [&](float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b) {
        cached_floor_grid_interleaved_.push_back(x0);
        cached_floor_grid_interleaved_.push_back(y0);
        cached_floor_grid_interleaved_.push_back(z0);
        cached_floor_grid_interleaved_.push_back(r);
        cached_floor_grid_interleaved_.push_back(g);
        cached_floor_grid_interleaved_.push_back(b);
        cached_floor_grid_interleaved_.push_back(x1);
        cached_floor_grid_interleaved_.push_back(y1);
        cached_floor_grid_interleaved_.push_back(z1);
        cached_floor_grid_interleaved_.push_back(r);
        cached_floor_grid_interleaved_.push_back(g);
        cached_floor_grid_interleaved_.push_back(b);
    };

    for(int i = 0; i <= (int)max_x; i++)
    {
        float r = 0.22f;
        float g = 0.24f;
        float b = 0.28f;
        if(i == 0)
        {
            r = 0.55f;
            g = 0.32f;
            b = 0.32f;
        }
        else if(i % 5 == 0)
        {
            r = 0.38f;
            g = 0.40f;
            b = 0.44f;
        }
        push_line((float)i, 0.0f, 0.0f, (float)i, 0.0f, max_z, r, g, b);
    }

    for(int i = 0; i <= (int)max_z; i++)
    {
        float r = 0.22f;
        float g = 0.24f;
        float b = 0.28f;
        if(i == 0)
        {
            r = 0.30f;
            g = 0.34f;
            b = 0.55f;
        }
        else if(i % 5 == 0)
        {
            r = 0.38f;
            g = 0.40f;
            b = 0.44f;
        }
        push_line(0.0f, 0.0f, (float)i, max_x, 0.0f, (float)i, r, g, b);
    }

    if(!cached_floor_grid_interleaved_.empty())
    {
        floor_grid_batch_.Upload(MeshBatch::Layout::PosColor,
                                 cached_floor_grid_interleaved_.data(),
                                 cached_floor_grid_interleaved_.size() / 6);
    }
    else
    {
        floor_grid_batch_.Destroy();
    }

    const float border_r = 0.45f;
    const float border_g = 0.55f;
    const float border_b = 0.48f;
    const float border[] = {
        0.0f, 0.0f, 0.0f, border_r, border_g, border_b,
        max_x, 0.0f, 0.0f, border_r, border_g, border_b,
        max_x, 0.0f, 0.0f, border_r, border_g, border_b,
        max_x, 0.0f, max_z, border_r, border_g, border_b,
        max_x, 0.0f, max_z, border_r, border_g, border_b,
        0.0f, 0.0f, max_z, border_r, border_g, border_b,
        0.0f, 0.0f, max_z, border_r, border_g, border_b,
        0.0f, 0.0f, 0.0f, border_r, border_g, border_b,
    };
    floor_border_batch_.Upload(MeshBatch::Layout::PosColor, border, 8);
}

void LEDViewport3D::DrawGrid()
{
    glDepthMask(GL_TRUE);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;

    if(cached_floor_grid_max_x != max_x || cached_floor_grid_max_z != max_z || !floor_grid_batch_.IsValid())
    {
        RebuildFloorGridCache(extents);
    }

    drawUnlitBatch(floor_grid_batch_, MeshBatch::Primitive::Lines, 1.0f, 1.0f);
    drawUnlitBatch(floor_border_batch_, MeshBatch::Primitive::Lines, 1.5f, 1.0f);
}

void LEDViewport3D::DrawAxes()
{

    if(!axes_lines_batch_.IsValid())
    {
        const float lines[] = {
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 1.0f,
        };
        axes_lines_batch_.Upload(MeshBatch::Layout::PosColor, lines, 6);
    }
    if(!axes_heads_batch_.IsValid())
    {
        const float heads[] = {
            3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            2.7f, 0.15f, 0.0f, 1.0f, 0.0f, 0.0f,
            2.7f, -0.15f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.15f, 2.7f, 0.0f, 0.0f, 1.0f, 0.0f,
            -0.15f, 2.7f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 1.0f,
            0.15f, 0.0f, 2.7f, 0.0f, 0.0f, 1.0f,
            -0.15f, 0.0f, 2.7f, 0.0f, 0.0f, 1.0f,
        };
        axes_heads_batch_.Upload(MeshBatch::Layout::PosColor, heads, 9);
    }

    drawUnlitBatch(axes_lines_batch_, MeshBatch::Primitive::Lines, 3.0f, 1.0f);
    drawUnlitBatch(axes_heads_batch_, MeshBatch::Primitive::Triangles, 1.0f, 1.0f);
}

void LEDViewport3D::paintRoomGuideLabels(QPainter& painter,
                                         const float modelview[16],
                                         const float projection[16],
                                         const int viewport[4]) const
{
    if(!show_room_guide_labels_)
    {
        return;
    }

    QFont font("Arial", 10, QFont::Bold);
    painter.setFont(font);
    const QFontMetrics fm(font);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;
    const float cx = max_x * 0.5f;
    const float cy = max_y * 0.5f;
    const float cz = max_z * 0.5f;

    double screen_x = 0.0;
    double screen_y = 0.0;
    double label_x = 0.0;
    double label_y = 0.0;
    const GLint viewport_gl[4] = {
        (GLint)viewport[0], (GLint)viewport[1], (GLint)viewport[2], (GLint)viewport[3]
    };

    QVector<QRectF> placed;
    placed.reserve(8);
    const qreal label_pad = 6.0;

    auto overlaps_placed = [&](const QRectF& candidate) {
        const QRectF padded = candidate.adjusted(-label_pad, -label_pad, label_pad, label_pad);
        for(const QRectF& existing : placed)
        {
            if(existing.intersects(padded))
            {
                return true;
            }
        }
        return false;
    };

    auto try_draw_label = [&](float wx,
                              float wy,
                              float wz,
                              const QColor& color,
                              const QString& text,
                              qreal offset_x,
                              qreal offset_y,
                              bool multiline = false) -> bool {
        ProjectPointToScreen(wx, wy, wz, modelview, projection, viewport, screen_x, screen_y);
        GlWindowPointToQtLogical(this, viewport_gl, screen_x, screen_y, label_x, label_y);
        const QPointF anchor(label_x + offset_x, label_y + offset_y);
        QRectF bounds;
        if(multiline)
        {
            const int ax = (int)std::lround(anchor.x());
            const int ay = (int)std::lround(anchor.y());
            const int box_h = (int)std::lround(fm.lineSpacing() * 2.6);
            bounds = QRectF(fm.boundingRect(QRect(ax, ay, 220, box_h), Qt::AlignLeft | Qt::AlignTop, text));
        }
        else
        {
            bounds = QRectF(fm.boundingRect(text));
            bounds.moveTopLeft(anchor);
        }
        if(bounds.width() < 1.0)
        {
            bounds.setWidth(1.0);
        }
        if(bounds.height() < 1.0)
        {
            bounds.setHeight(1.0);
        }
        if(overlaps_placed(bounds))
        {
            return false;
        }
        painter.setPen(color);
        if(multiline)
        {
            painter.drawText(bounds, Qt::AlignLeft | Qt::AlignTop, text);
        }
        else
        {
            painter.drawText(anchor, text);
        }
        placed.append(bounds);
        return true;
    };

    try_draw_label(0.0f, 0.0f, 0.0f, QColor(255, 255, 255),
                   QStringLiteral("Origin (0, 0, 0)\nFront-left floor"), 12.0, 10.0, true);

    try_draw_label(max_x, cy, cz, QColor(255, 100, 100), QString("Right wall (X=%1)").arg((int)max_x), 10.0, 0.0);
    try_draw_label(0.0f, cy, cz, QColor(255, 100, 100), QStringLiteral("Left wall (X=0)"), 10.0, 0.0);
    try_draw_label(cx, cy, max_z, QColor(100, 255, 100), QString("Back wall (Z=%1)").arg((int)max_z), 10.0, 0.0);
    try_draw_label(cx, cy, 0.0f, QColor(100, 255, 100), QStringLiteral("Front wall (Z=0)"), 10.0, 0.0);
    try_draw_label(cx, max_y, cz, QColor(100, 100, 255), QString("Ceiling (Y=%1)").arg((int)max_y), 10.0, 0.0);
    try_draw_label(cx, 0.0f, cz, QColor(100, 100, 255), QStringLiteral("Floor (Y=0)"), 10.0, 0.0);
}

void LEDViewport3D::DrawRoomViewportSelection()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;
    const float r = 1.0f;
    const float g = 0.85f;
    const float b = 0.1f;

    auto push_line = [](std::vector<float>& out, float x0, float y0, float z0, float x1, float y1, float z1,
                        float cr, float cg, float cb) {
        out.push_back(x0); out.push_back(y0); out.push_back(z0);
        out.push_back(cr); out.push_back(cg); out.push_back(cb);
        out.push_back(x1); out.push_back(y1); out.push_back(z1);
        out.push_back(cr); out.push_back(cg); out.push_back(cb);
    };

    std::vector<float> lines;
    lines.reserve(12 * 2 * 6);
    push_line(lines, 0.0f, 0.0f, 0.0f, max_x, 0.0f, 0.0f, r, g, b);
    push_line(lines, max_x, 0.0f, 0.0f, max_x, 0.0f, max_z, r, g, b);
    push_line(lines, max_x, 0.0f, max_z, 0.0f, 0.0f, max_z, r, g, b);
    push_line(lines, 0.0f, 0.0f, max_z, 0.0f, 0.0f, 0.0f, r, g, b);
    push_line(lines, 0.0f, max_y, 0.0f, max_x, max_y, 0.0f, r, g, b);
    push_line(lines, max_x, max_y, 0.0f, max_x, max_y, max_z, r, g, b);
    push_line(lines, max_x, max_y, max_z, 0.0f, max_y, max_z, r, g, b);
    push_line(lines, 0.0f, max_y, max_z, 0.0f, max_y, 0.0f, r, g, b);
    push_line(lines, 0.0f, 0.0f, 0.0f, 0.0f, max_y, 0.0f, r, g, b);
    push_line(lines, max_x, 0.0f, 0.0f, max_x, max_y, 0.0f, r, g, b);
    push_line(lines, max_x, 0.0f, max_z, max_x, max_y, max_z, r, g, b);
    push_line(lines, 0.0f, 0.0f, max_z, 0.0f, max_y, max_z, r, g, b);
    room_sel_lines_batch_.Upload(MeshBatch::Layout::PosColor, lines.data(), lines.size() / 6);

    const float fill[] = {
        0.0f, 0.0f, 0.0f, r, g, b,
        max_x, 0.0f, 0.0f, r, g, b,
        max_x, 0.0f, max_z, r, g, b,
        0.0f, 0.0f, 0.0f, r, g, b,
        max_x, 0.0f, max_z, r, g, b,
        0.0f, 0.0f, max_z, r, g, b,
    };
    room_sel_fill_batch_.Upload(MeshBatch::Layout::PosColor, fill, 6);

    drawUnlitBatch(room_sel_lines_batch_, MeshBatch::Primitive::Lines, 3.0f, 0.95f);
    drawUnlitBatch(room_sel_fill_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.12f);

    glDisable(GL_BLEND);
}

void LEDViewport3D::DrawRoomBoundary()
{
    if(!use_manual_room_dimensions)
    {
        return;
    }
    glEnable(GL_DEPTH_TEST);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    if(room_boundary_cached_max_x_ != max_x || room_boundary_cached_max_y_ != max_y ||
       room_boundary_cached_max_z_ != max_z || !room_boundary_batch_.IsValid())
    {
        const float r = 0.0f;
        const float g = 0.8f;
        const float b = 0.8f;
        auto push_line = [](std::vector<float>& out, float x0, float y0, float z0, float x1, float y1, float z1,
                            float cr, float cg, float cb) {
            out.push_back(x0); out.push_back(y0); out.push_back(z0);
            out.push_back(cr); out.push_back(cg); out.push_back(cb);
            out.push_back(x1); out.push_back(y1); out.push_back(z1);
            out.push_back(cr); out.push_back(cg); out.push_back(cb);
        };
        std::vector<float> lines;
        lines.reserve(12 * 2 * 6);
        push_line(lines, 0.0f, 0.0f, 0.0f, max_x, 0.0f, 0.0f, r, g, b);
        push_line(lines, max_x, 0.0f, 0.0f, max_x, 0.0f, max_z, r, g, b);
        push_line(lines, max_x, 0.0f, max_z, 0.0f, 0.0f, max_z, r, g, b);
        push_line(lines, 0.0f, 0.0f, max_z, 0.0f, 0.0f, 0.0f, r, g, b);
        push_line(lines, 0.0f, max_y, 0.0f, max_x, max_y, 0.0f, r, g, b);
        push_line(lines, max_x, max_y, 0.0f, max_x, max_y, max_z, r, g, b);
        push_line(lines, max_x, max_y, max_z, 0.0f, max_y, max_z, r, g, b);
        push_line(lines, 0.0f, max_y, max_z, 0.0f, max_y, 0.0f, r, g, b);
        push_line(lines, 0.0f, 0.0f, 0.0f, 0.0f, max_y, 0.0f, r, g, b);
        push_line(lines, max_x, 0.0f, 0.0f, max_x, max_y, 0.0f, r, g, b);
        push_line(lines, max_x, 0.0f, max_z, max_x, max_y, max_z, r, g, b);
        push_line(lines, 0.0f, 0.0f, max_z, 0.0f, max_y, max_z, r, g, b);
        room_boundary_batch_.Upload(MeshBatch::Layout::PosColor, lines.data(), lines.size() / 6);
        room_boundary_cached_max_x_ = max_x;
        room_boundary_cached_max_y_ = max_y;
        room_boundary_cached_max_z_ = max_z;
    }

    drawUnlitBatch(room_boundary_batch_, MeshBatch::Primitive::Lines, 2.0f, 1.0f);
}

