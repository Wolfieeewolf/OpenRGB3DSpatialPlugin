// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialEffect3D.h"

#include "Colors.h"
#include "RoomSpatialLightingUi.h"
#include "PluginLog.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "SpatialLighting/EmitterRelayMirror.h"
#include "ui/widgets/EffectRoomOutputPanel.h"
#include "ui/widgets/EffectSliderPromote.h"
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <cstdint>

void SpatialEffect3D::setRoomEmitterControllerIndex(int index, bool enabled)
{
    auto it = std::find(effect_emitter_controller_indices_.begin(), effect_emitter_controller_indices_.end(), index);
    if(enabled)
    {
        if(it == effect_emitter_controller_indices_.end())
        {
            effect_emitter_controller_indices_.push_back(index);
        }
        auto recv_it = std::find(effect_receiver_controller_indices_.begin(),
                                 effect_receiver_controller_indices_.end(),
                                 index);
        if(recv_it != effect_receiver_controller_indices_.end())
        {
            effect_receiver_controller_indices_.erase(recv_it);
        }
    }
    else if(it != effect_emitter_controller_indices_.end())
    {
        effect_emitter_controller_indices_.erase(it);
    }
    InvalidateRelayShadeCache();
}

void SpatialEffect3D::setRoomReceiverControllerIndex(int index, bool enabled)
{
    if(enabled && isRoomEmitterController(index))
    {
        return;
    }

    auto it = std::find(effect_receiver_controller_indices_.begin(), effect_receiver_controller_indices_.end(), index);
    if(enabled)
    {
        if(it == effect_receiver_controller_indices_.end())
        {
            effect_receiver_controller_indices_.push_back(index);
        }
        auto emit_it = std::find(effect_emitter_controller_indices_.begin(),
                                 effect_emitter_controller_indices_.end(),
                                 index);
        if(emit_it != effect_emitter_controller_indices_.end())
        {
            effect_emitter_controller_indices_.erase(emit_it);
        }
    }
    else if(it != effect_receiver_controller_indices_.end())
    {
        effect_receiver_controller_indices_.erase(it);
    }
    InvalidateRelayShadeCache();
}

bool SpatialEffect3D::isRoomEmitterController(int index) const
{
    return std::find(effect_emitter_controller_indices_.begin(),
                     effect_emitter_controller_indices_.end(),
                     index) != effect_emitter_controller_indices_.end();
}

bool SpatialEffect3D::isRoomReceiverController(int index) const
{
    if(!effect_receiver_controller_indices_.empty())
    {
        return std::find(effect_receiver_controller_indices_.begin(),
                         effect_receiver_controller_indices_.end(),
                         index) != effect_receiver_controller_indices_.end();
    }
    return !isRoomEmitterController(index);
}

bool SpatialEffect3D::appliesRoomOutputToController(int controller_index) const
{
    switch(effect_room_output_role_)
    {
    case SpatialRoom::SpatialRoomOutputRole::EmitterRelay:
        return isRoomEmitterController(controller_index) || isRoomReceiverController(controller_index);
    default:
        return true;
    }
}

RGBColor SpatialEffect3D::SampleRelayShadeAt(float x, float y, float z, const GridContext3D& grid) const
{
    return ShadeRelayReceiversAt(x, y, z, grid);
}

RGBColor SpatialEffect3D::ApplyLayerRoomAmbientShading(float /*room_x*/,
                                                       float /*room_y*/,
                                                       float /*room_z*/,
                                                       RGBColor color,
                                                       const GridContext3D& /*grid*/,
                                                       int /*shade_slot*/) const
{
    /* Direct patterns sample the grid only. Housing blockers are for emitter + relay. */
    return color;
}

