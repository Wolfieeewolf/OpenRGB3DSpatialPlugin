// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftSubEffect3D.h"
#include "MinecraftGame.h"
#include "Game/GameTelemetryStatusPanel.h"
#include "Game/GameTelemetryBridge.h"
#include "EffectRegisterer3D.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>

MinecraftSubEffect3D::MinecraftSubEffect3D(std::uint32_t channels,
                                           const char* effect_title,
                                           const char* effect_description,
                                           QWidget* parent)
    : SpatialEffect3D(parent),
      channels_(channels),
      effect_title_(effect_title),
      effect_description_(effect_description)
{
}

EffectInfo3D MinecraftSubEffect3D::BaseMinecraftEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = effect_title_;
    info.effect_description = effect_description_;
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
    info.show_fps_control = false;
    info.show_axis_control = false;
    info.show_color_controls = false;
    info.show_surface_control = false;
    return info;
}

void MinecraftSubEffect3D::ApplyControlVisibility()
{
    SpatialEffect3D::ApplyControlVisibility();

    SetControlGroupVisibility(speed_slider, speed_label, "Speed:", false);
    SetControlGroupVisibility(frequency_slider, frequency_label, "Frequency:", false);
    SetControlGroupVisibility(detail_slider, detail_label, "Detail:", false);
    SetControlGroupVisibility(size_slider, size_label, "Size:", false);
    SetControlGroupVisibility(scale_slider, scale_label, "Scale:", false);
    SetControlGroupVisibility(fps_slider, fps_label, "FPS:", false);

    SetControlGroupVisibility(brightness_slider, brightness_label, "Brightness:", true);
    SetControlGroupVisibility(intensity_slider, intensity_label, "Intensity:", true);
    SetControlGroupVisibility(sharpness_slider, sharpness_label, "Sharpness:", true);

    if(color_controls_group)
    {
        color_controls_group->setVisible(false);
    }
    if(surfaces_group)
    {
        surfaces_group->setVisible(false);
    }
    if(position_offset_group)
    {
        position_offset_group->setVisible(false);
    }
    if(edge_shape_group)
    {
        edge_shape_group->setVisible(false);
    }
    if(path_plane_group)
    {
        path_plane_group->setVisible(false);
    }

    if(effect_controls_group)
    {
        const QList<QGroupBox*> groups = effect_controls_group->findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
        for(QGroupBox* gb : groups)
        {
            const QString t = gb->title();
            if(t == QStringLiteral("Effect scale (X / Y / Z %)") ||
               t == QStringLiteral("Effect scale rotation (\u00B0)") ||
               t == QStringLiteral("Effect rotation (\u00B0)"))
            {
                gb->setVisible(false);
            }
        }
    }
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

        const QList<QCheckBox*> checks = settings->findChildren<QCheckBox*>();
        for(QCheckBox* cb : checks)
        {
            QObject::connect(cb, &QCheckBox::toggled, this, [this](bool) { emit ParametersChanged(); });
        }

        const QList<QSlider*> sliders = settings->findChildren<QSlider*>();
        for(QSlider* sl : sliders)
        {
            QObject::connect(sl, &QSlider::valueChanged, this, [this](int) { emit ParametersChanged(); });
        }

        const QList<QSpinBox*> spins = settings->findChildren<QSpinBox*>();
        for(QSpinBox* sp : spins)
        {
            QObject::connect(sp, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { emit ParametersChanged(); });
        }

        const QList<QComboBox*> combos = settings->findChildren<QComboBox*>();
        for(QComboBox* combo : combos)
        {
            QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { emit ParametersChanged(); });
        }
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
        : MinecraftSubEffect3D(ChHealth,
                               "Minecraft - Health",
                               "Health from Fabric UDP (127.0.0.1:9876): full-strip gradient or per-heart strip (LEDs/heart, axis). Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - Health";
        info.effect_description = "Health from Fabric UDP (127.0.0.1:9876): gradient or per-heart strip. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftHealth", "Minecraft - Health", "Game", []() { return new MinecraftHealthEffect3D; })
};

class MinecraftHungerEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftHungerEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChHunger,
                               "Minecraft - Hunger",
                               "Hunger gradient from Fabric UDP. Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - Hunger";
        info.effect_description = "Hunger gradient from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftHunger", "Minecraft - Hunger", "Game", []() { return new MinecraftHungerEffect3D; })
};

class MinecraftAirEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftAirEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChAir,
                               "Minecraft - Air",
                               "Air/breathing gradient from Fabric UDP. Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - Air";
        info.effect_description = "Air/breathing gradient from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftAir", "Minecraft - Air", "Game", []() { return new MinecraftAirEffect3D; })
};

class MinecraftDurabilityEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftDurabilityEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChDurability,
                               "Minecraft - Durability",
                               "Main-hand item durability from Fabric UDP. Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - Durability";
        info.effect_description = "Main-hand item durability from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftDurability", "Minecraft - Durability", "Game", []() { return new MinecraftDurabilityEffect3D; })
};

class MinecraftDamageEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftDamageEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChDamage,
                               "Minecraft - Damage",
                               "Directional damage flash from Fabric UDP. Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - Damage";
        info.effect_description = "Directional damage flash from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftDamage", "Minecraft - Damage", "Game", []() { return new MinecraftDamageEffect3D; })
};

class MinecraftWorldTintEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftWorldTintEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChWorldTint,
                               "Minecraft - World tint",
                               "Sky/mid/ground ambient tint from Fabric UDP. Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - World tint";
        info.effect_description = "Sky/mid/ground ambient tint from Fabric UDP. Stack per controller.";
        return info;
    }
    EFFECT_REGISTERER_3D("MinecraftWorldTint", "Minecraft - World tint", "Game", []() { return new MinecraftWorldTintEffect3D; })
};

class MinecraftLightningEffect3D : public MinecraftSubEffect3D
{
public:
    explicit MinecraftLightningEffect3D(QWidget* parent = nullptr)
        : MinecraftSubEffect3D(ChLightning,
                               "Minecraft - Lightning",
                               "Lightning flash (sky-heavy) from Fabric UDP. Stack per controller.",
                               parent)
    {
    }
    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info = BaseMinecraftEffectInfo();
        info.effect_name = "Minecraft - Lightning";
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
} // namespace
