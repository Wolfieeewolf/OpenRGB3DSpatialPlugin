// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAMEEFFECT3D_H
#define MINECRAFTGAMEEFFECT3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "MinecraftGameSettings.h"

class MinecraftGameEffect3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftGameEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("MinecraftGame", "Minecraft (Fabric, all)", "Game", []() { return new MinecraftGameEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    bool IsPointOnActiveSurface(float x, float y, float z, const GridContext3D& grid) const override;

    void ApplyControlVisibility() override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    MinecraftGame::Settings mc_settings_{};
    MinecraftGame::WorldTintSmoothState world_smooth_{};
};

#endif
