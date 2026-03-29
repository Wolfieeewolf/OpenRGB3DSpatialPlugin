// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAMEEFFECT3D_H
#define MINECRAFTGAMEEFFECT3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class GameTelemetryStatusPanel;

/** Minecraft (Fabric) telemetry-driven layer; add to stack and start like any other effect. */
class MinecraftGameEffect3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftGameEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("MinecraftGame", "Minecraft (Fabric)", "Game", []() { return new MinecraftGameEffect3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    void ApplyControlVisibility() override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    GameTelemetryStatusPanel* telemetry_status_panel = nullptr;
};

#endif
