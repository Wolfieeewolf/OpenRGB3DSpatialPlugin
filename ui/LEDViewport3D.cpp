// SPDX-License-Identifier: GPL-2.0-only

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QMessageBox>
#include <QEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QPainter>
#include <QOpenGLContext>
#include <QFont>
#include <QFontMetrics>
#include <QVector>
#include <QTimer>
#include <QShortcut>
#include <QMatrix4x4>

#include <cmath>
#include <cfloat>
#include <algorithm>
#include <map>
#include <cstring>

#include "QtCompat.h"

#include <QtGlobal>

#include "Colors.h"

#include "LEDViewport3D.h"
#include "ControllerDisplayUtils.h"
#include "ControllerLayout3D.h"
#include "viewport/ViewportGLFormat.h"
#include "viewport/ViewportMath.h"
#include "VirtualReferencePoint3D.h"
#include "ScreenCaptureManager.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

static inline Vector3D Subtract(const Vector3D& a, const Vector3D& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

namespace
{
constexpr float kRoomTurntableSpinSensitivity = 0.25f;
constexpr float kRoomTurntablePitchLimitDeg = 89.0f;
constexpr float kRoomVolumePickPadUnits = 0.02f;
}

static void DrawAxisAlignedBoxFaces(float x0, float y0, float z0, float x1, float y1, float z1,
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

static inline Vector3D CrossVec(const Vector3D& a, const Vector3D& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float DotVec(const Vector3D& a, const Vector3D& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void ProjectPointToScreen(float x,
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

static bool TryGetViewportGlobalLedIndex(RGBController* controller,
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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr float kViewportClickSlopGlPx = 3.0f;
constexpr float kRoomGridOverlayCellEps = 1e-4f;

int RoomGridOverlayCellCountAlong(float min_v, float max_v)
{
    return std::max(1, (int)std::floor((max_v - min_v) + 1.0f + kRoomGridOverlayCellEps));
}

int RoomGridOverlaySampleCountAlong(int cell_count, int step)
{
    const int s = std::max(1, step);
    if(cell_count <= 1)
    {
        return 1;
    }
    const int last_cell = cell_count - 1;
    int count = 0;
    for(int c = 0; c <= last_cell; c += s)
    {
        count++;
    }
    const int last_step_cell = (count > 0) ? std::min(last_cell, (count - 1) * s) : 0;
    if(last_step_cell != last_cell)
    {
        count++;
    }
    return std::max(1, count);
}

int RoomGridOverlaySampleCellIndex(int sample_index, int sample_count, int cell_count, int step)
{
    if(cell_count <= 1 || sample_count <= 1)
    {
        return 0;
    }
    const int last_cell = cell_count - 1;
    const int s = std::max(1, step);

    if(sample_index <= 0)
    {
        return 0;
    }
    if(sample_index >= sample_count - 1)
    {
        return last_cell;
    }

    const int ideal_samples = RoomGridOverlaySampleCountAlong(cell_count, step);
    if(sample_count >= ideal_samples)
    {
        return std::min(last_cell, sample_index * s);
    }

    return (sample_index * last_cell) / std::max(1, sample_count - 1);
}

void MapQtMouseToGluWindow(const QWidget* widget, const QMouseEvent* event, const GLint vp[4], int& out_win_x, int& out_win_y)
{
    const int qw = std::max(1, widget->width());
    const int qh = std::max(1, widget->height());
    const int vp_w = std::max(1, (int)vp[2]);
    const int vp_h = std::max(1, (int)vp[3]);
    const double scale_x = (double)vp_w / (double)qw;
    const double scale_y = (double)vp_h / (double)qh;
    out_win_x = vp[0] + (int)std::lround((double)MOUSE_EVENT_X(event) * scale_x);
    const int y_from_top = (int)std::lround((double)MOUSE_EVENT_Y(event) * scale_y);
    out_win_y = vp[1] + vp_h - y_from_top;
}

float ViewportQtToGlScaleX(const QWidget* widget, const GLint vp[4])
{
    return (float)vp[2] / (float)std::max(1, widget->width());
}

float ViewportQtToGlScaleY(const QWidget* widget, const GLint vp[4])
{
    return (float)vp[3] / (float)std::max(1, widget->height());
}

float ViewportQtDragDistanceGl(const QWidget* widget, const GLint vp[4], const QPoint& delta)
{
    const float gx = (float)delta.x() * ViewportQtToGlScaleX(widget, vp);
    const float gy = (float)delta.y() * ViewportQtToGlScaleY(widget, vp);
    return sqrtf(gx * gx + gy * gy);
}

void GlWindowPointToQtLogical(const QWidget* widget, const GLint vp[4], double gl_x, double gl_y, double& qt_x, double& qt_y)
{
    qt_x = (gl_x - (double)vp[0]) * (double)std::max(1, widget->width()) / (double)std::max(1, vp[2]);
    qt_y = (gl_y - (double)vp[1]) * (double)std::max(1, widget->height()) / (double)std::max(1, vp[3]);
}
} // namespace

bool LEDViewport3D::ensureGlCurrent() const
{
    if(!isValid())
    {
        return false;
    }
    QOpenGLContext* ctx = context();
    if(!ctx || !ctx->isValid())
    {
        return false;
    }
    const_cast<LEDViewport3D*>(this)->makeCurrent();
    return true;
}

LEDViewport3D::LEDViewport3D(QWidget *parent)
    : QOpenGLWidget(parent)
    , controller_transforms(nullptr)
    , selected_controller_idx(-1)
    , grid_x(10)
    , grid_y(10)
    , grid_z(10)
    , grid_snap_enabled(false)
    , grid_scale_mm(10.0f)
    , room_width(1000.0f)
    , room_depth(1000.0f)
    , room_height(1000.0f)
    , use_manual_room_dimensions(false)
    , reference_points(nullptr)
    , display_planes(nullptr)
    , selected_display_plane_idx(-1)
    , selected_ref_point_idx(-1)
    , show_screen_preview(false)
    , show_calibration_pattern(false)
    , show_room_grid_overlay(false)
    , room_grid_brightness(1.0f)
    , room_grid_point_size(3.0f)
    , room_grid_step(4)
    , room_grid_overlay_last_nx(-1)
    , room_grid_overlay_last_ny(-1)
    , room_grid_overlay_last_nz(-1)
    , room_grid_overlay_last_step(-1)
    , room_grid_overlay_use_bounds(false)
    , room_grid_overlay_min_x(0), room_grid_overlay_max_x(0)
    , room_grid_overlay_min_y(0), room_grid_overlay_max_y(0)
    , room_grid_overlay_min_z(0), room_grid_overlay_max_z(0)
    , room_grid_overlay_colors_dirty(true)
    , screen_preview_refresh_timer(nullptr)
    , camera_distance(20.0f)
    , camera_yaw(45.0f)
    , camera_pitch(30.0f)
    , camera_target_x(0.0f)
    , camera_target_y(0.0f)
    , camera_target_z(0.0f)
    , dragging_rotate(false)
    , dragging_pan(false)
    , dragging_grab(false)
    , dragging_room_turntable(false)
    , room_viewport_selected_(false)
    , room_turntable_yaw_deg(0.0f)
    , room_turntable_pitch_deg(0.0f)
    , last_gizmo_hover_axis(GIZMO_AXIS_NONE)
    , cached_floor_grid_max_x(-1.0f)
    , cached_floor_grid_max_z(-1.0f)
{
    ViewportGLFormat::ApplyToWidget(this, ViewportGLFormat::Backend::LegacyFixedFunction);
    QSurfaceFormat fmt = format();
    fmt.setSwapInterval(1);
    setFormat(fmt);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

LEDViewport3D::~LEDViewport3D()
{
    if(screen_preview_refresh_timer)
    {
        screen_preview_refresh_timer->stop();
        delete screen_preview_refresh_timer;
        screen_preview_refresh_timer = nullptr;
    }
    if(ensureGlCurrent())
    {
        viewport_renderer_.shutdown();
        for(std::map<std::string, GLuint>::iterator it = display_plane_textures.begin();
            it != display_plane_textures.end();
            ++it)
        {
            glDeleteTextures(1, &it->second);
        }
        doneCurrent();
    }
    else
    {
        viewport_renderer_.shutdown();
    }
    display_plane_textures.clear();
    display_plane_tex_upload_state.clear();
}

bool LEDViewport3D::PlaneWantsScreenPreview(DisplayPlane3D* plane) const
{
    if(!plane)
    {
        return false;
    }
    if(per_plane_preview_query)
    {
        return per_plane_preview_query(plane->GetName());
    }
    return show_screen_preview;
}

bool LEDViewport3D::PlaneWantsCalibrationPattern(DisplayPlane3D* plane) const
{
    if(!plane)
    {
        return false;
    }
    if(per_plane_calibration_pattern_query)
    {
        return per_plane_calibration_pattern_query(plane->GetName());
    }
    return show_calibration_pattern;
}

bool LEDViewport3D::AnyDisplayPlaneWantsScreenPreview() const
{
    if(!per_plane_preview_query)
    {
        return show_screen_preview;
    }
    if(!display_planes)
    {
        return false;
    }
    for(const std::unique_ptr<DisplayPlane3D>& plane_ptr : *display_planes)
    {
        DisplayPlane3D* plane = plane_ptr.get();
        if(!plane)
        {
            continue;
        }
        if(per_plane_preview_query(plane->GetName()))
        {
            return true;
        }
    }
    return false;
}

void LEDViewport3D::SetShowScreenPreview(bool show)
{
    if(show_screen_preview == show)
        return;
    show_screen_preview = show;
    SyncScreenPreviewTimer();
    update();
}

void LEDViewport3D::SyncScreenPreviewTimer()
{
    if(!viewport_paint_enabled_)
    {
        if(screen_preview_refresh_timer && screen_preview_refresh_timer->isActive())
        {
            screen_preview_refresh_timer->stop();
        }
        return;
    }

    const bool wants_preview = AnyDisplayPlaneWantsScreenPreview();
    if(wants_preview)
    {
        if(!screen_preview_refresh_timer)
        {
            screen_preview_refresh_timer = new QTimer(this);
            connect(screen_preview_refresh_timer, &QTimer::timeout, this, [this]() {
                if(!viewport_paint_enabled_)
                {
                    return;
                }
                makeCurrent();
                if(isValid())
                {
                    UpdateDisplayPlaneTextures();
                }
                doneCurrent();
                if(screen_preview_tick_cb)
                {
                    screen_preview_tick_cb();
                }
                update();
            });
        }
        if(!screen_preview_refresh_timer->isActive())
        {
            screen_preview_refresh_timer->start(OpenRGB3DUi::kScreenPreviewTimerIntervalMs);
        }
    }
    else
    {
        if(screen_preview_refresh_timer)
        {
            screen_preview_refresh_timer->stop();
        }
        if(!display_plane_textures.empty())
        {
            makeCurrent();
            if(isValid())
            {
                ClearDisplayPlaneTextures();
            }
            doneCurrent();
        }
    }
}

void LEDViewport3D::SetPerPlanePreviewQuery(PerPlaneFlagQuery preview_query, PerPlaneFlagQuery calibration_pattern_query)
{
    per_plane_preview_query = std::move(preview_query);
    per_plane_calibration_pattern_query = std::move(calibration_pattern_query);
    SyncScreenPreviewTimer();
    update();
}

void LEDViewport3D::SetGridScaleMM(float mm_per_unit)
{
    const float next_scale = (mm_per_unit > 0.001f) ? mm_per_unit : 10.0f;
    if(std::fabs(next_scale - grid_scale_mm) > 0.0001f)
    {
        grid_scale_mm = next_scale;
        ClearLightBlockerDrawCache();
    }
}

void LEDViewport3D::SetControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    controller_transforms = transforms;
    ClearLightBlockerDrawCache();

    if(!controller_transforms)
    {
        selected_controller_idx = -1;
        selected_controller_indices.clear();
        gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
        update();
        return;
    }

    const int controller_count = (int)controller_transforms->size();
    if(selected_controller_idx >= controller_count)
    {
        selected_controller_idx = -1;
    }
    if(selected_controller_idx >= 0)
    {
        ControllerTransform* selected = (*controller_transforms)[selected_controller_idx].get();
        if(!selected || selected->hidden_by_virtual)
        {
            selected_controller_idx = -1;
        }
    }

    std::vector<int> valid_selected_indices;
    valid_selected_indices.reserve(selected_controller_indices.size());
    for(int idx : selected_controller_indices)
    {
        if(idx < 0 || idx >= controller_count)
        {
            continue;
        }
        ControllerTransform* selected = (*controller_transforms)[idx].get();
        if(!selected || selected->hidden_by_virtual)
        {
            continue;
        }
        valid_selected_indices.push_back(idx);
    }
    selected_controller_indices.swap(valid_selected_indices);

    UpdateGizmoPosition();

    update();
}

void LEDViewport3D::SelectController(int index)
{
    if(controller_transforms && index >= 0 && index < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[index].get();
        if(ctrl && ctrl->hidden_by_virtual)
        {
            selected_controller_idx = -1;
            return;
        }

        selected_display_plane_idx = -1;
        selected_ref_point_idx = -1;
        selected_controller_idx = index;

        if(!IsControllerSelected(index))
        {
            AddControllerToSelection(index);
        }

        gizmo.SetTarget(ctrl);
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();

        update();
    }
}

void LEDViewport3D::SelectReferencePoint(int index)
{
    if(index < 0)
    {
        selected_ref_point_idx = -1;
        gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
        update();
        return;
    }

    if(reference_points && index >= 0 && index < (int)reference_points->size())
    {
        selected_controller_indices.clear();
        selected_controller_idx = -1;
        selected_display_plane_idx = -1;
        selected_ref_point_idx = index;

        VirtualReferencePoint3D* ref_point = (*reference_points)[index].get();
        gizmo.SetTarget(ref_point);
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();

        update();
    }
}

void LEDViewport3D::SetViewportPaintingEnabled(bool enabled)
{
    if(viewport_paint_enabled_ == enabled)
    {
        return;
    }
    viewport_paint_enabled_ = enabled;
    SyncScreenPreviewTimer();
    if(enabled)
    {
        update();
    }
}

void LEDViewport3D::UpdateColors()
{
    if(!viewport_paint_enabled_)
    {
        return;
    }
    update();
}

bool LEDViewport3D::FocusSelectionInView()
{
    float focus_x = camera_target_x;
    float focus_y = camera_target_y;
    float focus_z = camera_target_z;
    float span_units = 2.0f;

    if(display_planes && selected_display_plane_idx >= 0 &&
       selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            const Transform3D& t = plane->GetTransform();
            focus_x = t.position.x;
            focus_y = t.position.y;
            focus_z = t.position.z;
            const float w = plane->GetWidthMM() / std::max(grid_scale_mm, 0.001f);
            const float h = plane->GetHeightMM() / std::max(grid_scale_mm, 0.001f);
            span_units = std::max(w, h) * 1.25f;
        }
    }
    else if(reference_points && selected_ref_point_idx >= 0 &&
            selected_ref_point_idx < (int)reference_points->size())
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[selected_ref_point_idx].get();
        if(ref_point)
        {
            const Vector3D pos = ref_point->GetPosition();
            focus_x = pos.x;
            focus_y = pos.y;
            focus_z = pos.z;
            span_units = 1.5f;
        }
    }
    else if(controller_transforms && selected_controller_idx >= 0 &&
            selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl && !ctrl->hidden_by_virtual)
        {
            const Vector3D center = GetControllerCenter(ctrl);
            focus_x = center.x;
            focus_y = center.y;
            focus_z = center.z;

            const Vector3D size = GetControllerSize(ctrl);
            const float sx = size.x * std::max(ctrl->transform.scale.x, 0.001f);
            const float sy = size.y * std::max(ctrl->transform.scale.y, 0.001f);
            const float sz = size.z * std::max(ctrl->transform.scale.z, 0.001f);
            span_units = std::max(sx, std::max(sy, sz)) * 2.2f;
        }
    }
    else
    {
        if(room_viewport_selected_)
        {
            const GridExtents extents = GetRoomExtents();
            focus_x = extents.width_units * 0.5f;
            focus_y = extents.height_units * 0.5f;
            focus_z = extents.depth_units * 0.5f;
            span_units = std::max(extents.width_units, std::max(extents.height_units, extents.depth_units)) * 1.1f;
        }
        else
        {
            return false;
        }
    }

    span_units = std::max(span_units, 1.5f);
    camera_target_x = focus_x;
    camera_target_y = focus_y;
    camera_target_z = focus_z;

    const float fov_rad = 45.0f * (float)M_PI / 180.0f;
    float distance = (span_units * 0.55f) / tanf(fov_rad * 0.5f);
    distance = std::max(2.0f, std::min(50000.0f, distance));
    camera_distance = distance;
    camera_yaw = 45.0f;
    camera_pitch = 30.0f;
    update();
    return true;
}

void LEDViewport3D::UploadDisplayPlaneCaptureTexturesDuringEffectTick()
{
    if(!viewport_paint_enabled_ || !AnyDisplayPlaneWantsScreenPreview())
    {
        return;
    }
    makeCurrent();
    if(isValid())
    {
        UpdateDisplayPlaneTextures();
    }
    doneCurrent();
}

void LEDViewport3D::SetGridDimensions(int x, int y, int z)
{
    grid_x = x;
    grid_y = y;
    grid_z = z;
    cached_floor_grid_max_x = -1.0f;
    update();
}

void LEDViewport3D::SetGridSnapEnabled(bool enabled)
{
    grid_snap_enabled = enabled;
}

void LEDViewport3D::SetRoomDimensions(float width, float depth, float height, bool use_manual)
{
    room_width = width;
    room_depth = depth;
    room_height = height;
    use_manual_room_dimensions = use_manual;
    cached_floor_grid_max_x = -1.0f;
    update();
}

void LEDViewport3D::UpdateGizmoPosition()
{
    if(display_planes && selected_display_plane_idx >= 0 &&
       selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            Transform3D& transform = plane->GetTransform();
            gizmo.SetTarget(plane);
            gizmo.SetPosition(transform.position.x,
                              transform.position.y,
                              transform.position.z);
            return;
        }
    }

    if(reference_points && selected_ref_point_idx >= 0 &&
       selected_ref_point_idx < (int)reference_points->size())
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[selected_ref_point_idx].get();
        if(ref_point)
        {
            Vector3D pos = ref_point->GetPosition();
            gizmo.SetTarget(ref_point);
            gizmo.SetPosition(pos.x, pos.y, pos.z);
            return;
        }
    }

    if(controller_transforms && selected_controller_idx >= 0 &&
       selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl && !ctrl->hidden_by_virtual)
        {
            gizmo.SetTarget(ctrl);
            gizmo.SetPosition(ctrl->transform.position.x,
                              ctrl->transform.position.y,
                              ctrl->transform.position.z);
            return;
        }
    }

    gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
}

