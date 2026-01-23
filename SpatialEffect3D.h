// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALEFFECT3D_H
#define SPATIALEFFECT3D_H

#include <QWidget>
#include <QLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QColorDialog>

#include "RGBController.h"

#include <vector>

#include "LEDPosition3D.h"
#include "SpatialEffectTypes.h"
#include <nlohmann/json.hpp>

// Grid context describes the current room bounds in grid units.
struct GridContext3D
{
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;
    float width, height, depth;
    float center_x, center_y, center_z;
    float grid_scale_mm;

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
};

struct EffectInfo3D
{
    // Version field - set to 2 for effects using the new standardized system
    // Old effects won't set this (will be 0), new effects set it to 2
    int                 info_version;             // Set to 2 for new effects

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

    // Standardized parameter scaling (defaults suitable for most effects)
    float               default_speed_scale;      // Default: 10.0 (multiplier after normalization)
    float               default_frequency_scale;  // Default: 10.0 (multiplier after normalization)
    bool                use_size_parameter;       // Default: true (effect supports size scaling)

    // Control visibility flags - set to false if effect provides its own custom version
    bool                show_speed_control;       // Default: true (show base speed slider)
    bool                show_brightness_control;  // Default: true (show base brightness slider)
    bool                show_frequency_control;   // Default: true (show base frequency slider)
    bool                show_size_control;        // Default: true (show base size slider)
    bool                show_scale_control;       // Default: true (show base scale slider)
    bool                show_fps_control;         // Default: true (show base FPS slider)
    bool                show_axis_control;        // Default: true (show axis selection)
    bool                show_color_controls;      // Default: true (show color/rainbow controls)
};

class SpatialEffect3D : public QWidget
{
    Q_OBJECT

public:
    explicit SpatialEffect3D(QWidget* parent = nullptr);
    virtual ~SpatialEffect3D();
    // Public accessor for target FPS so UI can query without needing protected access
    unsigned int GetTargetFPSSetting() const { return GetTargetFPS(); }

    // Pure virtual methods each effect must implement
    virtual EffectInfo3D GetEffectInfo() = 0;
    virtual void SetupCustomUI(QWidget* parent) = 0;
    virtual void UpdateParams(SpatialEffectParams& params) = 0;
    virtual RGBColor CalculateColor(float x, float y, float z, float time) = 0;

    // Optional grid calculation hook (defaults to CalculateColor for legacy effects)
    virtual RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
    {
        // Default grid-aware behavior: compute origin with grid context and apply room-relative boundary.
        Vector3D origin_grid = GetEffectOriginGrid(grid);
        float rel_x = x - origin_grid.x;
        float rel_y = y - origin_grid.y;
        float rel_z = z - origin_grid.z;

        if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        {
            return 0x00000000; // Outside coverage area
        }

        // Adjust coordinates so legacy CalculateColor() computes the same rel_* using its own origin
        // x_adj = x - origin_grid + origin_legacy. For room-center, legacy origin is (0,0,0).
        Vector3D origin_legacy = GetEffectOrigin();
        float x_adj = x - origin_grid.x + origin_legacy.x;
        float y_adj = y - origin_grid.y + origin_legacy.y;
        float z_adj = z - origin_grid.z + origin_legacy.z;

        // Temporarily mark boundary as prevalidated so legacy boundary check passes
        boundary_prevalidated = true;
        RGBColor result = CalculateColor(x_adj, y_adj, z_adj, time);
        boundary_prevalidated = false;
        return result;
    }

    // Common effect controls shared by all effects
    virtual void CreateCommonEffectControls(QWidget* parent);
    virtual void UpdateCommonEffectParams(SpatialEffectParams& params);
    virtual void ApplyControlVisibility();      // Apply visibility flags from EffectInfo


    // Effect state management
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
    virtual void SetFrequency(unsigned int frequency);
    virtual unsigned int GetFrequency() const;

    // Reference point system
    virtual void SetReferenceMode(ReferenceMode mode);
    virtual ReferenceMode GetReferenceMode() const;
    virtual void SetGlobalReferencePoint(const Vector3D& point);
    virtual Vector3D GetGlobalReferencePoint() const;
    virtual void SetCustomReferencePoint(const Vector3D& point);
    virtual void SetUseCustomReference(bool use_custom);

    // Button accessors for parent tab to connect
    QPushButton* GetStartButton() { return start_effect_button; }
    QPushButton* GetStopButton() { return stop_effect_button; }

    // Get rotation values
    float GetRotationYaw() const { return effect_rotation_yaw; }
    float GetRotationPitch() const { return effect_rotation_pitch; }
    float GetRotationRoll() const { return effect_rotation_roll; }

    // Global post-processing helpers apply coverage/intensity shaping consistently
    RGBColor PostProcessColorGrid(float x, float y, float z, RGBColor color, const GridContext3D& grid) const;

    // Most spatial effects should operate in true world space so controller rotation/translation
    // changes their position in the room. Effects that must be controller-local can override.
    virtual bool RequiresWorldSpaceCoordinates() const { return true; }

    // When using world-space coordinates, most "room-locked" effects should still normalize against
    // ROOM-ALIGNED bounds (stable) rather than WORLD bounds (which can move with a single controller).
    // Effects like screen/ambilight mapping may prefer WORLD bounds.
    virtual bool RequiresWorldSpaceGridBounds() const { return false; }

