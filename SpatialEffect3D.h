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
#include "LEDPosition3D.h"
#include "RGBController.h"
#include "SpatialEffectTypes.h"

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

    virtual void SetColors(RGBColor start, RGBColor end, bool gradient);
    virtual void GetColors(RGBColor& start, RGBColor& end, bool& gradient);

signals:
    void ParametersChanged();

protected:
    /*---------------------------------------------------------*\
    | Common effect controls                                   |
    \*---------------------------------------------------------*/
    QGroupBox*          effect_controls_group;
    QSlider*            speed_slider;
    QSlider*            brightness_slider;
    QPushButton*        color_start_button;
    QPushButton*        color_end_button;
    QCheckBox*          gradient_check;
    QLabel*             speed_label;
    QLabel*             brightness_label;

    /*---------------------------------------------------------*\
    | Common 3D spatial controls                               |
    \*---------------------------------------------------------*/
    QGroupBox*          spatial_controls_group;

    /*---------------------------------------------------------*\
    | Origin controls                                          |
    \*---------------------------------------------------------*/
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
    | Effect parameters                                        |
    \*---------------------------------------------------------*/
    bool                effect_enabled;
    unsigned int        effect_speed;
    unsigned int        effect_brightness;
    RGBColor            color_start;
    RGBColor            color_end;
    bool                use_gradient;

private slots:
    void OnParameterChanged();
    void OnColorStartClicked();
    void OnColorEndClicked();

private:
    void CreateOriginControls(QWidget* parent);
    void CreateScaleControls(QWidget* parent);
    void CreateRotationControls(QWidget* parent);
    void CreateDirectionControls(QWidget* parent);
    void CreateMirrorControls(QWidget* parent);
};

#endif