bool LEDViewport3D::IsGizmoDragging() const
{
    return gizmo.IsDragging();
}

void LEDViewport3D::emitGizmoDragCompleted()
{
    if(selected_controller_idx >= 0 && controller_transforms &&
       selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl)
        {
            emit ControllerPositionChanged(selected_controller_idx,
                                             ctrl->transform.position.x,
                                             ctrl->transform.position.y,
                                             ctrl->transform.position.z);
            emit ControllerRotationChanged(selected_controller_idx,
                                           ctrl->transform.rotation.x,
                                           ctrl->transform.rotation.y,
                                           ctrl->transform.rotation.z);
        }
    }
    else if(selected_ref_point_idx >= 0 && reference_points &&
            selected_ref_point_idx < (int)reference_points->size())
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[selected_ref_point_idx].get();
        if(ref_point)
        {
            const Vector3D pos = ref_point->GetPosition();
            emit ReferencePointPositionChanged(selected_ref_point_idx, pos.x, pos.y, pos.z);
        }
    }
    else if(display_planes && selected_display_plane_idx >= 0 &&
            selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            const Transform3D& transform = plane->GetTransform();
            emit DisplayPlanePositionChanged(selected_display_plane_idx,
                                             transform.position.x,
                                             transform.position.y,
                                             transform.position.z);
            emit DisplayPlaneRotationChanged(selected_display_plane_idx,
                                             transform.rotation.x,
                                             transform.rotation.y,
                                             transform.rotation.z);
        }
    }
}

void LEDViewport3D::NotifyControllerTransformChanged()
{
    if(!controller_transforms) return;
    if(selected_controller_idx >= 0 && selected_controller_idx < (int)controller_transforms->size())
    {
        ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
        if(ctrl && !ctrl->hidden_by_virtual)
        {
            ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
        }
    }

    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.11f, 0.12f, 0.15f, 1.0f);
}

void LEDViewport3D::hideEvent(QHideEvent* event)
{
    QOpenGLWidget::hideEvent(event);
    viewport_paint_enabled_ = false;
}

