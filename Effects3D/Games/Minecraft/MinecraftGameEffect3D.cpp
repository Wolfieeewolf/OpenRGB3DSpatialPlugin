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
    info.effect_name = "Minecraft (Fabric)";
    info.effect_description = "Fabric mod telemetry (UDP 127.0.0.1:9876).";
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

void MinecraftGameEffect3D::ApplyControlVisibility()
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

void MinecraftGameEffect3D::SetupCustomUI(QWidget* parent)
{
    QGroupBox* w = new QGroupBox("Minecraft");
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);

    QWidget* settings = MinecraftGame::CreateSettingsWidget(w);
    if(settings)
    {
        layout->addWidget(settings);
    }

    telemetry_status_panel = new GameTelemetryStatusPanel(this);
    layout->addWidget(telemetry_status_panel);

    AddWidgetToParent(w, parent);
}

void MinecraftGameEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

nlohmann::json MinecraftGameEffect3D::SaveSettings() const
{
    return SpatialEffect3D::SaveSettings();
}

void MinecraftGameEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
}

RGBColor MinecraftGameEffect3D::CalculateColorGrid(float gx, float gy, float gz, float time, const GridContext3D& grid)
{
    const GameTelemetryBridge::TelemetrySnapshot t = GameTelemetryBridge::GetTelemetrySnapshot();
    const Vector3D effect_origin = GetEffectOriginGrid(grid);
    return MinecraftGame::RenderColor(t, time, gx, gy, gz, effect_origin.x, effect_origin.y, effect_origin.z, grid);
}
