// SPDX-License-Identifier: GPL-2.0-only

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QOpenGLContext>

#include <cmath>
#include <algorithm>

#include "Colors.h"
#include "LEDViewport3D.h"
#include "LEDViewport3D_Internal.h"
#include "ControllerDisplayUtils.h"
#include "ControllerLayout3D.h"
#include "viewport/ViewportGLIncludes.h"
#include "VirtualReferencePoint3D.h"

#include <GL/glu.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace
{
void DrawAxisAlignedBoxFaces(float x0, float y0, float z0, float x1, float y1, float z1,
                                    float r, float g, float b, float alpha)
{
    /* Alpha faces must not write depth or interior LED points disappear when zoomed in. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(r, g, b, alpha);
    glBegin(GL_QUADS);

    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z1);

    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);

    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y0, z1);

    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y0, z1);

    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y0, z0);

    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y0, z1);

    glEnd();
    glDepthMask(GL_TRUE);
}

void ProjectPointToScreen(float x,
                                 float y,
                                 float z,
                                 const GLdouble modelview[16],
                                 const GLdouble projection[16],
                                 const GLint viewport[4],
                                 double& screen_x,
                                 double& screen_y)
{
    GLdouble winX;
    GLdouble winY;
    GLdouble winZ;
    gluProject(x, y, z, modelview, projection, viewport, &winX, &winY, &winZ);
    screen_x = winX;
    screen_y = viewport[3] - winY;
}

bool TryGetViewportGlobalLedIndex(RGBController* controller,
                                         unsigned int zone_idx,
                                         unsigned int led_idx,
                                         unsigned int* global_led_idx)
{
    if(!controller || !global_led_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_led_idx = controller->zones[zone_idx].start_idx + led_idx;
    return (*global_led_idx < controller->colors.size());
}

void GlWindowPointToQtLogical(const QWidget* widget, const GLint vp[4], double gl_x, double gl_y, double& qt_x, double& qt_y)
{
    qt_x = (gl_x - (double)vp[0]) * (double)std::max(1, widget->width()) / (double)std::max(1, vp[2]);
    qt_y = (gl_y - (double)vp[1]) * (double)std::max(1, widget->height()) / (double)std::max(1, vp[3]);
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
    paintViewportLabelsAfterScene();
}

void LEDViewport3D::paintGlScene()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float camera_x = camera_target_x + camera_distance * cosf(camera_pitch * M_PI / 180.0f) * cosf(camera_yaw * M_PI / 180.0f);
    float camera_y = camera_target_y + camera_distance * sinf(camera_pitch * M_PI / 180.0f);
    float camera_z = camera_target_z + camera_distance * cosf(camera_pitch * M_PI / 180.0f) * sinf(camera_yaw * M_PI / 180.0f);

    gluLookAt(camera_x, camera_y, camera_z,
              camera_target_x, camera_target_y, camera_target_z,
              0.0f, 1.0f, 0.0f);

    glGetFloatv(GL_MODELVIEW_MATRIX, pick_view_modelview_);

    glPushMatrix();
    pushRoomTurntableGl();

    // Capture V*T (+ proj/viewport) while GL still matches what controllers/planes see.
    capturePickMatricesFromGl();

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
        gizmo.SetCameraDistance(camera_distance);
        gizmo.Render(pick_scene_modelview_, pick_projection_, pick_viewport_);
    }

    glPopMatrix();

}

void LEDViewport3D::paintViewportLabelsAfterScene()
{
    if(width() < 2 || height() < 2 || !viewport_paint_enabled_)
    {
        return;
    }

    paintViewportText2D();
}

void LEDViewport3D::paintViewportText2D()
{
    if(width() < 2 || height() < 2)
    {
        return;
    }

    prepareForQtPainterInPaintGl();

    float modelview[16];
    float projection[16];
    int viewport[4];
    loadScenePickMatrices(modelview, projection, viewport);

    DrawAxisLabels(modelview, projection, viewport);

    const bool has_selection = (selected_controller_idx >= 0 && controller_transforms &&
                                selected_controller_idx < (int)controller_transforms->size()) ||
                               (selected_ref_point_idx >= 0 && reference_points &&
                                selected_ref_point_idx < (int)reference_points->size()) ||
                               (selected_display_plane_idx >= 0 && display_planes &&
                                selected_display_plane_idx < (int)display_planes->size());

    if(has_selection && gizmo.GetMode() == GIZMO_MODE_ROTATE && gizmo.IsDragging())
    {
        float gx = 0.0f;
        float gy = 0.0f;
        float gz = 0.0f;
        gizmo.GetPosition(gx, gy, gz);
        double screen_x = 0.0;
        double screen_y = 0.0;
        GLdouble modelview_d[16];
        GLdouble projection_d[16];
        GLint viewport_i[4];
        for(int i = 0; i < 16; i++)
        {
            modelview_d[i] = (GLdouble)modelview[i];
            projection_d[i] = (GLdouble)projection[i];
        }
        for(int i = 0; i < 4; i++)
        {
            viewport_i[i] = (GLint)viewport[i];
        }
        ProjectPointToScreen(gx, gy, gz, modelview_d, projection_d, viewport_i, screen_x, screen_y);
        double label_x = 0.0;
        double label_y = 0.0;
        GlWindowPointToQtLogical(this, viewport_i, screen_x, screen_y, label_x, label_y);

        prepareForQtPainterInPaintGl();
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setPen(QColor(255, 255, 255));
        QFont font("Arial", 10, QFont::Bold);
        painter.setFont(font);

        const char axis_char = (gizmo.GetSelectedAxis() == GIZMO_AXIS_X) ? 'X' :
                               (gizmo.GetSelectedAxis() == GIZMO_AXIS_Y) ? 'Y' : 'Z';
        const QString snap_text = gizmo.IsRotateSnapActive() ? " | Snap 15 deg" : " | Hold Shift to snap 15 deg";
        const QString text = QString("Rotate %1: %2 deg%3")
                               .arg(QChar(axis_char),
                                    QString::number(gizmo.GetRotateAccumDegrees(), 'f', 1),
                                    snap_text);
        painter.drawText(QPointF(label_x + 12.0, label_y - 12.0), text);
        painter.end();
    }
}


void LEDViewport3D::prepareForQtPainterInPaintGl()
{
    if(!isValid())
    {
        return;
    }

    makeCurrent();

    /* OpenRGB's Qt build does not expose QOpenGLWidget::resetOpenGLState(); mirror the intent manually. */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
}

float LEDViewport3D::ledPreviewPointSizeGl() const
{
    float size = 220.0f / std::max(camera_distance, 4.0f);
    return std::max(4.0f, std::min(12.0f, size));
}

void LEDViewport3D::RebuildFloorGridCache(const GridExtents& extents)
{
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;
    cached_floor_grid_max_x = max_x;
    cached_floor_grid_max_z = max_z;

    cached_floor_grid_vertices.clear();
    cached_floor_grid_colors.clear();
    const int line_count = ((int)max_x + 1) + ((int)max_z + 1);
    cached_floor_grid_vertices.reserve((size_t)line_count * 6);
    cached_floor_grid_colors.reserve((size_t)line_count * 6);

    auto push_line = [&](float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b) {
        cached_floor_grid_vertices.push_back(x0);
        cached_floor_grid_vertices.push_back(y0);
        cached_floor_grid_vertices.push_back(z0);
        cached_floor_grid_vertices.push_back(x1);
        cached_floor_grid_vertices.push_back(y1);
        cached_floor_grid_vertices.push_back(z1);
        cached_floor_grid_colors.push_back(r);
        cached_floor_grid_colors.push_back(g);
        cached_floor_grid_colors.push_back(b);
        cached_floor_grid_colors.push_back(r);
        cached_floor_grid_colors.push_back(g);
        cached_floor_grid_colors.push_back(b);
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
}

void LEDViewport3D::DrawGrid()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glLineWidth(1.0f);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;

    if(cached_floor_grid_max_x != max_x || cached_floor_grid_max_z != max_z)
    {
        RebuildFloorGridCache(extents);
    }

    if(!cached_floor_grid_vertices.empty())
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, cached_floor_grid_vertices.data());
        glColorPointer(3, GL_FLOAT, 0, cached_floor_grid_colors.data());
        glDrawArrays(GL_LINES, 0, (GLsizei)(cached_floor_grid_vertices.size() / 3));
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    glLineWidth(1.5f);
    glColor3f(0.45f, 0.55f, 0.48f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, max_z);
    glVertex3f(0.0f, 0.0f, max_z);
    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawAxes()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(3.0f);

    glBegin(GL_LINES);

    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(3.0f, 0.0f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 3.0f, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 3.0f);

    glEnd();

    glLineWidth(2.0f);

    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(3.0f, 0.0f, 0.0f);
    glVertex3f(2.7f, 0.15f, 0.0f);
    glVertex3f(2.7f, -0.15f, 0.0f);
    glEnd();

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 3.0f, 0.0f);
    glVertex3f(0.15f, 2.7f, 0.0f);
    glVertex3f(-0.15f, 2.7f, 0.0f);
    glEnd();

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, 3.0f);
    glVertex3f(0.15f, 0.0f, 2.7f);
    glVertex3f(-0.15f, 0.0f, 2.7f);
    glEnd();

    glLineWidth(1.0f);
}

