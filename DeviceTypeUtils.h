// SPDX-License-Identifier: GPL-2.0-only
//
// Local device_type -> string helper.
//
// OpenRGB Plugin API v5 removed the free device_type_to_str() function; the
// only conversion left is RGBController::DeviceTypeToString(), a static on the
// concrete RGBController class whose .cpp the plugin no longer compiles. This
// inline mirror keeps the same labels without linking the OpenRGB backend.

#pragma once

#include <string>
#include "RGBController/RGBControllerInterface.h"

inline std::string PluginDeviceTypeToString(device_type type)
{
    switch(type)
    {
        case DEVICE_TYPE_MOTHERBOARD:   return "Motherboard";
        case DEVICE_TYPE_DRAM:          return "DRAM";
        case DEVICE_TYPE_GPU:           return "GPU";
        case DEVICE_TYPE_COOLER:        return "Cooler";
        case DEVICE_TYPE_LEDSTRIP:      return "LED Strip";
        case DEVICE_TYPE_KEYBOARD:      return "Keyboard";
        case DEVICE_TYPE_MOUSE:         return "Mouse";
        case DEVICE_TYPE_MOUSEMAT:      return "Mousemat";
        case DEVICE_TYPE_HEADSET:       return "Headset";
        case DEVICE_TYPE_HEADSET_STAND: return "Headset Stand";
        case DEVICE_TYPE_GAMEPAD:       return "Gamepad";
        case DEVICE_TYPE_LIGHT:         return "Light";
        case DEVICE_TYPE_SPEAKER:       return "Speaker";
        case DEVICE_TYPE_VIRTUAL:       return "Virtual";
        case DEVICE_TYPE_STORAGE:       return "Storage";
        case DEVICE_TYPE_CASE:          return "Case";
        case DEVICE_TYPE_MICROPHONE:    return "Microphone";
        case DEVICE_TYPE_ACCESSORY:     return "Accessory";
        case DEVICE_TYPE_KEYPAD:        return "Keypad";
        case DEVICE_TYPE_LAPTOP:        return "Laptop";
        case DEVICE_TYPE_MONITOR:       return "Monitor";
        default:                        return "Unknown";
    }
}
