// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftSubEffect3D.h"
#include "MinecraftGame.h"
#include "Game/GameTelemetryBridge.h"

MinecraftSubEffect3D::MinecraftSubEffect3D(std::uint32_t channels, const char* effect_title, QWidget* parent)
    : SpatialEffect3D(parent),
      channels_(channels),
      effect_title_(effect_title)
{
    effect_intensity = 85;
    effect_sharpness = 100;
}

EffectInfo3D MinecraftSubEffect3D::BaseMinecraftEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = effect_title_;
    info.effect_description = "";
    info.category = "Game";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 8.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_axis_control = false;
    info.show_color_controls = false;
    info.show_surface_control = false;
    return info;
}

void MinecraftSubEffect3D::ApplyControlVisibility()
{
    SpatialEffect3D::ApplyControlVisibility();
    MinecraftGame::ApplyFabricGameEffectChrome(this);
}

void MinecraftSubEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = MinecraftGame::CreateEffectWidget(parent,
                                                   QString::fromUtf8(effect_title_),
                                                   mc_settings_,
                                                   channels_,
                                                   this,
                                                   [this]() { emit ParametersChanged(); });
    AddWidgetToParent(w, parent);
    AttachRoomMappingPanel(parent);
}

void MinecraftSubEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

nlohmann::json MinecraftSubEffect3D::SaveMinecraftJson() const
{
    nlohmann::json mc;
    MinecraftGame::SettingsToJson(mc_settings_, mc);
    return mc;
}

void MinecraftSubEffect3D::LoadMinecraftJson(const nlohmann::json& settings)
{
    if(settings.is_object())
    {
        MinecraftGame::SettingsFromJson(settings, mc_settings_);
    }
}

nlohmann::json MinecraftSubEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["minecraft"] = SaveMinecraftJson();
    return j;
}

void MinecraftSubEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("minecraft") && settings["minecraft"].is_object())
    {
        LoadMinecraftJson(settings["minecraft"]);
    }
}

RGBColor MinecraftSubEffect3D::CalculateColorGrid(float gx, float gy, float gz, float time, const GridContext3D& grid)
{
    const GameTelemetryBridge::TelemetrySnapshot t = GameTelemetryBridge::GetTelemetrySnapshot();
    const Vector3D effect_origin = GetEffectOriginGrid(grid);
    RGBColor c = MinecraftGame::RenderColor(t, gx, gy, gz, effect_origin.x, effect_origin.y, effect_origin.z, grid,
                                            mc_settings_, channels_);
    if((channels_ & MinecraftGame::ChWorldTint) != 0u || (channels_ & MinecraftGame::ChRoomVrTint) != 0u)
    {
        return c;
    }
    return RemapSaturatedRgbWithRoomMapping(c, gx, gy, gz, time, grid);
}

bool MinecraftSubEffect3D::IsPointOnActiveSurface(float, float, float, const GridContext3D&) const
{
    return true;
}
