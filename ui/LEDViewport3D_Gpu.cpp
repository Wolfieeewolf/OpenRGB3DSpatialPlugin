// SPDX-License-Identifier: GPL-2.0-only
// Qt 5.15 viewport policy: legacy GL room in paintGL; optional GPU texture labels after the scene.
// Full shader room waits for OpenRGB on Qt 6 (OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE is dev-only).

#include "LEDViewport3D.h"
#include "viewport/ViewportGLFormat.h"
#include "viewport/ViewportMath.h"
#include "QtCompat.h"

#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QtGlobal>

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace
{
bool envFlagEnabled(const char* name)
{
    const QByteArray value = qgetenv(name);
    return (value == "1" || value.compare("true", Qt::CaseInsensitive) == 0);
}

bool viewportGpuEnvEnabled()
{
    static int cached = -1;
    if(cached < 0)
    {
        cached = envFlagEnabled("OPENRGB3D_VIEWPORT_GPU") ? 1 : 0;
    }
    return cached != 0;
}

bool viewportGpuSceneEnvEnabled()
{
    static int cached = -1;
    if(cached < 0)
    {
        cached = envFlagEnabled("OPENRGB3D_VIEWPORT_GPU_SCENE") ? 1 : 0;
    }
    return cached != 0;
}

bool viewportGpuLabelsEnvEnabled()
{
    static int cached = -1;
    if(cached < 0)
    {
        cached = envFlagEnabled("OPENRGB3D_VIEWPORT_GPU_LABELS") ? 1 : 0;
    }
    return cached != 0;
}

bool viewportGpuSceneForceEnvEnabled()
{
    static int cached = -1;
    if(cached < 0)
    {
        cached = envFlagEnabled("OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE") ? 1 : 0;
    }
    return cached != 0;
}

bool viewportHostAllowsGpuScene()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return true;
#else
    return false;
#endif
}

void GlWindowPointToQtLogical(const QWidget* widget, const GLint vp[4], double gl_x, double gl_y, double& qt_x, double& qt_y)
{
    qt_x = (gl_x - (double)vp[0]) * (double)std::max(1, widget->width()) / (double)std::max(1, vp[2]);
    qt_y = (gl_y - (double)vp[1]) * (double)std::max(1, widget->height()) / (double)std::max(1, vp[3]);
}

} // namespace

bool LEDViewport3D::viewportGpuRequested() const
{
    return viewportGpuEnvEnabled() || viewportGpuLabelsEnvEnabled() || viewportGpuSceneEnvEnabled()
           || viewport_gpu_labels_preferred_ || viewport_gpu_scene_preferred_;
}

bool LEDViewport3D::viewportGpuSceneWanted() const
{
    return viewportGpuSceneEnvEnabled() || viewport_gpu_scene_preferred_;
}

bool LEDViewport3D::viewportGpuSceneEnvRequested() const
{
    return viewportGpuSceneEnvEnabled();
}

bool LEDViewport3D::viewportGpuLabelsEnvRequested() const
{
    return viewportGpuLabelsEnvEnabled();
}

bool LEDViewport3D::viewportGpuLabelsWanted() const
{
    if(!show_room_guide_labels_)
    {
        return false;
    }
    return viewportGpuLabelsEnvEnabled() || viewport_gpu_labels_preferred_;
}

void LEDViewport3D::paintViewportLabelsAfterScene()
{
    if(width() < 2 || height() < 2 || !viewport_paint_enabled_)
    {
        return;
    }

    if(shouldPaintLegacyViewportText())
    {
        paintViewportText2D();
        return;
    }

    if(!viewportUseGpuLabelOverlay())
    {
        return;
    }

    tryInitializeViewportGpu();
    if(viewportSceneReady())
    {
        paintViewportTextGpuOverlay();
        return;
    }

    if(!viewport_gpu_labels_fallback_)
    {
        viewport_gpu_labels_fallback_ = true;
        qWarning("OpenRGB3DSpatial: GPU label overlay unavailable; using legacy viewport text for this session.");
    }
    paintViewportText2D();
}

