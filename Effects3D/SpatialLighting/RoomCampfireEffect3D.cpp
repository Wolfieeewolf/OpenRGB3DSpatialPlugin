// SPDX-License-Identifier: GPL-2.0-only

#include "RoomCampfireEffect3D.h"

#include "EffectHelpers.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include "GridSpaceUtils.h"

#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>

REGISTER_EFFECT_3D(RoomCampfireEffect3D);

namespace
{

constexpr float kMarginFrac = 0.1f;

float MmToRoomUnits(float mm, float grid_scale_mm)
{
    return MMToGridUnits(mm, grid_scale_mm);
}

} // namespace

RoomCampfireEffect3D::RoomCampfireEffect3D(QWidget* parent) : RoomSpatialLightingEffect3D(parent)
{
    SetReferenceMode(REF_MODE_USER_POSITION);
}

void RoomCampfireEffect3D::ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const
{
    scene.shade.ambient_level = 0.05f;
    scene.shade.ao_strength = ao_strength_ / 100.0f;
    scene.shade.room_fill_strength = (room_fill_ / 100.0f) * (effect_brightness / 100.0f);
    scene.shade.room_fill_atten = 1.0f;
    scene.shade.use_occlusion = use_occlusion_;
    scene.shade.use_ambient_occlusion = use_occlusion_ && ao_strength_ > 0.5f;
    scene.shade.direct_falloff = 0.9f;
}

void RoomCampfireEffect3D::UpdateSourceFromSliders(const GridContext3D& grid) const
{
    const float glow_u = MmToRoomUnits(glow_radius_mm_, grid.grid_scale_mm);
    const float reach_u = MmToRoomUnits(light_reach_mm_, grid.grid_scale_mm);
    const RGBColor user = GetColorAtPosition(0.5f);

    cached_scene_.source.radius = std::max(0.02f, glow_u);
    cached_scene_.source.light_radius = std::max(cached_scene_.source.radius * 1.5f, reach_u);
    cached_scene_.source.r = static_cast<float>((user >> 16) & 0xFF) / 255.0f;
    cached_scene_.source.g = static_cast<float>((user >> 8) & 0xFF) / 255.0f;
    cached_scene_.source.b = static_cast<float>(user & 0xFF) / 255.0f;
    cached_scene_.source.emissive_strength = 1.0f * (effect_brightness / 100.0f);
    cached_scene_.source.light_strength = 0.85f * (effect_brightness / 100.0f);

    const float room_diag =
        std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    cached_scene_.shade.ao_probe_span = std::clamp(room_diag * 0.035f, 0.35f, 8.0f);
}

EffectInfo3D RoomCampfireEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Room campfire / blob";
    info.effect_description =
        "Omnidirectional room light at a fixed placement in room space. "
        "Stack spatial anchor does not move the fire.";
    info.category = "Spatial · Lighting";
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.show_axis_control = false;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_position_offset_control = false;
    info.show_frequency_control = false;
    info.show_color_controls = true;
    info.user_colors = 1;
    info.default_speed_scale = 0.0f;
    return info;
}

void RoomCampfireEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("RoomCampfireSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    EffectLabeledComboRow* place_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Placement:"));
    QComboBox* place_combo = place_row->combo();
    place_combo->addItem(QStringLiteral("Near corner (room min)"));
    place_combo->addItem(QStringLiteral("Room center"));
    place_combo->addItem(QStringLiteral("Far corner (room max)"));
    place_combo->addItem(QStringLiteral("Custom (room %)"));
    place_combo->setCurrentIndex(std::max(0, std::min(placement_mode_, 3)));
    connect(place_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, on_changed](int idx) {
        placement_mode_ = idx;
        frozen_light_valid_ = false;
        InvalidateLightingScene();
        on_changed();
    });

    auto bind_pct = [&](EffectSliderRow* row, float& target) {
        if(!row)
        {
            return;
        }
        row->bindValueChanged(w, [&target, this, on_changed](int v) {
            target = static_cast<float>(v) / 100.0f;
            frozen_light_valid_ = false;
            InvalidateLightingScene();
            on_changed();
        }, [](int v) { return QString::number(v) + QStringLiteral("%"); });
    };

    bind_pct(EffectUiRows::AppendSliderRow(layout,
                                           QStringLiteral("Custom X %:"),
                                           0,
                                           100,
                                           static_cast<int>(custom_u_ * 100.0f)),
             custom_u_);
    bind_pct(EffectUiRows::AppendSliderRow(layout,
                                           QStringLiteral("Custom Y %:"),
                                           0,
                                           100,
                                           static_cast<int>(custom_v_ * 100.0f)),
             custom_v_);
    bind_pct(EffectUiRows::AppendSliderRow(layout,
                                           QStringLiteral("Custom Z %:"),
                                           0,
                                           100,
                                           static_cast<int>(custom_w_ * 100.0f)),
             custom_w_);

    auto bind_mm = [&](EffectSliderRow* row, float& target_mm) {
        if(!row)
        {
            return;
        }
        row->bindValueChanged(w, [&target_mm, this, on_changed](int v) {
            target_mm = static_cast<float>(v);
            InvalidateLightingScene();
            on_changed();
        }, [](int v) { return QString::number(v) + QStringLiteral(" mm"); });
    };

    bind_mm(EffectUiRows::AppendSliderRow(layout, QStringLiteral("Glow size:"), 10, 200, (int)glow_radius_mm_),
            glow_radius_mm_);
    bind_mm(EffectUiRows::AppendSliderRow(layout, QStringLiteral("Light reach:"), 50, 2000, (int)light_reach_mm_),
            light_reach_mm_);

    auto bind_percent_slider = [&](EffectSliderRow* row, float& target) {
        if(!row)
        {
            return;
        }
        row->bindValueChanged(w, [&target, this, on_changed](int v) {
            target = static_cast<float>(v);
            InvalidateLightingScene();
            on_changed();
        }, [](int v) { return QString::number(v) + QStringLiteral("%"); });
    };
    bind_percent_slider(EffectUiRows::AppendSliderRow(layout,
                                                      QStringLiteral("Room fill:"),
                                                      0,
                                                      100,
                                                      35),
                        room_fill_);
    bind_percent_slider(EffectUiRows::AppendSliderRow(layout,
                                                      QStringLiteral("Ambient occlusion:"),
                                                      0,
                                                      100,
                                                      static_cast<int>(ao_strength_)),
                        ao_strength_);

    auto* occ = new QCheckBox(QStringLiteral("Shadows (occlusion)"), w);
    occ->setChecked(use_occlusion_);
    layout->addWidget(occ);
    connect(occ, &QCheckBox::toggled, this, [this, on_changed](bool on) {
        use_occlusion_ = on;
        InvalidateLightingScene();
        on_changed();
    });

    auto* controllers = new QCheckBox(QStringLiteral("Block through controller bodies"), w);
    controllers->setChecked(use_controller_occlusion_);
    layout->addWidget(controllers);
    connect(controllers, &QCheckBox::toggled, this, [this, on_changed](bool on) {
        use_controller_occlusion_ = on;
        InvalidateLightingScene();
        on_changed();
    });

    auto* walls = new QCheckBox(QStringLiteral("Include room walls as blockers"), w);
    walls->setChecked(use_room_walls_);
    layout->addWidget(walls);
    connect(walls, &QCheckBox::toggled, this, [this, on_changed](bool on) {
        use_room_walls_ = on;
        InvalidateLightingScene();
        on_changed();
    });

    auto* hint = new QLabel(
        QStringLiteral(
            "Use manual room size in Grid settings so corners match the viewport box. "
            "Spatial anchor / reference does not move the fire. Glow size sets the bright core; "
            "Light reach sets how far direct light travels. Turn Room fill down to see shadows. "
            "Planes and controllers block light; uncheck room walls if the whole room goes dark."),
        w);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    AddWidgetToParent(w, parent);
}

void RoomCampfireEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

SpatialLighting::Vec3 RoomCampfireEffect3D::PlacementPosition(const GridContext3D& grid) const
{
    const float mx = grid.width * kMarginFrac;
    const float my = grid.height * kMarginFrac;
    const float mz = grid.depth * kMarginFrac;

    switch(placement_mode_)
    {
    case 1:
        return {grid.center_x, grid.center_y, grid.center_z};
    case 2:
        return {grid.max_x - mx, grid.max_y - my, grid.max_z - mz};
    case 3:
        return {grid.min_x + grid.width * custom_u_,
                grid.min_y + grid.height * custom_v_,
                grid.min_z + grid.depth * custom_w_};
    case 0:
    default:
        return {grid.min_x + mx, grid.min_y + my, grid.min_z + mz};
    }
}

void RoomCampfireEffect3D::RefreshOccluders(const GridContext3D& grid) const
{
    SpatialLighting::OccluderBuildOptions options{};
    options.display_planes = use_occlusion_;
    options.room_walls = use_occlusion_ && use_room_walls_;
    options.controllers = use_occlusion_ && use_controller_occlusion_;
    SpatialLighting::BuildSpatialOccluders(occluders_, occluder_aabbs_, grid, options);
    cached_scene_.occluders = occluders_;
    cached_scene_.occluder_aabbs = occluder_aabbs_;
}

