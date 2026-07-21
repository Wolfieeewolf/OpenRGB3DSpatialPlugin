// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialEffect3D.h"

#include "EffectMotionPanel.h"
#include "EffectOutputPanel.h"
#include "EffectGeometryPanel.h"
#include "EffectSurfacesPanel.h"
#include "EffectLayerBanner.h"
#include "EffectColorPanel.h"
#include "EffectCustomHost.h"
#include "EffectControlsRoot.h"
#include "EffectUiRows.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "Colors.h"
#include "EffectStratumBlend.h"
#include "SpatialKernelColormap.h"
#include "PluginUiUtils.h"
#include "ui/widgets/EffectRoomOutputPanel.h"
#include <QColorDialog>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <cstdint>

SpatialEffect3D::SpatialEffect3D(QWidget* parent) : QWidget(parent)
{
    effect_enabled = false;
    effect_running = false;
    effect_speed = 1;
    effect_brightness = 100;
    effect_frequency = 1;
    effect_detail = 100;
    effect_size = 100;
    effect_scale = 200;
    scale_inverted = false;
    effect_bounds_mode = (int)BOUNDS_MODE_GLOBAL;
    effect_fps = 30;
    rainbow_mode = false;
    spatial_mapping_mode = SpatialMappingMode::Off;
    compass_layer_spin_preset = 2;
    rainbow_progress = 0.0f;

    effect_sampler_influence_centi = 100;
    effect_sampler_compass_north_offset_deg = 0;
    effect_compass_discrete_zones = false;

    effect_rotation_yaw = 0.0f;
    effect_rotation_pitch = 0.0f;
    effect_rotation_roll = 0.0f;
    effect_axis_scale_rotation_yaw = 0.0f;
    effect_axis_scale_rotation_pitch = 0.0f;
    effect_axis_scale_rotation_roll = 0.0f;

    reference_mode = REF_MODE_LED_CENTROID;
    global_reference_point = {0.0f, 0.0f, 0.0f};
    custom_reference_point = {0.0f, 0.0f, 0.0f};
    use_custom_reference = false;

    colors.push_back(COLOR_RED);
    colors.push_back(COLOR_BLUE);

    effect_controls_group = nullptr;
    custom_effect_settings_host = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    detail_slider = nullptr;
    size_slider = nullptr;
    scale_slider = nullptr;
    scale_invert_check = nullptr;
    fps_slider = nullptr;
    speed_label = nullptr;
    brightness_label = nullptr;
    frequency_label = nullptr;
    detail_label = nullptr;
    size_label = nullptr;
    scale_label = nullptr;
    fps_label = nullptr;

    rotation_yaw_slider = nullptr;
    rotation_pitch_slider = nullptr;
    rotation_roll_slider = nullptr;
    rotation_yaw_label = nullptr;
    rotation_pitch_label = nullptr;
    rotation_roll_label = nullptr;
    rotation_reset_button = nullptr;
    axis_scale_rot_yaw_slider = nullptr;
    axis_scale_rot_pitch_slider = nullptr;
    axis_scale_rot_roll_slider = nullptr;
    axis_scale_rot_yaw_label = nullptr;
    axis_scale_rot_pitch_label = nullptr;
    axis_scale_rot_roll_label = nullptr;
    axis_scale_reset_button = nullptr;
    axis_scale_rot_reset_button = nullptr;

    color_controls_group = nullptr;
    color_pattern_settings_host = nullptr;
    band_modulation_settings_host = nullptr;
    rainbow_mode_check = nullptr;
    color_buttons_widget = nullptr;
    color_buttons_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    start_effect_button = nullptr;
    stop_effect_button = nullptr;

    intensity_slider = nullptr;
    intensity_label = nullptr;
    sharpness_slider = nullptr;
    sharpness_label = nullptr;
    smoothing_slider = nullptr;
    smoothing_label = nullptr;
    sampling_resolution_slider = nullptr;
    sampling_resolution_label = nullptr;
    edge_shape_group = nullptr;
    edge_profile_combo = nullptr;
    edge_thickness_slider = nullptr;
    edge_thickness_label = nullptr;
    glow_level_slider = nullptr;
    glow_level_label = nullptr;
    effect_intensity = 100;
    effect_sharpness = 0;
    effect_smoothing = 0;
    effect_sampling_resolution = 100;
    effect_edge_profile = 2;
    effect_edge_thickness = 50;
    effect_glow_level = 15;

    scale_x_slider = nullptr;
    scale_x_label  = nullptr;
    scale_y_slider = nullptr;
    scale_y_label  = nullptr;
    scale_z_slider = nullptr;
    scale_z_label  = nullptr;
    effect_scale_x = 100;
    effect_scale_y = 100;
    effect_scale_z = 100;

    effect_path_axis = 1;
    effect_plane = 1;
    effect_surface_mask = SURF_ALL;
    effect_offset_x = 0;
    effect_offset_y = 0;
    effect_offset_z = 0;

    position_offset_group = nullptr;
    offset_x_slider = nullptr;
    offset_y_slider = nullptr;
    offset_z_slider = nullptr;
    offset_x_label = nullptr;
    offset_y_label = nullptr;
    offset_z_label = nullptr;

    surfaces_group = nullptr;
    surfaces_section = nullptr;
    colors_patterns_section = nullptr;
    band_modulation_section = nullptr;
    effect_specific_section = nullptr;
    path_plane_group = nullptr;
    path_axis_combo = nullptr;
    plane_combo = nullptr;
}

SpatialEffect3D::~SpatialEffect3D() = default;

