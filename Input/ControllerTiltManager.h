// SPDX-License-Identifier: GPL-2.0-only

#ifndef CONTROLLERTILTMANAGER_H
#define CONTROLLERTILTMANAGER_H

#include "ControllerTiltTypes.h"
#include "LEDPosition3D.h"

#include <memory>
#include <vector>

class VirtualReferencePoint3D;

/**
 * Singleton that polls a Wii Remote (MotionPlus + accelerometer + optional IR) and
 * exposes a RoomGrid aim.
 *
 * Full 360° orientation from MotionPlus gyros. Accelerometer softly corrects tilt
 * only while nearly still (so free aim is not snapped flat). When the sensor bar is
 * visible, IR softly locks yaw to cut drift; outside the bar FOV yaw stays free.
 *
 * Home (Recenter): flat on the desk, buttons up, IR end toward the front wall (−Z)
 * / sensor bar. Beam aims along the IR end.
 *
 * Place a User (or Custom named "Wii") reference where you hold the remote — torch apex.
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

    /** Current Wii Remote link reason (Ok / busy / missing / …). */
    ControllerTiltLinkStatus LinkStatus() const;

    /**
     * Reset home: lay flat (buttons up, IR toward front wall), hold still, then click.
     * Sets orientation identity and resamples MotionPlus bias.
     */
    void RecenterHeading();

    /**
     * Call each render frame with live reference points so the torch apex
     * comes from a User / "Wii" Custom marker in the scene.
     */
    void UpdateLayoutOrigins(const std::vector<std::unique_ptr<VirtualReferencePoint3D>>& refs);

    /** Torch apex in RoomGrid from the last UpdateLayoutOrigins. */
    bool TorchOrigin(Vector3D& origin_out) const;

    /**
     * RoomGrid torch apex from layout: named Wii ref, else User, else Chair.
     * Returns false if none matched.
     */
    bool ResolveTorchOrigin(const std::vector<std::unique_ptr<VirtualReferencePoint3D>>& refs,
                            Vector3D& origin_out) const;

private:
    ControllerTiltManager();
    ~ControllerTiltManager();

    ControllerTiltManager(const ControllerTiltManager&) = delete;
    ControllerTiltManager& operator=(const ControllerTiltManager&) = delete;

    class Impl;
    Impl* impl_ = nullptr;
};

#endif
