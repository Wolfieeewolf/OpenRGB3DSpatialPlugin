// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLightingSceneProvider.h"

SpatialLightingSceneProvider* SpatialLightingSceneProvider::instance()
{
    static SpatialLightingSceneProvider inst;
    return &inst;
}

void SpatialLightingSceneProvider::SetControllers(
    const std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    controllers_ = transforms;
}
