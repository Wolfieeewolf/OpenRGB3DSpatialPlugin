// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTAIREFFECT3D_H
#define MINECRAFTAIREFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftAirEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftAirEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftAir", "Air", "minecraft", "Minecraft",
                              []() { return new MinecraftAirEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
};

#endif
