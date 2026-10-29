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
        case GIZMO_MODE_SCALE:    return "Scale";
        case GIZMO_MODE_SKEW:     return "Skew";
        case GIZMO_MODE_FREEROAM: return "Freeroam";
        default:                  return "Move";
    }
    return "Unknown";
}
} // namespace


void LEDViewport3D::paintGL()
{
    /* Field-engine Sessions bind their own FBOs on the shared 4.1 Core pool.
       Rebind the widget FBO and viewport every paint so atlas work cannot leave
       draws targeting the wrong framebuffer. */
    if(QOpenGLContext* ctx = context())
    {
        if(QOpenGLExtraFunctions* xf = ctx->extraFunctions())
        {
            xf->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        }
    }
    const int fb_w = viewportFramebufferWidth(width());
    const int fb_h = viewportFramebufferHeight(height());
    glViewport(0, 0, fb_w, fb_h);

    if(!viewport_shader_programs_ok_
       || !gl_prog_unlit_color_.IsValid()
       || !gl_prog_unlit_point_.IsValid()
       || !gl_prog_textured_unlit_.IsValid())
    {
        initViewportShaderPrograms();
    }

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

    DrawRoomFloor();
    DrawRoomWalls();
    DrawGrid();
    DrawAxes();
    DrawRoomBoundary();
    if(room_viewport_selected_)
    {
        DrawRoomViewportSelection();
    }

    DrawDisplayPlanes();
    DrawSceneProps();
    if(show_room_grid_overlay)
    {
        DrawRoomGridOverlay();
    }
    DrawControllers();
    DrawLightBlockerLayers();
    DrawUserFigure();
    DrawReferencePoints();
    DrawEffectVolume();

    const bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                          selected_controller_idx < (int)controller_transforms->size());
    const bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                         selected_ref_point_idx < (int)reference_points->size());
    const bool has_display_plane_selected = (selected_display_plane_idx >= 0 && display_planes &&
                                             selected_display_plane_idx < (int)display_planes->size());
    const bool has_scene_prop_selected = (selected_scene_prop_idx_ >= 0 && scene_props_ &&
                                          selected_scene_prop_idx_ < (int)scene_props_->size());

    if(has_controller_selected || has_ref_point_selected || has_display_plane_selected
       || has_scene_prop_selected || effect_volume_active_)
    {
        syncGizmoScreenScale();
        drawGizmo();
    }
}

