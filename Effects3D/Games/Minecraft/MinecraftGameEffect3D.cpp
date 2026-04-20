// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGameEffect3D.h"
#include "MinecraftGame.h"
#include "Game/GameTelemetryStatusPanel.h"
#include "Game/GameTelemetryBridge.h"

#include <QVBoxLayout>
#include <QGroupBox>

REGISTER_EFFECT_3D(MinecraftGameEffect3D);

MinecraftGameEffect3D::MinecraftGameEffect3D(QWidget* parent) : SpatialEffect3D(parent)
{
}

EffectInfo3D MinecraftGameEffect3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Minecraft (Fabric, all)";
    info.effect_description = "All Minecraft channels in one layer. For per-strip control use Minecraft - Health / Hunger / ... (Game). UDP 127.0.0.1:9876.";
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

void MinecraftGameEffect3D::ApplyControlVisibility()
{
    SpatialEffect3D::ApplyControlVisibility();
    MinecraftGame::ApplyFabricGameEffectChrome(this);
}

void MinecraftGameEffect3D::SetupCustomUI(QWidget* parent)
{
    QGroupBox* w = new QGroupBox("Minecraft");
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);

    QWidget* settings = MinecraftGame::CreateSettingsWidget(w, mc_settings_, MinecraftGame::ChAll);
    if(settings)
    {
        layout->addWidget(settings);
        MinecraftGame::WireChildWidgetsToParametersChanged(settings, [this]() { emit ParametersChanged(); });
    }

    layout->addWidget(new GameTelemetryStatusPanel(this));

    AddWidgetToParent(w, parent);
}

void MinecraftGameEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

nlohmann::json MinecraftGameEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    nlohmann::json mc;
    MinecraftGame::SettingsToJson(mc_settings_, mc);
    j["minecraft"] = mc;
    return j;
}

void MinecraftGameEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("minecraft") && settings["minecraft"].is_object())
    {
        MinecraftGame::SettingsFromJson(settings["minecraft"], mc_settings_);
    }
}

RGBColor MinecraftGameEffect3D::CalculateColorGrid(float gx, float gy, float gz, float time, const GridContext3D& grid)
{
    const GameTelemetryBridge::TelemetrySnapshot t = GameTelemetryBridge::GetTelemetrySnapshot();
    const Vector3D effect_origin = GetEffectOriginGrid(grid);
    return MinecraftGame::RenderColor(t, time, gx, gy, gz, effect_origin.x, effect_origin.y, effect_origin.z, grid,
                                      mc_settings_, MinecraftGame::ChAll, &world_smooth_);
}

bool MinecraftGameEffect3D::IsPointOnActiveSurface(float, float, float, const GridContext3D&) const
{
    return true;
}