void SpatialEffect3D::CreateCommonEffectControls(QWidget* parent, bool include_start_stop)
{
    auto* controls_root = new EffectControlsRoot();
    effect_controls_group = controls_root;
    QVBoxLayout* main_layout = controls_root->mainLayout();

    auto* layer_banner = new EffectLayerBanner(include_start_stop);
    main_layout->addWidget(PluginUiWrapInSettingsPanel(layer_banner));
    if(include_start_stop)
    {
        start_effect_button = layer_banner->startEffectButton();
        stop_effect_button  = layer_banner->stopEffectButton();
    }

    surfaces_group = new EffectSurfacesPanel(effect_surface_mask, this);
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Surfaces"),
                            QStringLiteral("Optional: only light LEDs near the selected room shells (floor, ceiling, walls). "
                                           "All checked = no surface filter. "
                                           "To run the effect on part of the room (strips, desk, one wall), set zone and bounds on this layer in the Effect Stack."),
                            surfaces_group,
                            &surfaces_section,
                            false);

    auto* motion_pattern_group = new EffectMotionPanel(effect_speed,
                                                                       effect_brightness,
                                                                       effect_frequency,
                                                                       effect_detail,
                                                                       effect_size,
                                                                       effect_scale,
                                                                       scale_inverted,
                                                                       effect_fps);
    motion_pattern_group->setToolTip(
        QStringLiteral("How fast and how large the pattern moves; use the Effect Stack for zone and global/local bounds."));

    speed_slider = motion_pattern_group->speedSlider();
    speed_label = motion_pattern_group->speedLabel();
    brightness_slider = motion_pattern_group->brightnessSlider();
    brightness_label = motion_pattern_group->brightnessLabel();
    frequency_slider = motion_pattern_group->frequencySlider();
    frequency_label = motion_pattern_group->frequencyLabel();
    detail_slider = motion_pattern_group->detailSlider();
    detail_label = motion_pattern_group->detailLabel();
    size_slider = motion_pattern_group->sizeSlider();
    size_label = motion_pattern_group->sizeLabel();
    scale_slider = motion_pattern_group->scaleSlider();
    scale_label = motion_pattern_group->scaleLabel();
    scale_invert_check = motion_pattern_group->scaleInvertCheck();
    fps_slider = motion_pattern_group->fpsSlider();
    fps_label = motion_pattern_group->fpsLabel();

    PluginUiAddSectionBlock(main_layout, QStringLiteral("Motion and pattern"),
                   QStringLiteral("How fast and how large the pattern moves; use the Effect Stack for zone and global/local bounds."),
                   motion_pattern_group);

    auto* output_shaping_group = new EffectOutputPanel(effect_intensity,
                                                                       effect_sharpness,
                                                                       effect_smoothing,
                                                                       effect_sampling_resolution);
    output_shaping_group->setToolTip(
        QStringLiteral("Final contrast and level before colors; pair with Brightness above."));

    intensity_slider = output_shaping_group->intensitySlider();
    intensity_label = output_shaping_group->intensityLabel();
    sharpness_slider = output_shaping_group->sharpnessSlider();
    sharpness_label = output_shaping_group->sharpnessLabel();
    smoothing_slider = output_shaping_group->smoothingSlider();
    smoothing_label = output_shaping_group->smoothingLabel();
    sampling_resolution_slider = output_shaping_group->samplingResolutionSlider();
    sampling_resolution_label = output_shaping_group->samplingResolutionLabel();

    if(QVBoxLayout* output_layout = qobject_cast<QVBoxLayout*>(output_shaping_group->layout()))
    {
        EffectSliderRow* room_ao_row = EffectUiRows::AppendSliderRow(
            output_layout,
            QStringLiteral("Room ambient occlusion"),
            0,
            100,
            static_cast<int>(std::clamp(effect_room_relay_params_.ao_strength, 0.0f, 100.0f)),
            QStringLiteral("Layer-wide AO for room-mapped patterns and emitter/relay receivers when blockers are on."));
        if(room_ao_row)
        {
            room_ao_slider = room_ao_row->slider();
            room_ao_label = room_ao_row->valueLabel();
            if(room_ao_label)
            {
                room_ao_label->setText(QString::number(static_cast<int>(effect_room_relay_params_.ao_strength)) +
                                       QStringLiteral("%"));
            }
        }

        EffectCheckRow* room_blockers_row = EffectUiRows::AppendCheckRow(
            output_layout,
            QStringLiteral("Blockers (shadows)"),
            effect_room_relay_params_.use_occlusion,
            QStringLiteral("Enable occlusion from controller blockers and other room blockers."));
        if(room_blockers_row)
        {
            room_blockers_check = room_blockers_row->checkBox();
        }

        EffectCheckRow* room_walls_row = EffectUiRows::AppendCheckRow(
            output_layout,
            QStringLiteral("Include room walls as blockers"),
            effect_room_relay_params_.use_room_walls,
            QStringLiteral("Treat room bounds as occluding surfaces when shading relay receivers."));
        if(room_walls_row)
        {
            room_walls_blockers_check = room_walls_row->checkBox();
        }
    }
    UpdateRoomShadingControlVisibility();

    PluginUiAddSectionBlock(main_layout, QStringLiteral("Output shaping"),
                   QStringLiteral("Final contrast and level before colors; pair with Brightness in Motion and pattern."),
                   output_shaping_group);

    auto* geometry_group = new EffectGeometryPanel(effect_scale_x,
                                                            effect_scale_y,
                                                            effect_scale_z,
                                                            effect_axis_scale_rotation_yaw,
                                                            effect_axis_scale_rotation_pitch,
                                                            effect_axis_scale_rotation_roll,
                                                            effect_offset_x,
                                                            effect_offset_y,
                                                            effect_offset_z,
                                                            effect_rotation_yaw,
                                                            effect_rotation_pitch,
                                                            effect_rotation_roll);
    geometry_group->setToolTip(
        QStringLiteral("LED sampling uses this order: effect origin (room center or reference, plus center offset below) → "
                       "per-axis scale (X/Y/Z %) → scale-axis rotation → effect rotation (yaw/pitch/roll). "
                       "Overall coverage uses the Scale slider under Motion and pattern. "
                       "Choose zone and global/target bounds for this layer in the Effect Stack."));

    scale_x_slider = geometry_group->scaleXSlider();
    scale_x_label = geometry_group->scaleXLabel();
    scale_y_slider = geometry_group->scaleYSlider();
    scale_y_label = geometry_group->scaleYLabel();
    scale_z_slider = geometry_group->scaleZSlider();
    scale_z_label = geometry_group->scaleZLabel();
    axis_scale_reset_button = geometry_group->axisScaleResetButton();

    axis_scale_rot_yaw_slider = geometry_group->axisScaleRotYawSlider();
    axis_scale_rot_yaw_label = geometry_group->axisScaleRotYawLabel();
    axis_scale_rot_pitch_slider = geometry_group->axisScaleRotPitchSlider();
    axis_scale_rot_pitch_label = geometry_group->axisScaleRotPitchLabel();
    axis_scale_rot_roll_slider = geometry_group->axisScaleRotRollSlider();
    axis_scale_rot_roll_label = geometry_group->axisScaleRotRollLabel();
    axis_scale_rot_reset_button = geometry_group->axisScaleRotResetButton();

    position_offset_group = geometry_group->positionOffsetGroup();
    offset_x_slider = geometry_group->offsetXSlider();
    offset_x_label = geometry_group->offsetXLabel();
    offset_y_slider = geometry_group->offsetYSlider();
    offset_y_label = geometry_group->offsetYLabel();
    offset_z_slider = geometry_group->offsetZSlider();
    offset_z_label = geometry_group->offsetZLabel();

    rotation_yaw_slider = geometry_group->rotationYawSlider();
    rotation_yaw_label = geometry_group->rotationYawLabel();
    rotation_pitch_slider = geometry_group->rotationPitchSlider();
    rotation_pitch_label = geometry_group->rotationPitchLabel();
    rotation_roll_slider = geometry_group->rotationRollSlider();
    rotation_roll_label = geometry_group->rotationRollLabel();
    rotation_reset_button = geometry_group->rotationResetButton();

    PluginUiAddSectionBlock(main_layout, QStringLiteral("Effect geometry"),
                   QStringLiteral(
                       "LED sampling uses this order: effect origin (room center or reference, plus center offset below) → "
                       "per-axis scale (X/Y/Z %) → scale-axis rotation → effect rotation (yaw/pitch/roll). "
                       "Overall coverage uses the Scale slider under Motion and pattern. "
                       "Choose zone and global/target bounds for this layer in the Effect Stack."),
                   geometry_group,
                   nullptr,
                   false);

    CreateColorControls();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Colors and patterns"),
                   QStringLiteral("Base colors (Rainbow/Stops) plus optional pattern-kernel color modifiers."),
                   color_controls_group,
                   &colors_patterns_section,
                   false);

    band_modulation_settings_host = new EffectCustomHost();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Height motion bands"),
                   QStringLiteral("Floor / mid / ceiling: blend pattern speed, tightness, and phase, plus optional per-band "
                                  "room scroll, phase drift, or roll so calm effects still move across the room."),
                   band_modulation_settings_host,
                   &band_modulation_section,
                   false);

    custom_effect_settings_host = new EffectCustomHost();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Effect-specific settings"),
                   QStringLiteral("Parameters unique to this effect type (e.g. plasma tweak, explosion type)."),
                   custom_effect_settings_host,
                   &effect_specific_section,
                   false);

    ConnectCommonEffectControlSignals(geometry_group);

    EnsureHeightBandsPanel();
    EnsureStripColormapPanel();

    AddWidgetToParent(effect_controls_group, parent);
}

