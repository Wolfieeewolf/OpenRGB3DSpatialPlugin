// SPDX-License-Identifier: GPL-2.0-only
// Scene pointers for spatial lighting occlusion (synced during effect render).

#ifndef SPATIALLIGHTINGSCENEPROVIDER_H
#define SPATIALLIGHTINGSCENEPROVIDER_H

#include <cstdint>
#include <memory>
#include <vector>

struct ControllerTransform;

class SpatialLightingSceneProvider
{
public:
    static SpatialLightingSceneProvider* instance();

    void SetControllers(const std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    const std::vector<std::unique_ptr<ControllerTransform>>* controllers() const { return controllers_; }

    void SetShadingControllerIndex(int index) { shading_controller_index_ = index; }
    int shadingControllerIndex() const { return shading_controller_index_; }

    /** Room grid overlay: skip AO and shadow rays; keep distance falloff (much faster). */
    void SetRoomGridOverlayPreview(bool preview) { room_grid_overlay_preview_ = preview; }
    bool roomGridOverlayPreview() const { return room_grid_overlay_preview_; }

private:
    SpatialLightingSceneProvider() = default;

    const std::vector<std::unique_ptr<ControllerTransform>>* controllers_ = nullptr;
    int shading_controller_index_ = -1;
    bool room_grid_overlay_preview_ = false;
};

#endif