void LEDViewport3D::paintRoomGuideLabels(QPainter& painter,
                                         const GLdouble modelview[16],
                                         const GLdouble projection[16],
                                         const GLint viewport[4]) const
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
        GlWindowPointToQtLogical(this, viewport, screen_x, screen_y, label_x, label_y);
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

void LEDViewport3D::DrawAxisLabels(const float modelview_f[16], const float projection_f[16], const int viewport_i[4])
{
    prepareForQtPainterInPaintGl();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    GLdouble modelview[16];
    GLdouble projection[16];
    GLint viewport[4];
    for(int i = 0; i < 16; i++)
    {
        modelview[i] = (GLdouble)modelview_f[i];
        projection[i] = (GLdouble)projection_f[i];
    }
    for(int i = 0; i < 4; i++)
    {
        viewport[i] = (GLint)viewport_i[i];
    }

    paintRoomGuideLabels(painter, modelview, projection, viewport);

    painter.end();
}

void LEDViewport3D::fillRoomViewportSelectionLineBuffers(std::vector<float>& positions, std::vector<float>& colors) const
{
    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    const auto add_line = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
        positions.push_back(x0);
        positions.push_back(y0);
        positions.push_back(z0);
        positions.push_back(x1);
        positions.push_back(y1);
        positions.push_back(z1);
        for(int i = 0; i < 2; ++i)
        {
            colors.push_back(1.0f);
            colors.push_back(0.85f);
            colors.push_back(0.1f);
            colors.push_back(1.0f);
        }
    };

    add_line(0.0f, 0.0f, 0.0f, max_x, 0.0f, 0.0f);
    add_line(max_x, 0.0f, 0.0f, max_x, 0.0f, max_z);
    add_line(max_x, 0.0f, max_z, 0.0f, 0.0f, max_z);
    add_line(0.0f, 0.0f, max_z, 0.0f, 0.0f, 0.0f);
    add_line(0.0f, max_y, 0.0f, max_x, max_y, 0.0f);
    add_line(max_x, max_y, 0.0f, max_x, max_y, max_z);
    add_line(max_x, max_y, max_z, 0.0f, max_y, max_z);
    add_line(0.0f, max_y, max_z, 0.0f, max_y, 0.0f);
    add_line(0.0f, 0.0f, 0.0f, 0.0f, max_y, 0.0f);
    add_line(max_x, 0.0f, 0.0f, max_x, max_y, 0.0f);
    add_line(max_x, 0.0f, max_z, max_x, max_y, max_z);
    add_line(0.0f, 0.0f, max_z, 0.0f, max_y, max_z);
}