SpatialRoom::SpatialRoomMode SpatialEffect3D::GetSpatialRoomMode() const
{
    if(effect_room_output_role_ == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
    {
        return SpatialRoom::SpatialRoomMode::EmissiveRelay;
    }
    if(effect_room_coordinate_mode_ == SpatialRoom::SpatialRoomCoordinateMode::RoomMapped)
    {
        return SpatialRoom::SpatialRoomMode::RoomMappedPattern;
    }
    return SpatialRoom::SpatialRoomMode::OriginField;
}

void SpatialEffect3D::RefreshRoomOutputControllerLists()
{
    if(room_output_panel_)
    {
        room_output_panel_->refreshControllerLists();
    }
}

void SpatialEffect3D::InvalidateRelayShadeCache()
{
    SpatialLightingSceneProvider::instance()->InvalidateFrameOccluders();
}

void SpatialEffect3D::ApplyRelayShadeSettings(const GridContext3D& grid) const
{
    relay_shade_.ao_strength = effect_room_relay_params_.ao_strength / 100.0f;
    relay_shade_.use_occlusion = effect_room_relay_params_.use_occlusion;
    relay_shade_.use_ambient_occlusion =
        effect_room_relay_params_.use_occlusion && effect_room_relay_params_.ao_strength > 0.01f;
    const float reach_u = MMToGridUnits(effect_room_relay_params_.light_reach_mm, grid.grid_scale_mm);
    const float room_diag =
        std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    relay_shade_.ao_probe_span = std::clamp(std::max(reach_u * 0.4f, room_diag * 0.05f), 0.5f, 18.0f);
}

RGBColor SpatialEffect3D::ShadeRelayReceiversAt(float x, float y, float z, const GridContext3D& grid) const
{
    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    ApplyRelayShadeSettings(grid);

    if(provider->emitterRelayMirrorActive())
    {
        EmitterRelayMirror::MirrorShadeContext shade_ctx{};
        shade_ctx.shade = &relay_shade_;
        shade_ctx.occluder_aabbs = &provider->frameOccluderAabbs();
        shade_ctx.occluders = &provider->frameOccluderQuads();
        return EmitterRelayMirror::SampleReceiver(provider->emitterRelayMirrorFrame(), x, y, z, &shade_ctx);
    }
    return 0x00000000;
}

void SpatialEffect3D::SetSpatialMappingMode(SpatialMappingMode mode)
{
    spatial_mapping_mode = mode;
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
    use_custom_reference = (mode == REF_MODE_CUSTOM_POINT);
    SyncRoomCoordinateModeFromReference();
}

void SpatialEffect3D::SyncRoomCoordinateModeFromReference()
{
    effect_room_coordinate_mode_ =
        (reference_mode == REF_MODE_ROOM_CENTER) ? SpatialRoom::SpatialRoomCoordinateMode::RoomMapped
                                                 : SpatialRoom::SpatialRoomCoordinateMode::EffectOrigin;
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

unsigned int SpatialEffect3D::GetTargetFPS() const
{
    return effect_fps;
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
    j["effect_sampler_influence_centi"] = effect_sampler_influence_centi;
    j["effect_sampler_compass_north_offset_deg"] = effect_sampler_compass_north_offset_deg;
    j["effect_compass_discrete_zones"] = effect_compass_discrete_zones;
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
    j["shear_xy"] = effect_shear_xy;
    j["shear_xz"] = effect_shear_xz;
    j["shear_yz"] = effect_shear_yz;
    j["volume_shape"] = effect_volume_shape;
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
    j["band_thickness"] = effect_band_thickness;
    j["edge_fade"] = effect_edge_fade;
    j["instance_count"] = effect_instance_count;
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

    j["room_output_role"] = (int)effect_room_output_role_;
    RoomSpatialLightingUi::SaveParamsToJson(j, "room_relay_light", effect_room_relay_params_);
    j["room_emitter_controllers"] = effect_emitter_controller_indices_;
    j["room_receiver_controllers"] = effect_receiver_controller_indices_;

    if(GetEffectInfo().supports_height_bands)
    {
        SaveEffectStratumSettings(j);
    }

    SaveEffectStripColormapSettings(j);

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
        SetSpatialMappingMode(static_cast<SpatialMappingMode>(std::clamp(v, 0, 2)));
    }

    if(settings.contains("compass_layer_spin_preset"))
    {
        compass_layer_spin_preset = std::clamp(settings["compass_layer_spin_preset"].get<int>(), 0, 3);
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
    if(settings.contains("effect_compass_discrete_zones"))
    {
        effect_compass_discrete_zones = settings["effect_compass_discrete_zones"].get<bool>();
    }

    if(settings.contains("effect_bounds_mode"))
        effect_bounds_mode = std::clamp(settings["effect_bounds_mode"].get<int>(), (int)BOUNDS_MODE_GLOBAL, (int)BOUNDS_MODE_TARGET_ZONE);
    else
        effect_bounds_mode = (int)BOUNDS_MODE_GLOBAL;

    if(settings.contains("intensity"))
        effect_intensity = settings["intensity"].get<unsigned int>();
    if(settings.contains("sharpness"))
        effect_sharpness = std::clamp(settings["sharpness"].get<unsigned int>(), 0u, 100u);
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
    if(settings.contains("shear_xy"))
        effect_shear_xy = std::clamp(settings["shear_xy"].get<float>(), -2.0f, 2.0f);
    if(settings.contains("shear_xz"))
        effect_shear_xz = std::clamp(settings["shear_xz"].get<float>(), -2.0f, 2.0f);
    if(settings.contains("shear_yz"))
        effect_shear_yz = std::clamp(settings["shear_yz"].get<float>(), -2.0f, 2.0f);
    if(settings.contains("volume_shape"))
        effect_volume_shape = std::clamp(settings["volume_shape"].get<int>(),
                                         (int)VOLUME_SHAPE_BOX, (int)VOLUME_SHAPE_TETRA);
    if(settings.contains("axis_scale_rotation_yaw"))
        effect_axis_scale_rotation_yaw = std::clamp(settings["axis_scale_rotation_yaw"].get<float>(), -180.0f, 180.0f);
    if(settings.contains("axis_scale_rotation_pitch"))
        effect_axis_scale_rotation_pitch = std::clamp(settings["axis_scale_rotation_pitch"].get<float>(), -180.0f, 180.0f);
    if(settings.contains("axis_scale_rotation_roll"))
        effect_axis_scale_rotation_roll = std::clamp(settings["axis_scale_rotation_roll"].get<float>(), -180.0f, 180.0f);

    SyncEffectSliderValue(rotation_yaw_slider, (int)effect_rotation_yaw);
    SyncEffectSliderValue(rotation_pitch_slider, (int)effect_rotation_pitch);
    SyncEffectSliderValue(rotation_roll_slider, (int)effect_rotation_roll);
    SyncEffectSliderValue(axis_scale_rot_yaw_slider, (int)effect_axis_scale_rotation_yaw);
    SyncEffectSliderValue(axis_scale_rot_pitch_slider, (int)effect_axis_scale_rotation_pitch);
    SyncEffectSliderValue(axis_scale_rot_roll_slider, (int)effect_axis_scale_rotation_roll);
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
    {
        SetReferenceMode((ReferenceMode)settings["reference_mode"].get<int>());
    }
    else
    {
        SyncRoomCoordinateModeFromReference();
    }

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

    SyncRoomCoordinateModeFromReference();

    if(settings.contains("room_output_role") && settings["room_output_role"].is_number_integer())
    {
        const int raw_role = settings["room_output_role"].get<int>();
        if(raw_role == (int)SpatialRoom::SpatialRoomOutputRole::Direct
           || raw_role == (int)SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
        {
            effect_room_output_role_ = (SpatialRoom::SpatialRoomOutputRole)raw_role;
        }
        else
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Ignoring unsupported room_output_role %d (need 0 or 1)",
                        raw_role);
            effect_room_output_role_ = SpatialRoom::SpatialRoomOutputRole::Direct;
        }
    }
    RoomSpatialLightingUi::LoadParamsFromJson(settings, "room_relay_light", effect_room_relay_params_);
    effect_emitter_controller_indices_.clear();
    if(settings.contains("room_emitter_controllers") && settings["room_emitter_controllers"].is_array())
    {
        for(const auto& v : settings["room_emitter_controllers"])
        {
            if(v.is_number_integer())
            {
                effect_emitter_controller_indices_.push_back(v.get<int>());
            }
        }
    }
    effect_receiver_controller_indices_.clear();
    if(settings.contains("room_receiver_controllers") && settings["room_receiver_controllers"].is_array())
    {
        for(const auto& v : settings["room_receiver_controllers"])
        {
            if(v.is_number_integer())
            {
                effect_receiver_controller_indices_.push_back(v.get<int>());
            }
        }
    }
    effect_receiver_controller_indices_.erase(
        std::remove_if(effect_receiver_controller_indices_.begin(),
                       effect_receiver_controller_indices_.end(),
                       [this](int idx) { return isRoomEmitterController(idx); }),
        effect_receiver_controller_indices_.end());
    InvalidateRelayShadeCache();
    if(room_output_panel_)
    {
        room_output_panel_->syncFromState(effect_room_output_role_,
                                          effect_room_relay_params_);
    }

    if(settings.contains("path_axis") && settings["path_axis"].is_number_integer())
        effect_path_axis = std::clamp(settings["path_axis"].get<int>(), 0, 2);
    if(settings.contains("band_thickness") && settings["band_thickness"].is_number())
        effect_band_thickness = (unsigned int)std::clamp((int)std::lround(settings["band_thickness"].get<double>()), 0, 100);
    if(settings.contains("edge_fade") && settings["edge_fade"].is_number())
        effect_edge_fade = (unsigned int)std::clamp((int)std::lround(settings["edge_fade"].get<double>()), 0, 100);
    if(settings.contains("instance_count") && settings["instance_count"].is_number())
        effect_instance_count = (unsigned int)std::clamp((int)std::lround(settings["instance_count"].get<double>()), 1, 48);
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
    SyncEffectSliderValue(thickness_slider, (int)effect_band_thickness);
    SyncEffectSliderValue(edge_fade_slider, (int)effect_edge_fade);
    SyncEffectSliderValue(count_slider, (int)effect_instance_count);
    if(plane_combo)
        plane_combo->setCurrentIndex(effect_plane);
    SyncEffectSliderValue(offset_x_slider, effect_offset_x);
    SyncEffectSliderValue(offset_y_slider, effect_offset_y);
    SyncEffectSliderValue(offset_z_slider, effect_offset_z);

    SyncEffectSliderValue(speed_slider, (int)effect_speed);
    SyncEffectSliderValue(brightness_slider, (int)effect_brightness);
    SyncEffectSliderValue(frequency_slider, (int)effect_frequency);
    SyncEffectSliderValue(detail_slider, (int)effect_detail);

    if(rainbow_mode_check)
    {
        QSignalBlocker blocker(rainbow_mode_check);
        rainbow_mode_check->setChecked(rainbow_mode);
    }

    SyncEffectSliderValue(intensity_slider, (int)effect_intensity);
    SyncEffectSliderValue(sharpness_slider, (int)effect_sharpness);
    SyncEffectSliderValue(smoothing_slider, (int)effect_smoothing);
    SyncEffectSliderValue(sampling_resolution_slider, (int)effect_sampling_resolution);
    if(edge_profile_combo)
    {
        QSignalBlocker blocker(edge_profile_combo);
        edge_profile_combo->setCurrentIndex(std::clamp(effect_edge_profile, 0, 4));
    }
    SyncEffectSliderValue(edge_thickness_slider, (int)effect_edge_thickness);
    if(edge_thickness_label)
        edge_thickness_label->setText(QString::number(effect_edge_thickness) + "%");
    SyncEffectSliderValue(glow_level_slider, (int)effect_glow_level);
    if(glow_level_label)
        glow_level_label->setText(QString::number(effect_glow_level) + "%");

    SyncEffectSliderValue(size_slider, (int)effect_size);
    SyncEffectSliderValue(scale_slider, (int)effect_scale);

    if(scale_invert_check)
    {
        QSignalBlocker blocker(scale_invert_check);
        scale_invert_check->setChecked(scale_inverted);
    }

    SyncEffectSliderValue(fps_slider, (int)effect_fps);
    SyncEffectSliderValue(scale_x_slider, (int)effect_scale_x);
    SyncEffectSliderValue(scale_y_slider, (int)effect_scale_y);
    SyncEffectSliderValue(scale_z_slider, (int)effect_scale_z);

    if(GetEffectInfo().supports_height_bands)
    {
        LoadEffectStratumSettings(settings);
    }

    LoadEffectStripColormapSettings(settings);
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