void LEDViewport3D::showEvent(QShowEvent* event)
{
    QOpenGLWidget::showEvent(event);
    viewport_paint_enabled_ = true;
    viewport_gl_frame_count_ = 0;
    viewport_gpu_paint_frames_ = 0;
    if(!viewport_gpu_session_fallback_)
    {
        viewport_gpu_init_attempted_ = false;
        viewport_gpu_usable_ = false;
    }

    viewport_gpu_startup_pending_ = false;
    viewport_gpu_labels_deferred_ = false;
    viewport_gpu_label_paint_frames_ = 0;
    warnGpuSceneDisabledOnQt515Once();

    if(viewportUseGpuScenePaint())
    {
        viewport_gpu_startup_pending_ = true;
        QTimer::singleShot(500, this, [this]() {
            viewport_gpu_startup_pending_ = false;
            viewport_gl_frame_count_ = 0;
            viewport_gpu_paint_frames_ = 0;
            viewport_gpu_label_paint_frames_ = 0;
            update();
        });
    }

    if(viewportGpuLabelsWanted() && !viewport_gpu_session_fallback_)
    {
        viewport_gpu_labels_fallback_ = false;
        viewport_gpu_labels_deferred_ = true;
        QTimer::singleShot(200, this, [this]() {
            viewport_gpu_labels_deferred_ = false;
            viewport_gpu_label_paint_frames_ = 0;
            update();
        });
    }
    else
    {
        viewport_gpu_labels_fallback_ = false;
    }

    update();
}

void LEDViewport3D::update()
{
    if(!viewport_paint_enabled_)
    {
        return;
    }
    QOpenGLWidget::update();
}

void LEDViewport3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(!viewport_paint_enabled_ || width() < 2 || height() < 2)
    {
        return;
    }

    if(viewportUseGpuScenePaint())
    {
        paintGlGpu();
        return;
    }

    if(viewportGpuLabelsWanted() && !viewport_gpu_labels_deferred_ && !viewport_gpu_labels_fallback_)
    {
        ++viewport_gpu_label_paint_frames_;
    }

    paintGlLegacyScene();
    paintViewportLabelsAfterScene();
}

void LEDViewport3D::paintGlLegacyScene()
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

    glPushMatrix();
    pushRoomTurntableLegacyGl();

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
        float modelview[16];
        float projection[16];
        int viewport[4];
        glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
        glGetFloatv(GL_PROJECTION_MATRIX, projection);
        glGetIntegerv(GL_VIEWPORT, viewport);

        gizmo.SetCameraDistance(camera_distance);
        gizmo.Render(modelview, projection, viewport);
    }

    glPopMatrix();

}

void LEDViewport3D::SetPreferGpuLabelOverlay(bool prefer)
{
    if(viewport_gpu_labels_preferred_ == prefer)
    {
        return;
    }
    viewport_gpu_labels_preferred_ = prefer;
    update();
}

void LEDViewport3D::SetPreferGpuScene(bool prefer)
{
    if(viewport_gpu_scene_preferred_ == prefer)
    {
        return;
    }
    viewport_gpu_scene_preferred_ = prefer;
    warnGpuSceneDisabledOnQt515Once();
    update();
}

void LEDViewport3D::paintViewportText2D()
{
    if(width() < 2 || height() < 2)
    {
        return;
    }

    if(!shouldPaintLegacyViewportText())
    {
        return;
    }

    prepareForQtPainterInPaintGl();

    float modelview[16];
    float projection[16];
    int viewport[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

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

int LEDViewport3D::viewportFramebufferWidth(int logical_w) const
{
    const qreal dpr = devicePixelRatioF();
    return std::max(1, (int)std::lround((double)std::max(1, logical_w) * dpr));
}

int LEDViewport3D::viewportFramebufferHeight(int logical_h) const
{
    const qreal dpr = devicePixelRatioF();
    return std::max(1, (int)std::lround((double)std::max(1, logical_h) * dpr));
}

void LEDViewport3D::resizeGL(int w, int h)
{
    if(h == 0) h = 1;
    if(w == 0) w = 1;

    const int fb_w = viewportFramebufferWidth(w);
    const int fb_h = viewportFramebufferHeight(h);

    glViewport(0, 0, fb_w, fb_h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)fb_w / (float)fb_h;
    gluPerspective(45.0, aspect, 0.1, 100000.0);

    glMatrixMode(GL_MODELVIEW);

    gizmo.SetViewportSize(fb_w, fb_h);
    viewport_renderer_.setViewportSize(fb_w, fb_h);
}

bool LEDViewport3D::event(QEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if(event->type() == QEvent::DevicePixelRatioChange)
    {
        update();
    }
#endif
    return QOpenGLWidget::event(event);
}

void LEDViewport3D::DefaultCamera(float& distance, float& yaw, float& pitch,
                                  float& target_x, float& target_y, float& target_z)
{
    distance = 20.0f;
    yaw = 45.0f;
    pitch = 30.0f;
    target_x = 0.0f;
    target_y = 0.0f;
    target_z = 0.0f;
}

void LEDViewport3D::getRoomTurntablePivot(float& pivot_x, float& pivot_y, float& pivot_z) const
{
    const GridExtents extents = GetRoomExtents();
    pivot_x = extents.width_units * 0.5f;
    pivot_y = extents.height_units * 0.5f;
    pivot_z = extents.depth_units * 0.5f;
}

void LEDViewport3D::getRoomVolumeAabb(Vector3D& box_min, Vector3D& box_max) const
{
    const GridExtents extents = GetRoomExtents();
    box_min.x = -kRoomVolumePickPadUnits;
    box_min.y = -kRoomVolumePickPadUnits;
    box_min.z = -kRoomVolumePickPadUnits;
    box_max.x = extents.width_units + kRoomVolumePickPadUnits;
    box_max.y = extents.height_units + kRoomVolumePickPadUnits;
    box_max.z = extents.depth_units + kRoomVolumePickPadUnits;
}

bool LEDViewport3D::hasRoomPreviewRotation() const
{
    return std::fabs(room_turntable_yaw_deg) > 1e-4f || std::fabs(room_turntable_pitch_deg) > 1e-4f;
}

ViewportMat4 LEDViewport3D::roomTurntableMatrix() const
{
    if(!hasRoomPreviewRotation())
    {
        return ViewportMath::Identity();
    }

    float pivot_x = 0.0f;
    float pivot_y = 0.0f;
    float pivot_z = 0.0f;
    getRoomTurntablePivot(pivot_x, pivot_y, pivot_z);
    using namespace ViewportMath;
    return Multiply(Translation(pivot_x, pivot_y, pivot_z),
                    Multiply(RotationY(room_turntable_yaw_deg),
                             Multiply(RotationX(room_turntable_pitch_deg), Translation(-pivot_x, -pivot_y, -pivot_z))));
}

void LEDViewport3D::transformPickRay(float ray_origin[3], float ray_direction[3]) const
{
    if(!hasRoomPreviewRotation())
    {
        return;
    }

    QMatrix4x4 matrix(roomTurntableMatrix().m);
    bool invertible = false;
    const QMatrix4x4 inverse = matrix.inverted(&invertible);
    if(!invertible)
    {
        return;
    }

    const QVector4D origin(ray_origin[0], ray_origin[1], ray_origin[2], 1.0f);
    const QVector4D direction(ray_direction[0], ray_direction[1], ray_direction[2], 0.0f);
    const QVector4D local_origin = inverse * origin;
    const QVector4D local_direction = inverse * direction;

    ray_origin[0] = local_origin.x();
    ray_origin[1] = local_origin.y();
    ray_origin[2] = local_origin.z();

    ray_direction[0] = local_direction.x();
    ray_direction[1] = local_direction.y();
    ray_direction[2] = local_direction.z();

    const float length = std::sqrt(ray_direction[0] * ray_direction[0] + ray_direction[1] * ray_direction[1] +
                                   ray_direction[2] * ray_direction[2]);
    if(length > 1e-6f)
    {
        ray_direction[0] /= length;
        ray_direction[1] /= length;
        ray_direction[2] /= length;
    }
}

bool LEDViewport3D::buildPickRay(int win_x, int win_y, float ray_origin[3], float ray_direction[3])
{
    float modelview[16];
    float projection[16];
    int viewport[4];
    loadPickMatrices(modelview, projection, viewport);

    GLdouble near_x = 0.0;
    GLdouble near_y = 0.0;
    GLdouble near_z = 0.0;
    GLdouble far_x = 0.0;
    GLdouble far_y = 0.0;
    GLdouble far_z = 0.0;

    GLdouble mv[16];
    GLdouble proj[16];
    GLint vp[4];
    for(int i = 0; i < 16; i++)
    {
        mv[i] = (GLdouble)modelview[i];
        proj[i] = (GLdouble)projection[i];
    }
    for(int i = 0; i < 4; i++)
    {
        vp[i] = (GLint)viewport[i];
    }

    if(gluUnProject((GLdouble)win_x, (GLdouble)win_y, 0.0, mv, proj, vp, &near_x, &near_y, &near_z) == GL_FALSE)
    {
        return false;
    }
    if(gluUnProject((GLdouble)win_x, (GLdouble)win_y, 1.0, mv, proj, vp, &far_x, &far_y, &far_z) == GL_FALSE)
    {
        return false;
    }

    ray_origin[0] = (float)near_x;
    ray_origin[1] = (float)near_y;
    ray_origin[2] = (float)near_z;
    ray_direction[0] = (float)(far_x - near_x);
    ray_direction[1] = (float)(far_y - near_y);
    ray_direction[2] = (float)(far_z - near_z);

    const float length = std::sqrt(ray_direction[0] * ray_direction[0] + ray_direction[1] * ray_direction[1] +
                                   ray_direction[2] * ray_direction[2]);
    if(length <= 1e-6f)
    {
        return false;
    }

    ray_direction[0] /= length;
    ray_direction[1] /= length;
    ray_direction[2] /= length;
    transformPickRay(ray_origin, ray_direction);
    return true;
}

bool LEDViewport3D::pickRoomVolume(int win_x, int win_y)
{
    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return false;
    }

    Vector3D box_min{};
    Vector3D box_max{};
    getRoomVolumeAabb(box_min, box_max);

    float distance = 0.0f;
    return RayBoxIntersect(ray_origin, ray_direction, box_min, box_max, distance);
}

void LEDViewport3D::clearSceneObjectSelection()
{
    selected_controller_indices.clear();
    selected_controller_idx = -1;
    selected_display_plane_idx = -1;
    selected_ref_point_idx = -1;
    gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
}

void LEDViewport3D::selectRoomViewport()
{
    clearSceneObjectSelection();
    if(room_viewport_selected_)
    {
        return;
    }

    room_viewport_selected_ = true;
    emit RoomViewportSelected(true);
    emit ControllerSelected(-1);
    emit ReferencePointSelected(-1);
    emit DisplayPlaneSelected(-1);
    update();
}

void LEDViewport3D::deselectRoomViewport()
{
    if(!room_viewport_selected_)
    {
        return;
    }
    room_viewport_selected_ = false;
    emit RoomViewportSelected(false);
    update();
}

void LEDViewport3D::pushRoomTurntableLegacyGl() const
{
    if(!hasRoomPreviewRotation())
    {
        return;
    }

    float pivot_x = 0.0f;
    float pivot_y = 0.0f;
    float pivot_z = 0.0f;
    getRoomTurntablePivot(pivot_x, pivot_y, pivot_z);
    glTranslatef(pivot_x, pivot_y, pivot_z);
    glRotatef(room_turntable_yaw_deg, 0.0f, 1.0f, 0.0f);
    glRotatef(room_turntable_pitch_deg, 1.0f, 0.0f, 0.0f);
    glTranslatef(-pivot_x, -pivot_y, -pivot_z);
}

void LEDViewport3D::multiplyModelviewByRoomTurntable(float modelview[16]) const
{
    if(!hasRoomPreviewRotation())
    {
        return;
    }

    ViewportMat4 view{};
    std::memcpy(view.m, modelview, sizeof(view.m));
    const ViewportMat4 combined = ViewportMath::Multiply(view, roomTurntableMatrix());
    std::memcpy(modelview, combined.m, sizeof(combined.m));
}

void LEDViewport3D::resetRoomPreviewSpin()
{
    room_turntable_yaw_deg = 0.0f;
    room_turntable_pitch_deg = 0.0f;
}

void LEDViewport3D::clampRoomTurntablePitch()
{
    if(room_turntable_pitch_deg > kRoomTurntablePitchLimitDeg)
    {
        room_turntable_pitch_deg = kRoomTurntablePitchLimitDeg;
    }
    if(room_turntable_pitch_deg < -kRoomTurntablePitchLimitDeg)
    {
        room_turntable_pitch_deg = -kRoomTurntablePitchLimitDeg;
    }
}

void LEDViewport3D::applyViewportClickPick(int gl_win_x, int gl_win_y)
{
    const int picked_controller = PickController(gl_win_x, gl_win_y);
    if(picked_controller >= 0)
    {
        ClearSelection();
        AddControllerToSelection(picked_controller);
        SelectController(picked_controller);
        emit ControllerSelected(picked_controller);
        return;
    }

    const int picked_plane = PickDisplayPlane(gl_win_x, gl_win_y);
    if(picked_plane >= 0)
    {
        ClearSelection();
        SelectDisplayPlane(picked_plane);
        emit DisplayPlaneSelected(picked_plane);
        return;
    }

    if(pickRoomVolume(gl_win_x, gl_win_y))
    {
        selectRoomViewport();
        return;
    }

    ClearSelection();
    SelectDisplayPlane(-1);
    emit ControllerSelected(-1);
    emit ReferencePointSelected(-1);
    emit DisplayPlaneSelected(-1);
}

void LEDViewport3D::ResetCameraToDefault()
{
    DefaultCamera(camera_distance, camera_yaw, camera_pitch,
                  camera_target_x, camera_target_y, camera_target_z);
    resetRoomPreviewSpin();
    deselectRoomViewport();
    update();
}

float LEDViewport3D::ledPreviewPointSizeGl() const
{
    float size = 220.0f / std::max(camera_distance, 4.0f);
    return std::max(4.0f, std::min(12.0f, size));
}

void LEDViewport3D::clearCameraDragState()
{
    dragging_rotate = false;
    dragging_pan = false;
    dragging_grab = false;
    dragging_room_turntable = false;
}

void LEDViewport3D::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    last_mouse_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    click_start_pos = QPoint((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));

    float modelview[16];
    float projection[16];
    int viewport[4];
    loadPickMatrices(modelview, projection, viewport);

    int gl_win_x = 0;
    int gl_win_y = 0;
    MapQtMouseToGluWindow(this, event, reinterpret_cast<const GLint*>(viewport), gl_win_x, gl_win_y);

    if(event->button() == Qt::LeftButton)
    {
        bool has_controller_selected = (selected_controller_idx >= 0 && controller_transforms &&
                                        selected_controller_idx < (int)controller_transforms->size());
        bool has_ref_point_selected = (selected_ref_point_idx >= 0 && reference_points &&
                                       selected_ref_point_idx < (int)reference_points->size());
        bool has_display_plane_selected = (selected_display_plane_idx >= 0 && display_planes &&
                                           selected_display_plane_idx < (int)display_planes->size());

        if(has_controller_selected || has_ref_point_selected || has_display_plane_selected)
        {
            gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
            gizmo.SetCameraDistance(camera_distance);
            multiplyModelviewByRoomTurntable(modelview);
            if(gizmo.HandleMousePress(event, gl_win_x, gl_win_y, modelview, projection, viewport))
            {
                update();
                return;
            }
        }

        int picked_ref_point = PickReferencePoint(gl_win_x, gl_win_y);
        if(picked_ref_point >= 0)
        {
            selected_controller_indices.clear();
            selected_controller_idx = -1;
            selected_display_plane_idx = -1;

            selected_ref_point_idx = picked_ref_point;

            if(reference_points && picked_ref_point < (int)reference_points->size())
            {
                VirtualReferencePoint3D* ref_point = (*reference_points)[picked_ref_point].get();
                gizmo.SetTarget(ref_point);
                gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
                UpdateGizmoPosition();
            }

            emit ReferencePointSelected(picked_ref_point);
            update();
            return;
        }

        int picked_controller = PickController(gl_win_x, gl_win_y);
        if(picked_controller >= 0)
        {
            selected_ref_point_idx = -1;
            selected_display_plane_idx = -1;
            if(event->modifiers() & Qt::ControlModifier)
            {
                if(IsControllerSelected(picked_controller))
                {
                    RemoveControllerFromSelection(picked_controller);
                }
                else
                {
                    AddControllerToSelection(picked_controller);
                    if(controller_transforms && picked_controller < (int)controller_transforms->size())
                    {
                        ControllerTransform* ctrl = (*controller_transforms)[picked_controller].get();
                        gizmo.SetTarget(ctrl);
                        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
                        UpdateGizmoPosition();
                    }
                }
                emit ControllerSelected(selected_controller_idx);
                update();
                return;
            }
            else
            {
                ClearSelection();
                AddControllerToSelection(picked_controller);
                SelectController(picked_controller);
                emit ControllerSelected(picked_controller);
                update();
                return;
            }
        }

        dragging_grab = false;
        dragging_pan = false;
        dragging_rotate = false;
        if(room_viewport_selected_)
        {
            dragging_room_turntable = true;
            return;
        }
        if(pickRoomVolume(gl_win_x, gl_win_y))
        {
            selectRoomViewport();
            dragging_room_turntable = true;
            return;
        }

        dragging_grab = true;
        return;
    }
    else if(event->button() == Qt::MiddleButton)
    {
        dragging_grab = false;
        dragging_pan = true;
        dragging_rotate = false;
    }
    else if(event->button() == Qt::RightButton)
    {
        dragging_grab = false;
        dragging_pan = false;
        dragging_rotate = true;
    }
}

