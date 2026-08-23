// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftDamageEffect3D.h"

REGISTER_EFFECT_3D(MinecraftDamageEffect3D);

MinecraftDamageEffect3D::MinecraftDamageEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChDamage, "Damage flash", parent)
{
}

EffectInfo3D MinecraftDamageEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description = "Directional damage flash from Fabric UDP. Stack per controller.";
    return info;
}
