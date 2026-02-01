// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEDVIEWPORT3D_H
#define LEDVIEWPORT3D_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>

#include <vector>
#include <memory>
#include <map>
#include <functional>

#include "LEDPosition3D.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "Gizmo3D.h"
#include "SpatialEffectTypes.h"
#include "VirtualController3D.h"
#include "DisplayPlane3D.h"

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
    void SetGridDimensions(int x, int y, int z);
    void SetGridSnapEnabled(bool enabled);
    bool IsGridSnapEnabled() const { return grid_snap_enabled; }
    void SetRoomDimensions(float width, float depth, float height, bool use_manual);
    // Set physical millimeters represented by one grid unit (default 10mm)
    void SetGridScaleMM(float mm_per_unit) { grid_scale_mm = (mm_per_unit > 0.001f) ? mm_per_unit : 10.0f; }
    void SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points);
    void SetDisplayPlanes(std::vector<std::unique_ptr<DisplayPlane3D>>* planes);
    void SelectDisplayPlane(int index);
    void NotifyDisplayPlaneChanged();
    void SetShowScreenPreview(bool show);
    void SetShowTestPattern(bool show) { show_test_pattern = show; update(); }
    void ClearDisplayPlaneTextures();

    /** Fill the room with a grid of lit points (virtual LEDs) so you can see the full room and distinguish real vs virtual LEDs. */
    void SetShowRoomGridOverlay(bool show) { show_room_grid_overlay = show; update(); }
    void SetRoomGridBrightness(float brightness) { room_grid_brightness = std::max(0.0f, std::min(1.0f, brightness)); update(); }
    void SetRoomGridPointSize(float size) { room_grid_point_size = std::max(0.5f, std::min(12.0f, size)); update(); }
    void SetRoomGridStep(int step) { room_grid_step = std::max(1, std::min(24, step)); update(); }
    bool GetShowRoomGridOverlay() const { return show_room_grid_overlay; }
    float GetRoomGridBrightness() const { return room_grid_brightness; }
    float GetRoomGridPointSize() const { return room_grid_point_size; }
    int GetRoomGridStep() const { return room_grid_step; }
    /** Layout for overlay: same as DrawRoomGridOverlay iteration (ix, iy, iz). Tab uses this to fill SetRoomGridColorBuffer. */
    void GetRoomGridOverlayDimensions(int* out_nx, int* out_ny, int* out_nz) const;
    /** Optional: precomputed colors (size must match nx*ny*nz from GetRoomGridOverlayDimensions). Faster than per-point callback. */
    void SetRoomGridColorBuffer(const std::vector<RGBColor>& buf);
    /** Optional: when set, overlay points use this to show effect output (fallback if no buffer). Cleared when effect stops. */
    void SetRoomGridColorCallback(std::function<RGBColor(float x, float y, float z)> cb) { room_grid_color_callback = std::move(cb); update(); }

    // Camera persistence helpers
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

    void UpdateGizmoPosition();
    void NotifyControllerTransformChanged();

    void AddControllerToSelection(int index);
    void RemoveControllerFromSelection(int index);
    void ClearSelection();
    bool IsControllerSelected(int index) const;
    const std::vector<int>& GetSelectedControllers() const { return selected_controller_indices; }

signals:
    void ControllerSelected(int index);
    void ControllerPositionChanged(int index, float x, float y, float z);
    void ControllerRotationChanged(int index, float x, float y, float z);
    void ControllerDeleteRequested(int index);
    void ReferencePointSelected(int index);
    void ReferencePointPositionChanged(int index, float x, float y, float z);
    void DisplayPlanePositionChanged(int index, float x, float y, float z);
    void DisplayPlaneRotationChanged(int index, float x, float y, float z);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    GridExtents GetRoomExtents() const;
    void DrawGrid();
    void DrawReferencePoints();
    void DrawAxes();
    void DrawAxisLabels();
    void DrawControllers();
    void DrawLEDs(ControllerTransform* ctrl);
    void DrawUserFigure();
    void DrawRoomBoundary();
    void DrawRoomGridOverlay();
    void DrawDisplayPlanes();
    void UpdateDisplayPlaneTextures();

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

    // Grid dimensions for proper bounds and visualization
    int     grid_x;
    int     grid_y;
    int     grid_z;
    bool    grid_snap_enabled;
    // Millimeters per grid unit used for converting manual room dimensions to grid units
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
    bool                                    show_test_pattern;
    bool                                    show_room_grid_overlay;   // Fill room with grid points as "virtual LEDs"
    float                                   room_grid_brightness;     // 0-1 for overlay points (so real LEDs stand out)
    float                                   room_grid_point_size;     // Point size for overlay (1-12)
    int                                     room_grid_step;            // Grid units between overlay points (1=dense, 24=sparse); affects CPU/GPU load
    std::vector<RGBColor>                   room_grid_color_buffer;     // Precomputed colors from tab (faster than callback)
    std::function<RGBColor(float, float, float)> room_grid_color_callback;  // Fallback when no buffer
    std::vector<float>                      room_grid_overlay_positions; // Cached x,y,z per point (3*N), invalidated when step/extents change
    std::vector<float>                      room_grid_overlay_colors;    // R,G,B per point (3*N), filled each frame
    int                                     room_grid_overlay_last_nx;
    int                                     room_grid_overlay_last_ny;
    int                                     room_grid_overlay_last_nz;
    std::map<std::string, GLuint>           display_plane_textures;  // Texture IDs per capture source
    QTimer*                                 screen_preview_refresh_timer;

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
};

#endif
