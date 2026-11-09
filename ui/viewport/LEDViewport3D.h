// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEDVIEWPORT3D_H
#define LEDVIEWPORT3D_H

#include <QOpenGLFramebufferObject>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_1_Core>
#include <QPointF>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "LEDPosition3D.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "Gizmo3D.h"
#include "EffectVolume3D.h"
#include "SpatialEffectTypes.h"
#include "VirtualController3D.h"
#include "DisplayPlane3D.h"
#include "SceneProp3D.h"
#include "viewport/ViewportMath.h"
#include "viewport/GlProgram.h"
#include "viewport/MeshAnim.h"
#include "viewport/MeshBatch.h"

class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QPainter;
class QShowEvent;
class QShortcut;
class QWheelEvent;
class QWidget;

class LEDViewport3D : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core
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
    void SetRotateSnapEnabled(bool enabled);
    bool IsRotateSnapEnabled() const { return rotate_snap_enabled_; }
    void SetRoomDimensions(float width, float depth, float height, bool use_manual);
    void SetGridScaleMM(float mm_per_unit);
    void SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points);
    void SetDisplayPlanes(std::vector<std::unique_ptr<DisplayPlane3D>>* planes);
    void SelectDisplayPlane(int index);
    void SetSceneProps(std::vector<std::unique_ptr<SceneProp3D>>* props);
    void SelectSceneProp(int index);
    void NotifyDisplayPlaneChanged();
    void NotifyScenePropChanged();
    /** Start/stop ~30 Hz refresh while any visible mesh prop has animation autoplay. */
    void SyncMeshAnimTimer();
    void UploadDisplayPlaneCaptureTexturesDuringEffectTick();
    void SetShowScreenPreview(bool show);
    /** When true, effect tick uploads plane textures; stop the independent preview timer. */
    void SetEffectRenderOwnsScreenPreviewUploads(bool owns);
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
    void SetRoomGridColorBuffer(std::vector<RGBColor> buf);
    /** Swap-adopt overlay colors (avoids a full copy; leaves `buf` with the previous buffer). */
    void SwapRoomGridColorBuffer(std::vector<RGBColor>& buf);
    void SetRoomGridColorCallback(std::function<RGBColor(float x, float y, float z)> cb)
    {
        if(!room_grid_color_callback && !cb)
        {
            return;
        }
        room_grid_color_callback = std::move(cb);
        update();
    }

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
    int                     GetSelectedScenePropIndex() const { return selected_scene_prop_idx_; }
    bool                    IsRoomViewportSelected() const { return room_viewport_selected_; }

    /** Stack effect volume box (gizmo target). Clears other scene-object selection. */
    void SelectEffectVolume(const EffectVolume3D& volume, const GridContext3D& grid);
    void ClearEffectVolumeSelection();
    bool HasEffectVolumeSelection() const { return effect_volume_active_; }
    int GetSelectedEffectStackIndex() const
    {
        return effect_volume_active_ ? effect_volume_.stack_index : -1;
    }
    EffectVolume3D* ActiveEffectVolume()
    {
        return effect_volume_active_ ? &effect_volume_ : nullptr;
    }
    const GridContext3D* ActiveEffectVolumeGrid() const
    {
        return effect_volume_grid_.get();
    }
    /** Refresh the grid snapshot used by ApplyToEffect / wireframe clipping. */
    void SetEffectVolumeGrid(const GridContext3D& grid);

    /** Animated Mesh stack-effect overlays (path + TRS + tint); replaced each render tick. */
    struct MeshAnimPartTint
    {
        int part_index = 0;
        bool visible = true;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float emis_scale = 1.0f;
    };
    struct MeshAnimOverlay
    {
        std::string path;
        Transform3D transform;
        float hw = 1.0f;
        float hh = 1.0f;
        float hd = 1.0f;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 0.72f;
        float time_sec = 0.0f;
        bool animate = true;
        /** Multiply GPU materials by part tint (Effect colour / Rainbow). GPU-only path. */
        bool effect_tint = false;
        float bright_scale = 1.0f;
        /** Visible parts + optional Effect tint RGB (shared AABB fit). */
        std::vector<MeshAnimPartTint> parts;
    };
    void SetMeshAnimOverlays(std::vector<MeshAnimOverlay> overlays);

    void update();

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
    void ScenePropSelected(int index);
    void ScenePropPositionChanged(int index, float x, float y, float z);
    void ScenePropRotationChanged(int index, float x, float y, float z);
    void DisplayPlanePositionChanged(int index, float x, float y, float z);
    void DisplayPlaneRotationChanged(int index, float x, float y, float z);
    void RoomViewportSelected(bool selected);
    /** Emitted when the effect-volume gizmo finishes a drag (stack index). */
    void EffectVolumeChanged(int stack_index);

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
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void focusSelectionFromKeyboard();
    void resetCameraFromKeyboard();
    void clearCameraDragState();
    void clearSceneObjectSelection();
    void resetRoomPreviewSpin();
    void clampRoomTurntablePitch();
    void applyViewportClickPick(int gl_win_x, int gl_win_y);
    void showGizmoModeFeedback();
    void selectRoomViewport();
    void deselectRoomViewport();
    bool hasRoomPreviewRotation() const;
    void getRoomVolumeAabb(Vector3D& box_min, Vector3D& box_max) const;
    void getRoomTurntablePivot(float& pivot_x, float& pivot_y, float& pivot_z) const;
    ViewportMat4 roomTurntableMatrix() const;
    bool buildPickRay(int win_x, int win_y, Ray3D& ray);
    bool pickRoomVolume(const Ray3D& ray);
    void DrawRoomViewportSelection();
    void drawUnlitBatch(const MeshBatch& batch,
                        MeshBatch::Primitive primitive,
                        float line_width,
                        float alpha,
                        const ViewportMat4* model = nullptr);
    void drawUnlitPoints(const MeshBatch& batch,
                         float point_size,
                         float alpha,
                         const ViewportMat4& model);
    void drawTexturedBatch(const MeshBatch& batch,
                           unsigned int texture_id,
                           float alpha,
                           const ViewportMat4* model = nullptr);
    void drawMeshMaterialBatch(const MeshBatch& batch,
                               const MeshAnim::Material& material,
                               unsigned int base_tex_id,
                               unsigned int emis_tex_id,
                               unsigned int normal_tex_id,
                               unsigned int mr_tex_id,
                               unsigned int occlusion_tex_id,
                               unsigned int shadow_tex_id,
                               const ViewportMat4* light_mvp,
                               float overlay_alpha,
                               float bright_scale,
                               float tint_r,
                               float tint_g,
                               float tint_b,
                               const ViewportMat4* model);
    void drawMeshDepthBatch(const MeshBatch& batch, const ViewportMat4& light_mvp);
    bool ensureMeshShadowFbo();
    unsigned int EnsureMeshAnimTexture(const std::string& scene_path,
                                       int image_index,
                                       const MeshAnim::ImageRGBA& image);
    void ClearMeshAnimTextures();
    void ClearMeshAnimGpuCache();
    /** Upload local-space part VBOs once; animate with node matrices each frame. */
    void EnsureMeshAnimGpuCache(const MeshAnimOverlay& overlay, const MeshAnim::Scene& anim);
    void DrawMeshAnimOverlaysGpu(const MeshAnimOverlay& overlay, const MeshAnim::Scene& anim);
    void applyGizmoMoveMode();
    void applyGizmoRotateMode();
    void applyGizmoScaleMode();
    void applyGizmoSkewMode();
    void applyGizmoFreeroamMode();
    void toggleGridSnapFromKeyboard();
    void tryDeleteSelectedFromKeyboard();
    GridExtents GetRoomExtents() const;
    void DrawGrid();
    void DrawRoomFloor();
    void DrawRoomWalls();
    void DrawReferencePoints();
    void DrawEffectVolume();
    void drawGizmo();
    void drawGizmoMeshPass(const GizmoDrawMesh& mesh, const ViewportMat4& model);
    /** Longest half-extent of the active gizmo target in grid units (0 = screen-constant). */
    float selectedObjectGizmoRadius() const;
    void syncGizmoScreenScale();
    void DrawAxes();
    void paintRoomGuideLabels(QPainter& painter,
                              const float modelview[16],
                              const float projection[16],
                              const int viewport[4]) const;
    void paintRotateReadout(QPainter& painter,
                            const float modelview[16],
                            const float projection[16],
                            const int viewport[4]) const;
    void paintGizmoModeToast(QPainter& painter) const;
    int viewportFramebufferWidth(int logical_w) const;
    int viewportFramebufferHeight(int logical_h) const;
    void paintViewportText2D();
    bool ensureGlCurrent() const;
    ViewportFrame BuildViewportFrame() const;
    void syncPickMatricesFromFrame(const ViewportFrame& frame);
    /** Rebuild pick caches from CPU matrices (no glGet). */
    void computePickMatricesFallback();
    /** Camera view only (no room turntable) — pan/orbit axes. */
    void loadPickMatrices(float modelview[16], float projection[16], int viewport[4]);
    /** Same modelview used to draw controllers/planes/gizmo (view * turntable). */
    void loadScenePickMatrices(float modelview[16], float projection[16], int viewport[4]);
    void paintGlScene();
    void prepareForQtPainterInPaintGl();
    void initViewportShaderPrograms();
    void destroyViewportGlResources();
    void abandonViewportGlHandles();
    size_t populateLedDrawBuffers(ControllerTransform* ctrl);
    void RebuildFloorGridCache(const GridExtents& extents);
    void RebuildRoomShellCache(const GridExtents& extents);
    void DrawControllers();
    void DrawLEDs(ControllerTransform* ctrl, const ViewportMat4& model);
    void DrawUserFigure();
    void DrawRoomBoundary();
    void DrawRoomGridOverlay();
    void getRoomGridOverlayExtents(float& min_x, float& max_x, float& min_y, float& max_y, float& min_z, float& max_z) const;
    void invalidateRoomGridOverlayColors();
    void DrawDisplayPlanes();
    void DrawSceneProps();
    void DrawMeshAnimOverlays();
    void EnsureBloomResources(int fb_w, int fb_h);
    void DestroyBloomResources();
    void CompositeViewportBloom();
    void DrawLightBlockerLayers();
    void UpdateDisplayPlaneTextures();
    void SyncScreenPreviewTimer();
    bool AnyDisplayPlaneWantsScreenPreview() const;
    bool PlaneWantsScreenPreview(DisplayPlane3D* plane) const;
    bool PlaneWantsCalibrationPattern(DisplayPlane3D* plane) const;

    int PickController(const Ray3D& ray);
    int PickReferencePoint(const Ray3D& ray);
    int PickDisplayPlane(const Ray3D& ray);
    int PickSceneProp(const Ray3D& ray);
    bool RayBoxIntersect(const Ray3D& ray,
                        const Vector3D& box_min, const Vector3D& box_max, float& distance);
    bool RaySphereIntersect(const Ray3D& ray,
                           const Vector3D& sphere_center, float sphere_radius, float& distance);

    void CalculateControllerBounds(const ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds) const;
    Vector3D GetControllerCenter(const ControllerTransform* ctrl) const;
    Vector3D GetControllerSize(const ControllerTransform* ctrl) const;

    Vector3D TransformLocalToWorld(const Vector3D& local_pos, const Transform3D& transform);

    std::vector<std::unique_ptr<ControllerTransform>>*  controller_transforms;
    int                                                  selected_controller_idx;
    std::vector<int>                                     selected_controller_indices;

    int     grid_x;
    int     grid_y;
    int     grid_z;
    bool    grid_snap_enabled;
    bool    rotate_snap_enabled_ = false;
    float   grid_scale_mm;

    float   room_width;
    float   room_depth;
    float   room_height;
    bool    use_manual_room_dimensions;

    std::vector<std::unique_ptr<VirtualReferencePoint3D>>* reference_points;
    std::vector<std::unique_ptr<DisplayPlane3D>>* display_planes;
    std::vector<std::unique_ptr<SceneProp3D>>* scene_props_ = nullptr;
    int                                     selected_display_plane_idx;
    int                                     selected_scene_prop_idx_ = -1;
    int                                     selected_ref_point_idx;
    bool                                    effect_volume_active_ = false;
    EffectVolume3D                          effect_volume_{};
    std::unique_ptr<GridContext3D>          effect_volume_grid_;
    bool                                    show_screen_preview;
    bool                                    effect_render_owns_preview_uploads_ = false;
    bool                                    show_calibration_pattern;
    bool                                    show_room_guide_labels_ = true;
    bool                                    show_room_grid_overlay;
    float                                   room_grid_brightness;
    float                                   room_grid_point_size;
    int                                     room_grid_step;
    std::vector<RGBColor>                   room_grid_color_buffer;
    std::function<RGBColor(float, float, float)> room_grid_color_callback;
    std::vector<float>                      room_grid_overlay_positions;
    std::vector<float>                      room_grid_overlay_interleaved_;
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
    QTimer*                                 mesh_anim_timer_ = nullptr;
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
    bool    dragging_room_turntable;
    bool    room_viewport_selected_;
    float   room_turntable_yaw_deg;
    float   room_turntable_pitch_deg;
    /** Captured during paintGlScene before QPainter — pick must match drawn pixels. */
    bool    pick_matrices_valid_ = false;
    float   pick_view_modelview_[16] = {};
    float   pick_scene_modelview_[16] = {};
    float   pick_projection_[16] = {};
    int     pick_viewport_[4] = {0, 0, 1, 1};
    QPoint  last_mouse_pos;
    QPoint  click_start_pos;

    Gizmo3D gizmo;
    GizmoAxis last_gizmo_hover_axis;
    QTimer  gizmo_mode_feedback_timer_;
    bool    viewport_keyboard_shortcuts_installed_ = false;
    bool    viewport_paint_enabled_ = false;

    GlProgram gl_prog_unlit_color_;
    GlProgram gl_prog_unlit_point_;
    GlProgram gl_prog_textured_unlit_;
    GlProgram gl_prog_mesh_unlit_;
    GlProgram gl_prog_mesh_depth_;
    GlProgram gl_prog_bloom_bright_;
    GlProgram gl_prog_bloom_blur_;
    GlProgram gl_prog_bloom_composite_;
    bool      viewport_shader_programs_ok_ = false;
    std::unique_ptr<QOpenGLFramebufferObject> bloom_scene_fbo_;
    std::unique_ptr<QOpenGLFramebufferObject> bloom_bright_fbo_;
    std::unique_ptr<QOpenGLFramebufferObject> bloom_blur_fbo_;
    std::unique_ptr<QOpenGLFramebufferObject> mesh_shadow_fbo_;
    static constexpr int kMeshShadowMapSize = 1024;
    MeshBatch bloom_fullscreen_batch_;
    int bloom_fb_w_ = 0;
    int bloom_fb_h_ = 0;
    MeshBatch floor_fill_batch_;
    MeshBatch floor_grid_batch_;
    MeshBatch floor_border_batch_;
    MeshBatch room_sel_lines_batch_;
    MeshBatch room_sel_fill_batch_;
    MeshBatch axes_lines_batch_;
    MeshBatch axes_heads_batch_;
    MeshBatch room_boundary_batch_;
    MeshBatch room_walls_batch_;
    MeshBatch controller_faces_batch_;
    MeshBatch controller_edges_batch_;
    MeshBatch controller_leds_batch_;
    MeshBatch controller_indicator_batch_;
    MeshBatch room_grid_overlay_batch_;
    MeshBatch display_plane_batch_;
    MeshBatch scene_prop_faces_batch_;
    MeshBatch scene_prop_edges_batch_;
    std::vector<MeshAnimOverlay> mesh_anim_overlays_;
    MeshBatch mesh_anim_textured_batch_;
    std::map<std::string, unsigned int> mesh_anim_textures_;
    /** GPU-resident Animated Mesh parts (local verts); rebuilt when path/tint/fit change. */
    struct MeshAnimGpuPart
    {
        int part_index = -1;
        int node_index = -1;
        int material_index = -1;
        int base_tex = -1;
        int emis_tex = -1;
        int normal_tex = -1;
        int mr_tex = -1;
        int occlusion_tex = -1;
        bool use_mesh_shader = false;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        MeshBatch batch;
    };
    struct MeshAnimGpuCache
    {
        std::string fingerprint;
        std::string path;
        float fit_cx = 0.0f;
        float fit_cy = 0.0f;
        float fit_cz = 0.0f;
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        float scale_z = 1.0f;
        std::vector<MeshAnimGpuPart> parts;
    };
    MeshAnimGpuCache mesh_anim_gpu_;
    MeshBatch gizmo_lines_batch_;
    MeshBatch gizmo_tris_batch_;
    MeshBatch effect_volume_lines_batch_;
    float     room_boundary_cached_max_x_ = -1.0f;
    float     room_boundary_cached_max_y_ = -1.0f;
    float     room_boundary_cached_max_z_ = -1.0f;

    float   cached_floor_grid_max_x;
    float   cached_floor_grid_max_z;
    std::vector<float> cached_floor_grid_interleaved_;
    std::vector<float> led_draw_interleaved_;
};

#endif
