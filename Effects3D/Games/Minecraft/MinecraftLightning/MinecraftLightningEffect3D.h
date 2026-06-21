// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTLIGHTNINGEFFECT3D_H
#define MINECRAFTLIGHTNINGEFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftLightningEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftLightningEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftLightning", "Lightning", "minecraft", "Minecraft",
                              []() { return new MinecraftLightningEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
};

#endif
