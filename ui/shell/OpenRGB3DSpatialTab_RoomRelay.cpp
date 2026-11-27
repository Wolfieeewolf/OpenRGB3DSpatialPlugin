// SPDX-License-Identifier: GPL-2.0-or-later
/* Emitter + relay receivers, lit from what the emitters actually show.

   Runs after a frame has given every LED its final colour (end of the effect stack, and
   right before an Action / trigger / Movie Sync frame is flushed), so anything that drives
   the emitters drives the receivers too. Uses the shared light engine (RoomLight /
   RoomLightScene, the same lights and ray maps as the Real light overlay), so what the overlay
   shows is what the receivers get. Per receiver the falloff and shadows are cached; each frame
   is a short weighted sum in linear light. */

#include "OpenRGB3DSpatialTab.h"

#include "ActionColorMath.h"
#include "ControllerLayout3D.h"
#include "GridSpaceUtils.h"
#include "LEDPosition3D.h"
#include "RoomSpatialLightingUi.h"
#include "SpatialLighting/LightOutput.h"
#include "SpatialLighting/RoomLight.h"
#include "SpatialLighting/RoomLightScene.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "VirtualController3D.h"
#include "LogManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{

struct RelayCacheEntry
{
    RoomLight::RelayModel model;
    std::uint64_t last_pass = 0;
};

std::unordered_map<const SpatialEffect3D*, RelayCacheEntry>& RelayModels()
{
    static std::unordered_map<const SpatialEffect3D*, RelayCacheEntry> models;
    return models;
}

bool GlobalLedIndex(RGBControllerInterface* c, unsigned int zone, unsigned int led, unsigned int* out)
{
    if(!c || zone >= c->GetZoneCount() || led >= c->GetZoneLEDsCount(zone))
    {
        return false;
    }
    *out = c->GetZoneStartIndex(zone) + led;
    return *out < c->GetLEDCount();
}

/** What the LED is showing (preview colour, or the device's own when unset). */
RGBColor ShownColor(const LEDPosition3D& led)
{
    RGBColor c = led.preview_color;
    unsigned int global = 0;
    if(c == 0x00FFFFFF && led.controller && GlobalLedIndex(led.controller, led.zone_idx, led.led_idx, &global))
    {
        c = led.controller->GetColor(global);
    }
    return c;
}

struct ReceiverOut
{
    LEDPosition3D* led = nullptr;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

} // namespace

