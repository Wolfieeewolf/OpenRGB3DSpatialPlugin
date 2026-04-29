// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALEFFECT3D_H
#define SPATIALEFFECT3D_H

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>

#include "RGBController.h"

#include <vector>
#include <algorithm>
#include <cmath>

#include "LEDPosition3D.h"
#include "SpatialEffectTypes.h"
#include "SpatialLayerCore.h"
#include <nlohmann/json.hpp>

/*
 * Effect sampling volume in the same scene convention as GridSpaceUtils / LEDViewport3D:
 *   X width, Y vertical (floor→ceiling), Z depth (front→back).
 * width/height/depth are spans (max−min) on those axes.
 */
struct GridContext3D
{
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;
    float width, height, depth;
    float center_x, center_y, center_z;
    float grid_scale_mm;

    /** Average position of mapped LEDs in this grid’s coordinate space (room vs world); optional. */
    float led_centroid_x = 0.0f;
    float led_centroid_y = 0.0f;
    float led_centroid_z = 0.0f;
    bool has_led_centroid = false;

    GridContext3D(float minX,
                  float maxX,
                  float minY,
                  float maxY,
                  float minZ,
                  float maxZ,
                  float scale_mm = 10.0f)
        : min_x(minX), max_x(maxX),
          min_y(minY), max_y(maxY),
          min_z(minZ), max_z(maxZ),
          grid_scale_mm(scale_mm)
    {
        width = max_x - min_x;
        height = max_y - min_y;
        depth = max_z - min_z;

        center_x = (min_x + max_x) / 2.0f;
        center_y = (min_y + max_y) / 2.0f;
        center_z = (min_z + max_z) / 2.0f;
    }

    void SetLedCentroid(float cx, float cy, float cz)
    {
        led_centroid_x = cx;
        led_centroid_y = cy;
        led_centroid_z = cz;
        has_led_centroid = true;
    }
};

struct EffectGridAxisHalfExtents
{
    float hw;
    float hh;
    float hd;
};

inline EffectGridAxisHalfExtents MakeEffectGridAxisHalfExtents(const GridContext3D& grid, float normalized_scale)
{
    float s = std::max(0.05f, normalized_scale);
    float hw = grid.width * 0.5f * s;
    float hh = grid.height * 0.5f * s;
    float hd = grid.depth * 0.5f * s;
    if(hw < 1e-6f) hw = 1.0f;
    if(hh < 1e-6f) hh = 1.0f;
    if(hd < 1e-6f) hd = 1.0f;
    return {hw, hh, hd};
}

inline float EffectGridBoundingRadius(const GridContext3D& grid, float normalized_scale)
{
    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, normalized_scale);
    return std::sqrt(e.hw * e.hw + e.hh * e.hh + e.hd * e.hd);
}

inline float EffectGridHorizontalRadialNormXZ(float rx, float rz, float hw, float hd)
{
    float lx = rx / hw;
    float lz = rz / hd;
    return std::sqrt(lx * lx + lz * lz);
}

inline float EffectGridHorizontalRadialNorm01(float r_xz_corner_sqrt2)
{
    constexpr float inv_sqrt2 = 0.70710678f;
    return std::min(1.0f, r_xz_corner_sqrt2 * inv_sqrt2);
}

inline float NormalizeGridAxis01(float value, float min_v, float max_v)
{
    float range = max_v - min_v;
    if(range <= 1e-5f)
    {
        return 0.5f;
    }
    return std::max(0.0f, std::min(1.0f, (value - min_v) / range));
}

inline float EffectGridMedianHalfExtent(const GridContext3D& grid, float normalized_scale)
{
    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, normalized_scale);
    float extents[3] = {e.hw, e.hh, e.hd};
    std::sort(extents, extents + 3);
    const float med = extents[1];
    const float max_e = extents[2];
    // Narrow zones (e.g. strips along one axis): the median half-axis can shrink toward zero even when
    // the long axis still spans usable distance. Floor by a fraction of the dominant half-axis so
    // radial falloff, rings, and depth cues stay tunable on sparse layouts and target-zone bounds.
    constexpr float kDominantAxisFraction = 0.2f;
    return std::max(med, kDominantAxisFraction * max_e);
}

