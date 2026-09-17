// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTDURABILITYEFFECT3D_H
#define MINECRAFTDURABILITYEFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftDurabilityEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftDurabilityEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftDurability", "Durability", "minecraft", "Minecraft",
                              []() { return new MinecraftDurabilityEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
};

#endif
