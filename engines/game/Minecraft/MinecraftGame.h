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
int GetRenderSampleIndex();
int GetRenderSampleCount();
/** Frame-to-frame LED blend using Output shaping → Smoothing (0 = off). */
RGBColor BlendLedTemporal(RGBColor previous, RGBColor current, unsigned int smoothing_pct);
void WireChildWidgetsToParametersChanged(QWidget* root, const std::function<void()>& on_changed);
QWidget* CreateSettingsWidget(QWidget* parent,
                              Settings& settings,
                              std::uint32_t channels);
QWidget* CreateEffectWidget(QWidget* parent,
                            const QString& title,
                            Settings& settings,
                            std::uint32_t channels,
                            QWidget* telemetry_owner,
                            const std::function<void()>& on_changed);
/** One telemetry copy per render frame (not per LED). Publishes room ambilight config once when active. */
const GameTelemetryBridge::TelemetrySnapshot& PrepareRenderFrame(const GridContext3D& grid,
                                                                 const Settings& settings,
                                                                 std::uint32_t channels,
                                                                 float origin_x,
                                                                 float origin_y,
                                                                 float origin_z);
RGBColor RenderColor(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& settings,
                     std::uint32_t channels);
}

#endif
