// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMCAMPFIREEFFECT3D_H
#define ROOMCAMPFIREEFFECT3D_H

#include "RoomSpatialLightingEffect3D.h"
#include "EffectRegisterer3D.h"
#include "SpatialLighting/SpatialLightingEngine.h"

#include <vector>

class QComboBox;

class RoomCampfireEffect3D : public RoomSpatialLightingEffect3D
{
    Q_OBJECT

public:
    explicit RoomCampfireEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomCampfire", "Room campfire / blob", "Spatial · Lighting",
                          []() { return new RoomCampfireEffect3D; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private:
    void RebuildScene(const GridContext3D& grid);
    void RefreshOccluders(const GridContext3D& grid) const;
    void UpdateSourceFromSliders(const GridContext3D& grid) const;
    void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const override;
    SpatialLighting::Vec3 PlacementPosition(const GridContext3D& grid) const;

    int placement_mode_ = 0;
    float custom_u_ = 0.15f;
    float custom_v_ = 0.15f;
    float custom_w_ = 0.12f;
    bool use_occlusion_ = true;
    bool use_room_walls_ = false;
    bool use_controller_occlusion_ = true;
    float ao_strength_ = 65.0f;
    float glow_radius_mm_ = 45.0f;
    float light_reach_mm_ = 280.0f;
    float room_fill_ = 35.0f;

    SpatialLighting::Vec3 frozen_light_pos_{};
    bool frozen_light_valid_ = false;

    mutable SpatialLighting::RoomScene cached_scene_{};
    mutable std::vector<SpatialLighting::OccluderQuad> occluders_{};
    mutable std::vector<SpatialLighting::OccluderAabb> occluder_aabbs_{};
};

#endif
