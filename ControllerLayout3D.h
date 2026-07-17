// SPDX-License-Identifier: GPL-2.0-only

#ifndef CONTROLLERLAYOUT3D_H
#define CONTROLLERLAYOUT3D_H

#include <vector>
#include <memory>
#include "RGBController.h"
#include "LEDPosition3D.h"

struct ControllerTransform;

class ControllerLayout3D
{
public:
    static std::vector<LEDPosition3D> GenerateCustomGridLayout(RGBControllerInterface* controller, int grid_x, int grid_y, bool center_layout = true);
    static std::vector<LEDPosition3D> GenerateCustomGridLayoutWithSpacing(RGBControllerInterface* controller, int grid_x, int grid_y, float spacing_mm_x, float spacing_mm_y, float spacing_mm_z, float grid_scale_mm, bool center_layout = true);
    static Vector3D CalculateWorldPosition(Vector3D local_pos, Transform3D transform);
    static Vector3D GetControllerCenterWorld(const ControllerTransform* ctrl_transform);
    static void UpdateWorldPositions(ControllerTransform* ctrl_transform);
    static void MarkWorldPositionsDirty(ControllerTransform* ctrl_transform);
    static void CalculateControllerLocalBounds(const ControllerTransform* ctrl_transform,
                                               Vector3D& min_bounds,
                                               Vector3D& max_bounds);

    /** Viewport-only: build evenly spaced draw samples along the mapped strip polyline (shared colour per section). */
    struct ViewportStripDrawSample
    {
        Vector3D position;
        size_t   logical_index;
    };

    static void BuildViewportStripDrawSamples(const ControllerTransform* ctrl_transform,
                                              float grid_scale_mm,
                                              std::vector<ViewportStripDrawSample>& out_samples);
};

#endif
