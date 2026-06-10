// SPDX-License-Identifier: GPL-2.0-only

#include "RoomWashLightEffect3D.h"

#include "RoomSpatialLightingUi.h"
#include "RoomWashLightSettingsPanel.h"

#include <vector>

REGISTER_EFFECT_3D(RoomWashLightEffect3D);

RoomWashLightEffect3D::RoomWashLightEffect3D(QWidget* parent) : RoomSpatialLightingEffect3D(parent)
{
    SetReferenceMode(REF_MODE_USER_POSITION);
    SetRainbowMode(false);

    room_light_.placement_mode = 1;
    room_light_.glow_radius_mm = 80.0f;
    room_light_.light_reach_mm = 900.0f;
    room_light_.room_fill = 58.0f;
    room_light_.ao_strength = 45.0f;
    room_light_.use_room_walls = false;

    std::vector<RGBColor> wash_palette;
    wash_palette.push_back(ToRGBColor(255, 240, 232));
    SetColors(wash_palette);
}

void RoomWashLightEffect3D::ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const
{
    RoomSpatialLightingEffect3D::ApplyLiveShadeSettings(scene);
    scene.shade.ambient_level = 0.06f;
    scene.shade.direct_falloff = 0.65f;
    const float bright = std::max(0.2f, effect_brightness / 100.0f);
    cached_scene_.source.emissive_strength = 0.35f * bright;
    cached_scene_.source.light_strength = 0.55f * bright;
}

EffectInfo3D RoomWashLightEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Room wash light";
    info.effect_description =
        "Soft, even fill light across the room. "
        "High Room fill; not a campfire — use Room campfire for localized flames.";
    info.category = "Spatial · Lighting";
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.show_axis_control = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_position_offset_control = false;
    info.show_frequency_control = false;
    info.show_color_controls = true;
    info.user_colors = 1;
    info.supports_strip_colormap = false;
    info.supports_height_bands = false;
    info.show_room_output_control = false;
    return info;
}

void RoomWashLightEffect3D::SetupCustomUI(QWidget* parent)
{
    auto* panel = new RoomWashLightSettingsPanel();
    panel->setObjectName(QStringLiteral("RoomWashLightSettings"));
    const auto on_placement_changed = [this]() {
        MarkRoomLightPlacementDirty();
        emit ParametersChanged();
    };
    const auto on_tune_changed = [this]() {
        InvalidateLightingScene();
        emit ParametersChanged();
    };
    panel->bind(this, room_light_, on_placement_changed, on_tune_changed);
    AddWidgetToParent(panel, parent);
}

void RoomWashLightEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

RGBColor RoomWashLightEffect3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)time;
    return ShadeRoomLightAt(x, y, z, grid, GetColorAtPosition(0.5f));
}

nlohmann::json RoomWashLightEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    RoomSpatialLightingUi::SaveParamsToJson(j, "room_wash_light", room_light_);
    return j;
}

void RoomWashLightEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    RoomSpatialLightingUi::LoadParamsFromJson(settings, "room_wash_light", "room_light_probe", room_light_);
    MarkRoomLightPlacementDirty();
    if(auto* panel = findChild<RoomWashLightSettingsPanel*>(QStringLiteral("RoomWashLightSettings")))
    {
        panel->syncFromState(this, room_light_);
    }
}
