// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTHUNGEREFFECT3D_H
#define MINECRAFTHUNGEREFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftHungerEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftHungerEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftHunger", "Hunger", "minecraft", "Minecraft",
                              []() { return new MinecraftHungerEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
};

#endif
