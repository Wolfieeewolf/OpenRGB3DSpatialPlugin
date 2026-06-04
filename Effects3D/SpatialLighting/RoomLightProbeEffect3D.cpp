// SPDX-License-Identifier: GPL-2.0-only

#include "RoomLightProbeEffect3D.h"

#include "GridSpaceUtils.h"

REGISTER_EFFECT_3D(RoomLightProbeEffect3D);

RoomLightProbeEffect3D::RoomLightProbeEffect3D(QWidget* parent) : RoomSpatialLightingEffect3D(parent)
{
    SetReferenceMode(REF_MODE_USER_POSITION);
}

void RoomLightProbeEffect3D::ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const
{
    scene.shade.ambient_level = 0.04f;
    scene.shade.ao_strength = 0.75f;
    scene.shade.room_fill_strength = 0.35f;
    scene.shade.use_occlusion = true;
    scene.shade.use_ambient_occlusion = true;
}

EffectInfo3D RoomLightProbeEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Room light probe (test)";
    info.effect_description =
        "Small white light at room center (fixed). Tests reach, room-wall shadows, and AO. "
        "Anchor does not move the probe.";
    info.category = "Spatial · Lighting";
    info.has_custom_settings = false;
    info.show_position_offset_control = false;
    info.show_size_control = false;
    info.show_speed_control = false;
    info.show_frequency_control = false;
    info.show_brightness_control = true;
    info.show_color_controls = false;
    info.user_colors = 0;
    return info;
}

void RoomLightProbeEffect3D::SetupCustomUI(QWidget* parent)
{
    (void)parent;
}

void RoomLightProbeEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

void RoomLightProbeEffect3D::RefreshOccluders(const GridContext3D& grid) const
{
    SpatialLighting::OccluderBuildOptions options{};
    options.display_planes = true;
    options.room_walls = true;
    options.controllers = true;
    SpatialLighting::BuildSpatialOccluders(occluders_, occluder_aabbs_, grid, options);
    cached_scene_.occluders = occluders_;
    cached_scene_.occluder_aabbs = occluder_aabbs_;
}

void RoomLightProbeEffect3D::RebuildScene(const GridContext3D& grid)
{
    const float glow_u = MMToGridUnits(35.0f, grid.grid_scale_mm);
    const float reach_u = MMToGridUnits(220.0f, grid.grid_scale_mm);
    const float bright = std::max(0.2f, effect_brightness / 100.0f);

    cached_scene_.source.position = {grid.center_x, grid.center_y, grid.center_z};
    cached_scene_.source.radius = glow_u;
    cached_scene_.source.light_radius = reach_u;
    cached_scene_.source.r = 1.0f;
    cached_scene_.source.g = 1.0f;
    cached_scene_.source.b = 1.0f;
    cached_scene_.source.emissive_strength = 0.8f * bright;
    cached_scene_.source.light_strength = 0.7f * bright;

    const float room_diag =
        std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    cached_scene_.shade.ao_probe_span = std::clamp(room_diag * 0.035f, 0.35f, 8.0f);

    ApplyLiveShadeSettings(cached_scene_);
    MarkLightingSceneBuilt(grid.render_sequence);
}

RGBColor RoomLightProbeEffect3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)time;
    static thread_local std::uint64_t shade_prepared_for = 0;
    if(shade_prepared_for != grid.render_sequence)
    {
        RefreshOccluders(grid);
        if(LightingSceneEpochChanged(grid.render_sequence))
        {
            RebuildScene(grid);
        }
        else
        {
            ApplyLiveShadeSettings(cached_scene_);
        }
        shade_prepared_for = grid.render_sequence;
    }

    return SpatialLighting::ShadeLed(cached_scene_, x, y, z);
}
