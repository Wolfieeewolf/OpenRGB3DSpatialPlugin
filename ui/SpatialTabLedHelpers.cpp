// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialTabLedHelpers.h"

bool TryGetObjectCreatorGlobalLedIndex(RGBController* controller,
                                      unsigned int zone_idx,
                                      unsigned int led_idx,
                                      unsigned int* global_led_idx)
{
    if(!controller || !global_led_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_led_idx = controller->zones[zone_idx].start_idx + led_idx;
    return (*global_led_idx < controller->leds.size());
}

bool TryGetCanonicalPhysicalSpacing(const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                                    RGBController* controller,
                                    float& out_x,
                                    float& out_y,
                                    float& out_z)
{
    if(!controller)
    {
        return false;
    }
    for(unsigned int i = 0; i < transforms.size(); i++)
    {
        const ControllerTransform* t = transforms[i].get();
        if(!t || t->controller != controller)
        {
            continue;
        }
        out_x = t->led_spacing_mm_x;
        out_y = t->led_spacing_mm_y;
        out_z = t->led_spacing_mm_z;
        return true;
    }
    return false;
}