void LEDViewport3D::mouseMoveEvent(QMouseEvent *event)
{
    QPoint current_pos((int)MOUSE_EVENT_X(event), (int)MOUSE_EVENT_Y(event));
    QPoint delta = current_pos - last_mouse_pos;

    float modelview[16];
    float projection[16];
    int viewport[4];
    loadPickMatrices(modelview, projection, viewport);

    const float delta_x_gl = (float)delta.x() * ViewportQtToGlScaleX(this, reinterpret_cast<const GLint*>(viewport));
    const float delta_y_gl = (float)delta.y() * ViewportQtToGlScaleY(this, reinterpret_cast<const GLint*>(viewport));

    int gl_win_x = 0;
    int gl_win_y = 0;
    MapQtMouseToGluWindow(this, event, reinterpret_cast<const GLint*>(viewport), gl_win_x, gl_win_y);

    gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
    gizmo.SetCameraDistance(camera_distance);

    const bool left_held = (event->buttons() & Qt::LeftButton) != 0;
    if(left_held)
    {
        multiplyModelviewByRoomTurntable(modelview);
    }
    if(left_held && gizmo.HandleMouseMove(event, gl_win_x, gl_win_y, modelview, projection, viewport))
    {
        if(gizmo.IsDragging())
        {
            if(selected_controller_idx >= 0 && controller_transforms &&
               selected_controller_idx < (int)controller_transforms->size())
            {
                ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
                if(ctrl)
                {
                    ControllerLayout3D::MarkWorldPositionsDirty(ctrl);
                    ControllerLayout3D::UpdateWorldPositions(ctrl);
                }
            }
        }

        last_gizmo_hover_axis = gizmo.GetHoverAxis();
        update();
        last_mouse_pos = current_pos;
        return;
    }

    if(left_held && gizmo.IsActive())
    {
        const GizmoAxis hover = gizmo.GetHoverAxis();
        if(hover != last_gizmo_hover_axis)
        {
            last_gizmo_hover_axis = hover;
            update();
            last_mouse_pos = current_pos;
            return;
        }
    }

    if(dragging_room_turntable && left_held)
    {
        room_turntable_yaw_deg += delta_x_gl * kRoomTurntableSpinSensitivity;
        room_turntable_pitch_deg += delta_y_gl * kRoomTurntableSpinSensitivity;
        clampRoomTurntablePitch();
        update();
    }
    else if(dragging_grab && left_held)
    {
        float grab_sensitivity = 0.003f * camera_distance;

        float yaw_rad = camera_yaw * M_PI / 180.0f;

        float right_x = -sinf(yaw_rad);
        float right_z = cosf(yaw_rad);

        float forward_x = cosf(yaw_rad);
        float forward_z = sinf(yaw_rad);

        camera_target_x -= right_x * delta_x_gl * grab_sensitivity;
        camera_target_z -= right_z * delta_x_gl * grab_sensitivity;

        camera_target_x += forward_x * delta_y_gl * grab_sensitivity;
        camera_target_z += forward_z * delta_y_gl * grab_sensitivity;

        update();
    }
    else if(dragging_rotate && (event->buttons() & Qt::RightButton))
    {
        float orbit_sensitivity = 0.3f;
        camera_yaw += delta_x_gl * orbit_sensitivity;
        camera_pitch += delta_y_gl * orbit_sensitivity;

        if(camera_pitch > 89.0f) camera_pitch = 89.0f;
        if(camera_pitch < -89.0f) camera_pitch = -89.0f;

        update();
    }
    else if(dragging_pan && (event->buttons() & Qt::MiddleButton))
    {
        float pan_sensitivity = 0.002f * camera_distance;

        const float right_x = modelview[0];
        const float right_y = modelview[4];
        const float right_z = modelview[8];
        const float up_x = modelview[1];
        const float up_y = modelview[5];
        const float up_z = modelview[9];

        camera_target_x += right_x * delta_x_gl * pan_sensitivity;
        camera_target_y += right_y * delta_x_gl * pan_sensitivity;
        camera_target_z += right_z * delta_x_gl * pan_sensitivity;
        camera_target_x += up_x * delta_y_gl * pan_sensitivity;
        camera_target_y += up_y * delta_y_gl * pan_sensitivity;
        camera_target_z += up_z * delta_y_gl * pan_sensitivity;

        update();
    }
    else
    {
        clearCameraDragState();
    }

    last_mouse_pos = current_pos;
}