struct EffectInfo3D
{
    int                 info_version;

    const char*         effect_name;
    const char*         effect_description;
    const char*         category;
    SpatialEffectType   effect_type;
    bool                is_reversible;
    bool                supports_random;
    unsigned int        max_speed;
    unsigned int        min_speed;
    unsigned int        user_colors;
    bool                has_custom_settings;
    bool                needs_3d_origin;
    bool                needs_direction;
    bool                needs_thickness;
    bool                needs_arms;
    bool                needs_frequency;

    float               default_speed_scale;
    float               default_frequency_scale;
    float               default_detail_scale = 10.0f;
    bool                use_size_parameter;

    bool                show_speed_control;
    bool                show_brightness_control;
    bool                show_frequency_control;
    bool                show_detail_control = true;
    bool                show_size_control;
    bool                show_scale_control;
    bool                show_fps_control = true;
    bool                show_axis_control;
    bool                show_color_controls;
    bool                show_surface_control = true;
    bool                show_path_axis_control = false;
    bool                show_plane_control = false;
    bool                show_position_offset_control = true;
};

class SpatialEffect3D;

namespace MinecraftGame {
void ApplyFabricGameEffectChrome(SpatialEffect3D* effect);
}

class SpatialEffect3D : public QWidget
{
    Q_OBJECT

public:
    enum EffectBoundsMode
    {
        BOUNDS_MODE_GLOBAL = 0,
        BOUNDS_MODE_TARGET_ZONE = 1
    };

    explicit SpatialEffect3D(QWidget* parent = nullptr);
    virtual ~SpatialEffect3D();
    unsigned int GetTargetFPSSetting() const { return GetTargetFPS(); }

