// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftSubEffect3D.h"
#include "MinecraftGame.h"
#include "Game/GameTelemetryStatusPanel.h"
#include "Game/GameTelemetryBridge.h"
#include "EffectRegisterer3D.h"

#include <QVBoxLayout>
#include <QGroupBox>

MinecraftSubEffect3D::MinecraftSubEffect3D(std::uint32_t channels, const char* effect_title, QWidget* parent)
    : SpatialEffect3D(parent),
      channels_(channels),
      effect_title_(effect_title)
{
    // The Minecraft channel blend already carries its own gain controls.
    // Keep generic post-process intensity neutral-to-slightly-soft by default.
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
    QGroupBox* w = new QGroupBox(QString::fromUtf8(effect_title_));
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);

    QWidget* settings = MinecraftGame::CreateSettingsWidget(w, mc_settings_, channels_);
    if(settings)
    {
        layout->addWidget(settings);
        MinecraftGame::WireChildWidgetsToParametersChanged(settings, [this]() { emit ParametersChanged(); });
    }

    auto* telemetry_status_panel = new GameTelemetryStatusPanel(this);
    layout->addWidget(telemetry_status_panel);

    AddWidgetToParent(w, parent);
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
    MinecraftGame::WorldTintSmoothState* wp = nullptr;
    if((channels_ & MinecraftGame::ChWorldTint) != 0u)
    {
        wp = &world_smooth_;
    }
    return MinecraftGame::RenderColor(t, time, gx, gy, gz, effect_origin.x, effect_origin.y, effect_origin.z, grid, mc_settings_, channels_, wp);
}

bool MinecraftSubEffect3D::IsPointOnActiveSurface(float, float, float, const GridContext3D&) const
{
    return true;
}

namespace
{
using namespace MinecraftGame;

class MinecraftHealthEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftHealthEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChHealth, "Minecraft - Health", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Health from Fabric UDP (127.0.0.1:9876): gradient or per-heart strip. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftHealth", "Minecraft - Health", "Game", []() { return new MinecraftHealthEffect3D; })
};

class MinecraftHungerEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftHungerEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChHunger, "Minecraft - Hunger", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Hunger gradient from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftHunger", "Minecraft - Hunger", "Game", []() { return new MinecraftHungerEffect3D; })
};

class MinecraftAirEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftAirEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChAir, "Minecraft - Air", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Air/breathing gradient from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftAir", "Minecraft - Air", "Game", []() { return new MinecraftAirEffect3D; })
};

class MinecraftDurabilityEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftDurabilityEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChDurability, "Minecraft - Durability", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Main-hand item durability from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftDurability", "Minecraft - Durability", "Game", []() { return new MinecraftDurabilityEffect3D; })
};

class MinecraftDamageEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftDamageEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChDamage, "Minecraft - Damage", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Directional damage flash from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftDamage", "Minecraft - Damage", "Game", []() { return new MinecraftDamageEffect3D; })
};

class MinecraftWorldTintEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftWorldTintEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChWorldTint, "Minecraft - World tint", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Sky/mid/ground ambient tint from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftWorldTint", "Minecraft - World tint", "Game", []() { return new MinecraftWorldTintEffect3D; })
};

class MinecraftLightningEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftLightningEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChLightning, "Minecraft - Lightning", parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_description = "Lightning flash (sky-heavy) from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftLightning", "Minecraft - Lightning", "Game", []() { return new MinecraftLightningEffect3D; })
};

REGISTER_EFFECT_3D(MinecraftHealthEffect3D);
REGISTER_EFFECT_3D(MinecraftHungerEffect3D);
REGISTER_EFFECT_3D(MinecraftAirEffect3D);
REGISTER_EFFECT_3D(MinecraftDurabilityEffect3D);
REGISTER_EFFECT_3D(MinecraftDamageEffect3D);
REGISTER_EFFECT_3D(MinecraftWorldTintEffect3D);
REGISTER_EFFECT_3D(MinecraftLightningEffect3D);
}
