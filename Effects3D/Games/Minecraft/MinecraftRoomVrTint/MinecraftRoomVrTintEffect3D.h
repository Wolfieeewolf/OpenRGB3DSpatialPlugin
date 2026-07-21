// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTROOMVRTINTEFFECT3D_H
#define MINECRAFTROOMVRTINTEFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

#include <vector>

class MinecraftRoomVrTintEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftRoomVrTintEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftRoomVrTint", "Room tint (VR)", "minecraft", "Minecraft",
                              []() { return new MinecraftRoomVrTintEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private:
    /** Per-LED EMA state for Output shaping → Smoothing. */
    std::vector<RGBColor> led_smooth_;
};

#endif