    virtual EffectInfo3D GetEffectInfo() = 0;
    virtual void SetupCustomUI(QWidget* parent) = 0;
    virtual void UpdateParams(SpatialEffectParams& params) = 0;
    virtual RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) = 0;
    /** Call from render paths: applies global Sampling (spatial) when enabled, then CalculateColorGrid. */
    RGBColor EvaluateColorGrid(float x, float y, float z, float time, const GridContext3D& grid);
    /** Override false for effects that only sample textures/screens (UV quantization handles resolution). */
    virtual bool UsesSpatialSamplingQuantization() const { return true; }

    bool EffectGridSampleOutsideVolume(float x, float y, float z, const GridContext3D& grid) const;
    void ApplyGridSampleCoordinateAdjustment(float& x, float& y, float& z, const GridContext3D& grid) const;

    virtual void CreateCommonEffectControls(QWidget* parent, bool include_start_stop = true);
    virtual void UpdateCommonEffectParams(SpatialEffectParams& params);
    virtual void ApplyControlVisibility();


    virtual void SetEffectEnabled(bool enabled) { effect_enabled = enabled; }
    virtual bool IsEffectEnabled() { return effect_enabled; }

    virtual void SetSpeed(unsigned int speed) { effect_speed = speed; }
    virtual unsigned int GetSpeed() { return effect_speed; }

    virtual void SetBrightness(unsigned int brightness) { effect_brightness = brightness; }
    virtual unsigned int GetBrightness() { return effect_brightness; }

    virtual void SetColors(const std::vector<RGBColor>& colors);
    virtual std::vector<RGBColor> GetColors() const;
    virtual void SetRainbowMode(bool enabled);
    virtual bool GetRainbowMode() const;
    bool UseSpatialRoomTint() const { return spatial_mapping_mode != SpatialMappingMode::Off; }

    SpatialMappingMode GetSpatialMappingMode() const { return spatial_mapping_mode; }
    void SetSpatialMappingMode(SpatialMappingMode mode);
    VoxelDriveMode GetVoxelDriveMode() const { return effect_voxel_drive_mode; }
    void SetVoxelDriveMode(VoxelDriveMode mode);
    virtual void SetFrequency(unsigned int frequency);
    virtual unsigned int GetFrequency() const;
    virtual void SetDetail(unsigned int detail);
    virtual unsigned int GetDetail() const;
    virtual unsigned int GetSmoothing() const { return effect_smoothing; }
    virtual unsigned int GetSamplingResolution() const { return effect_sampling_resolution; }
    /** Effective 0..100 sampling quality: global × per-layer (for media-style effects). */
    unsigned int CombineMediaSampling(unsigned int local_detail_percent) const;

    virtual void SetReferenceMode(ReferenceMode mode);
    virtual int GetPathAxis() const { return effect_path_axis; }
    virtual int GetPlane() const { return effect_plane; }
    virtual int GetSurfaceMask() const { return effect_surface_mask; }
    Vector3D GetReferencePointGrid(const GridContext3D& grid) const;
    virtual bool IsPointOnActiveSurface(float x, float y, float z, const GridContext3D& grid) const;
    virtual ReferenceMode GetReferenceMode() const;
    virtual void SetGlobalReferencePoint(const Vector3D& point);
    virtual Vector3D GetGlobalReferencePoint() const;
    virtual void SetCustomReferencePoint(const Vector3D& point);
    virtual void SetUseCustomReference(bool use_custom);

    QPushButton* GetStartButton() { return start_effect_button; }
    QPushButton* GetStopButton() { return stop_effect_button; }

    /** Set after CreateCommonEffectControls: add per-effect controls here from SetupCustomUI. */
    QWidget* GetCustomSettingsHost() const { return custom_effect_settings_host; }

    float GetRotationYaw() const { return effect_rotation_yaw; }
    float GetRotationPitch() const { return effect_rotation_pitch; }
    float GetRotationRoll() const { return effect_rotation_roll; }

    RGBColor PostProcessColorGrid(RGBColor color) const;

    Vector3D GetEffectOriginGrid(const GridContext3D& grid) const;
    float GetBoundaryMultiplier(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const;
    float ApplyEdgeToIntensity(float normalized_dist) const;

    void ApplyAxisScale(float& x, float& y, float& z, const GridContext3D& grid) const;
    void ApplyEffectRotation(float& x, float& y, float& z, const GridContext3D& grid) const;

    virtual bool RequiresWorldSpaceCoordinates() const { return true; }
    virtual bool RequiresWorldSpaceGridBounds() const { return false; }
    virtual bool UseWorldGridBounds() const;
    virtual bool UseZoneGrid() const;
    int GetEffectBoundsMode() const { return effect_bounds_mode; }
    void SetEffectBoundsMode(int mode) { effect_bounds_mode = std::clamp(mode, (int)BOUNDS_MODE_GLOBAL, (int)BOUNDS_MODE_TARGET_ZONE); }

    virtual nlohmann::json SaveSettings() const;
    virtual void LoadSettings(const nlohmann::json& settings);

signals:
    void ParametersChanged();

protected:
    void SetControlGroupVisibility(QSlider* slider, QLabel* value_label, const QString& label_text, bool visible);

    friend void MinecraftGame::ApplyFabricGameEffectChrome(SpatialEffect3D* effect);

    QWidget*            effect_controls_group;
    QWidget*            custom_effect_settings_host;
    QSlider*            speed_slider;
    QSlider*            brightness_slider;
    QSlider*            frequency_slider;
    QSlider*            detail_slider;
    QSlider*            size_slider;
    QSlider*            scale_slider;
    QCheckBox*          scale_invert_check;
    QSlider*            fps_slider;
    QLabel*             speed_label;
    QLabel*             brightness_label;
    QLabel*             frequency_label;
    QLabel*             detail_label;
    QLabel*             size_label;
    QLabel*             scale_label;
    QLabel*             fps_label;

    QSlider*            intensity_slider;
    QLabel*             intensity_label;
    QSlider*            sharpness_slider;
    QLabel*             sharpness_label;
    QSlider*            smoothing_slider;
    QLabel*             smoothing_label;
    QSlider*            sampling_resolution_slider;
    QLabel*             sampling_resolution_label;
    QGroupBox*          edge_shape_group;
    QComboBox*          edge_profile_combo;
    QSlider*            edge_thickness_slider;
    QLabel*             edge_thickness_label;
    QSlider*            glow_level_slider;
    QLabel*             glow_level_label;

    QSlider*            scale_x_slider;
    QLabel*             scale_x_label;
    QSlider*            scale_y_slider;
    QLabel*             scale_y_label;
    QSlider*            scale_z_slider;
    QLabel*             scale_z_label;
    QPushButton*        axis_scale_reset_button;

    QSlider*            rotation_yaw_slider;
    QSlider*            rotation_pitch_slider;
    QSlider*            rotation_roll_slider;
    QLabel*             rotation_yaw_label;
    QLabel*             rotation_pitch_label;
    QLabel*             rotation_roll_label;
    QPushButton*        rotation_reset_button;

    QSlider*            axis_scale_rot_yaw_slider;
    QSlider*            axis_scale_rot_pitch_slider;
    QSlider*            axis_scale_rot_roll_slider;
    QLabel*             axis_scale_rot_yaw_label;
    QLabel*             axis_scale_rot_pitch_label;
    QLabel*             axis_scale_rot_roll_label;
    QPushButton*        axis_scale_rot_reset_button;

    QWidget*            color_controls_group;
    QCheckBox*          rainbow_mode_check;

    QWidget*            sampler_mapper_group;
    QGroupBox*          compass_sampler_group;
    QComboBox*          spatial_mapping_combo;
    QComboBox*          compass_layer_spin_combo;
    QSlider*            sampler_influence_slider;
    QLabel*             sampler_influence_label;
    QSlider*            sampler_compass_north_slider;
    QLabel*             sampler_compass_north_label;

    QGroupBox*          voxel_volume_group;
    QSlider*            voxel_volume_mix_slider;
    QLabel*             voxel_volume_mix_label;
    QSlider*            voxel_volume_scale_slider;
    QLabel*             voxel_volume_scale_label;
    QSlider*            voxel_volume_heading_slider;
    QLabel*             voxel_volume_heading_label;
    QComboBox*          voxel_drive_combo;
    QWidget*            color_buttons_widget;
    QHBoxLayout*        color_buttons_layout;
    QPushButton*        add_color_button;
    QPushButton*        remove_color_button;
    std::vector<QPushButton*> color_buttons;
    std::vector<RGBColor> colors;

    QPushButton*        start_effect_button;
    QPushButton*        stop_effect_button;

    bool                effect_enabled;
    bool                effect_running;
    unsigned int        effect_speed;
    unsigned int        effect_brightness;
    unsigned int        effect_frequency;
    unsigned int        effect_detail;
    unsigned int        effect_size;
    unsigned int        effect_scale;
    bool                scale_inverted;
    int                 effect_bounds_mode;
    unsigned int        effect_fps;
    bool                rainbow_mode;
    SpatialMappingMode  spatial_mapping_mode;
    int                 compass_layer_spin_preset;
    float               rainbow_progress;

    unsigned int        effect_voxel_volume_mix;
    int                 effect_voxel_room_scale_centi;
    int                 effect_voxel_heading_offset;
    VoxelDriveMode      effect_voxel_drive_mode;
    /** 25–250, default 100: scales room-map and voxel palette drive strength. */
    int                 effect_sampler_influence_centi;
    /** −180…180°: rotates compass sector indexing (room “north” vs effect forward). */
    int                 effect_sampler_compass_north_offset_deg;

    unsigned int        effect_intensity;
    unsigned int        effect_sharpness;
    unsigned int        effect_smoothing;
    unsigned int        effect_sampling_resolution;
    int                 effect_edge_profile;   /* 0=Sharp, 1=Square, 2=Round, 3=Smooth, 4=Sharpen */
    unsigned int        effect_edge_thickness; /* 0..100: transition width / line thickness */
    unsigned int        effect_glow_level;     /* 0..100: soft glow beyond edge */
    unsigned int        effect_scale_x;
    unsigned int        effect_scale_y;
    unsigned int        effect_scale_z;

    float               effect_rotation_yaw;
    float               effect_rotation_pitch;
    float               effect_rotation_roll;

    float               effect_axis_scale_rotation_yaw;
    float               effect_axis_scale_rotation_pitch;
    float               effect_axis_scale_rotation_roll;

    int                 effect_path_axis;
    int                 effect_plane;
    int                 effect_surface_mask;

    int                 effect_offset_x;
    int                 effect_offset_y;
    int                 effect_offset_z;

    QGroupBox*          position_offset_group;
    QSlider*            offset_x_slider;
    QSlider*            offset_y_slider;
    QSlider*            offset_z_slider;
    QLabel*             offset_x_label;
    QLabel*             offset_y_label;
    QLabel*             offset_z_label;

    QGroupBox*          surfaces_group;
    QGroupBox*          path_plane_group;
    QComboBox*          path_axis_combo;
    QComboBox*          plane_combo;

    ReferenceMode       reference_mode;
    Vector3D            global_reference_point;
    Vector3D            custom_reference_point;
    bool                use_custom_reference;

    void AddWidgetToParent(QWidget* w, QWidget* container);

    Vector3D GetEffectOrigin() const;
    RGBColor GetRainbowColor(float hue);
    RGBColor GetColorAtPosition(float position);

    float GetNormalizedSpeed() const;
    float GetNormalizedFrequency() const;
    float GetNormalizedDetail() const;
    float GetNormalizedSize() const;
    float GetNormalizedScale() const;
    unsigned int GetTargetFPS() const;
    bool IsScaleInverted() const { return scale_inverted; }
    void SetScaleInverted(bool inverted);

    float GetScaledSpeed() const;
    float GetScaledFrequency() const;
    float GetScaledDetail() const;
    float CalculateProgress(float time) const;

    /** Solid palette 0..1 after room mapping (Off / Subtle / Compass / Voxel volume). */
    float ApplySpatialPalette01(float base_pos01,
                                const SpatialLayerCore::Basis& basis,
                                const SpatialLayerCore::SamplePoint& sp,
                                const SpatialLayerCore::MapperSettings& map,
                                float time,
                                const GridContext3D* grid = nullptr) const;
    /** Rainbow hue (deg) after room mapping; plane_pos01 is the effect's local wedge position 0..1. */
    float ApplySpatialRainbowHue(float hue_deg,
                                 float plane_pos01,
                                 const SpatialLayerCore::Basis& basis,
                                 const SpatialLayerCore::SamplePoint& sp,
                                 const SpatialLayerCore::MapperSettings& map,
                                 float time,
                                 const GridContext3D* grid = nullptr) const;
    /** Apply voxel/room scroll drive to an already-mapped palette coordinate. */
    float ApplyVoxelDriveToPalette01(float palette01, float x, float y, float z, float time, const GridContext3D& grid) const;

    bool IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const;

    Vector3D TransformPointByRotation(float x, float y, float z,
                                      const Vector3D& origin) const;
    static Vector3D RotateVectorByEuler(float dx, float dy, float dz, float yaw_deg, float pitch_deg, float roll_deg);
    float ApplySharpness(float value) const;

private slots:
    void OnParameterChanged();
    void OnRainbowModeChanged();
    void OnSpatialMappingComboChanged();
    void OnCompassLayerSpinComboChanged();
    void OnVoxelDriveComboChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnStartEffectClicked();
    void OnStopEffectClicked();
    void OnRotationChanged();
    void OnRotationResetClicked();
    void OnAxisScaleResetClicked();
    void OnAxisScaleRotationResetClicked();

private:
    void CreateColorControls();
    void CreateSamplerMapperControls();
    void CreateColorButton(RGBColor color);
    void RemoveLastColorButton();
    bool SampleVoxelRgbAtRoom(float x, float y, float z, const GridContext3D& grid, RGBColor& out_rgb, bool& got_hit) const;
    void SyncSpatialMappingControlVisibility();
};

#endif
