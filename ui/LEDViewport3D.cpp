// SPDX-License-Identifier: GPL-2.0-only

#include <QHideEvent>
#include <QShowEvent>
#include <QEvent>
#include <QTimer>
#include <QSurfaceFormat>
#include <QOpenGLContext>

#include <cmath>
#include <cfloat>
#include <algorithm>

#include "QtCompat.h"
#include "LEDViewport3D.h"
#include "LEDViewport3D_Internal.h"
#include "ControllerLayout3D.h"
#include "viewport/ViewportGLFormat.h"
#include "viewport/ViewportGLIncludes.h"
#include "VirtualReferencePoint3D.h"
#include "ScreenCaptureManager.h"

#include <GL/glu.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace
{
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
    ViewportGLFormat::ApplyToWidget(this);
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
        for(std::map<std::string, GLuint>::iterator it = display_plane_textures.begin();
            it != display_plane_textures.end();
            ++it)
        {
            glDeleteTextures(1, &it->second);
        }
        doneCurrent();
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

void LEDViewport3D::SetEffectRenderOwnsScreenPreviewUploads(bool owns)
{
    if(effect_render_owns_preview_uploads_ == owns)
    {
        return;
    }
    effect_render_owns_preview_uploads_ = owns;
    SyncScreenPreviewTimer();
}

void LEDViewport3D::SyncScreenPreviewTimer()
{
    if(!viewport_paint_enabled_ || effect_render_owns_preview_uploads_)
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
                if(!viewport_paint_enabled_ || effect_render_owns_preview_uploads_)
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
    }
}

void LEDViewport3D::SetControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    controller_transforms = transforms;

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

        deselectRoomViewport();
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
        deselectRoomViewport();
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
            const float w = MMToGridUnits(plane->GetWidthMM(), grid_scale_mm);
            const float h = MMToGridUnits(plane->GetHeightMM(), grid_scale_mm);
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

GridExtents LEDViewport3D::GetRoomExtents() const
{
    ManualRoomSettings settings = MakeManualRoomSettings(use_manual_room_dimensions,
                                                         room_width,
                                                         room_height,
                                                         room_depth);
    GridDimensionDefaults defaults = MakeGridDefaults(grid_x, grid_y, grid_z);
    return ResolveGridExtents(settings, grid_scale_mm, defaults);
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
        deselectRoomViewport();
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

