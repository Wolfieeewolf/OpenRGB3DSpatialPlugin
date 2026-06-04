// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEDVIEWPORT3D_H
#define LEDVIEWPORT3D_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QImage>
#include <QPointF>
#include <QString>
#include <QTimer>

#include <vector>
#include <memory>
#include <map>
#include <cstdint>
#include <functional>

#include "LEDPosition3D.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "Gizmo3D.h"
#include "SpatialEffectTypes.h"
#include "VirtualController3D.h"
#include "DisplayPlane3D.h"
#include "viewport/ViewportCamera.h"
#include "viewport/ViewportMath.h"
#include "viewport/ViewportRenderer.h"

class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QHideEvent;
class QPainter;
class QShowEvent;
class QShortcut;
class QWheelEvent;
class QWidget;

class LEDViewport3D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit LEDViewport3D(QWidget *parent = nullptr);
    ~LEDViewport3D();

    void SetControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    void SelectController(int index);
    void SelectReferencePoint(int index);
    void UpdateColors();
    void SetViewportPaintingEnabled(bool enabled);
    bool FocusSelectionInView();
    void SetGridDimensions(int x, int y, int z);
    void SetGridSnapEnabled(bool enabled);
    bool IsGridSnapEnabled() const { return grid_snap_enabled; }
    void SetRoomDimensions(float width, float depth, float height, bool use_manual);
    void SetGridScaleMM(float mm_per_unit) { grid_scale_mm = (mm_per_unit > 0.001f) ? mm_per_unit : 10.0f; }
    void SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points);
    void SetDisplayPlanes(std::vector<std::unique_ptr<DisplayPlane3D>>* planes);
    void SelectDisplayPlane(int index);
    void NotifyDisplayPlaneChanged();
    void UploadDisplayPlaneCaptureTexturesDuringEffectTick();
    void SetShowScreenPreview(bool show);
    void SetScreenPreviewTickCallback(std::function<void()> cb) { screen_preview_tick_cb = std::move(cb); }
    bool GetShowScreenPreview() const { return show_screen_preview; }
    bool IsScreenPreviewRefreshActive() const
    {
        return screen_preview_refresh_timer && screen_preview_refresh_timer->isActive();
    }
    void SetShowCalibrationPattern(bool show) { show_calibration_pattern = show; update(); }
    void ClearDisplayPlaneTextures();

    using PerPlaneFlagQuery = std::function<bool(const std::string& plane_name)>;
    void SetPerPlanePreviewQuery(PerPlaneFlagQuery preview_query, PerPlaneFlagQuery calibration_pattern_query);

    void SetShowRoomGuideLabels(bool show) { show_room_guide_labels_ = show; update(); }
    bool GetShowRoomGuideLabels() const { return show_room_guide_labels_; }
    void SetShowRoomGridOverlay(bool show) { show_room_grid_overlay = show; update(); }
    void SetRoomGridBrightness(float brightness);
    void SetRoomGridPointSize(float size);
    void SetRoomGridStep(int step);
    bool GetShowRoomGridOverlay() const { return show_room_grid_overlay; }
    float GetRoomGridBrightness() const { return room_grid_brightness; }
    float GetRoomGridPointSize() const { return room_grid_point_size; }
    int GetRoomGridStep() const { return room_grid_step; }
    void GetRoomGridOverlayDimensions(int* out_nx, int* out_ny, int* out_nz) const;
    void GetRoomGridOverlaySamplePosition(int ix, int iy, int iz, float& x, float& y, float& z) const;
    void SetRoomGridOverlayBounds(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z);
    void ClearRoomGridOverlayBounds();
    void SetRoomGridColorBuffer(const std::vector<RGBColor>& buf);
    void SetRoomGridColorCallback(std::function<RGBColor(float x, float y, float z)> cb) { room_grid_color_callback = std::move(cb); update(); }

    void SetCamera(float distance, float yaw, float pitch,
                   float target_x, float target_y, float target_z)
    {
        camera_distance = distance;
        camera_yaw = yaw;
        camera_pitch = pitch;
        camera_target_x = target_x;
        camera_target_y = target_y;
        camera_target_z = target_z;
        update();
    }
    void GetCamera(float& distance, float& yaw, float& pitch,
                   float& target_x, float& target_y, float& target_z) const
    {
        distance = camera_distance;
        yaw = camera_yaw;
        pitch = camera_pitch;
        target_x = camera_target_x;
        target_y = camera_target_y;
        target_z = camera_target_z;
    }
    void ResetCameraToDefault();
    static void DefaultCamera(float& distance, float& yaw, float& pitch,
                              float& target_x, float& target_y, float& target_z);
    float ledPreviewPointSizeGl() const;

    void UpdateGizmoPosition();
    void NotifyControllerTransformChanged();
    bool IsGizmoDragging() const;

    void AddControllerToSelection(int index);
    void RemoveControllerFromSelection(int index);
    void ClearSelection();
    bool IsControllerSelected(int index) const;
    const std::vector<int>& GetSelectedControllers() const { return selected_controller_indices; }
    int                     GetSelectedControllerIndex() const { return selected_controller_idx; }
    int                     GetSelectedReferencePointIndex() const { return selected_ref_point_idx; }
    int                     GetSelectedDisplayPlaneIndex() const { return selected_display_plane_idx; }

    void update();

    /** Qt 5.15-safe HUD: raster labels to texture after legacy scene (see docs/VIEWPORT_QT515.md). */
    void SetPreferGpuLabelOverlay(bool prefer);
    bool GetPreferGpuLabelOverlay() const { return viewport_gpu_labels_preferred_; }

    /** Shader-based room (Qt 6 host only; Qt 5.15 ignores — see docs/VIEWPORT_QT515.md). */
    void SetPreferGpuScene(bool prefer);
    bool GetPreferGpuScene() const { return viewport_gpu_scene_preferred_; }

    /** Register W/E/R/Q/F/Home/etc. on shortcut_scope (dialog or tab column); call once after embed. */
    void installViewportKeyboardShortcuts(QWidget* shortcut_scope = nullptr);
    void emitGizmoDragCompleted();

