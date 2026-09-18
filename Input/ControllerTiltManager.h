// SPDX-License-Identifier: GPL-2.0-only

#ifndef CONTROLLERTILTMANAGER_H
#define CONTROLLERTILTMANAGER_H

#include "ControllerTiltTypes.h"
#include "LEDPosition3D.h"

#include <memory>
#include <vector>

/**
 * Singleton that polls supported motion controllers and exposes a RoomGrid aim vector.
 * DualSense (USB/BT HID via hidapi on Windows and Linux): rigid IMU.
 * Torch is always pad-forward. Gravity holds pitch/roll while moving; heading is gyro
 * (no magnetometer). Like a real torch, a still pad is fully frozen — gyro AND gravity
 * are skipped, so the beam holds dead still and only follows real pad motion (rotation
 * with hysteresis, or |a| off 1g). Home is flat, facing front wall (−Z).
 * Layout position is the apex only.
 */
class ControllerTiltManager
{
public:
    static ControllerTiltManager* instance();

    void start();
    void stop();
    bool isRunning() const;

    /** Call once per effect render frame while running. */
    void Poll();

    /** Latest fused sample (may be invalid if no device / open failed). */
    ControllerTiltSample Latest() const;

    /**
     * Reset beam to front-wall home (flat, light bar toward −Z) and resample gyro bias.
     * Hold the DualSense still in that pose, then click. Use if twist heading wanders.
     */
    void RecenterHeading();

    /**
     * Call each render frame with the live layout so DualSense apex + orientation
     * come from the DualSense controller transform in the scene.
     */
    void UpdateLayoutOrigins(const std::vector<std::unique_ptr<ControllerTransform>>& transforms);

    /** DualSense controller position in RoomGrid from the last UpdateLayoutOrigins. */
    bool DualSenseOrigin(Vector3D& origin_out) const;

    /**
     * RoomGrid torch apex from layout: first DualSense controller transform position.
     * Returns false if none matched.
     */
    bool ResolveDualSenseOrigin(const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                                Vector3D& origin_out) const;

    /** Position + Euler rotation (degrees) of the DualSense layout object. */
    bool ResolveDualSenseLayoutPose(const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                                    Vector3D& origin_out,
                                    Rotation3D& rotation_out) const;

private:
    ControllerTiltManager();
    ~ControllerTiltManager();

    ControllerTiltManager(const ControllerTiltManager&) = delete;
    ControllerTiltManager& operator=(const ControllerTiltManager&) = delete;

    class Impl;
    Impl* impl_ = nullptr;
};

#endif
