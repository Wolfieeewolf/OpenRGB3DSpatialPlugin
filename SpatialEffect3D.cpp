// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialEffect3D.h"
#include "EffectMotionPanel.h"
#include "EffectOutputPanel.h"
#include "EffectGeometryPanel.h"
#include "EffectSurfacesPanel.h"
#include "EffectLayerBanner.h"
#include "EffectSamplerPanel.h"
#include "EffectColorPanel.h"
#include "EffectCustomHost.h"
#include "EffectControlsRoot.h"
#include "Colors.h"
#include "Geometry3DUtils.h"
#include "GameTelemetryBridge.h"
#include "VoxelMapping.h"
#include "PluginUiUtils.h"
#include <QColorDialog>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    /** Compass palette: horizontal aim stays on the effect reference (origin X/Z); vertical stratum uses that band's mid height so each floor/mid/ceiling layer has its own compass hub. */
    SpatialLayerCore::SamplePoint MakeCompassPaletteSamplePoint(const GridContext3D& grid,
                                                                  const SpatialLayerCore::MapperSettings& map,
                                                                  const SpatialLayerCore::SamplePoint& sp)
    {
        SpatialLayerCore::SamplePoint o = sp;
        const float yn = std::clamp(sp.y_norm, 0.0f, 1.0f);
        const float b0 = std::clamp(map.floor_end, 0.08f, 0.55f);
        const float b1 = std::clamp(std::max(b0 + 0.08f, map.upper_end), b0 + 0.08f, 0.92f);
        float y_norm_center;
        if(yn < b0)
        {
            y_norm_center = b0 * 0.5f;
        }
        else if(yn < b1)
        {
            y_norm_center = (b0 + b1) * 0.5f;
        }
        else
        {
            y_norm_center = (b1 + 1.0f) * 0.5f;
        }

        const float room_h = std::max(1e-6f, grid.max_y - grid.min_y);
        o.origin_y = grid.min_y + y_norm_center * room_h;
        return o;
    }
}

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

    effect_voxel_volume_mix = 0;
    effect_voxel_room_scale_centi = 18;
    effect_voxel_heading_offset = 0;
    effect_voxel_drive_mode = VoxelDriveMode::Off;
    effect_sampler_influence_centi = 100;
    effect_sampler_compass_north_offset_deg = 0;

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
    sampler_mapper_group = nullptr;
    compass_sampler_group = nullptr;
    spatial_mapping_combo = nullptr;
    compass_layer_spin_combo = nullptr;
    sampler_influence_slider = nullptr;
    sampler_influence_label = nullptr;
    sampler_compass_north_slider = nullptr;
    sampler_compass_north_label = nullptr;

    voxel_volume_group = nullptr;
    voxel_volume_mix_slider = nullptr;
    voxel_volume_mix_label = nullptr;
    voxel_volume_scale_slider = nullptr;
    voxel_volume_scale_label = nullptr;
    voxel_volume_heading_slider = nullptr;
    voxel_volume_heading_label = nullptr;
    voxel_drive_combo = nullptr;
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
    effect_sharpness = 100;
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

bool SpatialEffect3D::EffectGridSampleOutsideVolume(float x, float y, float z, const GridContext3D& grid) const
{
    Vector3D origin_grid = GetEffectOriginGrid(grid);
    float rel_x = x - origin_grid.x;
    float rel_y = y - origin_grid.y;
    float rel_z = z - origin_grid.z;
    return !IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid);
}

void SpatialEffect3D::ApplyGridSampleCoordinateAdjustment(float& x, float& y, float& z, const GridContext3D& grid) const
{
    Vector3D origin_grid = GetEffectOriginGrid(grid);
    Vector3D effect_origin = GetEffectOrigin();
    x = x - origin_grid.x + effect_origin.x;
    y = y - origin_grid.y + effect_origin.y;
    z = z - origin_grid.z + effect_origin.z;
}

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
                            &surfaces_section);

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

    // Bounds mode is controlled from the global Effect Stack panel.

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
                   geometry_group);

    CreateColorControls();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Colors and patterns"),
                   QStringLiteral("Base colors (Rainbow/Stops) plus optional pattern-kernel color modifiers."),
                   color_controls_group,
                   &colors_patterns_section);

    band_modulation_settings_host = new EffectCustomHost();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Band modulation"),
                   QStringLiteral("Shared floor/mid/ceiling modulation controls used by effects that support strata."),
                   band_modulation_settings_host,
                   &band_modulation_section);

    custom_effect_settings_host = new EffectCustomHost();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Effect-specific settings"),
                   QStringLiteral("Parameters unique to this effect type (e.g. plasma tweak, explosion type)."),
                   custom_effect_settings_host,
                   &effect_specific_section);

    CreateSamplerMapperControls();
    PluginUiAddSectionBlock(main_layout, QStringLiteral("Room sampler"),
                   QStringLiteral("Map LED position (and optional game voxels) to palette steering."),
                   sampler_mapper_group);

    ConnectCommonEffectControlSignals(geometry_group);

    AddWidgetToParent(effect_controls_group, parent);
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
    // Bounds mode is controlled from the global Effect Stack panel.
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

    /* Speed label is updated from OnParameterChanged via virtual SetSpeed() so effects
     * (e.g. GIF playback) can react when the slider moves. */
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

