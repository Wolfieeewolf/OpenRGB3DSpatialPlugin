// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialEffect3D.h"

#include "Colors.h"
#include "Effects3D/SpatialLighting/RoomSpatialLightingUi.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "SpatialLighting/EmitterRelayMirror.h"
#include "ui/widgets/EffectRoomOutputPanel.h"
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

RGBColor SpatialEffect3D::ApplyLayerRoomAmbientShading(float room_x,
                                                       float room_y,
                                                       float room_z,
                                                       RGBColor color,
                                                       const GridContext3D& grid,
                                                       int shade_slot) const
{
    if(color == 0x00000000)
    {
        return color;
    }
    if(effect_room_output_role_ == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
    {
        return color;
    }
    if(!effect_room_relay_params_.use_occlusion)
    {
        return color;
    }

    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    const std::vector<SpatialLighting::OccluderQuad>& occluders = provider->frameOccluderQuads();
    const std::vector<SpatialLighting::OccluderAabb>& occluder_aabbs = provider->frameOccluderAabbs();
    const SpatialLighting::RoomBlockerField& room_blocker_field = provider->frameRoomBlockerField();
    if(occluder_aabbs.empty() && occluders.empty() && !room_blocker_field.IsValid())
    {
        return color;
    }

    const float ao_strength = effect_room_relay_params_.ao_strength / 100.0f;
    const float reach_u = MMToGridUnits(effect_room_relay_params_.light_reach_mm, grid.grid_scale_mm);
    const float room_diag =
        std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    const float probe_span = std::clamp(std::max(reach_u * 0.4f, room_diag * 0.05f), 0.5f, 18.0f);

    const float shade_factor = provider->ComputeAmbientShadeFactorCached(shade_slot,
                                                                       room_x,
                                                                       room_y,
                                                                       room_z,
                                                                       grid.center_x,
                                                                       grid.center_y,
                                                                       grid.center_z,
                                                                       ao_strength,
                                                                       probe_span);
    if(shade_factor >= 0.999f)
    {
        return color;
    }

    const float rf = static_cast<float>(color & 0xFF) * shade_factor;
    const float gf = static_cast<float>((color >> 8) & 0xFF) * shade_factor;
    const float bf = static_cast<float>((color >> 16) & 0xFF) * shade_factor;
    return ToRGBColor(static_cast<uint8_t>(std::clamp(rf, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(gf, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(bf, 0.0f, 255.0f)));
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

    j["room_output_role"] = (int)effect_room_output_role_;
    RoomSpatialLightingUi::SaveParamsToJson(j, "room_relay_light", effect_room_relay_params_);
    j["room_emitter_controllers"] = effect_emitter_controller_indices_;
    j["room_receiver_controllers"] = effect_receiver_controller_indices_;

    if(GetEffectInfo().supports_height_bands)
    {
        SaveEffectStratumSettings(j);
    }

    if(GetEffectInfo().supports_strip_colormap)
    {
        SaveEffectStripColormapSettings(j);
    }

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
        effect_room_output_role_ =
            (raw_role == (int)SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
                ? SpatialRoom::SpatialRoomOutputRole::EmitterRelay
                : SpatialRoom::SpatialRoomOutputRole::Direct;
    }
    RoomSpatialLightingUi::LoadParamsFromJson(settings, "room_relay_light", effect_room_relay_params_);
    if(room_ao_slider)
    {
        const int ao_pct = std::clamp(static_cast<int>(effect_room_relay_params_.ao_strength), 0, 100);
        room_ao_slider->setValue(ao_pct);
        if(room_ao_label)
        {
            room_ao_label->setText(QString::number(ao_pct) + QStringLiteral("%"));
        }
    }
    if(room_blockers_check)
    {
        room_blockers_check->setChecked(effect_room_relay_params_.use_occlusion);
    }
    if(room_walls_blockers_check)
    {
        room_walls_blockers_check->setChecked(effect_room_relay_params_.use_room_walls);
    }
    UpdateRoomShadingControlVisibility();
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

    if(GetEffectInfo().supports_height_bands)
    {
        LoadEffectStratumSettings(settings);
    }

    if(GetEffectInfo().supports_strip_colormap)
    {
        LoadEffectStripColormapSettings(settings);
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