void LEDViewport3D::mouseReleaseEvent(QMouseEvent *event)
{
    int viewport[4];
    makeCurrent();
    glGetIntegerv(GL_VIEWPORT, viewport);
    int gl_win_x = 0;
    int gl_win_y = 0;
    MapQtMouseToGluWindow(this, event, reinterpret_cast<const GLint*>(viewport), gl_win_x, gl_win_y);

    if(dragging_grab && event->button() == Qt::LeftButton)
    {
        const QPoint delta = MOUSE_EVENT_POS(event) - click_start_pos;
        const float distance = ViewportQtDragDistanceGl(this, reinterpret_cast<const GLint*>(viewport), delta);

        if(distance < kViewportClickSlopGlPx)
        {
            applyViewportClickPick(gl_win_x, gl_win_y);
        }
    }
    else if(event->button() == Qt::MiddleButton)
    {
        const QPoint delta = MOUSE_EVENT_POS(event) - click_start_pos;
        const float distance = ViewportQtDragDistanceGl(this, reinterpret_cast<const GLint*>(viewport), delta);
        if(distance < kViewportClickSlopGlPx)
        {
            applyViewportClickPick(gl_win_x, gl_win_y);
        }
    }

    const bool gizmo_was_dragging = gizmo.IsDragging();
    gizmo.HandleMouseRelease(event);
    if(gizmo_was_dragging)
    {
        emitGizmoDragCompleted();
    }
    if(dragging_room_turntable && event->button() == Qt::LeftButton)
    {
        const QPoint delta = MOUSE_EVENT_POS(event) - click_start_pos;
        const float distance = ViewportQtDragDistanceGl(this, reinterpret_cast<const GLint*>(viewport), delta);
        if(distance < kViewportClickSlopGlPx && !pickRoomVolume(gl_win_x, gl_win_y))
        {
            deselectRoomViewport();
        }
    }
    if(event->button() == Qt::LeftButton)
    {
        dragging_grab = false;
        dragging_room_turntable = false;
    }
    else if(event->button() == Qt::MiddleButton)
    {
        dragging_pan = false;
    }
    else if(event->button() == Qt::RightButton)
    {
        dragging_rotate = false;
    }
    update();
}

void LEDViewport3D::leaveEvent(QEvent *event)
{
    QOpenGLWidget::leaveEvent(event);
    clearCameraDragState();
}

void LEDViewport3D::wheelEvent(QWheelEvent *event)
{
    float delta = 0.0f;
    const QPoint pixel_delta = event->pixelDelta();
    if(pixel_delta.y() != 0)
    {
        delta = (float)pixel_delta.y() / 120.0f;
    }
    else
    {
        delta = event->angleDelta().y() / 120.0f;
    }

    float zoom_factor = 1.0f - (delta * 0.1f);

    camera_distance *= zoom_factor;

    if(camera_distance < 1.0f) camera_distance = 1.0f;
    if(camera_distance > 50000.0f) camera_distance = 50000.0f;

    update();
}

void LEDViewport3D::applyGizmoMoveMode()
{
    gizmo.SetMode(GIZMO_MODE_MOVE);
    update();
}

void LEDViewport3D::applyGizmoRotateMode()
{
    gizmo.SetMode(GIZMO_MODE_ROTATE);
    update();
}

void LEDViewport3D::applyGizmoFreeroamMode()
{
    gizmo.SetMode(GIZMO_MODE_FREEROAM);
    update();
}

void LEDViewport3D::toggleGridSnapFromKeyboard()
{
    grid_snap_enabled = !grid_snap_enabled;
    gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
    update();
}

void LEDViewport3D::tryDeleteSelectedFromKeyboard()
{
    if(selected_controller_idx < 0 || !controller_transforms ||
       selected_controller_idx >= (int)controller_transforms->size())
    {
        return;
    }

    ControllerTransform* ctrl = (*controller_transforms)[selected_controller_idx].get();
    QString controller_name = QStringLiteral("Unknown");

    if(ctrl && ctrl->controller)
    {
        controller_name = ControllerDisplay::FormatRgbControllerTitle(ctrl->controller);
    }
    else if(ctrl && ctrl->virtual_controller)
    {
        controller_name = QString::fromStdString(ctrl->virtual_controller->GetName());
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Delete Controller"),
        QStringLiteral("Are you sure you want to remove '%1' from the 3D view?").arg(controller_name),
        QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        emit ControllerDeleteRequested(selected_controller_idx);
    }
}

void LEDViewport3D::installViewportKeyboardShortcuts(QWidget* shortcut_scope)
{
    if(viewport_keyboard_shortcuts_installed_)
    {
        return;
    }

    QWidget* scope = shortcut_scope ? shortcut_scope : parentWidget();
    if(!scope)
    {
        return;
    }

    const auto add_key = [&](Qt::Key key, void (LEDViewport3D::*handler)()) {
        QShortcut* shortcut = new QShortcut(QKeySequence(key), scope);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        shortcut->setAutoRepeat(false);
        connect(shortcut, &QShortcut::activated, this, handler);
    };

    add_key(Qt::Key_W, &LEDViewport3D::applyGizmoMoveMode);
    add_key(Qt::Key_E, &LEDViewport3D::applyGizmoRotateMode);
    add_key(Qt::Key_R, &LEDViewport3D::applyGizmoFreeroamMode);
    add_key(Qt::Key_Q, &LEDViewport3D::toggleGridSnapFromKeyboard);
    add_key(Qt::Key_Delete, &LEDViewport3D::tryDeleteSelectedFromKeyboard);
    add_key(Qt::Key_Backspace, &LEDViewport3D::tryDeleteSelectedFromKeyboard);
    add_key(Qt::Key_F, &LEDViewport3D::focusSelectionFromKeyboard);
    add_key(Qt::Key_Home, &LEDViewport3D::resetCameraFromKeyboard);

    viewport_keyboard_shortcuts_installed_ = true;
}

void LEDViewport3D::focusSelectionFromKeyboard()
{
    FocusSelectionInView();
}

void LEDViewport3D::resetCameraFromKeyboard()
{
    ResetCameraToDefault();
}