void OpenRGB3DSpatialTab::ApplyRoomRelayReceivers(std::unordered_set<RGBControllerInterface*>* touched)
{
    static std::uint64_t s_pass = 0;
    const std::uint64_t pass = ++s_pass;
    auto& models = RelayModels();

    /* Every enabled Emitter + relay layer in the stack, whether or not the stack is playing:
       the Action Editor and triggers can drive the emitters on their own. */
    std::vector<SpatialEffect3D*> relays;
    bool any_enabled = false;
    for(const std::unique_ptr<EffectInstance3D>& instance : effect_stack)
    {
        if(!instance || !instance->enabled || !instance->effect)
        {
            continue;
        }
        any_enabled = true;
        if(instance->effect->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
        {
            relays.push_back(instance->effect.get());
        }
    }
    /* Same rule as RenderEffectStack: the UI effect only plays when the stack has nothing enabled. */
    if(!any_enabled && effect_running && current_effect_ui
       && current_effect_ui->GetRoomOutputRole() == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
    {
        relays.push_back(current_effect_ui);
    }
    if(relays.empty() || controller_transforms.empty())
    {
        models.clear();
        return;
    }

    /* From an Action / trigger / Movie Sync frame: only relays whose emitters that frame
       painted. Checked before any setup, so frames that do not touch the emitters cost nothing.
       (Any enabled relay layer works with every source, stack playing or not; turn the layer
       off, or untick its receivers, to drive those devices directly.) */
    if(touched)
    {
        if(touched->empty())
        {
            return;
        }
        std::vector<SpatialEffect3D*> painted;
        for(SpatialEffect3D* relay : relays)
        {
            bool hit = false;
            for(int idx : relay->roomEmitterControllerIndices())
            {
                if(idx < 0 || idx >= (int)controller_transforms.size() || !controller_transforms[(size_t)idx])
                {
                    continue;
                }
                for(const LEDPosition3D& led : controller_transforms[(size_t)idx]->led_positions)
                {
                    if(led.controller && touched->count(led.controller))
                    {
                        hit = true;
                        break;
                    }
                }
                if(hit)
                {
                    break;
                }
            }
            if(hit)
            {
                painted.push_back(relay);
            }
            else if(models.count(relay))
            {
                models[relay].last_pass = pass + 1; /* survives this pass's prune */
            }
        }
        if(painted.empty())
        {
            return;
        }
        relays.swap(painted);
    }
    /* Every relay's emitters show the stack pattern, so none of them is ever a receiver. */
    std::unordered_set<int> all_emitters;
    for(SpatialEffect3D* relay : relays)
    {
        all_emitters.insert(relay->roomEmitterControllerIndices().begin(), relay->roomEmitterControllerIndices().end());
    }

    ManualRoomSettings room_settings = MakeManualRoomSettings(true, manual_room_width, manual_room_height, manual_room_depth);
    const GridBounds bounds = ComputeGridBounds(room_settings, grid_scale_mm, controller_transforms);
    GridContext3D room_grid(bounds.min_x, bounds.max_x, bounds.min_y, bounds.max_y, bounds.min_z, bounds.max_z,
                            grid_scale_mm);
    const Vector3D room_center{room_grid.center_x, room_grid.center_y, room_grid.center_z};

    /* Physical controllers driven through a virtual one are painted via the virtual. */
    std::unordered_set<RGBControllerInterface*> managed_by_virtual;
    for(const std::unique_ptr<ControllerTransform>& t : controller_transforms)
    {
        if(t && t->virtual_controller)
        {
            for(const GridLEDMapping& m : t->virtual_controller->GetMappings())
            {
                if(m.controller)
                {
                    managed_by_virtual.insert(m.controller);
                }
            }
        }
    }
    const auto usable = [&](int idx) -> ControllerTransform* {
        if(idx < 0 || idx >= (int)controller_transforms.size())
        {
            return nullptr;
        }
        ControllerTransform* t = controller_transforms[(size_t)idx].get();
        if(!t || t->hidden_by_virtual)
        {
            return nullptr;
        }
        if(t->controller && managed_by_virtual.count(t->controller))
        {
            return nullptr;
        }
        if(t->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(t);
        }
        return t;
    };

    /* The room's blockers, once for every relay (the same ones the overlay uses). */
    SpatialLightingSceneProvider::instance()->SetControllers(&controller_transforms);
    RoomLightScene::Blockers blockers;
    RoomLightScene::Gather(&controller_transforms, grid_scale_mm, blockers);
    const RoomLight::RayTestFn receiver_housing = RoomLightScene::OwnHousingTest(blockers);
    const std::vector<SpatialLighting::OccluderAabb> no_aabbs;

    std::unordered_map<LEDPosition3D*, size_t> out_slot;
    std::vector<ReceiverOut> outs;
    std::vector<float> lit;

    for(SpatialEffect3D* relay : relays)
    {
        const RoomSpatialLightingUi::RoomSpatialLightParams& lp = relay->roomRelayParams();

        /* Emitters -> the shared lights (and their ray maps), with this frame's colours. */
        std::unordered_set<int> emitter_set;
        for(int idx : relay->roomEmitterControllerIndices())
        {
            if(usable(idx))
            {
                emitter_set.insert(idx);
            }
        }
        RoomLightScene::Leds emit_leds;
        RoomLightScene::CollectLeds(controller_transforms, [&](int ti) { return emitter_set.count(ti) > 0; }, emit_leds);
        std::vector<RoomLight::Light> lights;
        std::vector<int> led_light;
        std::vector<const float*> ray_maps;
        RoomLightScene::BuildLightsWithRayMaps(emit_leds, blockers, grid_scale_mm, 768, lights, led_light, ray_maps);
        std::vector<float> light_lin(lights.size() * 3u, 0.0f);
        for(std::size_t i = 0; i < emit_leds.led.size(); ++i)
        {
            const LEDPosition3D& led = *emit_leds.led[i];
            const RGBColor c = ShownColor(led);
            /* Real output: lumens at full white for this device's LEDs. */
            const float scale = LightOutput::LedScale(controller_transforms[(std::size_t)emit_leds.owner[i]].get(), led);
            float* out = &light_lin[(std::size_t)led_light[i] * 3u];
            out[0] += Action::colormath::Srgb8ToLinear(RGBGetRValue(c)) * scale;
            out[1] += Action::colormath::Srgb8ToLinear(RGBGetGValue(c)) * scale;
            out[2] += Action::colormath::Srgb8ToLinear(RGBGetBValue(c)) * scale;
        }

        /* Receivers. */
        std::vector<Vector3D> recv_pos;
        std::vector<int> recv_owner;
        std::vector<char> recv_housed;
        std::vector<LEDPosition3D*> recv_led;
        for(int idx = 0; idx < (int)controller_transforms.size(); ++idx)
        {
            if(all_emitters.count(idx) || !relay->isRoomReceiverController(idx) || relay->isRoomEmitterController(idx))
            {
                continue;
            }
            ControllerTransform* t = usable(idx);
            if(!t)
            {
                continue;
            }
            const char housed = RoomLightScene::HasHousing(t) ? 1 : 0;
            for(LEDPosition3D& led : t->led_positions)
            {
                recv_pos.push_back(led.world_position);
                recv_owner.push_back(idx);
                recv_housed.push_back(housed);
                recv_led.push_back(&led);
            }
        }
        if(recv_led.empty())
        {
            continue;
        }

        /* Light reach and Glow size are distances; Room fill and Room exposure set the exposure. */
        const float reach_mm = std::max(40.0f, lp.light_reach_mm);
        const float core_mm = std::clamp(lp.glow_radius_mm, 1.0f, reach_mm);
        RoomLight::RelaySettings settings;
        settings.reach = std::max(1e-3f, MMToGridUnits(reach_mm, grid_scale_mm));
        settings.core2 = std::clamp((core_mm / reach_mm) * (core_mm / reach_mm), 1e-5f, 1.0f);
        /* Room fill 0% = no light, 35% (default) = the overlay's exposure; Room exposure (grid
           settings) scales both the overlay and every relay. */
        settings.exposure = 1.4f * std::clamp(lp.room_fill, 0.0f, 100.0f) / 35.0f
                            * (viewport ? viewport->GetRoomExposure() : 1.0f);
        settings.occlusion = lp.use_occlusion;
        settings.ambient_occlusion = settings.occlusion && lp.ao_strength > 0.01f;

        const float ao_strength = lp.ao_strength / 100.0f;
        const float room_diag = std::sqrt(room_grid.width * room_grid.width + room_grid.height * room_grid.height
                                          + room_grid.depth * room_grid.depth);
        const float ao_span = std::clamp(std::max(settings.reach * 0.4f, room_diag * 0.05f), 0.5f, 18.0f);
        settings.ambient_strength = ao_strength;
        settings.ambient_span = ao_span;
        settings.ambient_centre = room_center;
        const RoomLight::AmbientFn ambient = [&](const Vector3D& p, int owner) {
            return SpatialLighting::ComputeRoomAmbientShadeFactorSkip(p.x, p.y, p.z, room_center.x, room_center.y,
                                                                      room_center.z, no_aabbs, blockers.quads,
                                                                      blockers.grids, ao_strength, ao_span, nullptr,
                                                                      nullptr, owner, true);
        };

        RelayCacheEntry& entry = models[relay];
        entry.last_pass = pass;
        const auto t0 = std::chrono::steady_clock::now();
        if(entry.model.Prepare(lights, ray_maps, RoomLight::SharedRayMaps().Signature(), recv_pos, recv_owner,
                               recv_housed, settings, receiver_housing, ambient))
        {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            /* At most one line every 5 s (dragging a device can rebuild every mouse move). */
            static auto s_last_log = std::chrono::steady_clock::time_point{};
            const auto log_now = std::chrono::steady_clock::now();
            if(ms > 20.0 && log_now - s_last_log >= std::chrono::seconds(5))
            {
                s_last_log = log_now;
                LOG_INFO("[OpenRGB3DSpatialPlugin] Emitter + relay light cache rebuilt: %d emitter lights, %d receiver "
                         "LEDs, %d taps, shadows %s, %.1f ms",
                         entry.model.LightCount(), (int)recv_pos.size(), (int)entry.model.TapCount(),
                         settings.occlusion ? "on" : "off", ms);
            }
        }
        entry.model.Shade(light_lin, lit);
        if(lit.size() < recv_led.size() * 3u)
        {
            lit.assign(recv_led.size() * 3u, 0.0f);
        }

        for(size_t i = 0; i < recv_led.size(); ++i)
        {
            /* Receivers follow what the emitters show (their Brightness is already on them). */
            auto it = out_slot.find(recv_led[i]);
            if(it == out_slot.end())
            {
                it = out_slot.emplace(recv_led[i], outs.size()).first;
                outs.push_back({recv_led[i], 0.0f, 0.0f, 0.0f});
            }
            /* Two relays on one receiver: their light adds. */
            ReceiverOut& o = outs[it->second];
            o.r += lit[i * 3u + 0];
            o.g += lit[i * 3u + 1];
            o.b += lit[i * 3u + 2];
        }
    }

    for(auto it = models.begin(); it != models.end();)
    {
        it = (it->second.last_pass >= pass) ? std::next(it) : models.erase(it);
    }

    for(const ReceiverOut& o : outs)
    {
        const RGBColor c = ToRGBColor(Action::colormath::LinearToSrgb8(o.r),
                                      Action::colormath::LinearToSrgb8(o.g),
                                      Action::colormath::LinearToSrgb8(o.b));
        LEDPosition3D& led = *o.led;
        led.preview_color = c;
        unsigned int global = 0;
        if(led.controller && GlobalLedIndex(led.controller, led.zone_idx, led.led_idx, &global))
        {
            led.controller->SetColor(global, c);
            if(touched)
            {
                touched->insert(led.controller);
            }
        }
    }
}
