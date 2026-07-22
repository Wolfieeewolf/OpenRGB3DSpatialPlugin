// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftRoomAmbilightEffect3D.h"
#include "../MinecraftGame.h"

REGISTER_EFFECT_3D(MinecraftRoomAmbilightEffect3D);

MinecraftRoomAmbilightEffect3D::MinecraftRoomAmbilightEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChRoomAmbilight, "Room Ambilight", parent)
{
    // Temporal blend washes grass/sky into muddy teal — default off; user can raise if needed.
    effect_smoothing = 0;
    // Sharpness 0 = passthrough. Higher = more contrast.
    effect_sharpness = 0;
}

EffectInfo3D MinecraftRoomAmbilightEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description =
        "Ambilight viewport: room LEDs mirror what you see in-game (line-of-sight), not the whole world. "
        "Place a reference point at eye height and set this effect's 3D origin there. "
        "Keep Output shaping → Smoothing at 0 for accurate colours; raise only if LEDs flicker.";
    return info;
}

void MinecraftRoomAmbilightEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = MinecraftGame::CreateEffectWidget(parent,
                                                   QString::fromUtf8("Room Ambilight"),
                                                   mc_settings_,
                                                   channels_,
                                                   this,
                                                   [this]() { emit ParametersChanged(); });
    AddWidgetToParent(w, parent);
}

RGBColor MinecraftRoomAmbilightEffect3D::CalculateColorGrid(float gx, float gy, float gz, float time,
                                                         const GridContext3D& grid)
{
    (void)time;
    const Vector3D effect_origin = GetEffectOriginGrid(grid);
    const GameTelemetryBridge::TelemetrySnapshot& t =
        MinecraftGame::PrepareRenderFrame(grid, mc_settings_, channels_, effect_origin.x, effect_origin.y,
                                          effect_origin.z);
    const RGBColor raw = MinecraftGame::RenderColor(t, gx, gy, gz, effect_origin.x, effect_origin.y,
                                                    effect_origin.z, grid, mc_settings_, channels_);

    const unsigned int smoothing = GetSmoothing();
    if(smoothing == 0)
    {
        return raw;
    }

    const int idx = MinecraftGame::GetRenderSampleIndex();
    const int count = MinecraftGame::GetRenderSampleCount();
    if(idx < 0 || count <= 0 || idx >= count)
    {
        return raw;
    }

    if((int)led_smooth_.size() != count)
    {
        led_smooth_.assign((size_t)count, raw);
    }

    led_smooth_[(size_t)idx] = MinecraftGame::BlendLedTemporal(led_smooth_[(size_t)idx], raw, smoothing);
    return led_smooth_[(size_t)idx];
}
