// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftAirEffect3D.h"

REGISTER_EFFECT_3D(MinecraftAirEffect3D);

MinecraftAirEffect3D::MinecraftAirEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChAir, "Air", parent)
{
}

EffectInfo3D MinecraftAirEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description = "Air/breathing gradient from Fabric UDP. Stack per controller.";
    return info;
}
