/*---------------------------------------------------------*\
| SpatialEffect3D.h                                         |
|                                                           |
|   Base class for 3D spatial effects with custom UI      |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECT3D_H
#define SPATIALEFFECT3D_H

/*---------------------------------------------------------*\
| Qt Includes                                              |
\*---------------------------------------------------------*/
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

/*---------------------------------------------------------*\
| OpenRGB Includes                                         |
\*---------------------------------------------------------*/
#include "RGBController.h"

/*---------------------------------------------------------*\
| Local Includes                                           |
\*---------------------------------------------------------*/
#include "LEDPosition3D.h"
#include "SpatialEffectTypes.h"

/*---------------------------------------------------------*\
| Grid context for dynamic effect scaling                 |
\*---------------------------------------------------------*/
struct GridContext3D
{
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;
    float width, height, depth;

    GridContext3D(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
        : min_x(minX), max_x(maxX), min_y(minY), max_y(maxY), min_z(minZ), max_z(maxZ)
    {
        width = max_x - min_x + 1.0f;
        height = max_y - min_y + 1.0f;
        depth = max_z - min_z + 1.0f;
    }
};

struct EffectInfo3D
{
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
};

class SpatialEffect3D : public QWidget
{
    Q_OBJECT

public:
    explicit SpatialEffect3D(QWidget* parent = nullptr);
    virtual ~SpatialEffect3D();

    /*---------------------------------------------------------*\
    | Pure virtual methods each effect must implement         |
    \*---------------------------------------------------------*/
    virtual EffectInfo3D GetEffectInfo() = 0;
    virtual void SetupCustomUI(QWidget* parent) = 0;
    virtual void UpdateParams(SpatialEffectParams& params) = 0;
    virtual RGBColor CalculateColor(float x, float y, float z, float time) = 0;

    /*---------------------------------------------------------*\
    | Optional grid calculation (for backward compatibility)   |
    | Default implementation just calls CalculateColor()       |
    \*---------------------------------------------------------*/
    virtual RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
    {
        (void)grid;
        return CalculateColor(x, y, z, time);
    }

    /*---------------------------------------------------------*\
    | Common effect controls (all effects need these)        |
    \*---------------------------------------------------------*/
    virtual void CreateCommonEffectControls(QWidget* parent);
    virtual void UpdateCommonEffectParams(SpatialEffectParams& params);

    /*---------------------------------------------------------*\
    | Common 3D spatial controls                               |
    \*---------------------------------------------------------*/
    virtual void CreateCommon3DControls(QWidget* parent);
    virtual void UpdateCommon3DParams(SpatialEffectParams& params);

    /*---------------------------------------------------------*\
    | Effect state management                                  |
    \*---------------------------------------------------------*/
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

    /*---------------------------------------------------------*\
    | Button accessors for parent tab to connect              |
    \*---------------------------------------------------------*/
    QPushButton* GetStartButton() { return start_effect_button; }
    QPushButton* GetStopButton() { return stop_effect_button; }

signals:
    void ParametersChanged();

protected:
    /*---------------------------------------------------------*\
    | Common effect controls                                   |
    \*---------------------------------------------------------*/
    QGroupBox*          effect_controls_group;
    QSlider*            speed_slider;
    QSlider*            brightness_slider;
    QSlider*            frequency_slider;
    QLabel*             speed_label;
    QLabel*             brightness_label;
    QLabel*             frequency_label;

    /*---------------------------------------------------------*\
    | Color management controls                                |
    \*---------------------------------------------------------*/
    QGroupBox*          color_controls_group;
    QCheckBox*          rainbow_mode_check;
    QWidget*            color_buttons_widget;
    QHBoxLayout*        color_buttons_layout;
    QPushButton*        add_color_button;
    QPushButton*        remove_color_button;
    std::vector<QPushButton*> color_buttons;
    std::vector<RGBColor> colors;

    /*---------------------------------------------------------*\
    | Universal Axis & Direction Controls                      |
    \*---------------------------------------------------------*/
    QComboBox*          axis_combo;             // X/Y/Z/Radial/Custom
    QCheckBox*          reverse_check;          // Reverse direction
    QDoubleSpinBox*     custom_direction_x;     // Custom direction X
    QDoubleSpinBox*     custom_direction_y;     // Custom direction Y
    QDoubleSpinBox*     custom_direction_z;     // Custom direction Z

    /*---------------------------------------------------------*\
    | Common 3D spatial controls                               |
    \*---------------------------------------------------------*/
    QGroupBox*          spatial_controls_group;

    /*---------------------------------------------------------*\
    | Origin controls                                          |
    \*---------------------------------------------------------*/
    QComboBox*          origin_preset_combo;
    QDoubleSpinBox*     origin_x_spin;
    QDoubleSpinBox*     origin_y_spin;
    QDoubleSpinBox*     origin_z_spin;

    /*---------------------------------------------------------*\
    | Scale controls                                           |
    \*---------------------------------------------------------*/
    QDoubleSpinBox*     scale_x_spin;
    QDoubleSpinBox*     scale_y_spin;
    QDoubleSpinBox*     scale_z_spin;

    /*---------------------------------------------------------*\
    | Rotation controls                                        |
    \*---------------------------------------------------------*/
    QSlider*            rotation_x_slider;
    QSlider*            rotation_y_slider;
    QSlider*            rotation_z_slider;

    /*---------------------------------------------------------*\
    | Direction controls (for directional effects)            |
    \*---------------------------------------------------------*/
    QDoubleSpinBox*     direction_x_spin;
    QDoubleSpinBox*     direction_y_spin;
    QDoubleSpinBox*     direction_z_spin;

    /*---------------------------------------------------------*\
    | Mirror controls                                          |
    \*---------------------------------------------------------*/
    QCheckBox*          mirror_x_check;
    QCheckBox*          mirror_y_check;
    QCheckBox*          mirror_z_check;

    /*---------------------------------------------------------*\
    | Effect control buttons                                   |
    \*---------------------------------------------------------*/
    QPushButton*        start_effect_button;
    QPushButton*        stop_effect_button;

    /*---------------------------------------------------------*\
    | Effect parameters                                        |
    \*---------------------------------------------------------*/
    bool                effect_enabled;
    bool                effect_running;
    unsigned int        effect_speed;
    unsigned int        effect_brightness;
    unsigned int        effect_frequency;
    bool                rainbow_mode;
    float               rainbow_progress;

    /*---------------------------------------------------------*\
    | Universal Axis & Direction Parameters                    |
    \*---------------------------------------------------------*/
    EffectAxis          effect_axis;            // X/Y/Z/Radial/Custom
    bool                effect_reverse;         // Reverse direction
    Vector3D            custom_direction;       // Custom direction vector

    /*---------------------------------------------------------*\
    | Helper methods for derived classes                       |
    \*---------------------------------------------------------*/
    RGBColor GetRainbowColor(float hue);
    RGBColor GetColorAtPosition(float position);

private slots:
    void OnParameterChanged();
    void OnAxisChanged();
    void OnReverseChanged();
    void OnCustomDirectionChanged();
    void OnRainbowModeChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnStartEffectClicked();
    void OnStopEffectClicked();

private:
    void CreateOriginControls(QWidget* parent);
    void CreateScaleControls(QWidget* parent);
    void CreateColorControls(QWidget* parent);
    void CreateColorButton(RGBColor color);
    void RemoveLastColorButton();
    void CreateRotationControls(QWidget* parent);
    void CreateDirectionControls(QWidget* parent);
    void CreateMirrorControls(QWidget* parent);
};

#endif