void LEDViewport3D::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_W)
    {
        applyGizmoMoveMode();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_E)
    {
        applyGizmoRotateMode();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_R)
    {
        applyGizmoFreeroamMode();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Q)
    {
        toggleGridSnapFromKeyboard();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_F)
    {
        focusSelectionFromKeyboard();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Home)
    {
        resetCameraFromKeyboard();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Escape)
    {
        ClearSelection();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        tryDeleteSelectedFromKeyboard();
        event->accept();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

GridExtents LEDViewport3D::GetRoomExtents() const
{
    ManualRoomSettings settings = MakeManualRoomSettings(use_manual_room_dimensions,
                                                         room_width,
                                                         room_height,
                                                         room_depth);
    GridDimensionDefaults defaults = MakeGridDefaults(grid_x, grid_y, grid_z);
    return ResolveGridExtents(settings, grid_scale_mm, defaults);
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

void LEDViewport3D::focusInEvent(QFocusEvent *event)
{
    QOpenGLWidget::focusInEvent(event);
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

void LEDViewport3D::SetRoomGridOverlayBounds(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z)
{
    room_grid_overlay_use_bounds = true;
    room_grid_overlay_min_x = min_x;
    room_grid_overlay_max_x = max_x;
    room_grid_overlay_min_y = min_y;
    room_grid_overlay_max_y = max_y;
    room_grid_overlay_min_z = min_z;
    room_grid_overlay_max_z = max_z;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
    room_grid_overlay_colors_dirty = true;
}

void LEDViewport3D::ClearRoomGridOverlayBounds()
{
    room_grid_overlay_use_bounds = false;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
    room_grid_overlay_colors_dirty = true;
}

void LEDViewport3D::SetRoomGridStep(int step)
{
    const int s = std::max(1, std::min(24, step));
    if(room_grid_step == s)
    {
        return;
    }
    room_grid_step = s;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
    room_grid_color_buffer.clear();
    invalidateRoomGridOverlayColors();
    update();
}

void LEDViewport3D::getRoomGridOverlayExtents(float& min_x, float& max_x, float& min_y, float& max_y, float& min_z,
                                              float& max_z) const
{
    if(room_grid_overlay_use_bounds)
    {
        min_x = room_grid_overlay_min_x;
        max_x = room_grid_overlay_max_x;
        min_y = room_grid_overlay_min_y;
        max_y = room_grid_overlay_max_y;
        min_z = room_grid_overlay_min_z;
        max_z = room_grid_overlay_max_z;
        return;
    }

    const GridExtents extents = GetRoomExtents();
    min_x = 0.0f;
    max_x = extents.width_units;
    min_y = 0.0f;
    max_y = extents.height_units;
    min_z = 0.0f;
    max_z = extents.depth_units;
}

void LEDViewport3D::GetRoomGridOverlayDimensions(int* out_nx, int* out_ny, int* out_nz) const
{
    const int step = std::max(1, room_grid_step);
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    getRoomGridOverlayExtents(min_x, max_x, min_y, max_y, min_z, max_z);

    int nx = RoomGridOverlaySampleCountAlong(RoomGridOverlayCellCountAlong(min_x, max_x), step);
    int ny = RoomGridOverlaySampleCountAlong(RoomGridOverlayCellCountAlong(min_y, max_y), step);
    int nz = RoomGridOverlaySampleCountAlong(RoomGridOverlayCellCountAlong(min_z, max_z), step);
    const size_t max_overlay_points = 35000u;
    size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
    if(count > max_overlay_points)
    {
        const int cap = std::max(1, (int)std::floor(std::cbrt((double)max_overlay_points * 0.98)));
        nx = std::min(nx, cap);
        ny = std::min(ny, cap);
        nz = std::min(nz, cap);
    }
    if(out_nx) *out_nx = nx;
    if(out_ny) *out_ny = ny;
    if(out_nz) *out_nz = nz;
}

void LEDViewport3D::GetRoomGridOverlaySamplePosition(int ix, int iy, int iz, float& x, float& y, float& z) const
{
    const int step = std::max(1, room_grid_step);
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    getRoomGridOverlayExtents(min_x, max_x, min_y, max_y, min_z, max_z);

    const int cells_x = RoomGridOverlayCellCountAlong(min_x, max_x);
    const int cells_y = RoomGridOverlayCellCountAlong(min_y, max_y);
    const int cells_z = RoomGridOverlayCellCountAlong(min_z, max_z);

    int nx = 0;
    int ny = 0;
    int nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);

    const int cell_ix = RoomGridOverlaySampleCellIndex(ix, nx, cells_x, step);
    const int cell_iy = RoomGridOverlaySampleCellIndex(iy, ny, cells_y, step);
    const int cell_iz = RoomGridOverlaySampleCellIndex(iz, nz, cells_z, step);
    x = min_x + (float)cell_ix;
    y = min_y + (float)cell_iy;
    z = min_z + (float)cell_iz;
}

void LEDViewport3D::invalidateRoomGridOverlayColors()
{
    room_grid_overlay_colors_dirty = true;
}

void LEDViewport3D::SetRoomGridBrightness(float brightness)
{
    const float b = std::max(0.0f, std::min(1.0f, brightness));
    if(room_grid_brightness == b)
    {
        return;
    }
    room_grid_brightness = b;
    invalidateRoomGridOverlayColors();
    update();
}

void LEDViewport3D::SetRoomGridPointSize(float size)
{
    const float s = std::max(0.5f, std::min(12.0f, size));
    if(room_grid_point_size == s)
    {
        return;
    }
    room_grid_point_size = s;
    update();
}

void LEDViewport3D::SetRoomGridColorBuffer(const std::vector<RGBColor>& buf)
{
    room_grid_color_buffer = buf;
    room_grid_overlay_colors_dirty = true;
    update();
}

void LEDViewport3D::DrawRoomGridOverlay()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    int nx = 0, ny = 0, nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);
    const size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
    if(count <= 0) return;

    const bool use_buffer = (room_grid_color_buffer.size() == count);
    const bool use_callback = (!use_buffer && room_grid_color_callback != nullptr);
    const float default_r = 0.0f;
    const float default_g = 0.0f;
    const float default_b = 0.0f;

    const bool layout_changed = room_grid_overlay_positions.size() != (3u * count) ||
                                room_grid_overlay_last_nx != nx || room_grid_overlay_last_ny != ny ||
                                room_grid_overlay_last_nz != nz || room_grid_overlay_last_step != room_grid_step;
    if(layout_changed)
    {
        room_grid_overlay_last_nx = nx;
        room_grid_overlay_last_ny = ny;
        room_grid_overlay_last_nz = nz;
        room_grid_overlay_last_step = room_grid_step;
        room_grid_overlay_colors_dirty = true;
        room_grid_overlay_positions.resize(3u * count);
        float* pos = room_grid_overlay_positions.data();
        for(int ix = 0; ix < nx; ix++)
        {
            for(int iy = 0; iy < ny; iy++)
            {
                for(int iz = 0; iz < nz; iz++)
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    GetRoomGridOverlaySamplePosition(ix, iy, iz, x, y, z);
                    *pos++ = x;
                    *pos++ = y;
                    *pos++ = z;
                }
            }
        }
    }

    const bool need_color_refill = room_grid_overlay_colors_dirty || room_grid_overlay_colors.size() != (3u * count);
    if(need_color_refill)
    {
        room_grid_overlay_colors_dirty = false;
        room_grid_overlay_colors.resize(3u * count);
        float* col = room_grid_overlay_colors.data();
        for(int ix = 0; ix < nx; ix++)
        {
            for(int iy = 0; iy < ny; iy++)
            {
                for(int iz = 0; iz < nz; iz++)
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    GetRoomGridOverlaySamplePosition(ix, iy, iz, x, y, z);
                    float r, g, b;
                    if(use_buffer)
                    {
                        const size_t idx = (size_t)(ix * ny * nz + iy * nz + iz);
                        RGBColor c = room_grid_color_buffer[idx];
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else if(use_callback)
                    {
                        RGBColor c = room_grid_color_callback(x, y, z);
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else
                    {
                        r = default_r;
                        g = default_g;
                        b = default_b;
                    }
                    *col++ = r;
                    *col++ = g;
                    *col++ = b;
                }
            }
        }
    }

    glPointSize(room_grid_point_size);
    glVertexPointer(3, GL_FLOAT, 0, room_grid_overlay_positions.data());
    glColorPointer(3, GL_FLOAT, 0, room_grid_overlay_colors.data());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDrawArrays(GL_POINTS, 0, (GLsizei)count);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawDisplayPlanes()
{
    if(!display_planes) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(size_t plane_index = 0; plane_index < display_planes->size(); plane_index++)
    {
        DisplayPlane3D* plane_ptr = (*display_planes)[plane_index].get();
        if(!plane_ptr) continue;

        const bool wants_cal = PlaneWantsCalibrationPattern(plane_ptr);
        const bool wants_prev = PlaneWantsScreenPreview(plane_ptr);
        if(!plane_ptr->IsVisible() && !wants_cal && !wants_prev) continue;

        float width_units = MMToGridUnits(plane_ptr->GetWidthMM(), grid_scale_mm);
        float height_units = MMToGridUnits(plane_ptr->GetHeightMM(), grid_scale_mm);

        if(width_units <= 0.0f || height_units <= 0.0f) continue;

        float half_w = width_units * 0.5f;
        float half_h = height_units * 0.5f;

        Vector3D local_corners[4] = {
            { -half_w, -half_h, 0.0f },
            {  half_w, -half_h, 0.0f },
            {  half_w,  half_h, 0.0f },
            { -half_w,  half_h, 0.0f }
        };

        Vector3D world_corners[4];
        for(int i = 0; i < 4; ++i)
        {
            world_corners[i] = TransformLocalToWorld(local_corners[i], plane_ptr->GetTransform());
        }

        bool selected = ((int)plane_index == selected_display_plane_idx);
        float fill_color[4]   = { selected ? 0.35f : 0.2f,  selected ? 0.80f : 0.60f, 1.0f, selected ? 0.30f : 0.18f };
        float border_color[4] = { selected ? 0.65f : 0.35f, selected ? 0.90f : 0.70f, 1.0f, selected ? 1.00f : 0.85f };

        if(wants_cal)
        {
            Vector3D center;
            center.x = (world_corners[0].x + world_corners[2].x) * 0.5f;
            center.y = (world_corners[0].y + world_corners[2].y) * 0.5f;
            center.z = (world_corners[0].z + world_corners[2].z) * 0.5f;

            Vector3D mid_bottom, mid_top, mid_left, mid_right;
            mid_bottom.x = (world_corners[0].x + world_corners[1].x) * 0.5f;
            mid_bottom.y = (world_corners[0].y + world_corners[1].y) * 0.5f;
            mid_bottom.z = (world_corners[0].z + world_corners[1].z) * 0.5f;

            mid_top.x = (world_corners[2].x + world_corners[3].x) * 0.5f;
            mid_top.y = (world_corners[2].y + world_corners[3].y) * 0.5f;
            mid_top.z = (world_corners[2].z + world_corners[3].z) * 0.5f;

            mid_left.x = (world_corners[0].x + world_corners[3].x) * 0.5f;
            mid_left.y = (world_corners[0].y + world_corners[3].y) * 0.5f;
            mid_left.z = (world_corners[0].z + world_corners[3].z) * 0.5f;

            mid_right.x = (world_corners[1].x + world_corners[2].x) * 0.5f;
            mid_right.y = (world_corners[1].y + world_corners[2].y) * 0.5f;
            mid_right.z = (world_corners[1].z + world_corners[2].z) * 0.5f;

            glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
            glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_left.x, mid_left.y, mid_left.z);
            glEnd();

            glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
            glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
            glVertex3f(mid_right.x, mid_right.y, mid_right.z);
            glVertex3f(center.x, center.y, center.z);
            glEnd();

            glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_right.x, mid_right.y, mid_right.z);
            glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
            glVertex3f(mid_top.x, mid_top.y, mid_top.z);
            glEnd();

            glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(mid_left.x, mid_left.y, mid_left.z);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_top.x, mid_top.y, mid_top.z);
            glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
            glEnd();
        }
        else if(wants_prev)
        {
            std::string source_id = plane_ptr->GetCaptureSourceId();
            GLuint texture_id = 0;
            bool has_texture = false;

            if(!source_id.empty())
            {
                std::map<std::string, GLuint>::iterator texture_it = display_plane_textures.find(source_id);
                if(texture_it != display_plane_textures.end())
                {
                    texture_id = texture_it->second;
                    has_texture = true;
                }
            }

            if(has_texture)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texture_id);
                glColor4f(1.0f, 1.0f, 1.0f, 0.85f);

                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 1.0f);
                glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
                glTexCoord2f(1.0f, 1.0f);
                glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
                glTexCoord2f(1.0f, 0.0f);
                glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
                glTexCoord2f(0.0f, 0.0f);
                glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
                glEnd();

                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
            }
            else
            {
                Vector3D center;
                center.x = (world_corners[0].x + world_corners[2].x) * 0.5f;
                center.y = (world_corners[0].y + world_corners[2].y) * 0.5f;
                center.z = (world_corners[0].z + world_corners[2].z) * 0.5f;

                Vector3D mid_bottom, mid_top, mid_left, mid_right;
                mid_bottom.x = (world_corners[0].x + world_corners[1].x) * 0.5f;
                mid_bottom.y = (world_corners[0].y + world_corners[1].y) * 0.5f;
                mid_bottom.z = (world_corners[0].z + world_corners[1].z) * 0.5f;

                mid_top.x = (world_corners[2].x + world_corners[3].x) * 0.5f;
                mid_top.y = (world_corners[2].y + world_corners[3].y) * 0.5f;
                mid_top.z = (world_corners[2].z + world_corners[3].z) * 0.5f;

                mid_left.x = (world_corners[0].x + world_corners[3].x) * 0.5f;
                mid_left.y = (world_corners[0].y + world_corners[3].y) * 0.5f;
                mid_left.z = (world_corners[0].z + world_corners[3].z) * 0.5f;

                mid_right.x = (world_corners[1].x + world_corners[2].x) * 0.5f;
                mid_right.y = (world_corners[1].y + world_corners[2].y) * 0.5f;
                mid_right.z = (world_corners[1].z + world_corners[2].z) * 0.5f;

                glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glEnd();

                glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(center.x, center.y, center.z);
                glEnd();

                glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glEnd();

                glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
                glEnd();
            }
        }
        else
        {
            glColor4fv(fill_color);
            glBegin(GL_QUADS);
            for(int i = 0; i < 4; ++i)
            {
                glVertex3f(world_corners[i].x, world_corners[i].y, world_corners[i].z);
            }
            glEnd();
        }

        glColor4fv(border_color);
        glLineWidth(selected ? 3.0f : 2.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i < 4; ++i)
        {
            glVertex3f(world_corners[i].x, world_corners[i].y, world_corners[i].z);
        }
        glEnd();
        glLineWidth(1.0f);

    }

    glDisable(GL_BLEND);
}

