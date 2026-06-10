// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMCAMPFIREEFFECT3D_H
#define ROOMCAMPFIREEFFECT3D_H

#include "RoomSpatialLightingEffect3D.h"
#include "RoomCampfireParams.h"
#include "EffectRegisterer3D.h"
#include "SpatialLighting/SpatialLightingEngine.h"

class RoomCampfireEffect3D : public RoomSpatialLightingEffect3D
{
    Q_OBJECT

public:
    explicit RoomCampfireEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomCampfire", "Room campfire", "Spatial · Lighting",
                          []() { return new RoomCampfireEffect3D; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private:
    struct FireAppearance
    {
        RGBColor source_color = 0;
        float brightness = 1.0f;
        float spark_add = 0.0f;
    };

    void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const override;
    float RoomLightEmissiveMul() const override;
    float RoomLightDirectMul() const override;
    void SyncCampfireSettingsPanel();
    float FlameHeightUnits(const GridContext3D& grid) const;
    float FlameBaseRadiusUnits(const GridContext3D& grid) const;
    float ComputeFlameTongueMask(float x,
                                 float y,
                                 float z,
                                 float time,
                                 const SpatialLighting::Vec3& fire_pos,
                                 const GridContext3D& grid) const;
    FireAppearance SampleFireAppearance(float x,
                                        float y,
                                        float z,
                                        float time,
                                        const SpatialLighting::Vec3& fire_pos,
                                        const GridContext3D& grid) const;
    RGBColor ApplySparkTint(RGBColor base, float spark_add) const;

    RoomCampfireParams campfire_{};
};

#endif