void SpatialEffect3D::ApplyAxisScale(float& x, float& y, float& z, const GridContext3D& grid) const
{
    if(effect_scale_x == 100 && effect_scale_y == 100 && effect_scale_z == 100)
    {
        return;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    float dx = x - origin.x;
    float dy = y - origin.y;
    float dz = z - origin.z;

    bool use_axis_scale_rotation = (effect_axis_scale_rotation_yaw != 0.0f || effect_axis_scale_rotation_pitch != 0.0f || effect_axis_scale_rotation_roll != 0.0f);
    if(use_axis_scale_rotation)
    {
        Vector3D in_scale_frame = RotateVectorByEuler(dx, dy, dz,
            -effect_axis_scale_rotation_yaw, -effect_axis_scale_rotation_pitch, -effect_axis_scale_rotation_roll);
        float sx = (effect_scale_x != 100) ? (100.0f / (float)effect_scale_x) : 1.0f;
        float sy = (effect_scale_y != 100) ? (100.0f / (float)effect_scale_y) : 1.0f;
        float sz = (effect_scale_z != 100) ? (100.0f / (float)effect_scale_z) : 1.0f;
        Vector3D scaled;
        scaled.x = in_scale_frame.x * sx;
        scaled.y = in_scale_frame.y * sy;
        scaled.z = in_scale_frame.z * sz;
        Vector3D back = RotateVectorByEuler(scaled.x, scaled.y, scaled.z,
            effect_axis_scale_rotation_yaw, effect_axis_scale_rotation_pitch, effect_axis_scale_rotation_roll);
        x = origin.x + back.x;
        y = origin.y + back.y;
        z = origin.z + back.z;
        return;
    }

    if(effect_scale_x != 100) x = origin.x + dx * (100.0f / (float)effect_scale_x);
    if(effect_scale_y != 100) y = origin.y + dy * (100.0f / (float)effect_scale_y);
    if(effect_scale_z != 100) z = origin.z + dz * (100.0f / (float)effect_scale_z);
}

void SpatialEffect3D::ApplyEffectRotation(float& x, float& y, float& z, const GridContext3D& grid) const
{
    if(effect_rotation_yaw == 0.0f && effect_rotation_pitch == 0.0f && effect_rotation_roll == 0.0f)
    {
        return;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated = TransformPointByRotation(x, y, z, origin);
    x = rotated.x;
    y = rotated.y;
    z = rotated.z;
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
    if(colors_patterns_section)
    {
        colors_patterns_section->setVisible(true);
    }
}

void SpatialEffect3D::AddBandModulationWidget(QWidget* widget)
{
    AddWidgetToParent(widget, band_modulation_settings_host);
    if(band_modulation_section)
    {
        band_modulation_section->setVisible(true);
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

void SpatialEffect3D::CreateSamplerMapperControls()
{
    auto* mapper = new EffectSamplerPanel(spatial_mapping_mode,
                                                         compass_layer_spin_preset,
                                                         effect_sampler_influence_centi,
                                                         effect_sampler_compass_north_offset_deg,
                                                         effect_voxel_volume_mix,
                                                         effect_voxel_room_scale_centi,
                                                         effect_voxel_heading_offset,
                                                         effect_voxel_drive_mode);
    sampler_mapper_group = mapper;
    sampler_mapper_group->setToolTip(
        QStringLiteral("Uses LED position in the room (and optional game voxel data) to steer the palette. "
                       "Effect geometry above only shapes the pattern; this section answers \"what color for this corner / height / direction\"."));

    compass_sampler_group = mapper->compassSamplerGroup();
    spatial_mapping_combo = mapper->spatialMappingCombo();
    compass_layer_spin_combo = mapper->compassLayerSpinCombo();
    sampler_influence_slider = mapper->samplerInfluenceSlider();
    sampler_influence_label = mapper->samplerInfluenceLabel();
    sampler_compass_north_slider = mapper->samplerCompassNorthSlider();
    sampler_compass_north_label = mapper->samplerCompassNorthLabel();
    voxel_volume_group = mapper->voxelVolumeGroup();
    voxel_volume_mix_slider = mapper->voxelVolumeMixSlider();
    voxel_volume_mix_label = mapper->voxelVolumeMixLabel();
    voxel_volume_scale_slider = mapper->voxelVolumeScaleSlider();
    voxel_volume_scale_label = mapper->voxelVolumeScaleLabel();
    voxel_volume_heading_slider = mapper->voxelVolumeHeadingSlider();
    voxel_volume_heading_label = mapper->voxelVolumeHeadingLabel();
    voxel_drive_combo = mapper->voxelDriveCombo();

    connect(spatial_mapping_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &SpatialEffect3D::OnSpatialMappingComboChanged);
    connect(compass_layer_spin_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &SpatialEffect3D::OnCompassLayerSpinComboChanged);
    connect(sampler_influence_slider, &QSlider::valueChanged, this, [this](int v) {
        effect_sampler_influence_centi = std::clamp(v, 0, 250);
        if(sampler_influence_label)
        {
            sampler_influence_label->setText(QString::number(effect_sampler_influence_centi) + QStringLiteral("%"));
        }
        emit ParametersChanged();
    });
    connect(sampler_compass_north_slider, &QSlider::valueChanged, this, [this](int v) {
        effect_sampler_compass_north_offset_deg = std::clamp(v, -180, 180);
        if(sampler_compass_north_label)
        {
            sampler_compass_north_label->setText(QString::number(effect_sampler_compass_north_offset_deg) +
                                                 QStringLiteral("°"));
        }
        emit ParametersChanged();
    });
    connect(voxel_volume_mix_slider, &QSlider::valueChanged, this, [this](int v) {
        effect_voxel_volume_mix = (unsigned int)std::clamp(v, 0, 100);
        if(voxel_volume_mix_label)
        {
            voxel_volume_mix_label->setText(QString::number((int)effect_voxel_volume_mix) + QStringLiteral("%"));
        }
        emit ParametersChanged();
    });
    connect(voxel_volume_scale_slider, &QSlider::valueChanged, this, [this](int v) {
        effect_voxel_room_scale_centi = std::clamp(v, 2, 80);
        if(voxel_volume_scale_label)
        {
            voxel_volume_scale_label->setText(QString::number(effect_voxel_room_scale_centi / 100.0f, 'f', 2));
        }
        emit ParametersChanged();
    });
    connect(voxel_volume_heading_slider, &QSlider::valueChanged, this, [this](int v) {
        effect_voxel_heading_offset = std::clamp(v, -180, 180);
        if(voxel_volume_heading_label)
        {
            voxel_volume_heading_label->setText(QString::number(effect_voxel_heading_offset) + QStringLiteral("°"));
        }
        emit ParametersChanged();
    });
    connect(voxel_drive_combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &SpatialEffect3D::OnVoxelDriveComboChanged);

    SyncSpatialMappingControlVisibility();
}

RGBColor SpatialEffect3D::GetRainbowColor(float hue)
{
    hue = std::fmod(hue, 360.0f);
    if(hue < 0) hue += 360.0f;

    float c = 1.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));

    float r, g, b;
    if(hue < 60) { r = c; g = x; b = 0; }
    else if(hue < 120) { r = x; g = c; b = 0; }
    else if(hue < 180) { r = 0; g = c; b = x; }
    else if(hue < 240) { r = 0; g = x; b = c; }
    else if(hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    return ((int)(b * 255) << 16) | ((int)(g * 255) << 8) | (int)(r * 255);
}

RGBColor SpatialEffect3D::GetColorAtPosition(float position)
{
    position = std::clamp(position, 0.0f, 1.0f);

    if(rainbow_mode)
    {
        return GetRainbowColor(position * 360.0f);
    }

    if(colors.empty())
    {
        return COLOR_WHITE;
    }

    if(colors.size() == 1)
    {
        return colors[0];
    }

    float scaled_pos = position * (colors.size() - 1);
    int index = (int)scaled_pos;
    float frac = scaled_pos - index;

    if(index >= (int)colors.size() - 1)
    {
        return colors.back();
    }

    RGBColor color1 = colors[index];
    RGBColor color2 = colors[index + 1];

    int b1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int r1 = color1 & 0xFF;

    int b2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int r2 = color2 & 0xFF;

    int r = (int)(r1 + (r2 - r1) * frac);
    int g = (int)(g1 + (g2 - g1) * frac);
    int b = (int)(b1 + (b2 - b1) * frac);

    return (b << 16) | (g << 8) | r;
}

void SpatialEffect3D::OnRainbowModeChanged()
{
    rainbow_mode = rainbow_mode_check->isChecked();
    color_buttons_widget->setVisible(!rainbow_mode);
    emit ParametersChanged();
}

void SpatialEffect3D::OnSpatialMappingComboChanged()
{
    if(!spatial_mapping_combo)
    {
        return;
    }
    const int v = spatial_mapping_combo->currentData().toInt();
    spatial_mapping_mode = static_cast<SpatialMappingMode>(std::clamp(v, 0, 3));
    SyncSpatialMappingControlVisibility();
    emit ParametersChanged();
}

void SpatialEffect3D::OnCompassLayerSpinComboChanged()
{
    if(!compass_layer_spin_combo)
    {
        return;
    }
    compass_layer_spin_preset = std::clamp(compass_layer_spin_combo->currentIndex(), 0, 3);
    emit ParametersChanged();
}

void SpatialEffect3D::OnVoxelDriveComboChanged()
{
    if(!voxel_drive_combo)
    {
        return;
    }
    const int v = voxel_drive_combo->currentData().toInt();
    effect_voxel_drive_mode = static_cast<VoxelDriveMode>(std::clamp(v, 0, 5));
    emit ParametersChanged();
}

void SpatialEffect3D::SyncSpatialMappingControlVisibility()
{
    const bool compass_mode = (spatial_mapping_mode == SpatialMappingMode::CompassPalette);
    const bool room_active = (spatial_mapping_mode != SpatialMappingMode::Off);
    const bool voxel_mode = (spatial_mapping_mode == SpatialMappingMode::VoxelVolume);

    if(sampler_influence_slider)
    {
        sampler_influence_slider->setEnabled(room_active);
    }
    if(sampler_influence_label)
    {
        sampler_influence_label->setEnabled(room_active);
    }
    if(compass_layer_spin_combo)
    {
        compass_layer_spin_combo->setEnabled(compass_mode);
    }
    if(sampler_compass_north_slider)
    {
        sampler_compass_north_slider->setEnabled(room_active);
    }
    if(sampler_compass_north_label)
    {
        sampler_compass_north_label->setEnabled(room_active);
    }
    if(voxel_volume_group)
    {
        voxel_volume_group->setEnabled(voxel_mode);
    }
    if(voxel_drive_combo)
    {
        voxel_drive_combo->setEnabled(voxel_mode);
    }
}

void SpatialEffect3D::SetSpatialMappingMode(SpatialMappingMode mode)
{
    spatial_mapping_mode = mode;
    if(spatial_mapping_combo)
    {
        QSignalBlocker b(spatial_mapping_combo);
        for(int i = 0; i < spatial_mapping_combo->count(); i++)
        {
            if(spatial_mapping_combo->itemData(i).toInt() == (int)spatial_mapping_mode)
            {
                spatial_mapping_combo->setCurrentIndex(i);
                break;
            }
        }
    }
    SyncSpatialMappingControlVisibility();
}

void SpatialEffect3D::SetVoxelDriveMode(VoxelDriveMode mode)
{
    effect_voxel_drive_mode = mode;
    if(voxel_drive_combo)
    {
        QSignalBlocker b(voxel_drive_combo);
        for(int i = 0; i < voxel_drive_combo->count(); i++)
        {
            if(voxel_drive_combo->itemData(i).toInt() == (int)effect_voxel_drive_mode)
            {
                voxel_drive_combo->setCurrentIndex(i);
                break;
            }
        }
    }
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

void SpatialEffect3D::SetColors(const std::vector<RGBColor>& new_colors)
{
    colors = new_colors;
    if(colors.empty())
    {
        colors.push_back(COLOR_RED);
    }
}

std::vector<RGBColor> SpatialEffect3D::GetColors() const
{
    return colors;
}

void SpatialEffect3D::SetRainbowMode(bool enabled)
{
    rainbow_mode = enabled;
    if(rainbow_mode_check)
    {
        rainbow_mode_check->setChecked(enabled);
    }
}

bool SpatialEffect3D::GetRainbowMode() const
{
    return rainbow_mode;
}

void SpatialEffect3D::SetFrequency(unsigned int frequency)
{
    effect_frequency = frequency;
    if(frequency_slider)
    {
        frequency_slider->setValue(frequency);
    }
}

unsigned int SpatialEffect3D::GetFrequency() const
{
    return effect_frequency;
}

void SpatialEffect3D::SetDetail(unsigned int detail)
{
    effect_detail = detail;
    if(detail_slider)
    {
        detail_slider->setValue(detail);
    }
}

unsigned int SpatialEffect3D::GetDetail() const
{
    return effect_detail;
}

void SpatialEffect3D::SetReferenceMode(ReferenceMode mode)
{
    reference_mode = mode;
}

ReferenceMode SpatialEffect3D::GetReferenceMode() const
{
    return reference_mode;
}

void SpatialEffect3D::SetGlobalReferencePoint(const Vector3D& point)
{
    global_reference_point = point;
}

Vector3D SpatialEffect3D::GetGlobalReferencePoint() const
{
    return global_reference_point;
}

void SpatialEffect3D::SetCustomReferencePoint(const Vector3D& point)
{
    custom_reference_point = point;
}

void SpatialEffect3D::SetUseCustomReference(bool use_custom)
{
    use_custom_reference = use_custom;
}

Vector3D SpatialEffect3D::GetEffectOrigin() const
{
    if(use_custom_reference)
    {
        return custom_reference_point;
    }

    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_WORLD_ORIGIN:
        case REF_MODE_LED_CENTROID:
        case REF_MODE_TARGET_ZONE_CENTER:
        case REF_MODE_ROOM_CENTER:
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

Vector3D SpatialEffect3D::GetReferencePointGrid(const GridContext3D& grid) const
{
    if(use_custom_reference)
    {
        return custom_reference_point;
    }
    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_TARGET_ZONE_CENTER:
            return {grid.center_x, grid.center_y, grid.center_z};
        case REF_MODE_WORLD_ORIGIN:
            return {0.0f, 0.0f, 0.0f};
        case REF_MODE_LED_CENTROID:
            if(grid.has_led_centroid)
            {
                return {grid.led_centroid_x, grid.led_centroid_y, grid.led_centroid_z};
            }
            return {grid.center_x, grid.center_y, grid.center_z};
        case REF_MODE_ROOM_CENTER:
        default:
            return {grid.center_x, grid.center_y, grid.center_z};
    }
}

Vector3D SpatialEffect3D::GetEffectOriginGrid(const GridContext3D& grid) const
{
    Vector3D base = GetReferencePointGrid(grid);
    float half_w = grid.width * 0.5f;
    float half_h = grid.height * 0.5f;
    float half_d = grid.depth * 0.5f;
    base.x += (effect_offset_x / 100.0f) * half_w;
    base.y += (effect_offset_y / 100.0f) * half_h;
    base.z += (effect_offset_z / 100.0f) * half_d;
    return base;
}

float SpatialEffect3D::GetNormalizedSpeed() const
{
    float normalized = effect_speed / 200.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedFrequency() const
{
    float normalized = effect_frequency / 200.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedDetail() const
{
    float normalized = effect_detail / 200.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedSize() const
{
    return (effect_size / 200.0f) * 3.0f;
}

float SpatialEffect3D::GetNormalizedScale() const
{
    float normalized;

    if(effect_scale <= 200)
    {
        normalized = effect_scale / 200.0f;
    }
    else
    {
        normalized = 1.0f + ((effect_scale - 200) / 100.0f);
    }

    if(scale_inverted)
    {
        if(normalized <= 1.0f)
        {
            normalized = 1.0f - normalized;
        }
        else
        {
            float extra = normalized - 1.0f;
            normalized = std::max(0.0f, 1.0f - extra);
        }
    }

    return std::max(0.0f, normalized);
}

unsigned int SpatialEffect3D::CombineMediaSampling(unsigned int local_detail_percent) const
{
    const unsigned int g = std::min(100u, effect_sampling_resolution);
    const unsigned int l = std::min(100u, local_detail_percent);
    return (unsigned int)std::clamp((int)((l * g + 50u) / 100u), 0, 100);
}

namespace
{
void FillCompassSpinFromPreset(int preset, int out_spin[3])
{
    static const int kSpins[4][3] = {
        {1, 1, 1},
        {-1, -1, -1},
        {1, -1, 1},
        {-1, 1, -1},
    };
    const int p = std::clamp(preset, 0, 3);
    for(int i = 0; i < 3; i++)
    {
        out_spin[i] = kSpins[p][i];
    }
}

void ApplySpatialSamplingQuantization(float& x, float& y, float& z, const GridContext3D& grid, unsigned int resolution_pct)
{
    if(resolution_pct >= 100u)
    {
        return;
    }
    const float sx = std::max(1e-6f, grid.max_x - grid.min_x);
    const float sy = std::max(1e-6f, grid.max_y - grid.min_y);
    const float sz = std::max(1e-6f, grid.max_z - grid.min_z);
    float nx = std::clamp((x - grid.min_x) / sx, 0.0f, 1.0f);
    float ny = std::clamp((y - grid.min_y) / sy, 0.0f, 1.0f);
    float nz = std::clamp((z - grid.min_z) / sz, 0.0f, 1.0f);
    constexpr int kVirtualCells = 128;
    Geometry3D::QuantizeNormalizedAxis01(nx, resolution_pct, kVirtualCells);
    Geometry3D::QuantizeNormalizedAxis01(ny, resolution_pct, kVirtualCells);
    Geometry3D::QuantizeNormalizedAxis01(nz, resolution_pct, kVirtualCells);
    x = grid.min_x + nx * sx;
    y = grid.min_y + ny * sy;
    z = grid.min_z + nz * sz;
}

static RGBColor BlendRgbForVoxel(RGBColor rgb_a, RGBColor rgb_b, float mix01)
{
    mix01 = std::clamp(mix01, 0.0f, 1.0f);
    const int ar = (int)RGBGetRValue(rgb_a);
    const int ag = (int)RGBGetGValue(rgb_a);
    const int ab = (int)RGBGetBValue(rgb_a);
    const int br = (int)RGBGetRValue(rgb_b);
    const int bg = (int)RGBGetGValue(rgb_b);
    const int bb = (int)RGBGetBValue(rgb_b);
    const int rr = (int)std::lround(ar + (br - ar) * mix01);
    const int gg = (int)std::lround(ag + (bg - ag) * mix01);
    const int bl = (int)std::lround(ab + (bb - ab) * mix01);
    return ToRGBColor((unsigned char)std::clamp(rr, 0, 255),
                      (unsigned char)std::clamp(gg, 0, 255),
                      (unsigned char)std::clamp(bl, 0, 255));
}
}

RGBColor SpatialEffect3D::EvaluateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(UsesSpatialSamplingQuantization())
    {
        ApplySpatialSamplingQuantization(x, y, z, grid, GetSamplingResolution());
    }
    RGBColor base = CalculateColorGrid(x, y, z, time, grid);
    if(effect_voxel_volume_mix > 0)
    {
        thread_local std::uint64_t tls_telemetry_rev = ~(std::uint64_t)0;
        thread_local GameTelemetryBridge::TelemetrySnapshot tls_telemetry;
        const std::uint64_t rev = GameTelemetryBridge::TelemetryDataRevision();
        if(rev != tls_telemetry_rev)
        {
            tls_telemetry = GameTelemetryBridge::GetTelemetrySnapshot();
            tls_telemetry_rev = rev;
        }
        Vector3D origin = GetEffectOriginGrid(grid);
        bool got_voxel_sample = false;
        RGBColor vx = VoxelMapping::SampleAtRoomGrid(tls_telemetry,
                                                     (float)effect_voxel_heading_offset,
                                                     effect_voxel_room_scale_centi / 100.0f,
                                                     0.02f,
                                                     x,
                                                     y,
                                                     z,
                                                     origin.x,
                                                     origin.y,
                                                     origin.z,
                                                     &got_voxel_sample);
        if(got_voxel_sample)
        {
            const float t = (float)effect_voxel_volume_mix / 100.0f;
            base = BlendRgbForVoxel(base, vx, t);
        }
    }
    return base;
}

float SpatialEffect3D::ApplySpatialPalette01(float base_pos01,
                                            const SpatialLayerCore::Basis& basis,
                                            const SpatialLayerCore::SamplePoint& sp,
                                            const SpatialLayerCore::MapperSettings& map,
                                            float time,
                                            const GridContext3D* grid) const
{
    const float infl = std::clamp((float)effect_sampler_influence_centi / 100.0f, 0.0f, 2.5f);
    SpatialLayerCore::MapperSettings m = map;
    m.compass_azimuth_offset_rad = (float)effect_sampler_compass_north_offset_deg * 0.01745329251f;

    switch(spatial_mapping_mode)
    {
    case SpatialMappingMode::Off:
        return base_pos01;
    case SpatialMappingMode::SubtleTint:
    {
        // Keep "Subtle" gentle, but animate slightly so users can see mapping is active.
        const float h = SpatialLayerCore::CompassStratumHueOffsetDegrees(basis, sp, m, 3) * infl;
        const float drift_deg = time * std::max(0.08f, GetScaledFrequency()) * 6.0f * infl;
        const float h_use = h + drift_deg;
        return SpatialLayerCore::ShiftGradient01WithCompassHue(base_pos01, h_use);
    }
    case SpatialMappingMode::CompassPalette:
    {
        SpatialLayerCore::SamplePoint sp_map = sp;
        if(grid)
        {
            sp_map = MakeCompassPaletteSamplePoint(*grid, m, sp);
        }
        SpatialLayerCore::CompassStratumSample css{};
        if(!SpatialLayerCore::MapPositionToCompassStrata(basis, sp_map, m, 3, css) || !css.valid)
        {
            return base_pos01;
        }
        int spins[3];
        FillCompassSpinFromPreset(compass_layer_spin_preset, spins);
        const float scroll =
            time * std::max(0.12f, GetScaledFrequency()) * 0.30f * infl;
        const float plane_mix = 0.58f * std::clamp(GetScaledDetail(), 0.05f, 1.f) * infl;
        return SpatialLayerCore::CompassStratumPalettePosition01(css, spins, scroll, plane_mix, base_pos01);
    }
    case SpatialMappingMode::VoxelVolume:
    {
        if(!grid)
        {
            return base_pos01;
        }
        const float nx = NormalizeGridAxis01(sp.grid_x, grid->min_x, grid->max_x);
        const float ny = sp.y_norm;
        const float nz = NormalizeGridAxis01(sp.grid_z, grid->min_z, grid->max_z);
        const float vol01 =
            std::fmod(0.41f * nx + 0.33f * ny + 0.37f * nz + 0.11f * (nx * ny * nz), 1.0f);
        const float rx = (sp.grid_x - sp.origin_x) / std::max(grid->width, 1e-3f);
        const float ry = (sp.grid_y - sp.origin_y) / std::max(grid->height, 1e-3f);
        const float rz = (sp.grid_z - sp.origin_z) / std::max(grid->depth, 1e-3f);
        const float ref01 = std::fmod(0.29f * rx + 0.31f * ry + 0.27f * rz + 1.0f, 1.0f);
        float p = base_pos01 + infl * (0.78f * (vol01 - 0.5f) + 0.45f * (ref01 - 0.5f));
        p = std::fmod(p, 1.0f);
        if(p < 0.0f)
        {
            p += 1.0f;
        }
        return p;
    }
    }
    return base_pos01;
}

float SpatialEffect3D::ApplySpatialRainbowHue(float hue_deg,
                                               float plane_pos01,
                                               const SpatialLayerCore::Basis& basis,
                                               const SpatialLayerCore::SamplePoint& sp,
                                               const SpatialLayerCore::MapperSettings& map,
                                               float time,
                                               const GridContext3D* grid) const
{
    const float infl = std::clamp((float)effect_sampler_influence_centi / 100.0f, 0.0f, 2.5f);
    SpatialLayerCore::MapperSettings m = map;
    m.compass_azimuth_offset_rad = (float)effect_sampler_compass_north_offset_deg * 0.01745329251f;

    switch(spatial_mapping_mode)
    {
    case SpatialMappingMode::Off:
        return hue_deg;
    case SpatialMappingMode::SubtleTint:
    {
        const float h = SpatialLayerCore::CompassStratumHueOffsetDegrees(basis, sp, m, 3) * infl;
        const float drift_deg = time * std::max(0.08f, GetScaledFrequency()) * 6.0f * infl;
        return hue_deg + h + drift_deg;
    }
    case SpatialMappingMode::CompassPalette:
    {
        SpatialLayerCore::SamplePoint sp_map = sp;
        if(grid)
        {
            sp_map = MakeCompassPaletteSamplePoint(*grid, m, sp);
        }
        SpatialLayerCore::CompassStratumSample css{};
        if(!SpatialLayerCore::MapPositionToCompassStrata(basis, sp_map, m, 3, css) || !css.valid)
        {
            return hue_deg;
        }
        int spins[3];
        FillCompassSpinFromPreset(compass_layer_spin_preset, spins);
        const float scroll =
            time * std::max(0.12f, GetScaledFrequency()) * 0.30f * infl;
        const float plane_mix = 0.52f * std::clamp(GetScaledDetail(), 0.05f, 1.f) * infl;
        const float pos01 =
            SpatialLayerCore::CompassStratumPalettePosition01(css, spins, scroll, plane_mix, plane_pos01);
        return pos01 * 360.0f + hue_deg * 0.22f;
    }
    case SpatialMappingMode::VoxelVolume:
    {
        if(!grid)
        {
            return hue_deg;
        }
        const float nx = NormalizeGridAxis01(sp.grid_x, grid->min_x, grid->max_x);
        const float ny = sp.y_norm;
        const float nz = NormalizeGridAxis01(sp.grid_z, grid->min_z, grid->max_z);
        const float vol01 =
            std::fmod(0.41f * nx + 0.33f * ny + 0.37f * nz + 0.11f * (nx * ny * nz), 1.0f);
        const float rx = (sp.grid_x - sp.origin_x) / std::max(grid->width, 1e-3f);
        const float ry = (sp.grid_y - sp.origin_y) / std::max(grid->height, 1e-3f);
        const float rz = (sp.grid_z - sp.origin_z) / std::max(grid->depth, 1e-3f);
        const float ref01 = std::fmod(0.29f * rx + 0.31f * ry + 0.27f * rz + 1.0f, 1.0f);
        const float delta_deg = infl * 360.0f * (0.78f * (vol01 - 0.5f) + 0.45f * (ref01 - 0.5f));
        return hue_deg + delta_deg;
    }
    }
    return hue_deg;
}

bool SpatialEffect3D::SampleVoxelRgbAtRoom(float x,
                                           float y,
                                           float z,
                                           const GridContext3D& grid,
                                           RGBColor& out_rgb,
                                           bool& got_hit) const
{
    out_rgb = (RGBColor)0;
    got_hit = false;
    thread_local std::uint64_t tls_telemetry_rev = ~(std::uint64_t)0;
    thread_local GameTelemetryBridge::TelemetrySnapshot tls_telemetry;
    const std::uint64_t rev = GameTelemetryBridge::TelemetryDataRevision();
    if(rev != tls_telemetry_rev)
    {
        tls_telemetry = GameTelemetryBridge::GetTelemetrySnapshot();
        tls_telemetry_rev = rev;
    }
    const Vector3D origin = GetEffectOriginGrid(grid);
    out_rgb = VoxelMapping::SampleAtRoomGrid(tls_telemetry,
                                             (float)effect_voxel_heading_offset,
                                             effect_voxel_room_scale_centi / 100.0f,
                                             0.02f,
                                             x,
                                             y,
                                             z,
                                             origin.x,
                                             origin.y,
                                             origin.z,
                                             &got_hit);
    return got_hit;
}

float SpatialEffect3D::ApplyVoxelDriveToPalette01(float palette01,
                                                  float x,
                                                  float y,
                                                  float z,
                                                  float time,
                                                  const GridContext3D& grid) const
{
    if(effect_voxel_drive_mode == VoxelDriveMode::Off)
    {
        return palette01;
    }
    const float infl = std::clamp((float)effect_sampler_influence_centi / 100.0f, 0.0f, 2.5f);
    if(infl <= 1e-5f)
    {
        return palette01;
    }
    const float detail_gate = std::clamp(GetScaledDetail(), 0.05f, 1.f);
    float drive = 0.0f;

    switch(effect_voxel_drive_mode)
    {
    case VoxelDriveMode::Off:
        return palette01;
    case VoxelDriveMode::LumaField:
    {
        RGBColor vx{};
        bool got = false;
        SampleVoxelRgbAtRoom(x, y, z, grid, vx, got);
        if(!got)
        {
            return palette01;
        }
        const float r = (float)(vx & 0xFF);
        const float g = (float)((vx >> 8) & 0xFF);
        const float b = (float)((vx >> 16) & 0xFF);
        drive = (r + g + b) / (3.0f * 255.0f);
        break;
    }
    case VoxelDriveMode::ScrollRoomX:
        drive = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
        break;
    case VoxelDriveMode::ScrollRoomY:
        drive = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
        break;
    case VoxelDriveMode::ScrollRoomZ:
        drive = NormalizeGridAxis01(z, grid.min_z, grid.max_z);
        break;
    case VoxelDriveMode::VolumeRoll:
    {
        RGBColor vx{};
        bool got = false;
        SampleVoxelRgbAtRoom(x, y, z, grid, vx, got);
        const Vector3D o = GetEffectOriginGrid(grid);
        constexpr float kPi = 3.14159265f;
        float a = std::atan2(z - o.z, x - o.x) * (0.5f / kPi) + 0.5f;
        float luma = 0.5f;
        if(got)
        {
            luma = ((float)(vx & 0xFF) + (float)((vx >> 8) & 0xFF) + (float)((vx >> 16) & 0xFF)) / (3.0f * 255.0f);
        }
        drive = std::fmod(a * 0.65f + luma * 0.35f + time * 0.03f * detail_gate * infl, 1.0f);
        if(drive < 0.0f)
        {
            drive += 1.0f;
        }
        float p = std::fmod(palette01 + 0.55f * detail_gate * infl * drive, 1.0f);
        if(p < 0.0f)
        {
            p += 1.0f;
        }
        return p;
    }
    }
    const float mix_strength = 0.45f * detail_gate * infl;
    float p = std::fmod(palette01 + mix_strength * drive, 1.0f);
    if(p < 0.0f)
    {
        p += 1.0f;
    }
    return p;
}

unsigned int SpatialEffect3D::GetTargetFPS() const
{
    return effect_fps;
}

float SpatialEffect3D::GetScaledSpeed() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float speed_scale = (info.default_speed_scale > 0.0f) ? info.default_speed_scale : 10.0f;
    return GetNormalizedSpeed() * speed_scale;
}

float SpatialEffect3D::GetScaledFrequency() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float freq_scale = (info.default_frequency_scale > 0.0f) ? info.default_frequency_scale : 10.0f;
    return GetNormalizedFrequency() * freq_scale;
}

bool SpatialEffect3D::UseZoneGrid() const
{
    return (effect_bounds_mode == (int)BOUNDS_MODE_TARGET_ZONE);
}

bool SpatialEffect3D::UseWorldGridBounds() const
{
    if(effect_bounds_mode == (int)BOUNDS_MODE_TARGET_ZONE)
    {
        // Match bounds space to the effect sample space so origin/center math stays coherent.
        return RequiresWorldSpaceCoordinates();
    }
    return RequiresWorldSpaceGridBounds();
}

float SpatialEffect3D::GetScaledDetail() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float s = (info.default_detail_scale > 0.0f) ? info.default_detail_scale : 10.0f;
    return GetNormalizedDetail() * s;
}

float SpatialEffect3D::CalculateProgress(float time) const
{
    return time * GetScaledSpeed();
}

RGBColor SpatialEffect3D::PostProcessColorGrid(RGBColor color) const
{
    float intensity_normalized = effect_intensity / 200.0f;
    float intensity_mul = std::pow(intensity_normalized, 0.7f) * 1.7f;
    float brightness_mul = effect_brightness / 100.0f;
    float factor = intensity_mul * brightness_mul;
    if(factor <= 0.0f) return 0x00000000;

    unsigned char r = color & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = (color >> 16) & 0xFF;
    int rr = (int)(r * factor); if(rr > 255) rr = 255;
    int gg = (int)(g * factor); if(gg > 255) gg = 255;
    int bb = (int)(b * factor); if(bb > 255) bb = 255;

    if(effect_sharpness != 100)
    {
        const float gamma = std::pow(2.0f, (effect_sharpness - 100) / 100.0f);
        if(rr > 0)
        {
            float n = std::pow((float)rr / 255.0f, gamma);
            rr = (int)(n * 255.0f + 0.5f);
            if(rr > 255) rr = 255;
        }
        if(gg > 0)
        {
            float n = std::pow((float)gg / 255.0f, gamma);
            gg = (int)(n * 255.0f + 0.5f);
            if(gg > 255) gg = 255;
        }
        if(bb > 0)
        {
            float n = std::pow((float)bb / 255.0f, gamma);
            bb = (int)(n * 255.0f + 0.5f);
            if(bb > 255) bb = 255;
        }
    }

    return (bb << 16) | (gg << 8) | rr;
}

static float smoothstep_edge(float edge0, float edge1, float x)
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float SpatialEffect3D::ApplyEdgeToIntensity(float normalized_dist) const
{
    float thick = 0.05f + 0.45f * (effect_edge_thickness / 100.0f);
    float glow = 0.15f * (effect_glow_level / 100.0f);
    int profile = std::clamp(effect_edge_profile, 0, 4);

    float mult = 0.0f;
    switch(profile)
    {
    case 0:
        mult = (normalized_dist < 1.0f) ? 1.0f : 0.0f;
        break;
    case 1:
        mult = (normalized_dist <= 1.0f) ? 1.0f : 0.0f;
        break;
    case 2:
        mult = 1.0f - smoothstep_edge(1.0f - thick, 1.0f, normalized_dist);
        break;
    case 3:
        mult = 1.0f - smoothstep_edge(1.0f - thick * 1.5f, 1.0f + thick * 0.5f, normalized_dist);
        break;
    case 4:
        {
            float margin = 0.08f * (1.0f - effect_edge_thickness / 100.0f);
            mult = (normalized_dist < 1.0f - margin) ? 1.0f : (1.0f - smoothstep_edge(1.0f - margin, 1.0f, normalized_dist));
        }
        break;
    default:
        mult = (normalized_dist < 1.0f) ? 1.0f : 0.0f;
        break;
    }
    if(normalized_dist > 1.0f && glow > 0.0f)
    {
        float fall = std::exp(-(normalized_dist - 1.0f) * 2.5f);
        mult = std::max(mult, glow * fall);
    }
    return std::clamp(mult, 0.0f, 1.0f);
}

float SpatialEffect3D::GetBoundaryMultiplier(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    float half_w = grid.width * 0.5f;
    float half_h = grid.height * 0.5f;
    float half_d = grid.depth * 0.5f;
    float max_dist_sq = half_w * half_w + half_h * half_h + half_d * half_d;
    if(max_dist_sq < 1e-10f) return 1.0f;

    float scale_pct = GetNormalizedScale();
    float scale_radius_sq = max_dist_sq * scale_pct * scale_pct;
    float dist_sq = rel_x * rel_x + rel_y * rel_y + rel_z * rel_z;
    float dist = std::sqrt(std::max(0.0f, dist_sq));
    float radius = std::sqrt(std::max(1e-10f, scale_radius_sq));
    float normalized_dist = radius > 1e-10f ? (dist / radius) : 0.0f;

    return ApplyEdgeToIntensity(normalized_dist);
}

Vector3D SpatialEffect3D::TransformPointByRotation(float x, float y, float z, const Vector3D& origin) const
{
    float tx = x - origin.x;
    float ty = y - origin.y;
    float tz = z - origin.z;
    
    float yaw_rad = effect_rotation_yaw * 3.14159265359f / 180.0f;
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float nx = tx * cos_yaw - tz * sin_yaw;
    float nz = tx * sin_yaw + tz * cos_yaw;
    float ny = ty;

    float pitch_rad = effect_rotation_pitch * 3.14159265359f / 180.0f;
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);
    float py = ny * cos_pitch - nz * sin_pitch;
    float pz = ny * sin_pitch + nz * cos_pitch;
    float px = nx;

    float roll_rad = effect_rotation_roll * 3.14159265359f / 180.0f;
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);
    float fx = px * cos_roll - py * sin_roll;
    float fy = px * sin_roll + py * cos_roll;
    float fz = pz;

    return {fx + origin.x, fy + origin.y, fz + origin.z};
}

Vector3D SpatialEffect3D::RotateVectorByEuler(float dx, float dy, float dz, float yaw_deg, float pitch_deg, float roll_deg)
{
    float yaw_rad = yaw_deg * 3.14159265359f / 180.0f;
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float nx = dx * cos_yaw - dz * sin_yaw;
    float nz = dx * sin_yaw + dz * cos_yaw;
    float ny = dy;

    float pitch_rad = pitch_deg * 3.14159265359f / 180.0f;
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);
    float py = ny * cos_pitch - nz * sin_pitch;
    float pz = ny * sin_pitch + nz * cos_pitch;
    float px = nx;

    float roll_rad = roll_deg * 3.14159265359f / 180.0f;
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);
    float fx = px * cos_roll - py * sin_roll;
    float fy = px * sin_roll + py * cos_roll;
    float fz = pz;

    return {fx, fy, fz};
}

