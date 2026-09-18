// SPDX-License-Identifier: GPL-2.0-only
//
// Central controller tilt / IMU samples for spatial effects.
// Add new backends in ControllerTiltManager.cpp; effects only consume this API.

#ifndef CONTROLLERTILTTYPES_H
#define CONTROLLERTILTTYPES_H

#include "LEDPosition3D.h"

#include <cstdint>
#include <string>

enum class ControllerTiltDeviceKind : std::uint8_t
{
    None = 0,
    DualSense,
    // Future: DualShock4, WiiRemote, …
};

struct ControllerTiltSample
{
    ControllerTiltDeviceKind kind = ControllerTiltDeviceKind::None;
    bool valid = false;
    /**
     * Unit beam direction in RoomGrid — DualSense forward (light bar / front).
     * (+X right, +Y up, +Z back wall; forward/front wall = −Z).
     */
    Vector3D aim{0.0f, 0.0f, -1.0f};
    /**
     * Unit "up" for the beam cross-section (perpendicular to aim).
     * Flat DualSense face-up → room +Y; tip left/right rolls this around aim.
     */
    Vector3D up{0.0f, 1.0f, 0.0f};
    /** Raw normalized accelerometer (device frame). */
    Vector3D accel{0.0f, -1.0f, 0.0f};
    /** Bias-corrected gyro in deg/s (device frame), after deadzone. */
    Vector3D gyro_dps{0.0f, 0.0f, 0.0f};
    std::uint64_t timestamp_ms = 0;
};

inline const char* ControllerTiltDeviceKindName(ControllerTiltDeviceKind kind)
{
    switch(kind)
    {
    case ControllerTiltDeviceKind::None:
        return "None";
    case ControllerTiltDeviceKind::DualSense:
        return "DualSense";
    default:
        return "Unknown";
    }
}

/** True if an OpenRGB device name is a DualSense we support for tilt. */
inline bool ControllerTiltNameLooksLikeDualSense(const std::string& name)
{
    if(name.find("DualSense") != std::string::npos)
    {
        return true;
    }
    if(name.find("Dual Sense") != std::string::npos)
    {
        return true;
    }
    return false;
}

#endif
