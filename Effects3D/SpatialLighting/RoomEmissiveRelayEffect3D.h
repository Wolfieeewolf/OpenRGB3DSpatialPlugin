// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMEMISSIVERELAYEFFECT3D_H
#define ROOMEMISSIVERELAYEFFECT3D_H

#include "RoomSpatialLightingEffect3D.h"
#include "EffectRegisterer3D.h"

#include <vector>

class RoomEmissiveRelayEffect3D : public RoomSpatialLightingEffect3D
{
    Q_OBJECT

public:
    explicit RoomEmissiveRelayEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomEmissiveRelay", "Room emissive relay", "Spatial · Relay",
                          []() { return new RoomEmissiveRelayEffect3D; });

    SpatialRoom::SpatialRoomMode GetSpatialRoomMode() const override
    {
        return SpatialRoom::SpatialRoomMode::EmissiveRelay;
    }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    const std::vector<int>& emitterControllerIndices() const { return emitter_controller_indices_; }
    const RoomSpatialLightingUi::RoomSpatialLightParams& lightingParams() const { return room_light_; }
    void setEmitterControllerIndex(int index, bool enabled);
    bool isEmitterControllerIndex(int index) const;

protected:
    float RoomLightEmissiveMul() const override { return 0.55f; }
    float RoomLightDirectMul() const override { return 1.0f; }
    void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const override;

private:
    void InvalidateRelayScene() { InvalidateLightingScene(); }
    RGBColor ShadeRelayAt(float x, float y, float z, const GridContext3D& grid) const;
    void RefreshRelayOccluders(const GridContext3D& grid) const;

    std::vector<int> emitter_controller_indices_;
    mutable bool relay_scene_occluders_valid_ = false;
};

#endif