bool SpatialEffect3D::IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    Vector3D o = GetEffectOriginGrid(grid);
    float max_corner_sq = 0.0f;
    for(int ix = 0; ix < 2; ++ix)
    {
        float cx = ix ? grid.max_x : grid.min_x;
        float dx = cx - o.x;
        for(int iy = 0; iy < 2; ++iy)
        {
            float cy = iy ? grid.max_y : grid.min_y;
            float dy = cy - o.y;
            for(int iz = 0; iz < 2; ++iz)
            {
                float cz = iz ? grid.max_z : grid.min_z;
                float dz = cz - o.z;
                float sq = dx * dx + dy * dy + dz * dz;
                if(sq > max_corner_sq)
                {
                    max_corner_sq = sq;
                }
            }
        }
    }

    float scale_percentage = GetNormalizedScale();
    float scale_radius_sq = max_corner_sq * scale_percentage * scale_percentage;

    float dist_sq = rel_x * rel_x + rel_y * rel_y + rel_z * rel_z;
    return dist_sq <= scale_radius_sq;
}

bool SpatialEffect3D::IsPointOnActiveSurface(float x, float y, float z, const GridContext3D& grid) const
{
    if((effect_surface_mask & SURF_ALL) == SURF_ALL)
        return true;
    float d_floor = y - grid.min_y;
    float d_ceil = grid.max_y - y;
    float d_wxm = x - grid.min_x;
    float d_wxp = grid.max_x - x;
    float d_wzm = z - grid.min_z;
    float d_wzp = grid.max_z - z;
    float best = d_floor;
    int surf = SURF_FLOOR;
    if(d_ceil < best) { best = d_ceil; surf = SURF_CEIL; }
    if(d_wxm < best) { best = d_wxm; surf = SURF_WALL_XM; }
    if(d_wxp < best) { best = d_wxp; surf = SURF_WALL_XP; }
    if(d_wzm < best) { best = d_wzm; surf = SURF_WALL_ZM; }
    if(d_wzp < best) { surf = SURF_WALL_ZP; }
    return (effect_surface_mask & surf) != 0;
}

