// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMCOLORWHEELEFFECT3D_H
#define ROOMCOLORWHEELEFFECT3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "SpatialRoom/SpatialRoomDefaults.h"

class RoomColorWheelEffect3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit RoomColorWheelEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomColorWheel", "Color wheel (room)", "Spatial · Mapped",
                          []() { return new RoomColorWheelEffect3D; });

    SpatialRoom::SpatialRoomMode GetSpatialRoomMode() const override
    {
        return SpatialRoom::SpatialRoomMode::RoomMappedPattern;
    }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int direction = 0;
    /** 0 radial, 1 shear, 2 room volume gradient (not origin-based). */
    int hue_geometry_mode = 2;
    float room_edge_fade_pct = 35.0f;
};

#endif
