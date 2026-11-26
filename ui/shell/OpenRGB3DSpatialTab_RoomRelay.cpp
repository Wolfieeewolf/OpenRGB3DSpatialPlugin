// SPDX-License-Identifier: GPL-2.0-or-later
/* Emitter + relay receivers, lit from what the emitters actually show.

   Runs after a frame has given every LED its final colour (end of the effect stack, and
   right before an Action / trigger / Movie Sync frame is flushed), so anything that drives
   the emitters drives the receivers too. Uses the shared room light model
   (shared/SpatialLighting/RoomLight): emitter LEDs become a few hundred lights, the
   blocker shadows and falloff for every receiver are cached per layout, and each frame is
   a short weighted sum in linear light. */

#include "OpenRGB3DSpatialTab.h"

#include "ActionColorMath.h"
#include "ControllerLayout3D.h"
#include "GridSpaceUtils.h"
#include "LEDPosition3D.h"
#include "RoomSpatialLightingUi.h"
#include "SpatialLighting/RoomLight.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "VirtualController3D.h"
#include "LogManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
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

/* What the blockers actually are: the relay cache only rebuilds when this changes, not on
   every scene invalidate (those happen on many UI changes that move nothing). Hashed once
   per occluder rebuild. */
struct HashMix
{
    std::uint64_t h = 1469598103934665603ull;
    void Word(std::uint64_t v)
    {
        h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    }
    void F(float f)
    {
        std::uint32_t u = 0;
        std::memcpy(&u, &f, sizeof(u));
        Word(u);
    }
    template<typename T>
    void Bytes(const std::vector<T>& v)
    {
        Word(v.size());
        const std::size_t n = v.size() * sizeof(T);
        const unsigned char* p = (const unsigned char*)v.data();
        std::size_t i = 0;
        for(; i + 8 <= n; i += 8)
        {
            std::uint64_t w = 0;
            std::memcpy(&w, p + i, 8);
            Word(w);
        }
        std::uint64_t tail = 0;
        if(i < n)
        {
            std::memcpy(&tail, p + i, n - i);
        }
        Word(tail);
    }
};

