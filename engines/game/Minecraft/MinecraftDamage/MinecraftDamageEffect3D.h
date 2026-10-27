// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTDAMAGEEFFECT3D_H
#define MINECRAFTDAMAGEEFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftDamageEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftDamageEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftDamage", "Damage flash", "minecraft", "Minecraft",
                              []() { return new MinecraftDamageEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
};

#endif
