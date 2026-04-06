// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAME_H
#define MINECRAFTGAME_H

#include "Game/GameTelemetryBridge.h"
#include "MinecraftGameSettings.h"
#include "SpatialEffect3D.h"

class QWidget;

namespace MinecraftGame
{
QWidget* CreateSettingsWidget(QWidget* parent, Settings& settings, std::uint32_t channels);
RGBColor RenderColor(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                     float time,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& settings,
                     std::uint32_t channels,
                     WorldTintSmoothState* world_tint_smooth);
} // namespace MinecraftGame

#endif