void LEDViewport3D::DrawRoomViewportSelection()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(3.0f);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    glColor4f(1.0f, 0.85f, 0.1f, 0.95f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(max_x, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, 0.0f, max_z);
    glVertex3f(max_x, 0.0f, max_z);     glVertex3f(0.0f, 0.0f, max_z);
    glVertex3f(0.0f, 0.0f, max_z);      glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, max_y, 0.0f);      glVertex3f(max_x, max_y, 0.0f);
    glVertex3f(max_x, max_y, 0.0f);     glVertex3f(max_x, max_y, max_z);
    glVertex3f(max_x, max_y, max_z);    glVertex3f(0.0f, max_y, max_z);
    glVertex3f(0.0f, max_y, max_z);     glVertex3f(0.0f, max_y, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(0.0f, max_y, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, max_y, 0.0f);
    glVertex3f(max_x, 0.0f, max_z);     glVertex3f(max_x, max_y, max_z);
    glVertex3f(0.0f, 0.0f, max_z);      glVertex3f(0.0f, max_y, max_z);
    glEnd();

    glColor4f(1.0f, 0.85f, 0.1f, 0.12f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, max_z);
    glVertex3f(0.0f, 0.0f, max_z);
    glEnd();

    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawRoomBoundary()
{
    if(!use_manual_room_dimensions)
    {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_y = extents.height_units;
    const float max_z = extents.depth_units;

    glColor3f(0.0f, 0.8f, 0.8f);

    glBegin(GL_LINES);

    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(max_x, 0.0f, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, 0.0f, max_z);
    glVertex3f(max_x, 0.0f, max_z);     glVertex3f(0.0f, 0.0f, max_z);
    glVertex3f(0.0f, 0.0f, max_z);      glVertex3f(0.0f, 0.0f, 0.0f);

    glVertex3f(0.0f, max_y, 0.0f);      glVertex3f(max_x, max_y, 0.0f);
    glVertex3f(max_x, max_y, 0.0f);     glVertex3f(max_x, max_y, max_z);
    glVertex3f(max_x, max_y, max_z);    glVertex3f(0.0f, max_y, max_z);
    glVertex3f(0.0f, max_y, max_z);     glVertex3f(0.0f, max_y, 0.0f);

    glVertex3f(0.0f, 0.0f, 0.0f);       glVertex3f(0.0f, max_y, 0.0f);
    glVertex3f(max_x, 0.0f, 0.0f);      glVertex3f(max_x, max_y, 0.0f);
    glVertex3f(max_x, 0.0f, max_z);     glVertex3f(max_x, max_y, max_z);
    glVertex3f(0.0f, 0.0f, max_z);      glVertex3f(0.0f, max_y, max_z);

    glEnd();

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawControllers()
{
    if(!controller_transforms) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl || ctrl->hidden_by_virtual) continue;

        glPushMatrix();

        glTranslatef(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z);
        glRotatef(ctrl->transform.rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(ctrl->transform.scale.x, ctrl->transform.scale.y, ctrl->transform.scale.z);

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f
        };

        glTranslatef(center_offset.x, center_offset.y, center_offset.z);

        bool is_selected = IsControllerSelected((int)i);
        bool is_primary = ((int)i == selected_controller_idx);

        float face_r = 0.42f;
        float face_g = 0.46f;
        float face_b = 0.52f;
        float face_alpha = 0.10f;
        float edge_r = 0.50f;
        float edge_g = 0.54f;
        float edge_b = 0.60f;
        float edge_width = 1.25f;

        if(is_primary)
        {
            face_r = 0.95f;
            face_g = 0.85f;
            face_b = 0.20f;
            face_alpha = 0.18f;
            edge_r = 1.0f;
            edge_g = 0.95f;
            edge_b = 0.35f;
            edge_width = 2.0f;
        }
        else if(is_selected)
        {
            face_r = 0.90f;
            face_g = 0.70f;
            face_b = 0.15f;
            face_alpha = 0.14f;
            edge_r = 1.0f;
            edge_g = 0.80f;
            edge_b = 0.25f;
            edge_width = 1.75f;
        }

        DrawAxisAlignedBoxFaces(min_bounds.x, min_bounds.y, min_bounds.z,
                                max_bounds.x, max_bounds.y, max_bounds.z,
                                face_r, face_g, face_b, face_alpha);

        DrawLEDs(ctrl);

        glColor3f(edge_r, edge_g, edge_b);
        glLineWidth(edge_width);

        glBegin(GL_LINES);

        glVertex3f(min_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, min_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, min_bounds.y, max_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, min_bounds.y, max_bounds.z);
        glVertex3f(min_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, min_bounds.y, min_bounds.z);

        glVertex3f(min_bounds.x, max_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, max_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, max_bounds.z);
        glVertex3f(max_bounds.x, max_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, max_bounds.z);
        glVertex3f(min_bounds.x, max_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, min_bounds.z);

        glVertex3f(min_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, min_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, min_bounds.z);
        glVertex3f(max_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(max_bounds.x, max_bounds.y, max_bounds.z);
        glVertex3f(min_bounds.x, min_bounds.y, max_bounds.z); glVertex3f(min_bounds.x, max_bounds.y, max_bounds.z);

        glEnd();

        float top_y = max_bounds.y;
        float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        const float indicator_radius = 0.15f;
        const int sphere_segments = 8;

        glPushMatrix();
        glTranslatef(center_x, top_y, center_z);

        for(int i = 0; i < sphere_segments / 2; i++)
        {
            float lat0 = M_PI * (0.0f + (float)i / sphere_segments);
            float lat1 = M_PI * (0.0f + (float)(i + 1) / sphere_segments);
            float y0 = indicator_radius * sinf(lat0);
            float y1 = indicator_radius * sinf(lat1);
            float r0 = indicator_radius * cosf(lat0);
            float r1 = indicator_radius * cosf(lat1);

            glBegin(GL_QUAD_STRIP);
            for(int j = 0; j <= sphere_segments; j++)
            {
                float lng = 2.0f * M_PI * (float)j / sphere_segments;
                float x = cosf(lng);
                float z = sinf(lng);

                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

        for(int i = sphere_segments / 2; i < sphere_segments; i++)
        {
            float lat0 = M_PI * (0.0f + (float)i / sphere_segments);
            float lat1 = M_PI * (0.0f + (float)(i + 1) / sphere_segments);
            float y0 = indicator_radius * sinf(lat0);
            float y1 = indicator_radius * sinf(lat1);
            float r0 = indicator_radius * cosf(lat0);
            float r1 = indicator_radius * cosf(lat1);

            glBegin(GL_QUAD_STRIP);
            for(int j = 0; j <= sphere_segments; j++)
            {
                float lng = 2.0f * M_PI * (float)j / sphere_segments;
                float x = cosf(lng);
                float z = sinf(lng);

                glColor3f(1.0f, 0.0f, 0.0f);
                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

        glPopMatrix();

        glPopMatrix();
    }

    glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawLEDs(ControllerTransform* ctrl)
{
    if(!ctrl || ctrl->hidden_by_virtual) return;
    if(!ctrl->controller && !ctrl->virtual_controller) return;

    const size_t draw_count = populateLedDrawBuffers(ctrl);
    if(draw_count == 0)
    {
        return;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(ledPreviewPointSizeGl());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, led_draw_positions.data());
    glColorPointer(3, GL_FLOAT, 0, led_draw_colors.data());
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(draw_count));
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

size_t LEDViewport3D::populateLedDrawBuffers(ControllerTransform* ctrl)
{
    if(!ctrl || ctrl->hidden_by_virtual)
    {
        return 0;
    }
    if(!ctrl->controller && !ctrl->virtual_controller)
    {
        return 0;
    }

    const size_t led_count = ctrl->led_positions.size();
    if(led_count == 0)
    {
        return 0;
    }

    auto resolve_mapped_led_color = [](const LEDPosition3D& led_pos) -> RGBColor
    {
        RGBColor color = led_pos.preview_color;
        if(!led_pos.controller)
        {
            return color;
        }

        unsigned int global_led_idx = 0;
        if(!TryGetViewportGlobalLedIndex(led_pos.controller, led_pos.zone_idx, led_pos.led_idx, &global_led_idx))
        {
            return color;
        }

        const RGBColor live_color = led_pos.controller->colors[global_led_idx];
        if(color == 0x00FFFFFF)
        {
            return live_color;
        }

        return color;
    };

    std::vector<ControllerLayout3D::ViewportStripDrawSample> strip_samples;
    strip_samples.reserve(led_count * 3);

    led_draw_positions.clear();
    led_draw_colors.clear();
    led_draw_positions.reserve(led_count * 9);
    led_draw_colors.reserve(led_count * 9);

    ControllerLayout3D::BuildViewportStripDrawSamples(ctrl, grid_scale_mm, strip_samples);
    for(const ControllerLayout3D::ViewportStripDrawSample& sample : strip_samples)
    {
        const RGBColor mapped_color = resolve_mapped_led_color(ctrl->led_positions[sample.logical_index]);
        const float    r            = static_cast<float>(RGBGetRValue(mapped_color)) / 255.0f;
        const float    g            = static_cast<float>(RGBGetGValue(mapped_color)) / 255.0f;
        const float    b            = static_cast<float>(RGBGetBValue(mapped_color)) / 255.0f;

        led_draw_positions.push_back(sample.position.x);
        led_draw_positions.push_back(sample.position.y);
        led_draw_positions.push_back(sample.position.z);
        led_draw_colors.push_back(r);
        led_draw_colors.push_back(g);
        led_draw_colors.push_back(b);
    }

    return led_draw_positions.size() / 3;
}


namespace
{
void DrawLocalFlatQuadXY(float x0, float y0, float x1, float y1, float z,
                         float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z);
    glVertex3f(x1, y0, z);
    glVertex3f(x1, y1, z);
    glVertex3f(x0, y1, z);
    glEnd();
}

void DrawLocalFlatQuadBorderXY(float x0, float y0, float x1, float y1, float z,
                               float r, float g, float b, float a, float line_width)
{
    glColor4f(r, g, b, a);
    glLineWidth(line_width);
    glBegin(GL_LINE_LOOP);
    glVertex3f(x0, y0, z);
    glVertex3f(x1, y0, z);
    glVertex3f(x1, y1, z);
    glVertex3f(x0, y1, z);
    glEnd();
}
} // namespace

void LEDViewport3D::DrawLightBlockerLayers()
{
    if(!controller_transforms)
    {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    constexpr float kCellFillR = 0.22f;
    constexpr float kCellFillG = 0.14f;
    constexpr float kCellFillB = 0.29f;
    constexpr float kCellFillA = 0.38f;
    constexpr float kBorderR = 0.47f;
    constexpr float kBorderG = 0.27f;
    constexpr float kBorderB = 0.63f;
    constexpr float kBorderA = 0.85f;

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl || ctrl->hidden_by_virtual || !ctrl->virtual_controller)
        {
            continue;
        }

        VirtualController3D* layout = ctrl->virtual_controller;
        const std::vector<CustomControllerLightBlocker>& blockers = layout->GetLightBlockers();
        if(blockers.empty())
        {
            continue;
        }

        Vector3D min_bounds{};
        Vector3D max_bounds{};
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);
        const Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f,
        };

        const bool is_primary = ((int)i == selected_controller_idx);
        const bool is_selected = IsControllerSelected((int)i);
        const float cell_alpha = is_primary ? 0.50f : (is_selected ? 0.44f : kCellFillA);
        const float border_width = is_primary ? 2.5f : (is_selected ? 2.0f : 1.5f);

        glPushMatrix();
        glTranslatef(ctrl->transform.position.x, ctrl->transform.position.y, ctrl->transform.position.z);
        glRotatef(ctrl->transform.rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(ctrl->transform.rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(ctrl->transform.scale.x, ctrl->transform.scale.y, ctrl->transform.scale.z);
        glTranslatef(center_offset.x, center_offset.y, center_offset.z);

        for(const CustomControllerLightBlocker& blocker : blockers)
        {
            Vector3D local_min{};
            Vector3D local_max{};
            layout->CellLocalBoundsMm(blocker.x, blocker.y, blocker.z, &local_min, &local_max);
            const float x0 = MMToGridUnits(local_min.x, grid_scale_mm);
            const float y0 = MMToGridUnits(local_min.y, grid_scale_mm);
            const float x1 = MMToGridUnits(local_max.x, grid_scale_mm);
            const float y1 = MMToGridUnits(local_max.y, grid_scale_mm);
            const float z_plane = MMToGridUnits(local_max.z, grid_scale_mm);

            DrawLocalFlatQuadXY(x0, y0, x1, y1, z_plane,
                                kCellFillR, kCellFillG, kCellFillB, cell_alpha);
            DrawLocalFlatQuadBorderXY(x0, y0, x1, y1, z_plane,
                                      kBorderR, kBorderG, kBorderB, kBorderA, border_width);
        }

        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawUserFigure()
{
    if(!reference_points) return;

    for(size_t idx = 0; idx < reference_points->size(); idx++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
        if(ref_point->GetType() == REF_POINT_USER && ref_point->IsVisible())
        {
            bool is_selected = ((int)idx == selected_ref_point_idx);
            Vector3D pos = ref_point->GetPosition();
            Rotation3D rot = ref_point->GetRotation();
            RGBColor color = ref_point->GetDisplayColor();

            float r = (color & 0xFF) / 255.0f;
            float g = ((color >> 8) & 0xFF) / 255.0f;
            float b = ((color >> 16) & 0xFF) / 255.0f;

            glPushMatrix();
            glTranslatef(pos.x, pos.y, pos.z);
            glRotatef(rot.x, 1.0f, 0.0f, 0.0f);
            glRotatef(rot.y, 0.0f, 1.0f, 0.0f);
            glRotatef(rot.z, 0.0f, 0.0f, 1.0f);

            const float head_radius = 0.4f;
            const int segments = 20;

            glColor3f(r, g, b);
            glLineWidth(2.0f);

            glBegin(GL_LINE_LOOP);
            for(int i = 0; i < segments; i++)
            {
                float angle = 2.0f * M_PI * i / segments;
                float x = head_radius * cosf(angle);
                float y = head_radius * sinf(angle);
                glVertex3f(x, y, 0.0f);
            }
            glEnd();

            glPointSize(6.0f);
            glBegin(GL_POINTS);
            glVertex3f(-0.15f, 0.1f, 0.0f);
            glVertex3f(0.15f, 0.1f, 0.0f);
            glEnd();

            glBegin(GL_LINE_STRIP);
            for(int i = 0; i <= 10; i++)
            {
                float t = (float)i / 10.0f;
                float angle = M_PI + t * M_PI;
                float x = 0.25f * cosf(angle);
                float y = -0.05f + 0.25f * sinf(angle);
                glVertex3f(x, y, 0.0f);
            }
            glEnd();

            if(is_selected)
            {
                glDisable(GL_DEPTH_TEST);
                float box_size = head_radius * 1.5f;
                glColor3f(1.0f, 1.0f, 0.0f);
                glLineWidth(3.0f);

                glBegin(GL_LINES);
                glVertex3f(-box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, -box_size);
                glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, box_size);
                glVertex3f(box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, box_size);
                glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, -box_size);

                glVertex3f(-box_size, box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
                glVertex3f(box_size, box_size, -box_size); glVertex3f(box_size, box_size, box_size);
                glVertex3f(box_size, box_size, box_size); glVertex3f(-box_size, box_size, box_size);
                glVertex3f(-box_size, box_size, box_size); glVertex3f(-box_size, box_size, -box_size);

                glVertex3f(-box_size, -box_size, -box_size); glVertex3f(-box_size, box_size, -box_size);
                glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
                glVertex3f(box_size, -box_size, box_size); glVertex3f(box_size, box_size, box_size);
                glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, box_size, box_size);
                glEnd();

                glEnable(GL_DEPTH_TEST);
            }

            glLineWidth(1.0f);
            glPointSize(1.0f);
            glPopMatrix();

            break;
        }
    }
}

void LEDViewport3D::DrawReferencePoints()
{
    if(!reference_points) return;

    const float sphere_radius = 0.3f;
    const int segments = 16;
    const int rings = 12;

    for(size_t idx = 0; idx < reference_points->size(); idx++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
        if(!ref_point->IsVisible()) continue;

        if(ref_point->GetType() == REF_POINT_USER) continue;

        bool is_selected = ((int)idx == selected_ref_point_idx);

        Vector3D pos = ref_point->GetPosition();
        Rotation3D rot = ref_point->GetRotation();
        RGBColor color = ref_point->GetDisplayColor();

        float r = (color & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = ((color >> 16) & 0xFF) / 255.0f;

        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glRotatef(rot.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rot.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rot.z, 0.0f, 0.0f, 1.0f);

        glColor3f(r, g, b);
        for(int i = 0; i < rings; i++)
        {
            float lat0 = M_PI * (-0.5f + (float)i / rings);
            float lat1 = M_PI * (-0.5f + (float)(i + 1) / rings);
            float y0 = sphere_radius * sinf(lat0);
            float y1 = sphere_radius * sinf(lat1);
            float r0 = sphere_radius * cosf(lat0);
            float r1 = sphere_radius * cosf(lat1);

            glBegin(GL_QUAD_STRIP);
            for(int j = 0; j <= segments; j++)
            {
                float lng = 2.0f * M_PI * (float)j / segments;
                float x = cosf(lng);
                float z = sinf(lng);

                glVertex3f(x * r0, y0, z * r0);
                glVertex3f(x * r1, y1, z * r1);
            }
            glEnd();
        }

        if(is_selected)
        {
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(4.0f);
        }
        else
        {
            glColor3f(r * 0.5f, g * 0.5f, b * 0.5f);
            glLineWidth(2.0f);
        }

        glBegin(GL_LINE_LOOP);
        for(int i = 0; i < segments; i++)
        {
            float angle = 2.0f * M_PI * i / segments;
            float x = sphere_radius * cosf(angle);
            float z = sphere_radius * sinf(angle);
            glVertex3f(x, 0.0f, z);
        }
        glEnd();

        if(is_selected)
        {
            glDisable(GL_DEPTH_TEST);
            float box_size = sphere_radius * 1.5f;
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(3.0f);

            glBegin(GL_LINES);
            glVertex3f(-box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, -box_size);
            glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, -box_size, box_size);
            glVertex3f(box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, box_size);
            glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, -box_size, -box_size);

            glVertex3f(-box_size, box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
            glVertex3f(box_size, box_size, -box_size); glVertex3f(box_size, box_size, box_size);
            glVertex3f(box_size, box_size, box_size); glVertex3f(-box_size, box_size, box_size);
            glVertex3f(-box_size, box_size, box_size); glVertex3f(-box_size, box_size, -box_size);

            glVertex3f(-box_size, -box_size, -box_size); glVertex3f(-box_size, box_size, -box_size);
            glVertex3f(box_size, -box_size, -box_size); glVertex3f(box_size, box_size, -box_size);
            glVertex3f(box_size, -box_size, box_size); glVertex3f(box_size, box_size, box_size);
            glVertex3f(-box_size, -box_size, box_size); glVertex3f(-box_size, box_size, box_size);
            glEnd();

            glEnable(GL_DEPTH_TEST);
        }

        glLineWidth(1.0f);
        glPopMatrix();
    }
}

