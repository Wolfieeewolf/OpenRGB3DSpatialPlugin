// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERTYPES_H
#define CUSTOMCONTROLLERTYPES_H

#include "RGBController.h"

#include <string>

struct GridLEDMapping
{
    int x;
    int y;
    int z;
    RGBController* controller = nullptr;
    unsigned int zone_idx = 0;
    unsigned int led_idx = 0;
    int granularity = 0;
    /** Persisted identity for rebind after OpenRGB device rescan. */
    std::string controller_name;
    std::string controller_location;
};

struct CustomControllerLightBlocker
{
    int x = 0;
    int y = 0;
    int z = 0;
};

#endif
