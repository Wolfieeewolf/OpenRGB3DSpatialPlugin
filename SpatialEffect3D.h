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
    bool                use_size_parameter;

    bool                show_speed_control;
    bool                show_brightness_control;
    bool                show_frequency_control;
    bool                show_size_control;
    bool                show_scale_control;
    bool                show_fps_control;
    bool                show_axis_control;
    bool                show_color_controls;
};

class SpatialEffect3D : public QWidget
{
    Q_OBJECT

public:
    explicit SpatialEffect3D(QWidget* parent = nullptr);
    virtual ~SpatialEffect3D();
    unsigned int GetTargetFPSSetting() const { return GetTargetFPS(); }

    virtual EffectInfo3D GetEffectInfo() = 0;
    virtual void SetupCustomUI(QWidget* parent) = 0;
    virtual void UpdateParams(SpatialEffectParams& params) = 0;
    virtual RGBColor CalculateColor(float x, float y, float z, float time) = 0;

    virtual RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
    {
        Vector3D origin_grid = GetEffectOriginGrid(grid);
        float rel_x = x - origin_grid.x;
        float rel_y = y - origin_grid.y;
        float rel_z = z - origin_grid.z;

        if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        {
            return 0x00000000;
        }

        Vector3D effect_origin = GetEffectOrigin();
        float x_adj = x - origin_grid.x + effect_origin.x;
        float y_adj = y - origin_grid.y + effect_origin.y;
        float z_adj = z - origin_grid.z + effect_origin.z;
        boundary_prevalidated = true;
        RGBColor result = CalculateColor(x_adj, y_adj, z_adj, time);
        boundary_prevalidated = false;
        return result;
    }

    virtual void CreateCommonEffectControls(QWidget* parent, bool include_start_stop = true);
    virtual void UpdateCommonEffectParams(SpatialEffectParams& params);
    virtual void ApplyControlVisibility();      // Apply visibility flags from EffectInfo


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

    virtual void SetReferenceMode(ReferenceMode mode);
    virtual ReferenceMode GetReferenceMode() const;
    virtual void SetGlobalReferencePoint(const Vector3D& point);
    virtual Vector3D GetGlobalReferencePoint() const;
    virtual void SetCustomReferencePoint(const Vector3D& point);
    virtual void SetUseCustomReference(bool use_custom);

    QPushButton* GetStartButton() { return start_effect_button; }
    QPushButton* GetStopButton() { return stop_effect_button; }

    float GetRotationYaw() const { return effect_rotation_yaw; }
    float GetRotationPitch() const { return effect_rotation_pitch; }
    float GetRotationRoll() const { return effect_rotation_roll; }

    RGBColor PostProcessColorGrid(RGBColor color) const;
    void ApplyAxisScale(float& x, float& y, float& z, const GridContext3D& grid) const;
    void ApplyEffectRotation(float& x, float& y, float& z, const GridContext3D& grid) const;

    virtual bool RequiresWorldSpaceCoordinates() const { return true; }
    virtual bool RequiresWorldSpaceGridBounds() const { return false; }

    virtual nlohmann::json SaveSettings() const;
    virtual void LoadSettings(const nlohmann::json& settings);

signals:
    void ParametersChanged();

protected:
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

    QSlider*            intensity_slider;
    QLabel*             intensity_label;
    QSlider*            sharpness_slider;
    QLabel*             sharpness_label;

    QSlider*            scale_x_slider;
    QLabel*             scale_x_label;
    QSlider*            scale_y_slider;
    QLabel*             scale_y_label;
    QSlider*            scale_z_slider;
    QLabel*             scale_z_label;

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

    QGroupBox*          color_controls_group;
    QCheckBox*          rainbow_mode_check;
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
    unsigned int        effect_size;
    unsigned int        effect_scale;
    bool                scale_inverted;
    unsigned int        effect_fps;
    bool                rainbow_mode;
    float               rainbow_progress;
    bool                boundary_prevalidated;

    unsigned int        effect_intensity;
    unsigned int        effect_sharpness;
    unsigned int        effect_scale_x;
    unsigned int        effect_scale_y;
    unsigned int        effect_scale_z;

    float               effect_rotation_yaw;
    float               effect_rotation_pitch;
    float               effect_rotation_roll;

    /** Rotation of the axis-scale frame only (scale is applied in this frame); does not rotate the effect pattern. */
    float               effect_axis_scale_rotation_yaw;
    float               effect_axis_scale_rotation_pitch;
    float               effect_axis_scale_rotation_roll;

    ReferenceMode       reference_mode;
    Vector3D            global_reference_point;
    Vector3D            custom_reference_point;
    bool                use_custom_reference;

    void AddWidgetToParent(QWidget* w, QWidget* container);

    Vector3D GetEffectOrigin() const;
    Vector3D GetEffectOriginGrid(const GridContext3D& grid) const;
    RGBColor GetRainbowColor(float hue);
    RGBColor GetColorAtPosition(float position);

    float GetNormalizedSpeed() const;
    float GetNormalizedFrequency() const;
    float GetNormalizedSize() const;
    float GetNormalizedScale() const;
    unsigned int GetTargetFPS() const;
    bool IsScaleInverted() const { return scale_inverted; }
    void SetScaleInverted(bool inverted);

    float GetScaledSpeed() const;
    float GetScaledFrequency() const;
    float CalculateProgress(float time) const;

    bool IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z) const;
    bool IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const;

    Vector3D TransformPointByRotation(float x, float y, float z,
                                      const Vector3D& origin) const;
    /** Rotate vector (dx,dy,dz) by yaw/pitch/roll in degrees (same order as effect rotation). Used for axis-scale rotation. */
    static Vector3D RotateVectorByEuler(float dx, float dy, float dz, float yaw_deg, float pitch_deg, float roll_deg);
    float ApplySharpness(float value) const;

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