void RoomCampfireEffect3D::RebuildScene(const GridContext3D& grid)
{
    frozen_light_pos_ = PlacementPosition(grid);
    frozen_light_valid_ = true;

    cached_scene_.source.position = frozen_light_pos_;
    UpdateSourceFromSliders(grid);
    ApplyLiveShadeSettings(cached_scene_);
    MarkLightingSceneBuilt(grid.render_sequence);
}

RGBColor RoomCampfireEffect3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    static thread_local std::uint64_t shade_prepared_for = 0;
    if(shade_prepared_for != grid.render_sequence)
    {
        RefreshOccluders(grid);
        if(!frozen_light_valid_ || LightingSceneEpochChanged(grid.render_sequence))
        {
            RebuildScene(grid);
        }
        else
        {
            cached_scene_.source.position = frozen_light_pos_;
            UpdateSourceFromSliders(grid);
            ApplyLiveShadeSettings(cached_scene_);
        }
        shade_prepared_for = grid.render_sequence;
    }

    RGBColor base = SpatialLighting::ShadeLed(cached_scene_, x, y, z);

    const float speed_norm = GetNormalizedSpeed();
    if(speed_norm > 0.001f)
    {
        const float flicker =
            0.88f + 0.12f * std::sin(time * (2.0f + speed_norm * 8.0f) * 6.28318530718f) *
                        std::sin(time * (1.3f + speed_norm * 5.0f) * 3.14159265f);
        const float fr = static_cast<float>((base >> 16) & 0xFF) * flicker;
        const float fg = static_cast<float>((base >> 8) & 0xFF) * flicker;
        const float fb = static_cast<float>(base & 0xFF) * flicker;
        base = ToRGBColor((int)std::min(255.0f, fr), (int)std::min(255.0f, fg), (int)std::min(255.0f, fb));
    }

    return base;
}

nlohmann::json RoomCampfireEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["room_campfire"]["placement"] = placement_mode_;
    j["room_campfire"]["custom_u"] = custom_u_;
    j["room_campfire"]["custom_v"] = custom_v_;
    j["room_campfire"]["custom_w"] = custom_w_;
    j["room_campfire"]["occlusion"] = use_occlusion_;
    j["room_campfire"]["controller_occlusion"] = use_controller_occlusion_;
    j["room_campfire"]["room_walls"] = use_room_walls_;
    j["room_campfire"]["ao"] = ao_strength_;
    j["room_campfire"]["glow_mm"] = glow_radius_mm_;
    j["room_campfire"]["reach_mm"] = light_reach_mm_;
    j["room_campfire"]["room_fill"] = room_fill_;
    return j;
}

void RoomCampfireEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("room_campfire") && settings["room_campfire"].is_object())
    {
        const auto& rc = settings["room_campfire"];
        if(rc.contains("placement"))
        {
            placement_mode_ = rc["placement"].get<int>();
        }
        if(rc.contains("custom_u"))
        {
            custom_u_ = rc["custom_u"].get<float>();
        }
        if(rc.contains("custom_v"))
        {
            custom_v_ = rc["custom_v"].get<float>();
        }
        if(rc.contains("custom_w"))
        {
            custom_w_ = rc["custom_w"].get<float>();
        }
        if(rc.contains("occlusion"))
        {
            use_occlusion_ = rc["occlusion"].get<bool>();
        }
        if(rc.contains("controller_occlusion"))
        {
            use_controller_occlusion_ = rc["controller_occlusion"].get<bool>();
        }
        if(rc.contains("room_walls"))
        {
            use_room_walls_ = rc["room_walls"].get<bool>();
        }
        if(rc.contains("ao"))
        {
            ao_strength_ = rc["ao"].get<float>();
        }
        if(rc.contains("glow_mm"))
        {
            glow_radius_mm_ = rc["glow_mm"].get<float>();
        }
        if(rc.contains("reach_mm"))
        {
            light_reach_mm_ = rc["reach_mm"].get<float>();
        }
        if(rc.contains("room_fill"))
        {
            room_fill_ = rc["room_fill"].get<float>();
        }
        if(rc.contains("viewer_rim"))
        {
            (void)rc["viewer_rim"];
        }
    }
    frozen_light_valid_ = false;
    InvalidateLightingScene();
}
