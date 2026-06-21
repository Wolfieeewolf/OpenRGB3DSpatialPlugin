// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftLightningEffect3D.h"

REGISTER_EFFECT_3D(MinecraftLightningEffect3D);

MinecraftLightningEffect3D::MinecraftLightningEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChLightning, "Lightning", parent)
{
}

EffectInfo3D MinecraftLightningEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description = "Lightning flash (sky-heavy) from Fabric UDP. Stack per controller.";
    return info;
}
