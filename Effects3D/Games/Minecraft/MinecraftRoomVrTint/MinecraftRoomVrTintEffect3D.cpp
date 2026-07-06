// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftRoomVrTintEffect3D.h"
#include "../MinecraftGame.h"

REGISTER_EFFECT_3D(MinecraftRoomVrTintEffect3D);

MinecraftRoomVrTintEffect3D::MinecraftRoomVrTintEffect3D(QWidget* parent)
    : MinecraftSubEffect3D(MinecraftGame::ChRoomVrTint, "Room tint (VR)", parent)
{
}

EffectInfo3D MinecraftRoomVrTintEffect3D::GetEffectInfo() const
{
    EffectInfo3D info = BaseMinecraftEffectInfo();
    info.effect_description =
        "Ambilight viewport: room LEDs mirror what you see in-game (line-of-sight), not the whole world. "
        "Place a reference point at eye height and set this effect's 3D origin there.";
    return info;
}

void MinecraftRoomVrTintEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = MinecraftGame::CreateEffectWidget(parent,
                                                   QString::fromUtf8("Room tint (VR)"),
                                                   mc_settings_,
                                                   channels_,
                                                   this,
                                                   [this]() { emit ParametersChanged(); });
    AddWidgetToParent(w, parent);
}