std::uint64_t BlockerSignature(SpatialLightingSceneProvider* provider)
{
    static std::uint64_t s_epoch = ~0ull;
    static std::uint64_t s_sig = 0;
    if(provider->sceneGeometryEpoch() == s_epoch)
    {
        return s_sig;
    }
    s_epoch = provider->sceneGeometryEpoch();
    HashMix m;
    const auto& aabbs = provider->frameOccluderAabbs();
    m.Word(aabbs.size());
    for(const SpatialLighting::OccluderAabb& a : aabbs)
    {
        m.F(a.min.x); m.F(a.min.y); m.F(a.min.z);
        m.F(a.max.x); m.F(a.max.y); m.F(a.max.z);
        m.Word((std::uint64_t)(std::int64_t)a.controller_index);
    }
    const auto& quads = provider->frameOccluderQuads();
    m.Word(quads.size());
    for(const SpatialLighting::OccluderQuad& q : quads)
    {
        for(const auto& c : q.corners)
        {
            m.F(c.x); m.F(c.y); m.F(c.z);
        }
        m.Word(q.double_sided ? 1u : 0u);
    }
    const auto& grids = provider->frameBlockerGrids();
    m.Word(grids.size());
    for(const SpatialLighting::BlockerGridOccluder& g : grids)
    {
        m.F(g.world_min.x); m.F(g.world_min.y); m.F(g.world_min.z);
        m.F(g.world_max.x); m.F(g.world_max.y); m.F(g.world_max.z);
        m.Word((std::uint64_t)g.width | ((std::uint64_t)g.height << 20) | ((std::uint64_t)g.depth << 40));
        m.Bytes(g.dense_cells);
        /* The grid hit test follows its controller's live transform (rotation included). */
        const auto* transforms = provider->controllers();
        if(transforms && g.controller_index >= 0 && g.controller_index < (int)transforms->size()
           && (*transforms)[(size_t)g.controller_index])
        {
            const Transform3D& t = (*transforms)[(size_t)g.controller_index]->transform;
            m.F(t.position.x); m.F(t.position.y); m.F(t.position.z);
            m.F(t.rotation.x); m.F(t.rotation.y); m.F(t.rotation.z);
            m.F(t.scale.x); m.F(t.scale.y); m.F(t.scale.z);
        }
    }
    const SpatialLighting::RoomBlockerField& f = provider->frameRoomBlockerField();
    m.F(f.origin_x); m.F(f.origin_y); m.F(f.origin_z); m.F(f.cell_size);
    m.Word((std::uint64_t)f.width | ((std::uint64_t)f.height << 20) | ((std::uint64_t)f.depth << 40));
    m.Bytes(f.cells);
    s_sig = m.h;
    return s_sig;
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
       painted (a show that drives a receiver directly is left alone). Checked before any
       setup, so frames that do not touch the emitters cost nothing. */
    if(touched)
    {
        /* Any enabled relay layer works with every source, stack playing or not: Action
           Editor shows, triggers and Movie Sync drive its emitters, and its receivers follow.
           Turn the layer off (or untick its receivers) to drive those devices directly. */
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

    /* Reports to the log (every ~10 s) only when the pass is costing real frame time. */
    struct PassTimer
    {
        std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        ~PassTimer()
        {
            static double s_total_ms = 0.0;
            static double s_worst_ms = 0.0;
            static int s_calls = 0;
            static auto s_since = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(now - t0).count();
            s_total_ms += ms;
            s_worst_ms = std::max(s_worst_ms, ms);
            ++s_calls;
            if(std::chrono::duration<double>(now - s_since).count() >= 10.0)
            {
                const double avg = s_total_ms / std::max(1, s_calls);
                if(avg > 2.0)
                {
                    LOG_INFO("[OpenRGB3DSpatialPlugin] Emitter + relay pass: %d calls in 10 s, %.2f ms average, "
                             "%.1f ms worst",
                             s_calls, avg, s_worst_ms);
                }
                s_total_ms = 0.0;
                s_worst_ms = 0.0;
                s_calls = 0;
                s_since = now;
            }
        }
    } pass_timer;

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
        return t;
    };

    /* Blockers, built once for every relay that wants shadows. */
    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    provider->SetControllers(&controller_transforms);
    SpatialLighting::OccluderBuildOptions occ{};
    for(SpatialEffect3D* relay : relays)
    {
        const RoomSpatialLightingUi::RoomSpatialLightParams& lp = relay->roomRelayParams();
        if(!lp.use_occlusion)
        {
            continue;
        }
        const SpatialLighting::OccluderBuildOptions o = RoomSpatialLightingUi::BuildOccluderOptions(lp);
        occ.display_planes |= o.display_planes;
        occ.scene_props |= o.scene_props;
        occ.room_walls |= o.room_walls;
        occ.controllers |= o.controllers;
        occ.light_blockers |= o.light_blockers;
    }
    if(occ.display_planes || occ.scene_props || occ.room_walls || occ.controllers || occ.light_blockers)
    {
        provider->EnsureFrameOccluders(room_grid, occ);
    }
    const auto& aabbs = provider->frameOccluderAabbs();
    const auto& quads = provider->frameOccluderQuads();
    const auto& grids = provider->frameBlockerGrids();
    const SpatialLighting::RoomBlockerField* field =
        provider->frameRoomBlockerField().IsValid() ? &provider->frameRoomBlockerField() : nullptr;
    const SpatialLighting::OccluderSpatialIndex* index = &provider->frameOccluderSpatialIndex();
    const bool has_blockers = !aabbs.empty() || !quads.empty() || !grids.empty() || field;
    const std::uint64_t blocker_sig = has_blockers ? BlockerSignature(provider) : 0;

    std::unordered_map<LEDPosition3D*, size_t> out_slot;
    std::vector<ReceiverOut> outs;
    std::vector<Vector3D> emit_pos;
    std::vector<int> emit_owner;
    std::vector<int> recv_owner;
    std::vector<char> emit_exact;
    std::vector<char> recv_exact;
    /* Devices with their own light blocker housing (speakers, keyboards, ...): their LEDs are
       lit and light exactly, one by one, and their housing shapes where light goes. */
    const auto has_housing = [](const ControllerTransform* t) {
        return t && t->virtual_controller && !t->virtual_controller->GetLightBlockers().empty();
    };
    std::vector<float> emit_lin;
    std::vector<Vector3D> recv_pos;
    std::vector<LEDPosition3D*> recv_led;
    std::vector<float> lit;

    for(SpatialEffect3D* relay : relays)
    {
        const RoomSpatialLightingUi::RoomSpatialLightParams& lp = relay->roomRelayParams();

        emit_pos.clear();
        emit_owner.clear();
        emit_exact.clear();
        emit_lin.clear();
        std::unordered_set<int> emitter_set;
        for(int idx : relay->roomEmitterControllerIndices())
        {
            ControllerTransform* t = usable(idx);
            if(!t || !emitter_set.insert(idx).second)
            {
                continue;
            }
            if(t->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(t);
            }
            const char exact = has_housing(t) ? 1 : 0;
            for(const LEDPosition3D& led : t->led_positions)
            {
                const RGBColor c = ShownColor(led);
                emit_pos.push_back(led.room_position);
                emit_owner.push_back(idx);
                emit_exact.push_back(exact);
                emit_lin.push_back(Action::colormath::Srgb8ToLinear(RGBGetRValue(c)));
                emit_lin.push_back(Action::colormath::Srgb8ToLinear(RGBGetGValue(c)));
                emit_lin.push_back(Action::colormath::Srgb8ToLinear(RGBGetBValue(c)));
            }
        }

        recv_pos.clear();
        recv_owner.clear();
        recv_exact.clear();
        recv_led.clear();
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
            if(t->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(t);
            }
            const char exact = has_housing(t) ? 1 : 0;
            for(LEDPosition3D& led : t->led_positions)
            {
                recv_pos.push_back(led.room_position);
                recv_owner.push_back(idx);
                recv_exact.push_back(exact);
                recv_led.push_back(&led);
            }
        }
        if(recv_led.empty())
        {
            continue;
        }

        /* Light reach and Glow size are distances; Room fill sets the exposure. */
        const float reach_mm = std::max(40.0f, lp.light_reach_mm);
        const float core_mm = std::clamp(lp.glow_radius_mm, 1.0f, reach_mm);
        RoomLight::RelaySettings settings;
        settings.reach = std::max(1e-3f, MMToGridUnits(reach_mm, grid_scale_mm));
        /* ~6 cm lights, ~8 cm receiver shadow groups: fixed, so dragging Reach never moves
           a light or re-aims a shadow ray. */
        settings.light_cell = std::max(1e-3f, MMToGridUnits(60.0f, grid_scale_mm));
        settings.group_cell = std::max(1e-3f, MMToGridUnits(80.0f, grid_scale_mm));
        settings.core2 = std::clamp((core_mm / reach_mm) * (core_mm / reach_mm), 1e-5f, 1.0f);
        /* 0% = no light, 35% (default) = the Real light overlay's exposure, 100% ~ 3x that. */
        settings.exposure = 1.4f * std::clamp(lp.room_fill, 0.0f, 100.0f) / 35.0f;
        settings.occlusion = lp.use_occlusion && has_blockers;
        settings.ambient_occlusion = settings.occlusion && lp.ao_strength > 0.01f;

        const float ao_strength = lp.ao_strength / 100.0f;
        const float room_diag = std::sqrt(room_grid.width * room_grid.width + room_grid.height * room_grid.height
                                          + room_grid.depth * room_grid.depth);
        const float ao_span = std::clamp(std::max(settings.reach * 0.4f, room_diag * 0.05f), 0.5f, 18.0f);

        /* Everything else in the room (other devices' housings, display planes, props, walls),
           then each end's own housing walked exactly from its LED: light leaves a speaker only
           through its openings, and reaches a keyboard only from the side its keys face. */
        const RoomLight::SegmentBlockedFn blocked = [&](const Vector3D& a, int a_owner, const Vector3D& b, int b_owner) {
            if(SpatialLighting::SegmentIsOccludedSkip2(a.x, a.y, a.z, b.x, b.y, b.z, aabbs, quads, grids, field,
                                                       index, a_owner, b_owner, true))
            {
                return true;
            }
            return SpatialLighting::SegmentBlockedByOwnHousing(a.x, a.y, a.z, b.x, b.y, b.z, a_owner)
                   || SpatialLighting::SegmentBlockedByOwnHousing(b.x, b.y, b.z, a.x, a.y, a.z, b_owner);
        };
        settings.ambient_strength = ao_strength;
        settings.ambient_span = ao_span;
        settings.ambient_centre = room_center;
        const RoomLight::AmbientFn ambient = [&](const Vector3D& p, int owner) {
            return SpatialLighting::ComputeRoomAmbientShadeFactorSkip(p.x, p.y, p.z, room_center.x, room_center.y,
                                                                      room_center.z, aabbs, quads, grids, ao_strength,
                                                                      ao_span, index, field, owner, true);
        };

        RelayCacheEntry& entry = models[relay];
        entry.last_pass = pass;
        const auto t0 = std::chrono::steady_clock::now();
        if(entry.model.Prepare(emit_pos, emit_owner, recv_pos, recv_owner, emit_exact, recv_exact, settings, blocker_sig, blocked, ambient))
        {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            /* At most one line every 5 s (dragging a device can rebuild every mouse move). */
            static auto s_last_log = std::chrono::steady_clock::time_point{};
            const auto log_now = std::chrono::steady_clock::now();
            if(log_now - s_last_log >= std::chrono::seconds(5))
            {
            s_last_log = log_now;
            LOG_INFO("[OpenRGB3DSpatialPlugin] Emitter + relay light cache rebuilt: %d emitter lights, %d receiver LEDs "
                     "in %d groups, %d taps, shadows %s, %.1f ms",
                     entry.model.LightCount(), (int)recv_pos.size(), entry.model.GroupCount(),
                     (int)entry.model.TapCount(), settings.occlusion ? "on" : "off", ms);
            }
        }
        entry.model.Shade(emit_lin.empty() ? nullptr : emit_lin.data(), lit);
        if(lit.size() < recv_led.size() * 3u)
        {
            lit.assign(recv_led.size() * 3u, 0.0f);
        }

        for(size_t i = 0; i < recv_led.size(); ++i)
        {
            /* No Brightness / Sharpness here: the emitters already show them (the relay layer's
               own pattern is post-processed on the emitters), and applying them again dimmed
               receivers twice. Receivers follow what the emitters show. */
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
