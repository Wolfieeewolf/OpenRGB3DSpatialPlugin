// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTHEALTHEFFECT3D_H
#define MINECRAFTHEALTHEFFECT3D_H

#include "../MinecraftSubEffect3D.h"
#include "EffectRegisterer3D.h"

class MinecraftHealthEffect3D : public MinecraftSubEffect3D
{
    Q_OBJECT

public:
    explicit MinecraftHealthEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("MinecraftHealth", "Health", "minecraft", "Minecraft",
                              []() { return new MinecraftHealthEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
};

#endif
