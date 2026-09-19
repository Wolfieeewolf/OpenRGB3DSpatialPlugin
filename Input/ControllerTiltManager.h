// SPDX-License-Identifier: GPL-2.0-only

#ifndef CONTROLLERTILTMANAGER_H
#define CONTROLLERTILTMANAGER_H

#include "ControllerTiltTypes.h"
#include "LEDPosition3D.h"

#include <memory>
#include <vector>

/**
 * Singleton that polls supported motion controllers and exposes a RoomGrid aim vector.
 * DualSense IMU maps 1:1 into the room:
 *   (0,0,0)° = flat on desk, light bar toward the front wall (−Z) — same as a DualSense
 *   controller transform at identity. X/Y/Z degrees use the same Euler XYZ convention as
 *   every other object in the room. Pitch (X) and roll (Z) come from gravity (real tilt);
 *   yaw (Y) is gyro from Recenter (no magnetometer). Layout DualSense rotation is applied
 *   on top so a pad you placed in the grid matches the physical one.
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

    /** Current DualSense link reason (Ok / busy / missing / …). */
    ControllerTiltLinkStatus LinkStatus() const;

    /**
     * Reset to RoomGrid (0,0,0): hold DualSense flat, light bar toward front wall, then click.
     * Sets yaw home; pitch/roll then track real gravity tilt from that pose.
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
