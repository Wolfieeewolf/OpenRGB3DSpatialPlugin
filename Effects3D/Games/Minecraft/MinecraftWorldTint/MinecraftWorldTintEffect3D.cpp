// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftWorldTintEffect3D.h"
#include "../MinecraftGame.h"

REGISTER_EFFECT_3D(MinecraftWorldTintEffect3D);

MinecraftWorldTintEffect3D::MinecraftWorldTintEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChWorldTint, "World tint", parent)
{
}

EffectInfo3D MinecraftWorldTintEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description =
        "3D world tint from Fabric world_light directional probes, mapped by room position and "
        "player heading. Place a reference point at eye height and set 3D origin there.";
    return info;
}

void MinecraftWorldTintEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = MinecraftGame::CreateEffectWidget(parent,
                                                   QString::fromUtf8("World tint"),
                                                   mc_settings_,
                                                   channels_,
                                                   this,
                                                   [this]() { emit ParametersChanged(); });
    AddWidgetToParent(w, parent);
}