void LEDViewport3D::warnGpuSceneDisabledOnQt515Once()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return;
#else
    static bool warned = false;
    if(warned || !viewportGpuSceneEnvEnabled() || viewportGpuSceneForceEnvEnabled())
    {
        return;
    }
    warned = true;
    qWarning(
        "OpenRGB3DSpatial: OPENRGB3D_VIEWPORT_GPU_SCENE is ignored on Qt 5.15 OpenRGB (Qt5Gui compositor crash). "
        "Using legacy 3D room. Set OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE=1 only to retry shader scene. "
        "OPENRGB3D_VIEWPORT_GPU_LABELS=1 remains supported with the legacy room.");
#endif
}

bool LEDViewport3D::viewportUseGpuPaintPath() const
{
    return viewportGpuRequested() && !viewport_gpu_session_fallback_ && !viewport_gpu_startup_pending_;
}

bool LEDViewport3D::viewportUseGpuScenePaint() const
{
    if(!viewportUseGpuPaintPath() || !viewportGpuSceneWanted())
    {
        return false;
    }
    return viewportHostAllowsGpuScene() || viewportGpuSceneForceEnvEnabled();
}

bool LEDViewport3D::shouldPaintLegacyViewportText() const
{
    if(viewportUseGpuScenePaint())
    {
        if(!viewportGpuLabelsWanted())
        {
            return false;
        }
        if(viewport_gpu_labels_deferred_ || viewport_gpu_labels_fallback_)
        {
            return true;
        }
        static constexpr int kGpuLabelWarmupFrames = 4;
        return viewport_gpu_paint_frames_ < kGpuLabelWarmupFrames;
    }
    if(!viewportGpuLabelsWanted() || !viewportGpuRequested())
    {
        return true;
    }
    return viewport_gpu_labels_deferred_ || viewport_gpu_labels_fallback_ || !viewport_gpu_usable_;
}

bool LEDViewport3D::viewportUseGpuLabelOverlay() const
{
    if(viewportUseGpuScenePaint() || !viewportGpuLabelsWanted() || !viewportGpuRequested()
       || viewport_gpu_session_fallback_ || viewport_gpu_labels_deferred_ || viewport_gpu_labels_fallback_)
    {
        return false;
    }

    static constexpr int kGpuLabelWarmupFrames = 4;
    return viewport_gpu_label_paint_frames_ >= kGpuLabelWarmupFrames && viewport_gpu_usable_;
}

ViewportCameraState LEDViewport3D::BuildCameraState() const
{
    ViewportCameraState camera;
    camera.distance = camera_distance;
    camera.yaw_degrees = camera_yaw;
    camera.pitch_degrees = camera_pitch;
    camera.target_x = camera_target_x;
    camera.target_y = camera_target_y;
    camera.target_z = camera_target_z;
    return camera;
}

ViewportFrameMatrices LEDViewport3D::BuildFrameMatrices() const
{
    const int w = viewportFramebufferWidth(width());
    const int h = viewportFramebufferHeight(height());

    ViewportFrameMatrices frame;
    frame.projection = ViewportCamera::BuildProjectionMatrix(
        w, h,
        ViewportGLFormat::kDefaultFovyDegrees,
        ViewportGLFormat::kDefaultNearPlane,
        ViewportGLFormat::kDefaultFarPlane);
    frame.view = ViewportCamera::BuildViewMatrix(BuildCameraState());
    frame.viewport[0] = 0;
    frame.viewport[1] = 0;
    frame.viewport[2] = w;
    frame.viewport[3] = h;
    return frame;
}

void LEDViewport3D::FillPickMatrices(float modelview[16], float projection[16], int viewport[4]) const
{
    const ViewportFrameMatrices frame = BuildFrameMatrices();
    for(int i = 0; i < 16; i++)
    {
        modelview[i] = frame.view.m[i];
        projection[i] = frame.projection.m[i];
    }
    for(int i = 0; i < 4; i++)
    {
        viewport[i] = frame.viewport[i];
    }
}