void SpatialEffect3D::MountSettingsUi(QWidget* parent, SpatialEffectSettingsLayout layout)
{
    if(!parent)
    {
        return;
    }

    setParent(parent);

    switch(layout)
    {
    case SpatialEffectSettingsLayout::FullWithTransport:
        CreateCommonEffectControls(parent, true);
        break;
    case SpatialEffectSettingsLayout::CommonNoTransport:
        CreateCommonEffectControls(parent, false);
        break;
    case SpatialEffectSettingsLayout::CustomOnly:
        break;
    }

    QWidget* custom_host = GetCustomSettingsHost();
    SetupCustomUI(custom_host ? custom_host : parent);
}

void SpatialEffect3D::ConnectCommonEffectControlSignals(EffectGeometryPanel* geometry_panel)
{
    connect(speed_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(detail_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(size_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(fps_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    if(scale_invert_check)
    {
        connect(scale_invert_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);
    }
    connect(rotation_yaw_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnRotationChanged);
    connect(rotation_pitch_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnRotationChanged);
    connect(rotation_roll_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnRotationChanged);
    connect(rotation_reset_button, &QPushButton::clicked, this, &SpatialEffect3D::OnRotationResetClicked);
    connect(axis_scale_reset_button, &QPushButton::clicked, this, &SpatialEffect3D::OnAxisScaleResetClicked);
    connect(axis_scale_rot_reset_button, &QPushButton::clicked, this, &SpatialEffect3D::OnAxisScaleRotationResetClicked);
    if(geometry_panel)
    {
        connect(geometry_panel->offsetCenterResetButton(), &QPushButton::clicked, this, [this]() {
            effect_offset_x = effect_offset_y = effect_offset_z = 0;
            if(offset_x_slider)
            {
                offset_x_slider->setValue(0);
            }
            if(offset_y_slider)
            {
                offset_y_slider->setValue(0);
            }
            if(offset_z_slider)
            {
                offset_z_slider->setValue(0);
            }
            if(offset_x_label)
            {
                offset_x_label->setText(QStringLiteral("0%"));
            }
            if(offset_y_label)
            {
                offset_y_label->setText(QStringLiteral("0%"));
            }
            if(offset_z_label)
            {
                offset_z_label->setText(QStringLiteral("0%"));
            }
            emit ParametersChanged();
        });
    }
    connect(intensity_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(sharpness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(smoothing_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(sampling_resolution_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    if(room_ao_slider)
    {
        connect(room_ao_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    }
    if(room_blockers_check)
    {
        connect(room_blockers_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);
    }
    if(room_walls_blockers_check)
    {
        connect(room_walls_blockers_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnParameterChanged);
    }
    if(room_blockers_check)
    {
        connect(room_blockers_check, &QCheckBox::toggled, this, [this](bool) {
            UpdateRoomShadingControlVisibility();
        });
    }
    connect(scale_x_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(offset_x_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(offset_y_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(offset_z_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_y_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_z_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(axis_scale_rot_yaw_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(axis_scale_rot_pitch_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(axis_scale_rot_roll_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);

    ApplyControlVisibility();

    connect(rotation_yaw_slider, &QSlider::valueChanged, rotation_yaw_label, [this](int value) {
        rotation_yaw_label->setText(QString::number(value) + "°");
        effect_rotation_yaw = (float)value;
    });
    connect(rotation_pitch_slider, &QSlider::valueChanged, rotation_pitch_label, [this](int value) {
        rotation_pitch_label->setText(QString::number(value) + "°");
        effect_rotation_pitch = (float)value;
    });
    connect(rotation_roll_slider, &QSlider::valueChanged, rotation_roll_label, [this](int value) {
        rotation_roll_label->setText(QString::number(value) + "°");
        effect_rotation_roll = (float)value;
    });
    connect(axis_scale_rot_yaw_slider, &QSlider::valueChanged, axis_scale_rot_yaw_label, [this](int value) {
        axis_scale_rot_yaw_label->setText(QString::number(value) + "°");
        effect_axis_scale_rotation_yaw = (float)value;
    });
    connect(axis_scale_rot_pitch_slider, &QSlider::valueChanged, axis_scale_rot_pitch_label, [this](int value) {
        axis_scale_rot_pitch_label->setText(QString::number(value) + "°");
        effect_axis_scale_rotation_pitch = (float)value;
    });
    connect(axis_scale_rot_roll_slider, &QSlider::valueChanged, axis_scale_rot_roll_label, [this](int value) {
        axis_scale_rot_roll_label->setText(QString::number(value) + "°");
        effect_axis_scale_rotation_roll = (float)value;
    });

    connect(brightness_slider, &QSlider::valueChanged, brightness_label, [this](int value) {
        brightness_label->setText(QString::number(value));
        effect_brightness = value;
    });
    connect(frequency_slider, &QSlider::valueChanged, frequency_label, [this](int value) {
        frequency_label->setText(QString::number(value));
        effect_frequency = value;
    });
    connect(detail_slider, &QSlider::valueChanged, detail_label, [this](int value) {
        detail_label->setText(QString::number(value));
        effect_detail = value;
    });
    connect(size_slider, &QSlider::valueChanged, size_label, [this](int value) {
        size_label->setText(QString::number(value));
        effect_size = value;
    });
    connect(scale_slider, &QSlider::valueChanged, scale_label, [this](int value) {
        scale_label->setText(QString::number(value));
        effect_scale = value;
    });
    connect(fps_slider, &QSlider::valueChanged, fps_label, [this](int value) {
        fps_label->setText(QString::number(value));
        effect_fps = value;
    });
    connect(intensity_slider, &QSlider::valueChanged, intensity_label, [this](int value) {
        intensity_label->setText(QString::number(value));
        effect_intensity = value;
    });
    connect(sharpness_slider, &QSlider::valueChanged, sharpness_label, [this](int value) {
        sharpness_label->setText(QString::number(value));
        effect_sharpness = value;
    });
    connect(smoothing_slider, &QSlider::valueChanged, smoothing_label, [this](int value) {
        smoothing_label->setText(QString::number(value));
        effect_smoothing = (unsigned int)value;
    });
    connect(sampling_resolution_slider, &QSlider::valueChanged, sampling_resolution_label, [this](int value) {
        sampling_resolution_label->setText(QString::number(value));
        effect_sampling_resolution = (unsigned int)std::clamp(value, 0, 100);
    });
    connect(scale_x_slider, &QSlider::valueChanged, scale_x_label, [this](int value) {
        scale_x_label->setText(QString::number(value) + "%");
        effect_scale_x = (unsigned int)value;
    });
    connect(scale_y_slider, &QSlider::valueChanged, scale_y_label, [this](int value) {
        scale_y_label->setText(QString::number(value) + "%");
        effect_scale_y = (unsigned int)value;
    });
    connect(scale_z_slider, &QSlider::valueChanged, scale_z_label, [this](int value) {
        scale_z_label->setText(QString::number(value) + "%");
        effect_scale_z = (unsigned int)value;
    });
    connect(offset_x_slider, &QSlider::valueChanged, offset_x_label, [this](int value) {
        offset_x_label->setText(QString::number(value) + "%");
        effect_offset_x = value;
    });
    connect(offset_y_slider, &QSlider::valueChanged, offset_y_label, [this](int value) {
        offset_y_label->setText(QString::number(value) + "%");
        effect_offset_y = value;
    });
    connect(offset_z_slider, &QSlider::valueChanged, offset_z_label, [this](int value) {
        offset_z_label->setText(QString::number(value) + "%");
        effect_offset_z = value;
    });
}

QWidget* SpatialEffect3D::CustomSettingsPanelWidget() const
{
    if(!custom_effect_settings_host || !custom_effect_settings_host->layout())
    {
        return nullptr;
    }
    QLayout* layout = custom_effect_settings_host->layout();
    if(layout->count() < 1)
    {
        return nullptr;
    }
    if(QLayoutItem* item = layout->itemAt(0))
    {
        return item->widget();
    }
    return nullptr;
}

void SpatialEffect3D::AddWidgetToParent(QWidget* w, QWidget* container)
{
    if(w && container && container->layout())
    {
        container->layout()->addWidget(w);
        if(container == custom_effect_settings_host && effect_specific_section)
        {
            effect_specific_section->setVisible(true);
        }
        else if(container == color_pattern_settings_host && colors_patterns_section)
        {
            colors_patterns_section->setVisible(true);
        }
        else if(container == band_modulation_settings_host && band_modulation_section)
        {
            band_modulation_section->setVisible(true);
        }
    }
}

void SpatialEffect3D::AddColorPatternWidget(QWidget* widget)
{
    AddWidgetToParent(widget, color_pattern_settings_host);
    if(!shared_strip_cmap_panel)
    {
        if(auto* panel = qobject_cast<StripKernelColormapPanel*>(widget))
        {
            shared_strip_cmap_panel = panel;
            connect(shared_strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, [this]() {
                OnEffectStripColormapChanged();
            });
        }
    }
    SyncColorControlVisibilityForPatternMode();
    if(colors_patterns_section)
    {
        colors_patterns_section->setVisible(true);
    }
}

void SpatialEffect3D::EnsureStripColormapPanel()
{
    if(!GetEffectInfo().supports_strip_colormap || effect_strip_cmap_panel || !color_pattern_settings_host)
    {
        return;
    }
    effect_strip_cmap_panel = new StripKernelColormapPanel(color_pattern_settings_host);
    SyncEffectStripColormapPanelFromModel();
    AddColorPatternWidget(effect_strip_cmap_panel);
}

void SpatialEffect3D::OnEffectStripColormapChanged()
{
    if(effect_strip_cmap_panel)
    {
        effect_strip_cmap_on = effect_strip_cmap_panel->useStripColormap();
        effect_strip_cmap_kernel = effect_strip_cmap_panel->kernelId();
        effect_strip_cmap_rep = effect_strip_cmap_panel->kernelRepeats();
        effect_strip_cmap_unfold = effect_strip_cmap_panel->unfoldMode();
        effect_strip_cmap_dir = effect_strip_cmap_panel->directionDeg();
        effect_strip_cmap_color_style = effect_strip_cmap_panel->colorStyle();
    }
    SyncColorControlVisibilityForPatternMode();
    emit ParametersChanged();
}

void SpatialEffect3D::SyncEffectStripColormapPanelFromModel()
{
    if(effect_strip_cmap_panel)
    {
        effect_strip_cmap_panel->mirrorStateFromEffect(effect_strip_cmap_on,
                                                       effect_strip_cmap_kernel,
                                                       effect_strip_cmap_rep,
                                                       effect_strip_cmap_unfold,
                                                       effect_strip_cmap_dir,
                                                       effect_strip_cmap_color_style);
    }
}

void SpatialEffect3D::LoadEffectStripColormapSettings(const nlohmann::json& settings)
{
    effect_strip_cmap_on = false;
    effect_strip_cmap_kernel = 0;
    effect_strip_cmap_rep = 4.0f;
    effect_strip_cmap_unfold = 0;
    effect_strip_cmap_dir = 0.0f;
    effect_strip_cmap_color_style = 0;

    StripColormapLoadCanonical(settings,
                               effect_strip_cmap_on,
                               effect_strip_cmap_kernel,
                               effect_strip_cmap_rep,
                               effect_strip_cmap_unfold,
                               effect_strip_cmap_dir,
                               effect_strip_cmap_color_style);

    SyncEffectStripColormapPanelFromModel();
}

void SpatialEffect3D::SaveEffectStripColormapSettings(nlohmann::json& j) const
{
    bool on = effect_strip_cmap_on;
    int kern = effect_strip_cmap_kernel;
    float rep = effect_strip_cmap_rep;
    int unfold = effect_strip_cmap_unfold;
    float dir = effect_strip_cmap_dir;
    int color_style = effect_strip_cmap_color_style;
    if(effect_strip_cmap_panel)
    {
        on = effect_strip_cmap_panel->useStripColormap();
        kern = effect_strip_cmap_panel->kernelId();
        rep = effect_strip_cmap_panel->kernelRepeats();
        unfold = effect_strip_cmap_panel->unfoldMode();
        dir = effect_strip_cmap_panel->directionDeg();
        color_style = effect_strip_cmap_panel->colorStyle();
    }
    StripColormapSaveCanonical(j, on, kern, rep, unfold, dir, color_style);
}

void SpatialEffect3D::AddBandModulationWidget(QWidget* widget)
{
    AddWidgetToParent(widget, band_modulation_settings_host);
    if(band_modulation_section)
    {
        band_modulation_section->setVisible(true);
    }
}

void SpatialEffect3D::EnsureHeightBandsPanel()
{
    if(!GetEffectInfo().supports_height_bands || effect_stratum_panel || !band_modulation_settings_host)
    {
        return;
    }
    effect_stratum_panel = new StratumBandPanel(band_modulation_settings_host);
    effect_stratum_panel->setLayoutMode(effect_stratum_layout_mode);
    effect_stratum_panel->setTuning(effect_stratum_tuning_);
    AddBandModulationWidget(effect_stratum_panel);
    connect(effect_stratum_panel, &StratumBandPanel::bandParametersChanged, this,
            &SpatialEffect3D::OnEffectStratumBandChanged);
}

void SpatialEffect3D::OnEffectStratumBandChanged()
{
    if(effect_stratum_panel)
    {
        effect_stratum_layout_mode = effect_stratum_panel->layoutMode();
        effect_stratum_tuning_ = effect_stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void SpatialEffect3D::SyncEffectStratumPanelFromModel()
{
    if(effect_stratum_panel)
    {
        effect_stratum_panel->setLayoutMode(effect_stratum_layout_mode);
        effect_stratum_panel->setTuning(effect_stratum_tuning_);
    }
}


void SpatialEffect3D::LoadEffectStratumSettings(const nlohmann::json& settings)
{
    effect_stratum_layout_mode = 0;
    effect_stratum_tuning_ = {};

    EffectStratumBlend::LoadBandTuningJson(settings, effect_stratum_layout_mode, effect_stratum_tuning_);
    SyncEffectStratumPanelFromModel();
}

void SpatialEffect3D::SaveEffectStratumSettings(nlohmann::json& j) const
{
    int mode = effect_stratum_layout_mode;
    EffectStratumBlend::BandTuningPct tuning = effect_stratum_tuning_;
    if(effect_stratum_panel)
    {
        mode = effect_stratum_panel->layoutMode();
        tuning = effect_stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j, mode, tuning);
}

void SpatialEffect3D::SyncColorControlVisibilityForPatternMode()
{
    const bool use_pattern_palette =
        shared_strip_cmap_panel &&
        shared_strip_cmap_panel->useStripColormap() &&
        shared_strip_cmap_panel->colorStyle() == 0;

    if(rainbow_mode_check)
    {
        rainbow_mode_check->setVisible(!use_pattern_palette);
    }
    if(color_buttons_widget)
    {
        color_buttons_widget->setVisible(!use_pattern_palette && !rainbow_mode);
    }
}

void SpatialEffect3D::CreateColorControls()
{
    auto* color_panel = new EffectColorPanel(rainbow_mode);
    color_controls_group = color_panel;

    rainbow_mode_check = color_panel->rainbowModeCheck();
    color_buttons_widget = color_panel->colorButtonsWidget();
    color_buttons_layout = color_panel->colorButtonsLayout();
    color_pattern_settings_host = color_panel->patternHostWidget();
    shared_strip_cmap_panel = nullptr;
    add_color_button = color_panel->addColorButton();
    remove_color_button = color_panel->removeColorButton();
    remove_color_button->setEnabled(colors.size() > 1);

    for(unsigned int i = 0; i < colors.size(); i++)
    {
        CreateColorButton(colors[i]);
    }

    color_buttons_widget->setVisible(!rainbow_mode);

    connect(rainbow_mode_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnRainbowModeChanged);
    connect(add_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnRemoveColorClicked);
    SyncColorControlVisibilityForPatternMode();
}

void SpatialEffect3D::CreateColorButton(RGBColor color)
{
    QPushButton* color_button = new QPushButton();
    color_button->setMinimumSize(40, 30);
    color_button->setMaximumSize(40, 30);
    color_button->setToolTip("Click to change color");
    int r = color & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = (color >> 16) & 0xFF;
    PluginUiSetRgbSwatchButton(color_button, r, g, b);

    connect(color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnColorButtonClicked);

    color_buttons.push_back(color_button);

    int insert_pos = color_buttons_layout->count() - 3;
    if(insert_pos < 0) insert_pos = 0;
    color_buttons_layout->insertWidget(insert_pos, color_button);
}

void SpatialEffect3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* last_button = color_buttons.back();
        color_buttons_layout->removeWidget(last_button);
        color_buttons.pop_back();
        last_button->deleteLater();
    }
}


void SpatialEffect3D::OnRainbowModeChanged()
{
    rainbow_mode = rainbow_mode_check->isChecked();
    SyncColorControlVisibilityForPatternMode();
    emit ParametersChanged();
}

void SpatialEffect3D::ConnectStackRoomOutputPanel(EffectRoomOutputPanel* panel,
                                                  const std::function<void()>& on_changed,
                                                  const std::function<QString(int)>& transform_label)
{
    room_output_panel_ = panel;
    if(!room_output_panel_)
    {
        return;
    }
    room_output_panel_->bind(this,
                             effect_room_output_role_,
                             effect_room_relay_params_,
                             effect_emitter_controller_indices_,
                             effect_receiver_controller_indices_,
                             [this, on_changed]()
                             {
                                 InvalidateRelayShadeCache();
                                 if(on_changed)
                                 {
                                     on_changed();
                                 }
                             },
                             transform_label);
}

void SpatialEffect3D::DisconnectStackRoomOutputPanel()
{
    room_output_panel_ = nullptr;
}

void SpatialEffect3D::OnAddColorClicked()
{
    RGBColor new_color = GetRainbowColor(colors.size() * 60.0f);
    colors.push_back(new_color);
    CreateColorButton(new_color);

    remove_color_button->setEnabled(colors.size() > 1);
    emit ParametersChanged();
}

void SpatialEffect3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        remove_color_button->setEnabled(colors.size() > 1);
        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnColorButtonClicked()
{
    QPushButton* clicked_button = qobject_cast<QPushButton*>(sender());
    if(!clicked_button) return;

    std::vector<QPushButton*>::iterator it = std::find(color_buttons.begin(), color_buttons.end(), clicked_button);
    if(it == color_buttons.end()) return;

    int index = std::distance(color_buttons.begin(), it);
    if(index >= (int)colors.size()) return;

    QColorDialog color_dialog;
    int r = colors[index] & 0xFF;
    int g = (colors[index] >> 8) & 0xFF;
    int b = (colors[index] >> 16) & 0xFF;
    color_dialog.setCurrentColor(QColor(r, g, b));

    if(color_dialog.exec() == QDialog::Accepted)
    {
        QColor new_color = color_dialog.currentColor();
        colors[index] = ((unsigned int)new_color.blue() << 16) | ((unsigned int)new_color.green() << 8) | (unsigned int)new_color.red();

        PluginUiSetRgbSwatchButton(clicked_button, new_color.red(), new_color.green(), new_color.blue());

        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnStartEffectClicked()
{
    effect_running = true;
    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);
    emit ParametersChanged();
}

void SpatialEffect3D::OnStopEffectClicked()
{
    effect_running = false;
    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);
    emit ParametersChanged();
}

void SpatialEffect3D::SetControlGroupVisibility(QSlider* slider, QLabel* value_label, const QString& label_text, bool visible)
{
    if(slider && value_label)
    {
        slider->setVisible(visible);
        value_label->setVisible(visible);

        QWidget* parent = slider->parentWidget();
        if(parent)
        {
            QList<QLabel*> labels = parent->findChildren<QLabel*>();
            for(int i = 0; i < labels.size(); i++)
            {
                QLabel* label = labels[i];
                if(label->text() == label_text)
                {
                    label->setVisible(visible);
                    break;
                }
            }
        }

        if(slider == scale_slider && scale_invert_check)
        {
            scale_invert_check->setVisible(visible);
        }
    }
}

void SpatialEffect3D::ApplyControlVisibility()
{
    const EffectInfo3D info = GetEffectInfo();

    SetControlGroupVisibility(speed_slider, speed_label, "Speed:", info.show_speed_control);
    SetControlGroupVisibility(brightness_slider, brightness_label, "Brightness:", info.show_brightness_control);
    SetControlGroupVisibility(frequency_slider, frequency_label, "Frequency:", info.show_frequency_control);
    SetControlGroupVisibility(detail_slider, detail_label, "Detail:", info.show_detail_control);
    SetControlGroupVisibility(size_slider, size_label, "Size:", info.show_size_control);
    SetControlGroupVisibility(scale_slider, scale_label, "Scale:", info.show_scale_control);
    SetControlGroupVisibility(fps_slider, fps_label, "FPS:", info.show_fps_control);

    if(color_controls_group)
    {
        color_controls_group->setVisible(info.show_color_controls);
    }
    if(colors_patterns_section && color_pattern_settings_host && color_pattern_settings_host->layout())
    {
        colors_patterns_section->setVisible(color_pattern_settings_host->layout()->count() > 0);
    }
    if(band_modulation_section && band_modulation_settings_host && band_modulation_settings_host->layout())
    {
        band_modulation_section->setVisible(band_modulation_settings_host->layout()->count() > 0);
    }
    if(effect_specific_section && custom_effect_settings_host && custom_effect_settings_host->layout())
    {
        effect_specific_section->setVisible(custom_effect_settings_host->layout()->count() > 0);
    }

    if(surfaces_section)
    {
        surfaces_section->setVisible(info.show_surface_control);
    }
    if(position_offset_group)
    {
        position_offset_group->setVisible(info.show_position_offset_control);
    }
}

void SpatialEffect3D::OnParameterChanged()
{
    if(speed_slider)
    {
        SetSpeed((unsigned int)std::max(0, speed_slider->value()));
        if(speed_label)
        {
            speed_label->setText(QString::number(effect_speed));
        }
    }

    if(brightness_slider && brightness_label)
    {
        effect_brightness = brightness_slider->value();
        brightness_label->setText(QString::number(effect_brightness));
    }

    if(frequency_slider && frequency_label)
    {
        effect_frequency = frequency_slider->value();
        frequency_label->setText(QString::number(effect_frequency));
    }

    if(size_slider && size_label)
    {
        effect_size = size_slider->value();
        size_label->setText(QString::number(effect_size));
    }

    if(scale_slider && scale_label)
    {
        effect_scale = scale_slider->value();
        scale_label->setText(QString::number(effect_scale));
    }

    if(scale_invert_check)
    {
        scale_inverted = scale_invert_check->isChecked();
    }

    if(fps_slider && fps_label)
    {
        effect_fps = fps_slider->value();
        fps_label->setText(QString::number(effect_fps));
    }
    if(intensity_slider)
    {
        effect_intensity = intensity_slider->value();
        if(intensity_label)
        {
            intensity_label->setText(QString::number(effect_intensity));
        }
    }
    if(sharpness_slider)
    {
        effect_sharpness = sharpness_slider->value();
        if(sharpness_label)
        {
            sharpness_label->setText(QString::number(effect_sharpness));
        }
    }
    if(smoothing_slider)
    {
        effect_smoothing = (unsigned int)std::clamp(smoothing_slider->value(), 0, 100);
        if(smoothing_label)
        {
            smoothing_label->setText(QString::number(effect_smoothing));
        }
    }
    if(sampling_resolution_slider)
    {
        const int sv = sampling_resolution_slider->value();
        effect_sampling_resolution = (unsigned int)std::clamp(sv, 0, 100);
        if(sampling_resolution_label)
        {
            sampling_resolution_label->setText(QString::number(effect_sampling_resolution));
        }
    }
    if(room_ao_slider)
    {
        effect_room_relay_params_.ao_strength = static_cast<float>(std::clamp(room_ao_slider->value(), 0, 100));
        if(room_ao_label)
        {
            room_ao_label->setText(QString::number(static_cast<int>(effect_room_relay_params_.ao_strength)) +
                                   QStringLiteral("%"));
        }
    }
    if(room_blockers_check)
    {
        effect_room_relay_params_.use_occlusion = room_blockers_check->isChecked();
    }
    if(room_walls_blockers_check)
    {
        effect_room_relay_params_.use_room_walls = room_walls_blockers_check->isChecked();
    }
    InvalidateRelayShadeCache();
    UpdateRoomShadingControlVisibility();
    if(edge_profile_combo)
    {
        effect_edge_profile = std::clamp(edge_profile_combo->currentIndex(), 0, 4);
    }
    if(offset_x_slider) effect_offset_x = offset_x_slider->value();
    if(offset_y_slider) effect_offset_y = offset_y_slider->value();
    if(offset_z_slider) effect_offset_z = offset_z_slider->value();
    if(offset_x_label) offset_x_label->setText(QString::number(effect_offset_x) + "%");
    if(offset_y_label) offset_y_label->setText(QString::number(effect_offset_y) + "%");
    if(offset_z_label) offset_z_label->setText(QString::number(effect_offset_z) + "%");
    emit ParametersChanged();
}

void SpatialEffect3D::OnRotationChanged()
{
    if(rotation_yaw_slider)
    {
        effect_rotation_yaw = (float)rotation_yaw_slider->value();
        if(rotation_yaw_label)
        {
            rotation_yaw_label->setText(QString::number((int)effect_rotation_yaw) + "°");
        }
    }
    if(rotation_pitch_slider)
    {
        effect_rotation_pitch = (float)rotation_pitch_slider->value();
        if(rotation_pitch_label)
        {
            rotation_pitch_label->setText(QString::number((int)effect_rotation_pitch) + "°");
        }
    }
    if(rotation_roll_slider)
    {
        effect_rotation_roll = (float)rotation_roll_slider->value();
        if(rotation_roll_label)
        {
            rotation_roll_label->setText(QString::number((int)effect_rotation_roll) + "°");
        }
    }
    emit ParametersChanged();
}

void SpatialEffect3D::UpdateRoomShadingControlVisibility()
{
    const bool blockers_on = room_blockers_check && room_blockers_check->isChecked();
    if(room_walls_blockers_check && room_walls_blockers_check->parentWidget())
    {
        room_walls_blockers_check->parentWidget()->setVisible(blockers_on);
    }
    if(room_ao_slider && room_ao_slider->parentWidget())
    {
        room_ao_slider->parentWidget()->setVisible(blockers_on);
    }
}

void SpatialEffect3D::OnRotationResetClicked()
{
    effect_rotation_yaw = 0.0f;
    effect_rotation_pitch = 0.0f;
    effect_rotation_roll = 0.0f;
    
    if(rotation_yaw_slider)
    {
        rotation_yaw_slider->setValue(0);
    }
    if(rotation_pitch_slider)
    {
        rotation_pitch_slider->setValue(0);
    }
    if(rotation_roll_slider)
    {
        rotation_roll_slider->setValue(0);
    }
    
    emit ParametersChanged();
}

void SpatialEffect3D::OnAxisScaleResetClicked()
{
    effect_scale_x = 100;
    effect_scale_y = 100;
    effect_scale_z = 100;

    if(scale_x_slider)
    {
        scale_x_slider->setValue(100);
    }
    if(scale_y_slider)
    {
        scale_y_slider->setValue(100);
    }
    if(scale_z_slider)
    {
        scale_z_slider->setValue(100);
    }
    if(scale_x_label)
    {
        scale_x_label->setText("100%");
    }
    if(scale_y_label)
    {
        scale_y_label->setText("100%");
    }
    if(scale_z_label)
    {
        scale_z_label->setText("100%");
    }

    emit ParametersChanged();
}

void SpatialEffect3D::OnAxisScaleRotationResetClicked()
{
    effect_axis_scale_rotation_yaw = 0.0f;
    effect_axis_scale_rotation_pitch = 0.0f;
    effect_axis_scale_rotation_roll = 0.0f;

    if(axis_scale_rot_yaw_slider)
    {
        axis_scale_rot_yaw_slider->setValue(0);
    }
    if(axis_scale_rot_pitch_slider)
    {
        axis_scale_rot_pitch_slider->setValue(0);
    }
    if(axis_scale_rot_roll_slider)
    {
        axis_scale_rot_roll_slider->setValue(0);
    }
    if(axis_scale_rot_yaw_label)
    {
        axis_scale_rot_yaw_label->setText("0°");
    }
    if(axis_scale_rot_pitch_label)
    {
        axis_scale_rot_pitch_label->setText("0°");
    }
    if(axis_scale_rot_roll_label)
    {
        axis_scale_rot_roll_label->setText("0°");
    }

    emit ParametersChanged();
}
