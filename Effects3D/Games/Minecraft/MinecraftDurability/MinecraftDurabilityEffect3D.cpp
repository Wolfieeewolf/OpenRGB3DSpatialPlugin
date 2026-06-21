// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftDurabilityEffect3D.h"

REGISTER_EFFECT_3D(MinecraftDurabilityEffect3D);

MinecraftDurabilityEffect3D::MinecraftDurabilityEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChDurability, "Durability", parent)
{
}

EffectInfo3D MinecraftDurabilityEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description = "Main-hand item durability from Fabric UDP. Stack per controller.";
    return info;
}