void LEDViewport3D::loadPickMatrices(float modelview[16], float projection[16], int viewport[4])
{
    if(viewportUseGpuPaintPath() && viewport_gpu_usable_)
    {
        FillPickMatrices(modelview, projection, viewport);
        return;
    }

    if(ensureGlCurrent())
    {
        glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
        glGetFloatv(GL_PROJECTION_MATRIX, projection);
        glGetIntegerv(GL_VIEWPORT, viewport);
    }
}

void LEDViewport3D::tryInitializeViewportGpu()
{
    if(!viewportGpuRequested() || !viewport_paint_enabled_ || viewport_gpu_init_attempted_
       || width() < 2 || height() < 2)
    {
        return;
    }

    viewport_gpu_init_attempted_ = true;
    viewport_gpu_usable_ = viewport_renderer_.initialize(this) && viewport_renderer_.gpuPathReady();
    if(!viewport_gpu_usable_)
    {
        qWarning("OpenRGB3DSpatial: viewport GPU shader init failed.");
    }
}

bool LEDViewport3D::viewportSceneReady() const
{
    return viewport_gpu_usable_ && viewport_renderer_.gpuPathReady();
}

void LEDViewport3D::syncViewportRendererForGpu()
{
    viewport_renderer_.setViewportSize(width(), height());
    viewport_renderer_.setCamera(BuildCameraState());
}

void LEDViewport3D::applyCompatGlCameraMatrices()
{
    const int w = std::max(1, width());
    const int h = std::max(1, height());

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((double)ViewportGLFormat::kDefaultFovyDegrees,
                   (double)w / (double)h,
                   (double)ViewportGLFormat::kDefaultNearPlane,
                   (double)ViewportGLFormat::kDefaultFarPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const ViewportCameraState camera = BuildCameraState();
    const float pitch_rad = camera.pitch_degrees * (float)(M_PI / 180.0);
    const float yaw_rad = camera.yaw_degrees * (float)(M_PI / 180.0);
    const float camera_x = camera.target_x + camera.distance * cosf(pitch_rad) * cosf(yaw_rad);
    const float camera_y = camera.target_y + camera.distance * sinf(pitch_rad);
    const float camera_z = camera.target_z + camera.distance * cosf(pitch_rad) * sinf(yaw_rad);

    gluLookAt((double)camera_x, (double)camera_y, (double)camera_z,
              (double)camera.target_x, (double)camera.target_y, (double)camera.target_z,
              0.0, 1.0, 0.0);
}

void LEDViewport3D::resetLegacyGlBeforeGpuOverlay()
{
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
}

void LEDViewport3D::finalizePaintGlForQtCompositor()
{
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
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glFlush();
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

    glFlush();
}

void LEDViewport3D::paintViewportTextOntoImage(QImage& image)
{
    if(image.isNull() || image.width() < 2 || image.height() < 2)
    {
        return;
    }

    const ViewportFrameMatrices frame = BuildFrameMatrices();

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    GLdouble modelview[16];
    GLdouble projection[16];
    GLint viewport[4];
    for(int i = 0; i < 16; i++)
    {
        modelview[i] = (GLdouble)frame.view.m[i];
        projection[i] = (GLdouble)frame.projection.m[i];
    }
    for(int i = 0; i < 4; i++)
    {
        viewport[i] = (GLint)frame.viewport[i];
    }

    paintRoomGuideLabels(painter, modelview, projection, viewport);

    GLint vp[4] = {
        frame.viewport[0],
        frame.viewport[1],
        frame.viewport[2],
        frame.viewport[3],
    };

    float screen_x = 0.0f;
    float screen_y = 0.0f;
    double label_x = 0.0;
    double label_y = 0.0;

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
        if(ViewportMath::ProjectWorldToScreen(frame.view, frame.projection, frame.viewport, gx, gy, gz, screen_x, screen_y))
        {
            GlWindowPointToQtLogical(this, vp, (double)screen_x, (double)screen_y, label_x, label_y);
            painter.setPen(QColor(255, 255, 255));
            const char axis_char = (gizmo.GetSelectedAxis() == GIZMO_AXIS_X) ? 'X' :
                                   (gizmo.GetSelectedAxis() == GIZMO_AXIS_Y) ? 'Y' : 'Z';
            const QString snap_text = gizmo.IsRotateSnapActive() ? " | Snap 15 deg" : " | Hold Shift to snap 15 deg";
            const QString text = QString("Rotate %1: %2 deg%3")
                                     .arg(QChar(axis_char),
                                          QString::number(gizmo.GetRotateAccumDegrees(), 'f', 1),
                                          snap_text);
            painter.drawText(QPointF(label_x + 12.0, label_y - 12.0), text);
        }
    }

    painter.end();
}