void SpatialEffect3D::UpdateCommonEffectParams(SpatialEffectParams& /* params */)
{
}

void SpatialEffect3D::SetSurfaceMaskFlag(int flag, bool enabled)
{
    if(enabled)
    {
        effect_surface_mask |= flag;
    }
    else
    {
        effect_surface_mask &= ~flag;
    }
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
    SetControlGroupVisibility(speed_slider, speed_label, "Speed:", true);
    SetControlGroupVisibility(brightness_slider, brightness_label, "Brightness:", true);
    SetControlGroupVisibility(frequency_slider, frequency_label, "Frequency:", true);
    SetControlGroupVisibility(detail_slider, detail_label, "Detail:", true);
    SetControlGroupVisibility(size_slider, size_label, "Size:", true);
    SetControlGroupVisibility(scale_slider, scale_label, "Scale:", true);
    SetControlGroupVisibility(fps_slider, fps_label, "FPS:", GetEffectInfo().show_fps_control);

    if(color_controls_group)
    {
        color_controls_group->setVisible(true);
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
    if(sampler_mapper_group)
    {
        sampler_mapper_group->setVisible(true);
    }

    if(surfaces_section)
    {
        surfaces_section->setVisible(true);
    }
    if(position_offset_group) position_offset_group->setVisible(true);
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
    // effect_bounds_mode is managed externally (Effect Stack panel).

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

nlohmann::json SpatialEffect3D::SaveSettings() const
{
    nlohmann::json j;

    j["speed"] = effect_speed;
    j["brightness"] = effect_brightness;
    j["frequency"] = effect_frequency;
    j["detail"] = effect_detail;
    j["size"] = effect_size;
    j["scale_value"] = effect_scale;
    j["scale_inverted"] = scale_inverted;
    j["effect_bounds_mode"] = effect_bounds_mode;
    j["rainbow_mode"] = rainbow_mode;
    j["spatial_mapping_mode"] = (int)spatial_mapping_mode;
    j["compass_layer_spin_preset"] = compass_layer_spin_preset;
    j["effect_voxel_volume_mix"] = effect_voxel_volume_mix;
    j["effect_voxel_room_scale_centi"] = effect_voxel_room_scale_centi;
    j["effect_voxel_heading_offset"] = effect_voxel_heading_offset;
    j["effect_voxel_drive_mode"] = (int)effect_voxel_drive_mode;
    j["effect_sampler_influence_centi"] = effect_sampler_influence_centi;
    j["effect_sampler_compass_north_offset_deg"] = effect_sampler_compass_north_offset_deg;
    j["intensity"] = effect_intensity;
    j["sharpness"] = effect_sharpness;
    j["smoothing"] = effect_smoothing;
    j["sampling_resolution"] = effect_sampling_resolution;
    j["edge_profile"] = effect_edge_profile;
    j["edge_thickness"] = effect_edge_thickness;
    j["glow_level"] = effect_glow_level;
    j["axis_scale_x"] = effect_scale_x;
    j["axis_scale_y"] = effect_scale_y;
    j["axis_scale_z"] = effect_scale_z;
    j["rotation_yaw"] = effect_rotation_yaw;
    j["rotation_pitch"] = effect_rotation_pitch;
    j["rotation_roll"] = effect_rotation_roll;
    j["axis_scale_rotation_yaw"] = effect_axis_scale_rotation_yaw;
    j["axis_scale_rotation_pitch"] = effect_axis_scale_rotation_pitch;
    j["axis_scale_rotation_roll"] = effect_axis_scale_rotation_roll;

    nlohmann::json colors_array = nlohmann::json::array();
    for(size_t i = 0; i < colors.size(); i++)
    {
        RGBColor color = colors[i];
        colors_array.push_back({
            {"r", RGBGetRValue(color)},
            {"g", RGBGetGValue(color)},
            {"b", RGBGetBValue(color)}
        });
    }
    j["colors"] = colors_array;

    j["fps"] = effect_fps;

    j["path_axis"] = effect_path_axis;
    j["plane"] = effect_plane;
    j["surface_mask"] = effect_surface_mask;
    j["offset_x"] = effect_offset_x;
    j["offset_y"] = effect_offset_y;
    j["offset_z"] = effect_offset_z;

    j["reference_mode"] = (int)reference_mode;
    j["global_ref_x"] = global_reference_point.x;
    j["global_ref_y"] = global_reference_point.y;
    j["global_ref_z"] = global_reference_point.z;
    j["custom_ref_x"] = custom_reference_point.x;
    j["custom_ref_y"] = custom_reference_point.y;
    j["custom_ref_z"] = custom_reference_point.z;
    j["use_custom_ref"] = use_custom_reference;

    return j;
}

void SpatialEffect3D::LoadSettings(const nlohmann::json& settings)
{
    if(settings.contains("speed"))
    {
        unsigned int spd = settings["speed"].get<unsigned int>();
        SetSpeed(std::min(200u, spd));
    }

    if(settings.contains("brightness"))
        SetBrightness(settings["brightness"].get<unsigned int>());

    if(settings.contains("frequency"))
        SetFrequency(settings["frequency"].get<unsigned int>());

    if(settings.contains("detail"))
        SetDetail(settings["detail"].get<unsigned int>());

    if(settings.contains("rainbow_mode"))
        SetRainbowMode(settings["rainbow_mode"].get<bool>());

    if(settings.contains("spatial_mapping_mode"))
    {
        const int v = settings["spatial_mapping_mode"].get<int>();
        SetSpatialMappingMode(static_cast<SpatialMappingMode>(std::clamp(v, 0, 3)));
    }

    if(settings.contains("compass_layer_spin_preset"))
    {
        compass_layer_spin_preset = std::clamp(settings["compass_layer_spin_preset"].get<int>(), 0, 3);
        if(compass_layer_spin_combo)
        {
            QSignalBlocker b(compass_layer_spin_combo);
            compass_layer_spin_combo->setCurrentIndex(compass_layer_spin_preset);
        }
    }

    if(settings.contains("effect_voxel_volume_mix"))
        effect_voxel_volume_mix = std::clamp(settings["effect_voxel_volume_mix"].get<unsigned int>(), 0u, 100u);
    if(settings.contains("effect_voxel_room_scale_centi"))
        effect_voxel_room_scale_centi = std::clamp(settings["effect_voxel_room_scale_centi"].get<int>(), 2, 80);
    if(settings.contains("effect_voxel_heading_offset"))
        effect_voxel_heading_offset = std::clamp(settings["effect_voxel_heading_offset"].get<int>(), -180, 180);

    if(settings.contains("effect_voxel_drive_mode"))
    {
        const int vd = settings["effect_voxel_drive_mode"].get<int>();
        SetVoxelDriveMode(static_cast<VoxelDriveMode>(std::clamp(vd, 0, 5)));
    }

    if(settings.contains("effect_sampler_influence_centi"))
    {
        effect_sampler_influence_centi = std::clamp(settings["effect_sampler_influence_centi"].get<int>(), 0, 250);
    }
    if(settings.contains("effect_sampler_compass_north_offset_deg"))
    {
        effect_sampler_compass_north_offset_deg =
            std::clamp(settings["effect_sampler_compass_north_offset_deg"].get<int>(), -180, 180);
    }

    if(settings.contains("effect_bounds_mode"))
        effect_bounds_mode = std::clamp(settings["effect_bounds_mode"].get<int>(), (int)BOUNDS_MODE_GLOBAL, (int)BOUNDS_MODE_TARGET_ZONE);
    else
        effect_bounds_mode = (int)BOUNDS_MODE_GLOBAL;

    if(settings.contains("intensity"))
        effect_intensity = settings["intensity"].get<unsigned int>();
    if(settings.contains("sharpness"))
        effect_sharpness = settings["sharpness"].get<unsigned int>();
    if(settings.contains("smoothing"))
        effect_smoothing = std::clamp(settings["smoothing"].get<unsigned int>(), 0u, 100u);
    if(settings.contains("sampling_resolution"))
        effect_sampling_resolution = std::clamp(settings["sampling_resolution"].get<unsigned int>(), 0u, 100u);
    if(settings.contains("edge_profile"))
        effect_edge_profile = std::clamp(settings["edge_profile"].get<int>(), 0, 4);
    if(settings.contains("edge_thickness"))
        effect_edge_thickness = std::clamp(settings["edge_thickness"].get<unsigned int>(), 0u, 100u);
    if(settings.contains("glow_level"))
        effect_glow_level = std::clamp(settings["glow_level"].get<unsigned int>(), 0u, 100u);

    if(settings.contains("axis_scale_x"))
        effect_scale_x = std::clamp(settings["axis_scale_x"].get<unsigned int>(), 1u, 400u);
    if(settings.contains("axis_scale_y"))
        effect_scale_y = std::clamp(settings["axis_scale_y"].get<unsigned int>(), 1u, 400u);
    if(settings.contains("axis_scale_z"))
        effect_scale_z = std::clamp(settings["axis_scale_z"].get<unsigned int>(), 1u, 400u);
    
    if(settings.contains("rotation_yaw"))
        effect_rotation_yaw = settings["rotation_yaw"].get<float>();
    else
        effect_rotation_yaw = 0.0f;
    if(settings.contains("rotation_pitch"))
        effect_rotation_pitch = settings["rotation_pitch"].get<float>();
    else
        effect_rotation_pitch = 0.0f;
    if(settings.contains("rotation_roll"))
        effect_rotation_roll = settings["rotation_roll"].get<float>();
    else
        effect_rotation_roll = 0.0f;
    if(settings.contains("axis_scale_rotation_yaw"))
        effect_axis_scale_rotation_yaw = std::clamp(settings["axis_scale_rotation_yaw"].get<float>(), -180.0f, 180.0f);
    if(settings.contains("axis_scale_rotation_pitch"))
        effect_axis_scale_rotation_pitch = std::clamp(settings["axis_scale_rotation_pitch"].get<float>(), -180.0f, 180.0f);
    if(settings.contains("axis_scale_rotation_roll"))
        effect_axis_scale_rotation_roll = std::clamp(settings["axis_scale_rotation_roll"].get<float>(), -180.0f, 180.0f);

    if(rotation_yaw_slider)
    {
        rotation_yaw_slider->setValue((int)effect_rotation_yaw);
    }
    if(rotation_pitch_slider)
    {
        rotation_pitch_slider->setValue((int)effect_rotation_pitch);
    }
    if(rotation_roll_slider)
    {
        rotation_roll_slider->setValue((int)effect_rotation_roll);
    }
    if(axis_scale_rot_yaw_slider)
        axis_scale_rot_yaw_slider->setValue((int)effect_axis_scale_rotation_yaw);
    if(axis_scale_rot_pitch_slider)
        axis_scale_rot_pitch_slider->setValue((int)effect_axis_scale_rotation_pitch);
    if(axis_scale_rot_roll_slider)
        axis_scale_rot_roll_slider->setValue((int)effect_axis_scale_rotation_roll);
    if(settings.contains("size"))
        effect_size = std::clamp(settings["size"].get<unsigned int>(), 0u, 200u);
    if(settings.contains("scale_value"))
        effect_scale = std::clamp(settings["scale_value"].get<unsigned int>(), 0u, 300u);
    if(settings.contains("scale_inverted"))
        scale_inverted = settings["scale_inverted"].get<bool>();
    if(settings.contains("fps"))
        effect_fps = std::clamp(settings["fps"].get<unsigned int>(), 1u, 120u);


    if(settings.contains("colors"))
    {
        std::vector<RGBColor> loaded_colors;
        const nlohmann::json& colors_array = settings["colors"];
        for(size_t i = 0; i < colors_array.size(); i++)
        {
            const nlohmann::json& color_json = colors_array[i];
            unsigned char r = color_json["r"].get<unsigned char>();
            unsigned char g = color_json["g"].get<unsigned char>();
            unsigned char b = color_json["b"].get<unsigned char>();
            loaded_colors.push_back(ToRGBColor(r, g, b));
        }
        SetColors(loaded_colors);
    }

    if(settings.contains("reference_mode"))
        SetReferenceMode((ReferenceMode)settings["reference_mode"].get<int>());

    if(settings.contains("global_ref_x") && settings.contains("global_ref_y") && settings.contains("global_ref_z"))
    {
        Vector3D ref_point;
        ref_point.x = settings["global_ref_x"].get<float>();
        ref_point.y = settings["global_ref_y"].get<float>();
        ref_point.z = settings["global_ref_z"].get<float>();
        SetGlobalReferencePoint(ref_point);
    }

    if(settings.contains("custom_ref_x") && settings.contains("custom_ref_y") && settings.contains("custom_ref_z"))
    {
        Vector3D ref_point;
        ref_point.x = settings["custom_ref_x"].get<float>();
        ref_point.y = settings["custom_ref_y"].get<float>();
        ref_point.z = settings["custom_ref_z"].get<float>();
        SetCustomReferencePoint(ref_point);
    }

    if(settings.contains("use_custom_ref"))
        SetUseCustomReference(settings["use_custom_ref"].get<bool>());

    if(settings.contains("path_axis") && settings["path_axis"].is_number_integer())
        effect_path_axis = std::clamp(settings["path_axis"].get<int>(), 0, 2);
    if(settings.contains("plane") && settings["plane"].is_number_integer())
        effect_plane = std::clamp(settings["plane"].get<int>(), 0, 2);
    if(settings.contains("surface_mask") && settings["surface_mask"].is_number_integer())
        effect_surface_mask = settings["surface_mask"].get<int>() & SURF_ALL;
    if(effect_surface_mask == 0)
        effect_surface_mask = SURF_ALL;
    if(settings.contains("offset_x") && settings["offset_x"].is_number_integer())
        effect_offset_x = std::clamp(settings["offset_x"].get<int>(), -100, 100);
    if(settings.contains("offset_y") && settings["offset_y"].is_number_integer())
        effect_offset_y = std::clamp(settings["offset_y"].get<int>(), -100, 100);
    if(settings.contains("offset_z") && settings["offset_z"].is_number_integer())
        effect_offset_z = std::clamp(settings["offset_z"].get<int>(), -100, 100);
    if(path_axis_combo)
        path_axis_combo->setCurrentIndex(effect_path_axis);
    if(plane_combo)
        plane_combo->setCurrentIndex(effect_plane);
    if(offset_x_slider)
        offset_x_slider->setValue(effect_offset_x);
    if(offset_y_slider)
        offset_y_slider->setValue(effect_offset_y);
    if(offset_z_slider)
        offset_z_slider->setValue(effect_offset_z);
    if(offset_x_label)
        offset_x_label->setText(QString::number(effect_offset_x) + "%");
    if(offset_y_label)
        offset_y_label->setText(QString::number(effect_offset_y) + "%");
    if(offset_z_label)
        offset_z_label->setText(QString::number(effect_offset_z) + "%");

    if(speed_slider)
    {
        QSignalBlocker blocker(speed_slider);
        speed_slider->setValue(effect_speed);
    }
    if(speed_label)
    {
        speed_label->setText(QString::number(effect_speed));
    }

    if(brightness_slider)
    {
        QSignalBlocker blocker(brightness_slider);
        brightness_slider->setValue(effect_brightness);
    }
    if(brightness_label)
    {
        brightness_label->setText(QString::number(effect_brightness));
    }

    if(frequency_slider)
    {
        QSignalBlocker blocker(frequency_slider);
        frequency_slider->setValue(effect_frequency);
    }
    if(frequency_label)
    {
        frequency_label->setText(QString::number(effect_frequency));
    }

    if(detail_slider)
    {
        QSignalBlocker blocker(detail_slider);
        detail_slider->setValue(effect_detail);
    }
    if(detail_label)
    {
        detail_label->setText(QString::number(effect_detail));
    }

    if(rainbow_mode_check)
    {
        QSignalBlocker blocker(rainbow_mode_check);
        rainbow_mode_check->setChecked(rainbow_mode);
    }

    if(spatial_mapping_combo)
    {
        QSignalBlocker blocker(spatial_mapping_combo);
        for(int i = 0; i < spatial_mapping_combo->count(); i++)
        {
            if(spatial_mapping_combo->itemData(i).toInt() == (int)spatial_mapping_mode)
            {
                spatial_mapping_combo->setCurrentIndex(i);
                break;
            }
        }
    }
    if(compass_layer_spin_combo)
    {
        QSignalBlocker blocker(compass_layer_spin_combo);
        compass_layer_spin_combo->setCurrentIndex(std::clamp(compass_layer_spin_preset, 0, 3));
    }
    SyncSpatialMappingControlVisibility();

    if(voxel_volume_mix_slider)
    {
        QSignalBlocker b(voxel_volume_mix_slider);
        voxel_volume_mix_slider->setValue((int)effect_voxel_volume_mix);
    }
    if(voxel_volume_mix_label)
    {
        voxel_volume_mix_label->setText(QString::number((int)effect_voxel_volume_mix) + "%");
    }
    if(voxel_volume_scale_slider)
    {
        QSignalBlocker b(voxel_volume_scale_slider);
        voxel_volume_scale_slider->setValue(effect_voxel_room_scale_centi);
    }
    if(voxel_volume_scale_label)
    {
        voxel_volume_scale_label->setText(QString::number(effect_voxel_room_scale_centi / 100.0f, 'f', 2));
    }
    if(voxel_volume_heading_slider)
    {
        QSignalBlocker b(voxel_volume_heading_slider);
        voxel_volume_heading_slider->setValue(effect_voxel_heading_offset);
    }
    if(voxel_volume_heading_label)
    {
        voxel_volume_heading_label->setText(QString::number(effect_voxel_heading_offset) + QStringLiteral("°"));
    }
    if(voxel_drive_combo)
    {
        QSignalBlocker b(voxel_drive_combo);
        for(int i = 0; i < voxel_drive_combo->count(); i++)
        {
            if(voxel_drive_combo->itemData(i).toInt() == (int)effect_voxel_drive_mode)
            {
                voxel_drive_combo->setCurrentIndex(i);
                break;
            }
        }
    }
    if(sampler_influence_slider)
    {
        QSignalBlocker b(sampler_influence_slider);
        sampler_influence_slider->setValue(effect_sampler_influence_centi);
    }
    if(sampler_influence_label)
    {
        sampler_influence_label->setText(QString::number(effect_sampler_influence_centi) + QStringLiteral("%"));
    }
    if(sampler_compass_north_slider)
    {
        QSignalBlocker b(sampler_compass_north_slider);
        sampler_compass_north_slider->setValue(effect_sampler_compass_north_offset_deg);
    }
    if(sampler_compass_north_label)
    {
        sampler_compass_north_label->setText(QString::number(effect_sampler_compass_north_offset_deg) +
                                             QStringLiteral("°"));
    }

    if(intensity_slider)
    {
        QSignalBlocker blocker(intensity_slider);
        intensity_slider->setValue(effect_intensity);
    }

    if(sharpness_slider)
    {
        QSignalBlocker blocker(sharpness_slider);
        sharpness_slider->setValue(effect_sharpness);
    }
    if(smoothing_slider)
    {
        QSignalBlocker blocker(smoothing_slider);
        smoothing_slider->setValue((int)effect_smoothing);
    }
    if(smoothing_label)
    {
        smoothing_label->setText(QString::number(effect_smoothing));
    }
    if(sampling_resolution_slider)
    {
        QSignalBlocker blocker(sampling_resolution_slider);
        sampling_resolution_slider->setValue((int)effect_sampling_resolution);
    }
    if(sampling_resolution_label)
    {
        sampling_resolution_label->setText(QString::number(effect_sampling_resolution));
    }
    if(edge_profile_combo)
    {
        QSignalBlocker blocker(edge_profile_combo);
        edge_profile_combo->setCurrentIndex(std::clamp(effect_edge_profile, 0, 4));
    }
    if(edge_thickness_slider)
    {
        QSignalBlocker blocker(edge_thickness_slider);
        edge_thickness_slider->setValue((int)effect_edge_thickness);
    }
    if(edge_thickness_label)
        edge_thickness_label->setText(QString::number(effect_edge_thickness) + "%");
    if(glow_level_slider)
    {
        QSignalBlocker blocker(glow_level_slider);
        glow_level_slider->setValue((int)effect_glow_level);
    }
    if(glow_level_label)
        glow_level_label->setText(QString::number(effect_glow_level) + "%");

    if(size_slider)
    {
        QSignalBlocker blocker(size_slider);
        size_slider->setValue(effect_size);
    }
    if(size_label)
    {
        size_label->setText(QString::number(effect_size));
    }

    if(scale_slider)
    {
        QSignalBlocker blocker(scale_slider);
        scale_slider->setValue(effect_scale);
    }
    if(scale_label)
    {
        scale_label->setText(QString::number(effect_scale));
    }

    if(scale_invert_check)
    {
        QSignalBlocker blocker(scale_invert_check);
        scale_invert_check->setChecked(scale_inverted);
    }
    // Bounds mode is controlled from the global Effect Stack panel.

    if(fps_slider)
    {
        QSignalBlocker blocker(fps_slider);
        fps_slider->setValue(effect_fps);
    }
    if(fps_label)
    {
        fps_label->setText(QString::number(effect_fps));
    }

    if(scale_x_slider)
    {
        QSignalBlocker blocker(scale_x_slider);
        scale_x_slider->setValue((int)effect_scale_x);
    }
    if(scale_x_label)
    {
        scale_x_label->setText(QString::number(effect_scale_x) + "%");
    }
    if(scale_y_slider)
    {
        QSignalBlocker blocker(scale_y_slider);
        scale_y_slider->setValue((int)effect_scale_y);
    }
    if(scale_y_label)
    {
        scale_y_label->setText(QString::number(effect_scale_y) + "%");
    }
    if(scale_z_slider)
    {
        QSignalBlocker blocker(scale_z_slider);
        scale_z_slider->setValue((int)effect_scale_z);
    }
    if(scale_z_label)
    {
        scale_z_label->setText(QString::number(effect_scale_z) + "%");
    }
}

void SpatialEffect3D::SetScaleInverted(bool inverted)
{
    if(scale_inverted == inverted)
    {
        return;
    }

    scale_inverted = inverted;
    if(scale_invert_check)
    {
        QSignalBlocker blocker(scale_invert_check);
        scale_invert_check->setChecked(inverted);
    }
    emit ParametersChanged();
}