signals:
    void ControllerSelected(int index);
    void ControllerPositionChanged(int index, float x, float y, float z);
    void ControllerRotationChanged(int index, float x, float y, float z);
    void ControllerDeleteRequested(int index);
    void ReferencePointSelected(int index);
    void ReferencePointPositionChanged(int index, float x, float y, float z);
    void DisplayPlaneSelected(int index);
    void DisplayPlanePositionChanged(int index, float x, float y, float z);
    void DisplayPlaneRotationChanged(int index, float x, float y, float z);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void focusSelectionFromKeyboard();
    void resetCameraFromKeyboard();
    void applyGizmoMoveMode();
    void applyGizmoRotateMode();
    void applyGizmoFreeroamMode();
    void toggleGridSnapFromKeyboard();
    void tryDeleteSelectedFromKeyboard();
    GridExtents GetRoomExtents() const;
    void DrawGrid();
    void DrawReferencePoints();
    void DrawAxes();
    void DrawAxisLabels(const float modelview[16], const float projection[16], const int viewport[4]);
    void paintRoomGuideLabels(QPainter& painter,
                              const GLdouble modelview[16],
                              const GLdouble projection[16],
                              const GLint viewport[4]) const;
    int viewportFramebufferWidth(int logical_w) const;
    int viewportFramebufferHeight(int logical_h) const;
    void paintViewportTextOntoImage(QImage& image);
    void paintViewportText2D();
    void paintViewportTextGpuOverlay();
    void paintViewportLabelsAfterScene();
    bool viewportGpuLabelsWanted() const;
    bool viewportGpuSceneWanted() const;
    void paintViewportLabelsInActiveGpuFrame();
    bool ensureGlCurrent() const;
    void loadPickMatrices(float modelview[16], float projection[16], int viewport[4]);
    bool viewportGpuRequested() const;
    bool viewportGpuSceneEnvRequested() const;
    bool viewportGpuLabelsEnvRequested() const;
    static void warnGpuSceneDisabledOnQt515Once();
    bool shouldPaintLegacyViewportText() const;
    bool viewportUseGpuPaintPath() const;
    bool viewportUseGpuScenePaint() const;
    bool viewportUseGpuLabelOverlay() const;
    bool viewportSceneReady() const;
    void paintGlLegacyScene();
    void paintGlGpu();
    void applyCompatGlCameraMatrices();
    void finalizePaintGlForQtCompositor();
    void resetLegacyGlBeforeGpuOverlay();
    void prepareForQtPainterInPaintGl();
    void tryInitializeViewportGpu();
    void syncViewportRendererForGpu();
    ViewportCameraState BuildCameraState() const;
    ViewportFrameMatrices BuildFrameMatrices() const;
    void FillPickMatrices(float modelview[16], float projection[16], int viewport[4]) const;
    void drawViewportSceneGpu();
    void renderGizmoGpu();
    ViewportMat4 buildControllerLocalMatrix(const Transform3D& transform, const Vector3D& center_offset) const;
    ViewportMat4 buildObjectLocalMatrix(float px, float py, float pz, const Rotation3D& rot) const;
    bool fillLedDrawBuffers(ControllerTransform* ctrl);
    void ensureRoomGridOverlayBuffers();
    void RebuildFloorGridCache(const GridExtents& extents);
    void DrawControllers();
    void DrawLEDs(ControllerTransform* ctrl);
    void DrawUserFigure();
    void DrawRoomBoundary();
    void DrawRoomGridOverlay();
    void getRoomGridOverlayExtents(float& min_x, float& max_x, float& min_y, float& max_y, float& min_z, float& max_z) const;
    void invalidateRoomGridOverlayColors();
    void DrawDisplayPlanes();
    void DrawLightBlockerLayers();
    void UpdateDisplayPlaneTextures();
    void SyncScreenPreviewTimer();
    bool AnyDisplayPlaneWantsScreenPreview() const;
    bool PlaneWantsScreenPreview(DisplayPlane3D* plane) const;
    bool PlaneWantsCalibrationPattern(DisplayPlane3D* plane) const;

    /* mouse_x/mouse_y are OpenGL window coordinates (see MapQtMouseToGluWindow in .cpp), not raw Qt widget Y. */
    int PickController(int mouse_x, int mouse_y);
    int PickReferencePoint(int mouse_x, int mouse_y);
    int PickDisplayPlane(int mouse_x, int mouse_y);
    bool RayBoxIntersect(float ray_origin[3], float ray_direction[3],
                        const Vector3D& box_min, const Vector3D& box_max, float& distance);
    bool RaySphereIntersect(float ray_origin[3], float ray_direction[3],
                           const Vector3D& sphere_center, float sphere_radius, float& distance);

    void CalculateControllerBounds(ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds);
    Vector3D GetControllerCenter(ControllerTransform* ctrl);
    Vector3D GetControllerSize(ControllerTransform* ctrl);

    bool IsControllerAboveFloor(ControllerTransform* ctrl);
    float GetControllerMinY(ControllerTransform* ctrl);

    Vector3D TransformLocalToWorld(const Vector3D& local_pos, const Transform3D& transform);

    std::vector<std::unique_ptr<ControllerTransform>>*  controller_transforms;
    int                                                  selected_controller_idx;
    std::vector<int>                                     selected_controller_indices;

    int     grid_x;
    int     grid_y;
    int     grid_z;
    bool    grid_snap_enabled;
    float   grid_scale_mm;

    float   room_width;
    float   room_depth;
    float   room_height;
    bool    use_manual_room_dimensions;

    std::vector<std::unique_ptr<VirtualReferencePoint3D>>* reference_points;
    std::vector<std::unique_ptr<DisplayPlane3D>>* display_planes;
    int                                     selected_display_plane_idx;
    int                                     selected_ref_point_idx;
    bool                                    show_screen_preview;
    bool                                    show_calibration_pattern;
    bool                                    show_room_guide_labels_ = true;
    bool                                    show_room_grid_overlay;
    float                                   room_grid_brightness;
    float                                   room_grid_point_size;
    int                                     room_grid_step;
    std::vector<RGBColor>                   room_grid_color_buffer;
    std::function<RGBColor(float, float, float)> room_grid_color_callback;
    std::vector<float>                      room_grid_overlay_positions;
    std::vector<float>                      room_grid_overlay_colors;
    int                                     room_grid_overlay_last_nx;
    int                                     room_grid_overlay_last_ny;
    int                                     room_grid_overlay_last_nz;
    int                                     room_grid_overlay_last_step;
    bool                                    room_grid_overlay_use_bounds;
    float                                   room_grid_overlay_min_x, room_grid_overlay_max_x;
    float                                   room_grid_overlay_min_y, room_grid_overlay_max_y;
    float                                   room_grid_overlay_min_z, room_grid_overlay_max_z;
    bool                                    room_grid_overlay_colors_dirty;
    struct DisplayPlaneTexUploadState
    {
        std::uint64_t frame_id = 0;
        int width = 0;
        int height = 0;
    };
    std::map<std::string, GLuint>           display_plane_textures;
    std::map<std::string, DisplayPlaneTexUploadState> display_plane_tex_upload_state;
    std::map<std::string, bool>             display_plane_tex_params_set;
    QTimer*                                 screen_preview_refresh_timer;
    std::function<void()>                   screen_preview_tick_cb;

    PerPlaneFlagQuery                       per_plane_preview_query;
    PerPlaneFlagQuery                       per_plane_calibration_pattern_query;

    float   camera_distance;
    float   camera_yaw;
    float   camera_pitch;
    float   camera_target_x;
    float   camera_target_y;
    float   camera_target_z;

    bool    dragging_rotate;
    bool    dragging_pan;
    bool    dragging_grab;
    QPoint  last_mouse_pos;
    QPoint  click_start_pos;

    Gizmo3D gizmo;
    GizmoAxis last_gizmo_hover_axis;
    bool    viewport_keyboard_shortcuts_installed_ = false;
    bool    viewport_paint_enabled_ = false;
    bool    viewport_gpu_startup_pending_ = false;
    bool    viewport_gpu_labels_deferred_ = false;
    bool    viewport_gpu_labels_preferred_ = false;
    bool    viewport_gpu_scene_preferred_ = false;
    bool    viewport_gpu_labels_fallback_ = false;
    int     viewport_gl_frame_count_ = 0;
    int     viewport_gpu_paint_frames_ = 0;
    int     viewport_gpu_label_paint_frames_ = 0;
    bool    viewport_gpu_init_attempted_ = false;
    bool    viewport_gpu_usable_ = false;
    bool    viewport_gpu_session_fallback_ = false;
    ViewportRenderer viewport_renderer_;

    float   cached_floor_grid_max_x;
    float   cached_floor_grid_max_z;
    std::vector<float> cached_floor_grid_vertices;
    std::vector<float> cached_floor_grid_colors;
    std::vector<float> led_draw_positions;
    std::vector<float> led_draw_colors;
};

#endif
