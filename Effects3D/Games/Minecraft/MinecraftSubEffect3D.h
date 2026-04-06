// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTSUBEFFECT3D_H
#define MINECRAFTSUBEFFECT3D_H

#include "SpatialEffect3D.h"
#include "MinecraftGameSettings.h"

class MinecraftSubEffect3D : public SpatialEffect3D
{
protected:
    explicit MinecraftSubEffect3D(std::uint32_t channels,
                                  const char* effect_title,
                                  const char* effect_description,
                                  QWidget* parent = nullptr);

    EffectInfo3D BaseMinecraftEffectInfo() const;

    std::uint32_t channels_ = 0;
    const char* effect_title_ = "";
    const char* effect_description_ = "";
    MinecraftGame::Settings mc_settings_{};
    MinecraftGame::WorldTintSmoothState world_smooth_{};

    nlohmann::json SaveMinecraftJson() const;
    void LoadMinecraftJson(const nlohmann::json& settings);

public:
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool IsPointOnActiveSurface(float x, float y, float z, const GridContext3D& grid) const override;
    void ApplyControlVisibility() override;
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
};

#endif