    // Serialization helpers
    virtual nlohmann::json SaveSettings() const;
    virtual void LoadSettings(const nlohmann::json& settings);

signals:
    void ParametersChanged();

protected:
    // Common effect controls
    QGroupBox*          effect_controls_group;
    QSlider*            speed_slider;
    QSlider*            brightness_slider;
    QSlider*            frequency_slider;
    QSlider*            size_slider;
    QSlider*            scale_slider;
    QCheckBox*          scale_invert_check;
    QSlider*            fps_slider;
    QLabel*             speed_label;
    QLabel*             brightness_label;
    QLabel*             frequency_label;
    QLabel*             size_label;
    QLabel*             scale_label;
    QLabel*             fps_label;

    // Shaping controls
    QSlider*            intensity_slider;     // 0..200 (100 = neutral)
    QSlider*            sharpness_slider;     // 0..200 (100 = neutral)

    // 3D Rotation controls (replaces axis/coverage)
    QSlider*            rotation_yaw_slider;   // 0-360 degrees, horizontal rotation
    QSlider*            rotation_pitch_slider; // 0-360 degrees, vertical rotation
    QSlider*            rotation_roll_slider;  // 0-360 degrees, twist rotation
    QLabel*             rotation_yaw_label;
    QLabel*             rotation_pitch_label;
    QLabel*             rotation_roll_label;
    QPushButton*        rotation_reset_button;

    // Color management controls
    QGroupBox*          color_controls_group;
    QCheckBox*          rainbow_mode_check;
    QWidget*            color_buttons_widget;
    QHBoxLayout*        color_buttons_layout;
    QPushButton*        add_color_button;
    QPushButton*        remove_color_button;
    std::vector<QPushButton*> color_buttons;
    std::vector<RGBColor> colors;

    // Effect control buttons
    QPushButton*        start_effect_button;
    QPushButton*        stop_effect_button;

    // Effect parameters
    bool                effect_enabled;
    bool                effect_running;
    unsigned int        effect_speed;
    unsigned int        effect_brightness;
    unsigned int        effect_frequency;
    unsigned int        effect_size;
    unsigned int        effect_scale;
    bool                scale_inverted;
    unsigned int        effect_fps;
    bool                rainbow_mode;
    float               rainbow_progress;
    // When true, legacy boundary check is bypassed (grid boundary already validated)
    bool                boundary_prevalidated;

    // Global shaping params
    unsigned int        effect_intensity;     // 0..200 (100 neutral)
    unsigned int        effect_sharpness;     // 0..200 (100 neutral)

    // 3D Rotation parameters (replaces axis/coverage)
    float               effect_rotation_yaw;   // 0-360 degrees, horizontal rotation around Y axis
    float               effect_rotation_pitch; // 0-360 degrees, vertical rotation around X axis
    float               effect_rotation_roll;  // 0-360 degrees, twist rotation around Z axis

    // Reference Point System (for effect origin)
    ReferenceMode       reference_mode;         // Room center / User position / Custom
    Vector3D            global_reference_point; // Set by parent tab (user position or 0,0,0)
    Vector3D            custom_reference_point; // Effect-specific override (future)
    bool                use_custom_reference;   // Override global setting

    // Helper methods for derived classes
    Vector3D GetEffectOrigin() const;           // Returns correct origin based on reference mode (legacy - returns 0,0,0 for room center)
    Vector3D GetEffectOriginGrid(const GridContext3D& grid) const;  // Grid-aware origin (uses grid.center for room center)
    RGBColor GetRainbowColor(float hue);
    RGBColor GetColorAtPosition(float position);

    // Standardized parameter calculation helpers
    float GetNormalizedSpeed() const;           // Returns 0.0-1.0 speed with consistent curve
    float GetNormalizedFrequency() const;       // Returns 0.0-1.0 frequency with consistent curve
    float GetNormalizedSize() const;            // Returns 0.1-2.0 size multiplier (linear)
    float GetNormalizedScale() const;           // Returns 0.1-2.0 scale multiplier (affects area coverage)
    unsigned int GetTargetFPS() const;          // Returns FPS setting (1-60)
    bool IsScaleInverted() const { return scale_inverted; }
    void SetScaleInverted(bool inverted);

    // Advanced: Scaled versions that apply effect-specific multipliers
    float GetScaledSpeed() const;               // Returns speed * effect's speed_scale
    float GetScaledFrequency() const;           // Returns frequency * effect's frequency_scale

    // Universal progress calculator - use this for ANY effect animation
    float CalculateProgress(float time) const;  // Returns time * scaled_speed (handles reverse too)

    // Boundary checking helpers
    bool IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z) const;  // Legacy version (uses fixed radius)
    bool IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const;  // Room-aware version (RECOMMENDED)

    // 3D Rotation transformation helper
    Vector3D TransformPointByRotation(float x, float y, float z, 
                                      const Vector3D& origin) const;
    float ApplySharpness(float value) const;    // gamma-like shaping around 1.0

private slots:
    void OnParameterChanged();
    void OnRainbowModeChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnStartEffectClicked();
    void OnStopEffectClicked();
    void OnRotationChanged();
    void OnRotationResetClicked();

private:
    void CreateColorControls();
    void CreateColorButton(RGBColor color);
    void RemoveLastColorButton();
    void SetControlGroupVisibility(QSlider* slider, QLabel* value_label, const QString& label_text, bool visible);
};

#endif
