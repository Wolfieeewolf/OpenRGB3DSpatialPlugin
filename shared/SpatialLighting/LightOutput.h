// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* How bright each LED really is, from its device's light output in lumens at full white.

   Set per custom controller ("light_output_lm" in its JSON); when not set, a default per LED
   by device type is used. Colour is handled by the light maths itself: lumens weight green
   most and blue least, so pure blue gives ~7% of the white rating and pure green ~72%.
   Returned as a scale against a 5 lm strip LED (the look the room had before), used by the
   Real light overlay and Emitter + relay emitters. */

#include "ControllerLayout3D.h"
#include "LEDPosition3D.h"
#include "RGBController.h"
#include "VirtualController3D.h"

#include <algorithm>

namespace LightOutput
{

constexpr float kReferenceLedLm = 5.0f;

/** Default lumens per LED at full white, by device type (rough real-world figures). */
inline float DefaultLedLm(const LEDPosition3D& led)
{
    if(!led.controller)
    {
        return kReferenceLedLm;
    }
    switch(led.controller->GetDeviceType())
    {
    case DEVICE_TYPE_KEYBOARD:
    case DEVICE_TYPE_KEYPAD:
    case DEVICE_TYPE_LAPTOP:
        return 0.8f;
    case DEVICE_TYPE_MOUSE:
    case DEVICE_TYPE_MOUSEMAT:
    case DEVICE_TYPE_HEADSET:
    case DEVICE_TYPE_HEADSET_STAND:
    case DEVICE_TYPE_GAMEPAD:
    case DEVICE_TYPE_MICROPHONE:
        return 1.5f;
    case DEVICE_TYPE_DRAM:
    case DEVICE_TYPE_MOTHERBOARD:
    case DEVICE_TYPE_GPU:
    case DEVICE_TYPE_COOLER:
    case DEVICE_TYPE_CASE:
    case DEVICE_TYPE_STORAGE:
        return 2.5f;
    case DEVICE_TYPE_SPEAKER:
        return 3.0f;
    case DEVICE_TYPE_LIGHT:
        return 200.0f; /* smart bulbs / lamps: few LEDs, lots of light */
    default:
        return kReferenceLedLm;
    }
}

/** Light scale of one LED of transform t (1 = a 5 lm strip LED at full white). */
inline float LedScale(const ControllerTransform* t, const LEDPosition3D& led)
{
    float lm = DefaultLedLm(led);
    if(t && !t->led_positions.empty())
    {
        if(t->virtual_controller && t->virtual_controller->GetLightOutputLm() > 0.0f)
        {
            lm = t->virtual_controller->GetLightOutputLm() / (float)t->led_positions.size();
        }
        else if(t->light_output_lm > 0.0f)
        {
            lm = t->light_output_lm / (float)t->led_positions.size();
        }
    }
    return std::max(0.0f, lm) / kReferenceLedLm;
}

} // namespace LightOutput