void LEDViewport3D::paintViewportLabelsInActiveGpuFrame()
{
    const int w = width();
    const int h = height();
    if(w < 2 || h < 2 || w > 8192 || h > 8192 || !viewport_renderer_.gpuPathReady())
    {
        return;
    }

    QImage image(w, h, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    paintViewportTextOntoImage(image);
    viewport_renderer_.uploadScreenOverlayFromImage(image);
    viewport_renderer_.drawScreenOverlay(1.0f);
}

void LEDViewport3D::paintViewportTextGpuOverlay()
{
    const int w = width();
    const int h = height();
    if(w < 2 || h < 2 || w > 8192 || h > 8192 || !viewport_renderer_.gpuPathReady())
    {
        return;
    }

    QImage image(w, h, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    paintViewportTextOntoImage(image);

    resetLegacyGlBeforeGpuOverlay();
    viewport_renderer_.beginGpuFrame();
    viewport_renderer_.uploadScreenOverlayFromImage(image);
    viewport_renderer_.drawScreenOverlay(1.0f);
    viewport_renderer_.endGpuFrame();
    finalizePaintGlForQtCompositor();
}

void LEDViewport3D::paintGlGpu()
{
    const int frame = ++viewport_gpu_paint_frames_;
    ++viewport_gl_frame_count_;

    static constexpr int kGpuClearWarmupFrames = 4;
    static constexpr int kGpuLabelDeferFrames = 4;

    if(frame < kGpuClearWarmupFrames)
    {
        paintGlLegacyScene();
        update();
        return;
    }

    tryInitializeViewportGpu();
    syncViewportRendererForGpu();

    if(!viewportSceneReady())
    {
        if(viewport_gpu_init_attempted_)
        {
            viewport_gpu_session_fallback_ = true;
            qWarning("OpenRGB3DSpatial: viewport GPU shaders unavailable; using legacy scene paint for this session.");
        }
        paintGlLegacyScene();
        return;
    }

    if(!viewport_renderer_.beginGpuSceneFramebuffer())
    {
        viewport_gpu_session_fallback_ = true;
        qWarning("OpenRGB3DSpatial: viewport scene FBO unavailable; using legacy scene for this session.");
        paintGlLegacyScene();
        return;
    }

    drawViewportSceneGpu();

    const bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                          selected_controller_idx < (int)controller_transforms->size());
    const bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                         selected_ref_point_idx < (int)reference_points->size());
    const bool has_display_plane_selected = (selected_display_plane_idx >= 0 && display_planes &&
                                             selected_display_plane_idx < (int)display_planes->size());

    if(has_controller_selected || has_ref_point_selected || has_display_plane_selected)
    {
        gizmo.SetCameraDistance(camera_distance);
        renderGizmoGpu();
    }

    const bool want_gpu_labels = viewportGpuLabelsWanted() && !viewport_gpu_labels_fallback_
                                 && !viewport_gpu_labels_deferred_ && frame >= kGpuLabelDeferFrames;
    if(want_gpu_labels)
    {
        paintViewportLabelsInActiveGpuFrame();
    }

    viewport_renderer_.endGpuSceneFramebuffer();
    finalizePaintGlForQtCompositor();
}
