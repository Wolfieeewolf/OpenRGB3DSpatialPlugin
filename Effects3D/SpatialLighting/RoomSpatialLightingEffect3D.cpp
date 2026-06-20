// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSpatialLightingEffect3D.h"

#include "SpatialLighting/BlockerGridOccluder.h"
#include "SpatialLighting/OccluderSpatialIndex.h"
#include "SpatialRoom/SpatialRoomFrame.h"
#include "GridSpaceUtils.h"

#include <cmath>

void RoomSpatialLightingEffect3D::MarkRoomLightPlacementDirty()
{
    fixed_light_valid_ = false;
    InvalidateLightingScene();
}

void RoomSpatialLightingEffect3D::ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const
{
    SpatialRoom::ApplyDepthPresetToShadeSettings(scene.shade, SpatialRoom::CurrentFrameContext().depth_preset);

    scene.shade.ambient_level = 0.05f;
    scene.shade.ao_strength = room_light_.ao_strength / 100.0f;
    scene.shade.room_fill_strength = (room_light_.room_fill / 100.0f) * (effect_brightness / 100.0f) * 1.15f;
    scene.shade.room_fill_atten = 1.0f;
    scene.shade.use_occlusion = scene.shade.use_occlusion && room_light_.use_occlusion;
    scene.shade.use_ambient_occlusion =
        scene.shade.use_ambient_occlusion && room_light_.use_occlusion && room_light_.ao_strength > 0.01f;
    scene.shade.direct_falloff = 0.62f;
    scene.shade.room_fill_atten = 0.72f;
}

void RoomSpatialLightingEffect3D::RefreshRoomLightOccluders(const GridContext3D& grid) const
{
    const SpatialLighting::OccluderBuildOptions options =
        RoomSpatialLightingUi::BuildOccluderOptions(room_light_);
    SpatialLighting::BuildSpatialOccluders(occluders_, occluder_aabbs_, grid, options);
    blocker_grids_.clear();
    room_blocker_field_ = SpatialLighting::RoomBlockerField{};
    if(options.light_blockers)
    {
        SpatialLighting::BuildBlockerGridOccluders(blocker_grids_, grid.grid_scale_mm);
        SpatialLighting::BuildRoomBlockerField(room_blocker_field_, grid.grid_scale_mm, &grid);
    }
    SpatialLighting::BuildOccluderAabbSpatialIndex(occluder_aabbs_, grid, occluder_aabb_index_);
    cached_scene_.occluders = occluders_;
    cached_scene_.occluder_aabbs = occluder_aabbs_;
    cached_scene_.blocker_grids = blocker_grids_;
    cached_scene_.room_blocker_field = room_blocker_field_;
    cached_scene_.occluder_aabb_index = occluder_aabb_index_.IsBuilt() ? &occluder_aabb_index_ : nullptr;
}

void RoomSpatialLightingEffect3D::UpdateRoomLightSource(const GridContext3D& grid, RGBColor color) const
{
    const float glow_u = MMToGridUnits(room_light_.glow_radius_mm, grid.grid_scale_mm);
    const float reach_u = MMToGridUnits(room_light_.light_reach_mm, grid.grid_scale_mm);
    const float bright = std::max(0.2f, effect_brightness / 100.0f);

    cached_scene_.source.radius = std::max(0.02f, glow_u);
    cached_scene_.source.light_radius = std::max(cached_scene_.source.radius * 1.5f, reach_u);
    // RGBColor is 0x00BBGGRR per OpenRGB RGBController API.
    cached_scene_.source.r = static_cast<float>(color & 0xFF) / 255.0f;
    cached_scene_.source.g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    cached_scene_.source.b = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    cached_scene_.source.emissive_strength = RoomLightEmissiveMul() * bright;
    cached_scene_.source.light_strength = RoomLightDirectMul() * bright;

    const float room_diag =
        std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    cached_scene_.shade.ao_probe_span = std::clamp(std::max(reach_u * 0.4f, room_diag * 0.04f), 0.5f, 14.0f);
}

void RoomSpatialLightingEffect3D::RebuildRoomLightScene(const GridContext3D& grid, RGBColor color)
{
    fixed_light_pos_ = RoomSpatialLightingUi::PlacementPosition(grid, room_light_);
    fixed_light_valid_ = true;

    cached_scene_.source.position = fixed_light_pos_;
    UpdateRoomLightSource(grid, color);
    ApplyLiveShadeSettings(cached_scene_);
    MarkLightingSceneBuilt(grid.render_sequence);
}

SpatialLighting::Vec3 RoomSpatialLightingEffect3D::ResolvedRoomLightPosition(const GridContext3D& grid) const
{
    if(fixed_light_valid_)
    {
        return fixed_light_pos_;
    }
    return RoomSpatialLightingUi::PlacementPosition(grid, room_light_);
}

RGBColor RoomSpatialLightingEffect3D::ShadeRoomLightAt(float x,
                                                         float y,
                                                         float z,
                                                         const GridContext3D& grid,
                                                         RGBColor source_color)
{
    static thread_local std::uint64_t shade_prepared_for = 0;
    if(shade_prepared_for != grid.render_sequence)
    {
        RefreshRoomLightOccluders(grid);
        if(!fixed_light_valid_ || LightingSceneEpochChanged(grid.render_sequence))
        {
            RebuildRoomLightScene(grid, source_color);
        }
        else
        {
            cached_scene_.source.position = fixed_light_pos_;
            UpdateRoomLightSource(grid, source_color);
            ApplyLiveShadeSettings(cached_scene_);
        }
        shade_prepared_for = grid.render_sequence;
    }

    return SpatialLighting::ShadeLed(cached_scene_, x, y, z);
}