void LEDViewport3D::UpdateDisplayPlaneTextures()
{
    if(!display_planes) return;

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
        if(!capture_mgr.IsInitialized()) return;
    }

    for(size_t i = 0; i < display_planes->size(); i++)
    {
        DisplayPlane3D* plane = (*display_planes)[i].get();
        if(!plane) continue;
        if(!plane->IsVisible() && !PlaneWantsScreenPreview(plane)) continue;

        std::string source_id = plane->GetCaptureSourceId();
        if(source_id.empty()) continue;

        if(!capture_mgr.IsCapturing(source_id))
        {
            capture_mgr.StartCapture(source_id);
        }

        std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(source_id);
        if(!frame || !frame->valid || frame->data.empty()) continue;

        DisplayPlaneTexUploadState& up = display_plane_tex_upload_state[source_id];
        if(up.frame_id == frame->frame_id && up.width == frame->width && up.height == frame->height)
        {
            continue;
        }

        GLuint texture_id = 0;
        std::map<std::string, GLuint>::iterator texture_it = display_plane_textures.find(source_id);
        const bool created_texture = (texture_it == display_plane_textures.end());
        if(created_texture)
        {
            glGenTextures(1, &texture_id);
            display_plane_textures[source_id] = texture_id;
        }
        else
        {
            texture_id = texture_it->second;
        }

        glBindTexture(GL_TEXTURE_2D, texture_id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        const bool same_size = (up.width == frame->width && up.height == frame->height && !created_texture);
        if(same_size)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame->width, frame->height,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame->data.data());
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data.data());
        }
        if(created_texture || !display_plane_tex_params_set[source_id])
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            display_plane_tex_params_set[source_id] = true;
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        up.frame_id = frame->frame_id;
        up.width = frame->width;
        up.height = frame->height;
    }
}

void LEDViewport3D::ClearDisplayPlaneTextures()
{
    for(std::map<std::string, GLuint>::iterator it = display_plane_textures.begin();
        it != display_plane_textures.end(); ++it)
    {
        if(it->second != 0)
        {
            glDeleteTextures(1, &(it->second));
        }
    }
    display_plane_textures.clear();
    display_plane_tex_upload_state.clear();
    display_plane_tex_params_set.clear();
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

void ExpandBoundsForBlockerCell(Vector3D& min_bounds,
                                Vector3D& max_bounds,
                                float x0,
                                float y0,
                                float z0,
                                float x1,
                                float y1,
                                float z1)
{
    if(x0 < min_bounds.x) min_bounds.x = x0;
    if(y0 < min_bounds.y) min_bounds.y = y0;
    if(z0 < min_bounds.z) min_bounds.z = z0;
    if(x1 > max_bounds.x) max_bounds.x = x1;
    if(y1 > max_bounds.y) max_bounds.y = y1;
    if(z1 > max_bounds.z) max_bounds.z = z1;
}

void VirtualControllerGridScales(const VirtualController3D* vc,
                                 float grid_scale_mm,
                                 float& scale_x,
                                 float& scale_y,
                                 float& scale_z)
{
    scale_x = 1.0f;
    scale_y = 1.0f;
    scale_z = 1.0f;
    if(!vc)
    {
        return;
    }
    if(vc->GetSpacingX() > 0.001f)
    {
        scale_x = MMToGridUnits(vc->GetSpacingX(), grid_scale_mm);
    }
    if(vc->GetSpacingY() > 0.001f)
    {
        scale_y = MMToGridUnits(vc->GetSpacingY(), grid_scale_mm);
    }
    if(vc->GetSpacingZ() > 0.001f)
    {
        scale_z = MMToGridUnits(vc->GetSpacingZ(), grid_scale_mm);
    }
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

        float scale_x = 1.0f;
        float scale_y = 1.0f;
        float scale_z = 1.0f;
        VirtualControllerGridScales(layout, grid_scale_mm, scale_x, scale_y, scale_z);
        (void)scale_x;
        (void)scale_y;
        (void)scale_z;

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

int LEDViewport3D::PickController(int win_x, int win_y)
{
    if(!controller_transforms) return -1;

    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return -1;
    }

    float closest_distance = FLT_MAX;
    int closest_controller = -1;

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl) continue;
        if(ctrl->hidden_by_virtual) continue;

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        Vector3D center_offset = {
            -(min_bounds.x + max_bounds.x) * 0.5f,
            -(min_bounds.y + max_bounds.y) * 0.5f,
            -(min_bounds.z + max_bounds.z) * 0.5f
        };

        Vector3D local_corners[8] = {
            {min_bounds.x + center_offset.x, min_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, min_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {min_bounds.x + center_offset.x, max_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, max_bounds.y + center_offset.y, min_bounds.z + center_offset.z},
            {min_bounds.x + center_offset.x, min_bounds.y + center_offset.y, max_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, min_bounds.y + center_offset.y, max_bounds.z + center_offset.z},
            {min_bounds.x + center_offset.x, max_bounds.y + center_offset.y, max_bounds.z + center_offset.z},
            {max_bounds.x + center_offset.x, max_bounds.y + center_offset.y, max_bounds.z + center_offset.z}
        };

        Vector3D world_min = TransformLocalToWorld(local_corners[0], ctrl->transform);
        Vector3D world_max = world_min;
        for(int corner = 1; corner < 8; corner++)
        {
            Vector3D world_corner = TransformLocalToWorld(local_corners[corner], ctrl->transform);
            if(world_corner.x < world_min.x) world_min.x = world_corner.x;
            if(world_corner.y < world_min.y) world_min.y = world_corner.y;
            if(world_corner.z < world_min.z) world_min.z = world_corner.z;
            if(world_corner.x > world_max.x) world_max.x = world_corner.x;
            if(world_corner.y > world_max.y) world_max.y = world_corner.y;
            if(world_corner.z > world_max.z) world_max.z = world_corner.z;
        }

        if(world_min.x > world_max.x) { float temp = world_min.x; world_min.x = world_max.x; world_max.x = temp; }
        if(world_min.y > world_max.y) { float temp = world_min.y; world_min.y = world_max.y; world_max.y = temp; }
        if(world_min.z > world_max.z) { float temp = world_min.z; world_min.z = world_max.z; world_max.z = temp; }

        float distance;
        if(RayBoxIntersect(ray_origin, ray_direction, world_min, world_max, distance))
        {
            if(distance < closest_distance)
            {
                closest_distance = distance;
                closest_controller = (int)i;
            }
        }
    }

    return closest_controller;
}

bool LEDViewport3D::RayBoxIntersect(float ray_origin[3], float ray_direction[3],
                                   const Vector3D& box_min, const Vector3D& box_max, float& distance)
{
    float tmin = 0.0f;
    float tmax = 1000.0f;

    for(int i = 0; i < 3; i++)
    {
        float box_min_val = (i == 0) ? box_min.x : (i == 1) ? box_min.y : box_min.z;
        float box_max_val = (i == 0) ? box_max.x : (i == 1) ? box_max.y : box_max.z;

        if(fabsf(ray_direction[i]) < 1e-6f)
        {
            if(ray_origin[i] < box_min_val || ray_origin[i] > box_max_val)
                return false;
        }
        else
        {
            float t1 = (box_min_val - ray_origin[i]) / ray_direction[i];
            float t2 = (box_max_val - ray_origin[i]) / ray_direction[i];

            if(t1 > t2) { float temp = t1; t1 = t2; t2 = temp; }

            tmin = fmaxf(tmin, t1);
            tmax = fminf(tmax, t2);

            if(tmin > tmax)
                return false;
        }
    }

    distance = tmin;
    return true;
}

bool LEDViewport3D::RaySphereIntersect(float ray_origin[3], float ray_direction[3],
                                       const Vector3D& sphere_center, float sphere_radius, float& distance)
{
    float oc[3] = {
        ray_origin[0] - sphere_center.x,
        ray_origin[1] - sphere_center.y,
        ray_origin[2] - sphere_center.z
    };

    float a = ray_direction[0] * ray_direction[0] +
              ray_direction[1] * ray_direction[1] +
              ray_direction[2] * ray_direction[2];

    float b = 2.0f * (oc[0] * ray_direction[0] +
                      oc[1] * ray_direction[1] +
                      oc[2] * ray_direction[2]);

    float c = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - sphere_radius * sphere_radius;

    float discriminant = b * b - 4.0f * a * c;

    if(discriminant < 0.0f)
    {
        return false;
    }

    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

    if(t1 > 0.0f)
    {
        distance = t1;
        return true;
    }
    else if(t2 > 0.0f)
    {
        distance = t2;
        return true;
    }

    return false;
}

int LEDViewport3D::PickReferencePoint(int win_x, int win_y)
{
    if(!reference_points) return -1;

    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return -1;
    }

    int closest_ref_point = -1;
    float closest_distance = FLT_MAX;
    const float sphere_radius = 0.3f;

    for(unsigned int i = 0; i < reference_points->size(); i++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
        if(!ref_point->IsVisible()) continue;

        Vector3D pos = ref_point->GetPosition();
        float distance;

        if(RaySphereIntersect(ray_origin, ray_direction, pos, sphere_radius, distance))
        {
            if(distance < closest_distance)
            {
                closest_distance = distance;
                closest_ref_point = (int)i;
            }
        }
    }

    return closest_ref_point;
}

void LEDViewport3D::CalculateControllerBounds(ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds)
{
    bool have_bounds = false;
    if(ctrl && !ctrl->led_positions.empty())
    {
        Vector3D first_pos = ctrl->led_positions[0].local_position;
        min_bounds = first_pos;
        max_bounds = first_pos;
        have_bounds = true;

        for(unsigned int i = 0; i < ctrl->led_positions.size(); i++)
        {
            const Vector3D& pos = ctrl->led_positions[i].local_position;

            if(pos.x < min_bounds.x) min_bounds.x = pos.x;
            if(pos.y < min_bounds.y) min_bounds.y = pos.y;
            if(pos.z < min_bounds.z) min_bounds.z = pos.z;

            if(pos.x > max_bounds.x) max_bounds.x = pos.x;
            if(pos.y > max_bounds.y) max_bounds.y = pos.y;
            if(pos.z > max_bounds.z) max_bounds.z = pos.z;
        }
    }

    if(ctrl && ctrl->virtual_controller)
    {
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        float scale_z = 1.0f;
        VirtualControllerGridScales(ctrl->virtual_controller, grid_scale_mm, scale_x, scale_y, scale_z);

        for(const CustomControllerLightBlocker& blocker : ctrl->virtual_controller->GetLightBlockers())
        {
            Vector3D local_min{};
            Vector3D local_max{};
            ctrl->virtual_controller->CellLocalBoundsMm(blocker.x, blocker.y, blocker.z, &local_min, &local_max);
            const float x0 = MMToGridUnits(local_min.x, grid_scale_mm);
            const float y0 = MMToGridUnits(local_min.y, grid_scale_mm);
            const float z0 = MMToGridUnits(local_min.z, grid_scale_mm);
            const float x1 = MMToGridUnits(local_max.x, grid_scale_mm);
            const float y1 = MMToGridUnits(local_max.y, grid_scale_mm);
            const float z1 = MMToGridUnits(local_max.z, grid_scale_mm);

            if(!have_bounds)
            {
                min_bounds = {x0, y0, z0};
                max_bounds = {x1, y1, z1};
                have_bounds = true;
            }
            else
            {
                ExpandBoundsForBlockerCell(min_bounds, max_bounds, x0, y0, z0, x1, y1, z1);
            }
        }
    }

    if(!have_bounds)
    {
        min_bounds = {-0.5f, -0.5f, -0.5f};
        max_bounds = {0.5f, 0.5f, 0.5f};
        return;
    }

    float size_x = max_bounds.x - min_bounds.x;
    float size_y = max_bounds.y - min_bounds.y;
    float size_z = max_bounds.z - min_bounds.z;

    float min_dimension = 0.2f;

    if(size_x < 0.001f)
    {
        float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        min_bounds.x = center_x - min_dimension;
        max_bounds.x = center_x + min_dimension;
    }
    if(size_y < 0.001f)
    {
        float center_y = (min_bounds.y + max_bounds.y) * 0.5f;
        min_bounds.y = center_y - min_dimension;
        max_bounds.y = center_y + min_dimension;
    }
    if(size_z < 0.001f)
    {
        float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        min_bounds.z = center_z - min_dimension;
        max_bounds.z = center_z + min_dimension;
    }

    float padding = 0.1f;
    min_bounds.x -= padding;
    min_bounds.y -= padding;
    min_bounds.z -= padding;
    max_bounds.x += padding;
    max_bounds.y += padding;
    max_bounds.z += padding;
}

