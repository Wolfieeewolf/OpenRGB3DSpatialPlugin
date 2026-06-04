// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERTYPES_H
#define CUSTOMCONTROLLERTYPES_H

#include "RGBController.h"

struct GridLEDMapping
{
    int x;
    int y;
    int z;
    RGBController* controller;
    unsigned int zone_idx;
    unsigned int led_idx;
    int granularity;
};

struct CustomControllerLightBlocker
{
    int x = 0;
    int y = 0;
    int z = 0;
};

#endif
