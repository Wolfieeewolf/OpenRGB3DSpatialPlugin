// SPDX-License-Identifier: GPL-2.0-only

#include "RoomEmissiveRelayEffect3D.h"

#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "RoomSpatialLightSettingsPanel.h"
#include "LEDPosition3D.h"
#include "GridSpaceUtils.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include "ControllerDisplayUtils.h"

#include <algorithm>

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(RoomEmissiveRelayEffect3D);

namespace
{

constexpr const char* kRelayHint =
    "Place pattern effects below this layer on emitter devices only. "
    "This layer shades the rest of the room from those LED colors (occlusion, fill, AO). "
    "Emitters keep the pattern from layers below; use Add blend on this layer.";

} // namespace

RoomEmissiveRelayEffect3D::RoomEmissiveRelayEffect3D(QWidget* parent) : RoomSpatialLightingEffect3D(parent)
{
    SetReferenceMode(REF_MODE_USER_POSITION);
    SetRainbowMode(false);
    room_light_.glow_radius_mm = 22.0f;
    room_light_.light_reach_mm = 520.0f;
    room_light_.room_fill = 42.0f;
    room_light_.ao_strength = 55.0f;
    room_light_.use_occlusion = true;
    room_light_.use_controller_occlusion = true;
}

EffectInfo3D RoomEmissiveRelayEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Room emissive relay";
    info.effect_description =
        "Keyboard (or chosen devices) run effects below; other LEDs receive shaded light with occlusion.";
    info.category = SpatialRoom::LibraryGroupForMode(SpatialRoom::SpatialRoomMode::EmissiveRelay);
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.show_axis_control = false;
    info.show_speed_control = false;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_position_offset_control = false;
    info.show_brightness_control = true;
    info.show_color_controls = false;
    info.supports_strip_colormap = false;
    info.supports_height_bands = false;
    info.show_room_output_control = false;
    return info;
}

void RoomEmissiveRelayEffect3D::setEmitterControllerIndex(int index, bool enabled)
{
    auto it = std::find(emitter_controller_indices_.begin(), emitter_controller_indices_.end(), index);
    if(enabled)
    {
        if(it == emitter_controller_indices_.end())
        {
            emitter_controller_indices_.push_back(index);
        }
    }
    else if(it != emitter_controller_indices_.end())
    {
        emitter_controller_indices_.erase(it);
    }
}

bool RoomEmissiveRelayEffect3D::isEmitterControllerIndex(int index) const
{
    return std::find(emitter_controller_indices_.begin(), emitter_controller_indices_.end(), index) !=
           emitter_controller_indices_.end();
}

void RoomEmissiveRelayEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("RoomEmissiveRelaySettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);

    auto* hint = new QLabel(QString::fromUtf8(kRelayHint));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* emitters_group = new QGroupBox(tr("Emitter devices"));
    auto* emitters_layout = new QVBoxLayout(emitters_group);
    emitters_group->setObjectName(QStringLiteral("emitterDevicesGroup"));

    const auto* transforms = SpatialLightingSceneProvider::instance()->controllers();
    if(transforms)
    {
        for(size_t i = 0; i < transforms->size(); ++i)
        {
            const ControllerTransform* t = (*transforms)[i].get();
            if(!t || t->hidden_by_virtual)
            {
                continue;
            }
            const QString label = ControllerDisplay::FormatControllerTransformLabel(t, static_cast<int>(i));
            auto* box = new QCheckBox(label);
            box->setChecked(isEmitterControllerIndex((int)i));
            const int ctrl_idx = (int)i;
            connect(box, &QCheckBox::toggled, this, [this, ctrl_idx](bool on) {
                setEmitterControllerIndex(ctrl_idx, on);
                InvalidateRelayScene();
                emit ParametersChanged();
            });
            emitters_layout->addWidget(box);
        }
    }

    layout->addWidget(emitters_group);

    auto* light_panel = new RoomSpatialLightSettingsPanel();
    light_panel->setObjectName(QStringLiteral("relayLightPanel"));
    light_panel->setShowRoomFill(true);
    light_panel->setShowPlacement(false);
    light_panel->setHintText(tr("Occlusion and fill for light cast from emitter LEDs."));
    const auto on_tune = [this]() {
        InvalidateRelayScene();
        emit ParametersChanged();
    };
    light_panel->bindParams(this, room_light_, on_tune, on_tune);
    light_panel->syncFromParams(room_light_);
    layout->addWidget(light_panel);

    AddWidgetToParent(w, parent);
}

void RoomEmissiveRelayEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

void RoomEmissiveRelayEffect3D::ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const
{
    RoomSpatialLightingEffect3D::ApplyLiveShadeSettings(scene);
    scene.shade.ambient_level = 0.04f;
    scene.shade.direct_falloff = 0.85f;
}

void RoomEmissiveRelayEffect3D::RefreshRelayOccluders(const GridContext3D& grid) const
{
    RefreshRoomLightOccluders(grid);
    relay_scene_occluders_valid_ = true;
}

RGBColor RoomEmissiveRelayEffect3D::ShadeRelayAt(float x, float y, float z, const GridContext3D& grid) const
{
    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    if(!provider->emitterRelayActive() || provider->emitterRelaySources().empty())
    {
        return 0x00000000;
    }

    static thread_local std::uint64_t shade_prepared_for = 0;
    if(shade_prepared_for != grid.render_sequence || !relay_scene_occluders_valid_)
    {
        RefreshRelayOccluders(grid);
        cached_scene_.sources = provider->emitterRelaySources();
        cached_scene_.source = cached_scene_.sources.empty() ? SpatialLighting::EmissiveSource{}
                                                             : cached_scene_.sources.front();
        ApplyLiveShadeSettings(cached_scene_);
        const float room_diag =
            std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
        cached_scene_.shade.ao_probe_span = std::clamp(room_diag * 0.035f, 0.35f, 8.0f);
        shade_prepared_for = grid.render_sequence;
    }

    return SpatialLighting::ShadeLed(cached_scene_, x, y, z);
}

RGBColor RoomEmissiveRelayEffect3D::CalculateColorGrid(float x,
                                                         float y,
                                                         float z,
                                                         float time,
                                                         const GridContext3D& grid)
{
    (void)time;
    const int shading_ctrl = SpatialLightingSceneProvider::instance()->shadingControllerIndex();
    if(shading_ctrl >= 0 && isEmitterControllerIndex(shading_ctrl))
    {
        return 0x00000000;
    }
    return ShadeRelayAt(x, y, z, grid);
}

nlohmann::json RoomEmissiveRelayEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    RoomSpatialLightingUi::SaveParamsToJson(j, "room_emissive_relay", room_light_);
    j["emitter_controllers"] = emitter_controller_indices_;
    return j;
}

void RoomEmissiveRelayEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    RoomSpatialLightingUi::LoadParamsFromJson(settings, "room_emissive_relay", "room_light_probe", room_light_);
    emitter_controller_indices_.clear();
    if(settings.contains("emitter_controllers") && settings["emitter_controllers"].is_array())
    {
        for(const auto& v : settings["emitter_controllers"])
        {
            if(v.is_number_integer())
            {
                emitter_controller_indices_.push_back(v.get<int>());
            }
        }
    }
    InvalidateRelayScene();
}