float LEDViewport3D::selectedObjectGizmoRadius() const
{
    /* Median of three extents — long thin runs grow a little without matching length. */
    auto median3 = [](float a, float b, float c) {
        if(a > b) std::swap(a, b);
        if(b > c) std::swap(b, c);
        if(a > b) std::swap(a, b);
        return b;
    };

    if(effect_volume_active_)
    {
        const Vector3D& p = effect_volume_.transform.position;
        const Vector3D& s = effect_volume_.transform.scale;
        float hx = std::fabs(s.x);
        float hy = std::fabs(s.y);
        float hz = std::fabs(s.z);
        /* Prefer the volume∩grid box so cover-from-origin domains don't inflate the gizmo. */
        if(effect_volume_grid_)
        {
            const GridContext3D& g = *effect_volume_grid_;
            const float x0 = std::max(p.x - hx, g.min_x);
            const float x1 = std::min(p.x + hx, g.max_x);
            const float y0 = std::max(p.y - hy, g.min_y);
            const float y1 = std::min(p.y + hy, g.max_y);
            const float z0 = std::max(p.z - hz, g.min_z);
            const float z1 = std::min(p.z + hz, g.max_z);
            hx = 0.5f * std::max(0.0f, x1 - x0);
            hy = 0.5f * std::max(0.0f, y1 - y0);
            hz = 0.5f * std::max(0.0f, z1 - z0);
        }
        return median3(hx, hy, hz);
    }

    if(selected_scene_prop_idx_ >= 0 && scene_props_ &&
       selected_scene_prop_idx_ < (int)scene_props_->size())
    {
        SceneProp3D* prop = (*scene_props_)[(size_t)selected_scene_prop_idx_].get();
        if(prop)
        {
            const Transform3D& t = prop->GetTransform();
            const float w = MMToGridUnits(prop->GetWidthMM(), grid_scale_mm) * std::max(t.scale.x, 0.001f);
            const float h = MMToGridUnits(prop->GetHeightMM(), grid_scale_mm) * std::max(t.scale.y, 0.001f);
            const float d = MMToGridUnits(prop->GetDepthMM(), grid_scale_mm) * std::max(t.scale.z, 0.001f);
            return median3(0.5f * w, 0.5f * h, 0.5f * d);
        }
    }

    if(selected_display_plane_idx >= 0 && display_planes &&
       selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            const float w = MMToGridUnits(plane->GetWidthMM(), grid_scale_mm);
            const float h = MMToGridUnits(plane->GetHeightMM(), grid_scale_mm);
            return 0.5f * std::max(w, h);
        }
    }

    if(selected_controller_idx >= 0 && controller_transforms &&
       selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl && !ctrl->hidden_by_virtual)
        {
            const Vector3D size = GetControllerSize(ctrl);
            const float sx = size.x * std::max(ctrl->transform.scale.x, 0.001f);
            const float sy = size.y * std::max(ctrl->transform.scale.y, 0.001f);
            const float sz = size.z * std::max(ctrl->transform.scale.z, 0.001f);
            return median3(0.5f * sx, 0.5f * sy, 0.5f * sz);
        }
    }

    /* Ref points and unknown targets stay screen-constant. */
    return 0.0f;
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

    if(!std::isfinite(depth) || depth < 0.01f)
    {
        depth = 0.01f;
    }
    if(depth > 5000.0f)
    {
        depth = 5000.0f;
    }
    float object_radius = selectedObjectGizmoRadius();
    if(!std::isfinite(object_radius) || object_radius < 0.0f)
    {
        object_radius = 0.0f;
    }
    gizmo.SetObjectRadius(object_radius);
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

    /* Floor slightly below Y=0 to avoid z-fighting the grid lines. */
    {
        std::vector<float> floor;
        floor.reserve(6 * 6);
        const float y = -0.002f;
        const float fr = 0.0f;
        const float fg = 0.8f;
        const float fb = 0.8f;
        MeshGeometry::PushQuadAsTris(floor,
                                     0.0f, y, 0.0f,
                                     max_x, y, 0.0f,
                                     max_x, y, max_z,
                                     0.0f, y, max_z,
                                     fr, fg, fb);
        floor_fill_batch_.Upload(MeshBatch::Layout::PosColor, floor.data(), floor.size() / 6);
    }

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
        float r = 0.16f;
        float g = 0.22f;
        float b = 0.21f;
        if(i == 0)
        {
            r = 0.48f;
            g = 0.30f;
            b = 0.28f;
        }
        else if(i % 5 == 0)
        {
            r = 0.24f;
            g = 0.32f;
            b = 0.30f;
        }
        push_line((float)i, 0.0f, 0.0f, (float)i, 0.0f, max_z, r, g, b);
    }

    for(int i = 0; i <= (int)max_z; i++)
    {
        float r = 0.16f;
        float g = 0.22f;
        float b = 0.21f;
        if(i == 0)
        {
            r = 0.28f;
            g = 0.32f;
            b = 0.45f;
        }
        else if(i % 5 == 0)
        {
            r = 0.24f;
            g = 0.32f;
            b = 0.30f;
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

    const float border_r = 0.28f;
    const float border_g = 0.40f;
    const float border_b = 0.38f;
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

void LEDViewport3D::DrawRoomFloor()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;
    if(cached_floor_grid_max_x != max_x || cached_floor_grid_max_z != max_z || !floor_fill_batch_.IsValid())
    {
        RebuildFloorGridCache(extents);
    }

    drawUnlitBatch(floor_fill_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.14f);
    glDepthMask(GL_TRUE);
}

void LEDViewport3D::DrawRoomWalls()
{
    if(!use_manual_room_dimensions)
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    if(room_boundary_cached_max_x_ != max_x || room_boundary_cached_max_y_ != max_y ||
       room_boundary_cached_max_z_ != max_z || !room_walls_batch_.IsValid() || !room_boundary_batch_.IsValid())
    {
        RebuildRoomShellCache(extents);
    }

    drawUnlitBatch(room_walls_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.14f);
    glDepthMask(GL_TRUE);
}

void LEDViewport3D::RebuildRoomShellCache(const GridExtents& extents)
{
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    const float wr = 0.34f;
    const float wg = 0.33f;
    const float wb = 0.32f;
    std::vector<float> walls;
    walls.reserve(5 * 6 * 6);
    MeshGeometry::PushQuadAsTris(walls,
                                 0.0f, 0.0f, 0.0f,
                                 max_x, 0.0f, 0.0f,
                                 max_x, max_y, 0.0f,
                                 0.0f, max_y, 0.0f,
                                 wr, wg, wb);
    MeshGeometry::PushQuadAsTris(walls,
                                 max_x, 0.0f, max_z,
                                 0.0f, 0.0f, max_z,
                                 0.0f, max_y, max_z,
                                 max_x, max_y, max_z,
                                 wr, wg, wb);
    MeshGeometry::PushQuadAsTris(walls,
                                 0.0f, 0.0f, max_z,
                                 0.0f, 0.0f, 0.0f,
                                 0.0f, max_y, 0.0f,
                                 0.0f, max_y, max_z,
                                 wr, wg, wb);
    MeshGeometry::PushQuadAsTris(walls,
                                 max_x, 0.0f, 0.0f,
                                 max_x, 0.0f, max_z,
                                 max_x, max_y, max_z,
                                 max_x, max_y, 0.0f,
                                 wr, wg, wb);
    MeshGeometry::PushQuadAsTris(walls,
                                 0.0f, max_y, 0.0f,
                                 max_x, max_y, 0.0f,
                                 max_x, max_y, max_z,
                                 0.0f, max_y, max_z,
                                 wr, wg, wb);
    room_walls_batch_.Upload(MeshBatch::Layout::PosColor, walls.data(), walls.size() / 6);

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

void LEDViewport3D::DrawGrid()
{
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;

    if(cached_floor_grid_max_x != max_x || cached_floor_grid_max_z != max_z || !floor_grid_batch_.IsValid())
    {
        RebuildFloorGridCache(extents);
    }

    drawUnlitBatch(floor_grid_batch_, MeshBatch::Primitive::Lines, 1.0f, 0.55f);
    drawUnlitBatch(floor_border_batch_, MeshBatch::Primitive::Lines, 1.5f, 0.75f);
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

    drawUnlitBatch(axes_lines_batch_, MeshBatch::Primitive::Lines, 2.0f, 0.85f);
    drawUnlitBatch(axes_heads_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.85f);
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
    const float r = 0.92f;
    const float g = 0.78f;
    const float b = 0.28f;

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

    drawUnlitBatch(room_sel_lines_batch_, MeshBatch::Primitive::Lines, 2.5f, 0.88f);
    drawUnlitBatch(room_sel_fill_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.10f);

    glDisable(GL_BLEND);
}

void LEDViewport3D::DrawEffectVolume()
{
    if(!effect_volume_active_)
    {
        return;
    }

    /* Selected volume: bright wireframe only (no face fill — a room-sized fill
       washed the LED grid cyan and lingered after remove). */
    const float sx = std::max(1e-4f, effect_volume_.transform.scale.x);
    const float sy = std::max(1e-4f, effect_volume_.transform.scale.y);
    const float sz = std::max(1e-4f, effect_volume_.transform.scale.z);
    const float wr = 0.45f;
    const float wg = 0.98f;
    const float wb = 1.0f;

    auto push_line = [&](std::vector<float>& out, float ax, float ay, float az, float bx, float by, float bz) {
        const Vector3D a = effect_volume_.LocalToWorld(ax, ay, az);
        const Vector3D b = effect_volume_.LocalToWorld(bx, by, bz);
        PushPosColor(out, a.x, a.y, a.z, wr, wg, wb);
        PushPosColor(out, b.x, b.y, b.z, wr, wg, wb);
    };

    std::vector<float> lines;
    const int shape = effect_volume_.shape;
    const int segs = 24;

    switch(shape)
    {
        case SpatialEffect3D::VOLUME_SHAPE_SPHERE:
        {
            for(int i = 0; i < segs; ++i)
            {
                const float a0 = 2.0f * (float)M_PI * ((float)i / (float)segs);
                const float a1 = 2.0f * (float)M_PI * ((float)(i + 1) / (float)segs);
                const float c0 = std::cos(a0);
                const float s0 = std::sin(a0);
                const float c1 = std::cos(a1);
                const float s1 = std::sin(a1);
                push_line(lines, sx * c0, 0.0f, sz * s0, sx * c1, 0.0f, sz * s1);
                push_line(lines, 0.0f, sy * c0, sz * s0, 0.0f, sy * c1, sz * s1);
                push_line(lines, sx * c0, sy * s0, 0.0f, sx * c1, sy * s1, 0.0f);
            }
            break;
        }
        case SpatialEffect3D::VOLUME_SHAPE_CYLINDER:
        {
            for(int i = 0; i < segs; ++i)
            {
                const float a0 = 2.0f * (float)M_PI * ((float)i / (float)segs);
                const float a1 = 2.0f * (float)M_PI * ((float)(i + 1) / (float)segs);
                const float x0 = sx * std::cos(a0);
                const float z0 = sz * std::sin(a0);
                const float x1 = sx * std::cos(a1);
                const float z1 = sz * std::sin(a1);
                push_line(lines, x0,  sy, z0, x1,  sy, z1);
                push_line(lines, x0, -sy, z0, x1, -sy, z1);
                if((i % 4) == 0)
                {
                    push_line(lines, x0, -sy, z0, x0, sy, z0);
                }
            }
            break;
        }
        case SpatialEffect3D::VOLUME_SHAPE_CONE:
        {
            for(int i = 0; i < segs; ++i)
            {
                const float a0 = 2.0f * (float)M_PI * ((float)i / (float)segs);
                const float a1 = 2.0f * (float)M_PI * ((float)(i + 1) / (float)segs);
                const float x0 = sx * std::cos(a0);
                const float z0 = sz * std::sin(a0);
                const float x1 = sx * std::cos(a1);
                const float z1 = sz * std::sin(a1);
                push_line(lines, x0, -sy, z0, x1, -sy, z1);
                if((i % 4) == 0)
                {
                    push_line(lines, 0.0f, sy, 0.0f, x0, -sy, z0);
                }
            }
            break;
        }
        case SpatialEffect3D::VOLUME_SHAPE_TETRA:
        {
            const float v[4][3] = {
                { sx,  sy, -sz},
                { sx, -sy,  sz},
                {-sx,  sy,  sz},
                {-sx, -sy, -sz}
            };
            static const int edges[6][2] = {
                {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}
            };
            for(int e = 0; e < 6; ++e)
            {
                const float* a = v[edges[e][0]];
                const float* b = v[edges[e][1]];
                push_line(lines, a[0], a[1], a[2], b[0], b[1], b[2]);
            }
            break;
        }
        case SpatialEffect3D::VOLUME_SHAPE_BOX:
        default:
        {
            /* Always draw the true local OBB (same frame as SampleVolumeLocalUv01).
               Clipping to room∩AABB shifted the wireframe off the effect origin. */
            Vector3D corners[8];
            effect_volume_.GetCorners(corners);
            static const int edges[12][2] = {
                {0, 1}, {2, 3}, {4, 5}, {6, 7},
                {0, 2}, {1, 3}, {4, 6}, {5, 7},
                {0, 4}, {1, 5}, {2, 6}, {3, 7}
            };
            lines.reserve(12 * 2 * 6);
            for(int e = 0; e < 12; ++e)
            {
                const Vector3D& a = corners[edges[e][0]];
                const Vector3D& c = corners[edges[e][1]];
                PushPosColor(lines, a.x, a.y, a.z, wr, wg, wb);
                PushPosColor(lines, c.x, c.y, c.z, wr, wg, wb);
            }
            break;
        }
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if(!lines.empty())
    {
        effect_volume_lines_batch_.Upload(MeshBatch::Layout::PosColor, lines.data(), lines.size() / 6);
        drawUnlitBatch(effect_volume_lines_batch_, MeshBatch::Primitive::Lines, 2.4f, 0.92f);
    }
}

void LEDViewport3D::DrawRoomBoundary()
{
    if(!use_manual_room_dimensions)
    {
        return;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    if(room_boundary_cached_max_x_ != max_x || room_boundary_cached_max_y_ != max_y ||
       room_boundary_cached_max_z_ != max_z || !room_boundary_batch_.IsValid())
    {
        RebuildRoomShellCache(extents);
    }

    drawUnlitBatch(room_boundary_batch_, MeshBatch::Primitive::Lines, 2.0f, 1.0f);
}

