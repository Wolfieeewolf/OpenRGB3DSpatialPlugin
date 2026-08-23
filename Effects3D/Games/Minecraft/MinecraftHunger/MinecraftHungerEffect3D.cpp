// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftHungerEffect3D.h"

REGISTER_EFFECT_3D(MinecraftHungerEffect3D);

MinecraftHungerEffect3D::MinecraftHungerEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChHunger, "Hunger", parent)
{
}

EffectInfo3D MinecraftHungerEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description = "Hunger gradient from Fabric UDP. Stack per controller.";
    return info;
}