Vector3D LEDViewport3D::GetControllerCenter(ControllerTransform* ctrl)
{
    return ControllerLayout3D::GetControllerCenterWorld(ctrl);
}

Vector3D LEDViewport3D::GetControllerSize(ControllerTransform* ctrl)
{
    Vector3D min_bounds, max_bounds;
    CalculateControllerBounds(ctrl, min_bounds, max_bounds);

    return {
        max_bounds.x - min_bounds.x,
        max_bounds.y - min_bounds.y,
        max_bounds.z - min_bounds.z
    };
}

bool LEDViewport3D::IsControllerAboveFloor(ControllerTransform* ctrl)
{
    if(!ctrl) return true;

    return GetControllerMinY(ctrl) >= 0.0f;
}

float LEDViewport3D::GetControllerMinY(ControllerTransform* ctrl)
{
    if(!ctrl) return 0.0f;

    Vector3D min_bounds, max_bounds;
    CalculateControllerBounds(ctrl, min_bounds, max_bounds);

    Vector3D corners[8] = {
        {min_bounds.x, min_bounds.y, min_bounds.z},
        {max_bounds.x, min_bounds.y, min_bounds.z},
        {min_bounds.x, max_bounds.y, min_bounds.z},
        {max_bounds.x, max_bounds.y, min_bounds.z},
        {min_bounds.x, min_bounds.y, max_bounds.z},
        {max_bounds.x, min_bounds.y, max_bounds.z},
        {min_bounds.x, max_bounds.y, max_bounds.z},
        {max_bounds.x, max_bounds.y, max_bounds.z}
    };

    float min_y = FLT_MAX;
    for(int i = 0; i < 8; i++)
    {
        Vector3D world_corner = TransformLocalToWorld(corners[i], ctrl->transform);
        if(world_corner.y < min_y)
        {
            min_y = world_corner.y;
        }
    }

    return min_y;
}

int LEDViewport3D::PickDisplayPlane(int win_x, int win_y)
{
    if(!display_planes) return -1;

    float ray_origin[3]{};
    float ray_direction[3]{};
    if(!buildPickRay(win_x, win_y, ray_origin, ray_direction))
    {
        return -1;
    }

    float closest_distance = FLT_MAX;
    int closest_plane = -1;

    for(size_t i = 0; i < display_planes->size(); i++)
    {
        DisplayPlane3D* plane = (*display_planes)[i].get();
        if(!plane || !plane->IsVisible()) continue;

        float width_units = MMToGridUnits(plane->GetWidthMM(), grid_scale_mm);
        float height_units = MMToGridUnits(plane->GetHeightMM(), grid_scale_mm);
        if(width_units <= 0.0f || height_units <= 0.0f) continue;

        float half_w = width_units * 0.5f;
        float half_h = height_units * 0.5f;

        Vector3D local_corners[4] = {
            { -half_w, -half_h, 0.0f },
            {  half_w, -half_h, 0.0f },
            {  half_w,  half_h, 0.0f },
            { -half_w,  half_h, 0.0f }
        };

        Vector3D world_corners[4];
        for(int corner = 0; corner < 4; ++corner)
        {
            world_corners[corner] = TransformLocalToWorld(local_corners[corner], plane->GetTransform());
        }

        Vector3D edge_u = Subtract(world_corners[1], world_corners[0]);
        Vector3D edge_v = Subtract(world_corners[3], world_corners[0]);
        Vector3D normal = CrossVec(edge_u, edge_v);
        float normal_len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if(normal_len < 1e-6f)
        {
            continue;
        }
        normal.x /= normal_len;
        normal.y /= normal_len;
        normal.z /= normal_len;

        float denom = normal.x * ray_direction[0] + normal.y * ray_direction[1] + normal.z * ray_direction[2];
        if(fabsf(denom) < 1e-6f)
        {
            continue;
        }

        Vector3D diff0 = { world_corners[0].x - ray_origin[0],
                           world_corners[0].y - ray_origin[1],
                           world_corners[0].z - ray_origin[2] };
        float t = (diff0.x * normal.x + diff0.y * normal.y + diff0.z * normal.z) / denom;
        if(t < 0.0f)
        {
            continue;
        }

        Vector3D hit = { ray_origin[0] + ray_direction[0] * t,
                         ray_origin[1] + ray_direction[1] * t,
                         ray_origin[2] + ray_direction[2] * t };

        Vector3D hit_vec = Subtract(hit, world_corners[0]);
        float dot00 = DotVec(edge_u, edge_u);
        float dot01 = DotVec(edge_u, edge_v);
        float dot11 = DotVec(edge_v, edge_v);
        float dot20 = DotVec(hit_vec, edge_u);
        float dot21 = DotVec(hit_vec, edge_v);
        float denom_uv = dot00 * dot11 - dot01 * dot01;
        if(fabsf(denom_uv) < 1e-6f)
        {
            continue;
        }

        float u = (dot11 * dot20 - dot01 * dot21) / denom_uv;
        float v = (dot00 * dot21 - dot01 * dot20) / denom_uv;

        if(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
        {
            if(t < closest_distance)
            {
                closest_distance = t;
                closest_plane = (int)i;
            }
        }
    }

    return closest_plane;
}

Vector3D LEDViewport3D::TransformLocalToWorld(const Vector3D& local_pos, const Transform3D& transform)
{
    Vector3D scaled = {
        local_pos.x * transform.scale.x,
        local_pos.y * transform.scale.y,
        local_pos.z * transform.scale.z
    };

    Vector3D rotated = scaled;

    float rx = transform.rotation.x * M_PI / 180.0f;
    float ry = transform.rotation.y * M_PI / 180.0f;
    float rz = transform.rotation.z * M_PI / 180.0f;

    float temp_y = rotated.y * cosf(rx) - rotated.z * sinf(rx);
    float temp_z = rotated.y * sinf(rx) + rotated.z * cosf(rx);
    rotated.y = temp_y;
    rotated.z = temp_z;

    float temp_x = rotated.x * cosf(ry) + rotated.z * sinf(ry);
    temp_z = -rotated.x * sinf(ry) + rotated.z * cosf(ry);
    rotated.x = temp_x;
    rotated.z = temp_z;

    temp_x = rotated.x * cosf(rz) - rotated.y * sinf(rz);
    temp_y = rotated.x * sinf(rz) + rotated.y * cosf(rz);
    rotated.x = temp_x;
    rotated.y = temp_y;

    Vector3D world = {
        rotated.x + transform.position.x,
        rotated.y + transform.position.y,
        rotated.z + transform.position.z
    };

    return world;
}

void LEDViewport3D::AddControllerToSelection(int index)
{
    if(index < 0 || !controller_transforms || index >= (int)controller_transforms->size())
        return;

    for(unsigned int i = 0; i < selected_controller_indices.size(); i++)
    {
        if(selected_controller_indices[i] == index)
            return;
    }

    selected_controller_indices.push_back(index);
    selected_controller_idx = index;
}

void LEDViewport3D::RemoveControllerFromSelection(int index)
{
    std::vector<int>::iterator it = std::find(selected_controller_indices.begin(), selected_controller_indices.end(), index);
    if(it != selected_controller_indices.end())
    {
        selected_controller_indices.erase(it);

        if(selected_controller_idx == index)
        {
            selected_controller_idx = selected_controller_indices.empty() ? -1 : selected_controller_indices[0];
        }
    }
}

void LEDViewport3D::ClearSelection()
{
    clearSceneObjectSelection();
    deselectRoomViewport();
}

bool LEDViewport3D::IsControllerSelected(int index) const
{
    return std::find(selected_controller_indices.begin(), selected_controller_indices.end(), index) != selected_controller_indices.end();
}

void LEDViewport3D::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
}

void LEDViewport3D::SetDisplayPlanes(std::vector<std::unique_ptr<DisplayPlane3D>>* planes)
{
    display_planes = planes;
    if(!display_planes)
    {
        selected_display_plane_idx = -1;
    }
    else if(selected_display_plane_idx >= (int)display_planes->size())
    {
        selected_display_plane_idx = -1;
    }

    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::SelectDisplayPlane(int index)
{
    if(display_planes && index >= 0 && index < (int)display_planes->size())
    {
        selected_controller_indices.clear();
        selected_controller_idx = -1;
        selected_ref_point_idx = -1;
        selected_display_plane_idx = index;
        gizmo.SetTarget((*display_planes)[index].get());
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();
    }
    else
    {
        selected_display_plane_idx = -1;

        if(selected_controller_idx < 0 && selected_ref_point_idx < 0)
        {
            gizmo.SetTarget(static_cast<DisplayPlane3D*>(nullptr));
        }
    }

    NotifyDisplayPlaneChanged();
    update();
}

void LEDViewport3D::NotifyDisplayPlaneChanged()
{
    if(display_planes && selected_display_plane_idx >= 0 &&
       selected_display_plane_idx < (int)display_planes->size())
    {
        DisplayPlane3D* plane = (*display_planes)[selected_display_plane_idx].get();
        if(plane)
        {
            Transform3D& transform = plane->GetTransform();
            emit DisplayPlanePositionChanged(selected_display_plane_idx,
                                             transform.position.x,
                                             transform.position.y,
                                             transform.position.z);
            emit DisplayPlaneRotationChanged(selected_display_plane_idx,
                                             transform.rotation.x,
                                             transform.rotation.y,
                                             transform.rotation.z);
        }
    }
    else
    {
        emit DisplayPlanePositionChanged(-1, 0.0f, 0.0f, 0.0f);
        emit DisplayPlaneRotationChanged(-1, 0.0f, 0.0f, 0.0f);
    }

    UpdateGizmoPosition();
    update();
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

