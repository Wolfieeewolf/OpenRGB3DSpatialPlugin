// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAME_H
#define MINECRAFTGAME_H

#include "Game/GameTelemetryBridge.h"
#include "MinecraftGameSettings.h"

#include <functional>

class QWidget;
class QString;
struct GridContext3D;

namespace MinecraftGame
{
void SetRenderSampleIndexContext(int led_index, int led_count);
void ClearRenderSampleIndexContext();
void WireChildWidgetsToParametersChanged(QWidget* root, const std::function<void()>& on_changed);
QWidget* CreateSettingsWidget(QWidget* parent, Settings& settings, std::uint32_t channels);
QWidget* CreateEffectWidget(QWidget* parent,
                            const QString& title,
                            Settings& settings,
                            std::uint32_t channels,
                            QWidget* telemetry_owner,
                            const std::function<void()>& on_changed);
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
}

#endif
