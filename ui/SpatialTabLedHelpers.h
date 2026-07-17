// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "ControllerLayout3D.h"
#include "RGBController/RGBController.h"

#include <memory>
#include <vector>

bool TryGetObjectCreatorGlobalLedIndex(RGBControllerInterface* controller,
                                      unsigned int zone_idx,
                                      unsigned int led_idx,
                                      unsigned int* global_led_idx);

bool TryGetCanonicalPhysicalSpacing(const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                                    RGBControllerInterface* controller,
                                    float& out_x,
                                    float& out_y,
                                    float& out_z);
