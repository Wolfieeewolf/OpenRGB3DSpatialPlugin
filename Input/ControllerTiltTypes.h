// SPDX-License-Identifier: GPL-2.0-only
//
// Central controller tilt / motion samples for spatial effects.
// Add new backends in ControllerTiltManager.cpp; effects only consume this API.

#ifndef CONTROLLERTILTTYPES_H
#define CONTROLLERTILTTYPES_H

#include "LEDPosition3D.h"

#include <cstdint>
#include <string>

enum class ControllerTiltDeviceKind : std::uint8_t
{
    None = 0,
    WiiRemote,
};

/** Why Wii Remote motion may be unavailable — for torch status UI. */
enum class ControllerTiltLinkStatus : std::uint8_t
{
    Stopped = 0,
    Ok,
    NoDevice,
    DeviceBusy,
    WaitingInit,
    NoHidapi,
};

struct ControllerTiltSample
{
    ControllerTiltDeviceKind kind = ControllerTiltDeviceKind::None;
    bool valid = false;
    /**
     * Unit beam direction in RoomGrid — Wii Remote IR camera / pointer forward.
     * (+X right, +Y up, +Z back wall; forward/front wall = −Z).
     */
    Vector3D aim{0.0f, 0.0f, -1.0f};
    /**
     * Unit "up" for the beam cross-section (perpendicular to aim).
     * Pointing at the sensor bar with buttons facing ceiling → room +Y;
     * tip left/right rolls this around aim.
     */
    Vector3D up{0.0f, 1.0f, 0.0f};
    /**
     * RoomGrid Euler XYZ in degrees — same convention as controller transforms.
     * (0,0,0) = flat on desk, buttons up, IR toward front wall (−Z).
     * Beam aims along the IR end. x = pitch, y = yaw, z = roll.
     */
    Rotation3D euler_deg{0.0f, 0.0f, 0.0f};
    /** Raw normalized accelerometer (device frame). */
    Vector3D accel{0.0f, 0.0f, -1.0f};
    /** Bias-corrected MotionPlus gyro in deg/s (device frame), after deadzone. */
    Vector3D gyro_dps{0.0f, 0.0f, 0.0f};
    /** True when at least one IR dot is tracked (sensor bar visible). */
    bool ir_locked = false;
    /** True when a live MotionPlus extension sample was decoded this frame. */
    bool mplus_live = false;
    std::uint64_t timestamp_ms = 0;
    ControllerTiltLinkStatus link = ControllerTiltLinkStatus::Stopped;
};

inline const char* ControllerTiltLinkStatusText(ControllerTiltLinkStatus status)
{
    switch(status)
    {
    case ControllerTiltLinkStatus::Stopped:
        return "Tilt session stopped";
    case ControllerTiltLinkStatus::Ok:
        return "Wii Remote connected";
    case ControllerTiltLinkStatus::NoDevice:
        return "No Wii Remote found (pair via BT / Mayflash DolphinBar mode 4)";
    case ControllerTiltLinkStatus::DeviceBusy:
        return "Wii Remote HID busy — close Dolphin / other Wiimote apps";
    case ControllerTiltLinkStatus::WaitingInit:
        return "Wii Remote: enabling IR + MotionPlus…";
    case ControllerTiltLinkStatus::NoHidapi:
        return "Built without hidapi — Wii Remote motion disabled";
    }
    return "Unknown tilt status";
}

inline const char* ControllerTiltDeviceKindName(ControllerTiltDeviceKind kind)
{
    switch(kind)
    {
    case ControllerTiltDeviceKind::None:
        return "None";
    case ControllerTiltDeviceKind::WiiRemote:
        return "Wii Remote";
    }
    return "Unknown";
}

/** True if a reference-point name looks like a Wii torch apex marker. */
inline bool ControllerTiltNameLooksLikeWii(const std::string& name)
{
    std::string lower;
    lower.reserve(name.size());
    for(char ch : name)
    {
        if(ch >= 'A' && ch <= 'Z')
        {
            lower.push_back(static_cast<char>(ch - 'A' + 'a'));
        }
        else
        {
            lower.push_back(ch);
        }
    }
    return lower.find("wii") != std::string::npos
        || lower.find("wiimote") != std::string::npos
        || lower.find("remote") != std::string::npos;
}

#endif
