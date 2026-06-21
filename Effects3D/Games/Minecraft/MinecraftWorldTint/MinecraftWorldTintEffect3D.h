// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTWORLDTINTEFFECT3D_H
#define MINECRAFTWORLDTINTEFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftWorldTintEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftWorldTintEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftWorldTint", "World tint", "minecraft", "Minecraft",
                              []() { return new MinecraftWorldTintEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
};

#endif
