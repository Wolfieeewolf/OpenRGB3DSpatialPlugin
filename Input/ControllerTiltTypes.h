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

/** Why DualSense IMU may be unavailable — for torch status UI. */
enum class ControllerTiltLinkStatus : std::uint8_t
{
    Stopped = 0,
    Ok,
    NoDevice,
    DeviceBusy,
    WaitingEnhancedBt,
    NoHidapi,
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
    /**
     * RoomGrid Euler XYZ in degrees — same convention as controller transforms.
     * (0,0,0) = flat on desk, light bar toward front wall (−Z).
     * x = pitch (tip Triangle up/down), y = yaw (twist on desk), z = roll (tilt left/right).
     * Real-world tilt should match these angles 1:1 in the room.
     */
    Rotation3D euler_deg{0.0f, 0.0f, 0.0f};
    /** Raw normalized accelerometer (device frame). */
    Vector3D accel{0.0f, -1.0f, 0.0f};
    /** Bias-corrected gyro in deg/s (device frame), after deadzone. */
    Vector3D gyro_dps{0.0f, 0.0f, 0.0f};
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
        return "DualSense IMU connected";
    case ControllerTiltLinkStatus::NoDevice:
        return "No DualSense found (USB/BT)";
    case ControllerTiltLinkStatus::DeviceBusy:
        return "DualSense HID busy — wait for OpenRGB rescan to finish, or close Steam/DualSenseClient";
    case ControllerTiltLinkStatus::WaitingEnhancedBt:
        return "Bluetooth DualSense: waiting for enhanced IMU report (0x31)";
    case ControllerTiltLinkStatus::NoHidapi:
        return "Built without hidapi — DualSense IMU disabled";
    }
    return "Unknown tilt status";
}

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
