// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftHealthEffect3D.h"

REGISTER_EFFECT_3D(MinecraftHealthEffect3D);

MinecraftHealthEffect3D::MinecraftHealthEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChHealth, "Health", parent)
{
}

EffectInfo3D MinecraftHealthEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description =
        "Health from Fabric UDP (127.0.0.1:9876): gradient or per-heart strip. Stack per controller.";
    return info;
}
