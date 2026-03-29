// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAME_H
#define MINECRAFTGAME_H

#include "Game/GameTelemetryBridge.h"
#include "SpatialEffect3D.h"

class QWidget;

/** Minecraft (Fabric) visuals and settings; used by MinecraftGameEffect3D. */
namespace MinecraftGame
{
QWidget* CreateSettingsWidget(QWidget* parent);
RGBColor RenderColor(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                     float time,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid);
}

#endif
