// SPDX-License-Identifier: GPL-2.0-or-later

#include "RoomLight.h"

#include <cstring>
#include <functional>
#include <thread>
#include <unordered_map>

namespace RoomLight
{

namespace
{

uint64_t CellKey(float x, float y, float z, float inv)
{
    const int64_t ix = (int64_t)std::floor(x * inv);
    const int64_t iy = (int64_t)std::floor(y * inv);
    const int64_t iz = (int64_t)std::floor(z * inv);
    return ((uint64_t)(ix & 0x1FFFFF) << 42) | ((uint64_t)(iy & 0x1FFFFF) << 21) | (uint64_t)(iz & 0x1FFFFF);
}

/* 8 bytes per step (positions are hashed on every frame). */
void HashBytes(uint64_t& h, const void* data, std::size_t n)
{
    const unsigned char* p = (const unsigned char*)data;
    std::size_t i = 0;
    for(; i + 8 <= n; i += 8)
    {
        uint64_t w = 0;
        std::memcpy(&w, p + i, 8);
        h ^= w + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    }
    uint64_t tail = 0;
    if(i < n)
    {
        std::memcpy(&tail, p + i, n - i);
    }
    h ^= tail + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2) + n;
}

template<typename T>
void HashValue(uint64_t& h, const T& v)
{
    HashBytes(h, &v, sizeof(T));
}

constexpr int kMaxRelayLights = 768;

} // namespace

void ClusterLights(const std::vector<LitPoint>& lit, float reach, int max_lights, std::vector<LitPoint>& out)
{
    out.clear();
    if(lit.empty() || max_lights <= 0)
    {
        return;
    }
    struct Acc
    {
        float px = 0.0f, py = 0.0f, pz = 0.0f, w = 0.0f;
        float r = 0.0f, g = 0.0f, b = 0.0f;
    };
    float cell = std::max(1e-3f, reach * 0.22f);
    std::unordered_map<uint64_t, Acc> cells;
    for(int attempt = 0; attempt < 8; ++attempt)
    {
        cells.clear();
        cells.reserve(std::min<std::size_t>(lit.size(), (std::size_t)max_lights * 2u));
        const float inv = 1.0f / cell;
        for(const LitPoint& l : lit)
        {
            Acc& k = cells[CellKey(l.x, l.y, l.z, inv)];
            const float w = l.r + l.g + l.b + 1e-3f;
            k.px += l.x * w;
            k.py += l.y * w;
            k.pz += l.z * w;
            k.w += w;
            k.r += l.r;
            k.g += l.g;
            k.b += l.b;
        }
        if((int)cells.size() <= max_lights)
        {
            break;
        }
        cell *= 1.6f;
    }
    out.reserve(std::min<std::size_t>(cells.size(), (std::size_t)max_lights));
    for(const auto& kv : cells)
    {
        if((int)out.size() >= max_lights)
        {
            break;
        }
        const Acc& k = kv.second;
        out.push_back({k.px / k.w, k.py / k.w, k.pz / k.w, k.r, k.g, k.b});
    }
}

bool RelayModel::Prepare(const std::vector<Vector3D>& emitters,
                         const std::vector<int>& emitter_owner,
                         const std::vector<Vector3D>& receivers,
                         const std::vector<int>& receiver_owner,
                         const RelaySettings& settings,
                         std::uint64_t layout_epoch,
                         const SegmentBlockedFn& blocked,
                         const AmbientFn& ambient)
{
    exposure_ = settings.exposure;

    /* Cache stages, each redone only when its own inputs change:
         layout   (LED positions, owners, cell sizes)  -> lights and receiver groups
         blockers (shadows on/off, blocker geometry)    -> forget stored shadow results
         reach                                          -> which lights each group can see
                                                           (stored shadow results are reused;
                                                           only newly reached lights are tested)
         ambient  (AO settings)                         -> AO per group
         glow                                           -> falloff weights per LED
       Lights and groups use fixed cell sizes, never the reach, so moving a slider never
       moves a light or re-aims a shadow ray: shadows stay put while dragging. */
    uint64_t layout_sig = 1469598103934665603ull;
    HashValue(layout_sig, emitters.size());
    HashValue(layout_sig, receivers.size());
    if(!emitters.empty())
    {
        HashBytes(layout_sig, emitters.data(), emitters.size() * sizeof(Vector3D));
    }
    if(!receivers.empty())
    {
        HashBytes(layout_sig, receivers.data(), receivers.size() * sizeof(Vector3D));
    }
    if(!emitter_owner.empty())
    {
        HashBytes(layout_sig, emitter_owner.data(), emitter_owner.size() * sizeof(int));
    }
    if(!receiver_owner.empty())
    {
        HashBytes(layout_sig, receiver_owner.data(), receiver_owner.size() * sizeof(int));
    }
    HashValue(layout_sig, settings.light_cell);
    HashValue(layout_sig, settings.group_cell);

    uint64_t block_sig = layout_sig;
    HashValue(block_sig, settings.occlusion);
    if(settings.occlusion)
    {
        HashValue(block_sig, layout_epoch);
    }

    uint64_t reach_sig = block_sig;
    HashValue(reach_sig, settings.reach);

    uint64_t amb_sig = layout_sig;
    HashValue(amb_sig, settings.ambient_occlusion);
    if(settings.ambient_occlusion)
    {
        HashValue(amb_sig, settings.ambient_strength);
        HashValue(amb_sig, settings.ambient_span);
        HashValue(amb_sig, settings.ambient_centre);
        HashValue(amb_sig, layout_epoch);
    }

    uint64_t weight_sig = reach_sig;
    HashValue(weight_sig, settings.core2);

    const bool redo_layout = !valid_ || layout_sig != layout_sig_;
    const bool redo_block = redo_layout || block_sig != block_sig_;
    const bool redo_reach = redo_block || reach_sig != reach_sig_;
    const bool redo_amb = redo_layout || amb_sig != amb_sig_;
    const bool redo_weights = redo_reach || redo_amb || weight_sig != weight_sig_;
    if(!redo_weights)
    {
        return false;
    }
    layout_sig_ = layout_sig;
    block_sig_ = block_sig;
    reach_sig_ = reach_sig;
    amb_sig_ = amb_sig;
    weight_sig_ = weight_sig;
    valid_ = true;

    const float reach = std::max(1e-3f, settings.reach);
    const float r2 = reach * reach;

    unsigned int max_threads = std::max(1u, std::thread::hardware_concurrency());
    max_threads = std::min<unsigned int>(max_threads, 8u);

    /* Run fn(begin, end) over [0, count) on worker threads. A worker that fails, or one that
       cannot be started, leaves its range for the calling thread, so a thread error never
       takes the app down. */
    const auto parallel = [max_threads](std::size_t count, std::size_t work,
                                        const std::function<void(std::size_t, std::size_t)>& fn,
                                        const std::function<void(std::size_t, std::size_t)>& reset) {
        unsigned int threads = max_threads;
        if(work < 20000 || count < 8)
        {
            threads = 1;
        }
        if(threads <= 1)
        {
            fn(0, count);
            return;
        }
        std::vector<std::thread> pool;
        std::vector<std::pair<std::size_t, std::size_t>> redo;
        std::vector<char> failed(threads, 0);
        const std::size_t chunk = (count + threads - 1) / threads;
        for(unsigned int t = 0; t < threads; ++t)
        {
            const std::size_t b = (std::size_t)t * chunk;
            const std::size_t e = std::min(count, b + chunk);
            if(b >= e)
            {
                break;
            }
            try
            {
                pool.emplace_back([&fn, &failed, b, e, t]() {
                    try
                    {
                        fn(b, e);
                    }
                    catch(...)
                    {
                        failed[t] = 1;
                    }
                });
            }
            catch(...)
            {
                redo.emplace_back(b, e);
            }
        }
        for(std::thread& th : pool)
        {
            th.join();
        }
        for(unsigned int t = 0; t < threads; ++t)
        {
            if(failed[t])
            {
                const std::size_t b = (std::size_t)t * chunk;
                redo.emplace_back(b, std::min(count, b + chunk));
            }
        }
        for(const auto& range : redo)
        {
            reset(range.first, range.second);
            fn(range.first, range.second);
        }
    };

    if(redo_layout)
    {
        emitter_count_ = emitters.size();
        receiver_count_ = receivers.size();
        receivers_ = receivers;
        emitters_ = emitters;

        const auto owner_of = [](const std::vector<int>& owners, std::size_t i) {
            return i < owners.size() ? owners[i] : -1;
        };
        const auto owner_key = [](uint64_t key, int owner) {
            return key ^ ((uint64_t)(owner + 2) * 0x9E3779B97F4A7C15ull);
        };

        /* 1. Emitter LEDs -> lights, by position and controller. The cell only grows when a
              huge layout would exceed the light budget, never with the reach. */
        members_.clear();
        emitter_light_.assign(emitters.size(), 0);
        {
            float cell = std::max(1e-3f, settings.light_cell);
            std::unordered_map<uint64_t, int> index;
            for(int attempt = 0; attempt < 12; ++attempt)
            {
                index.clear();
                centre_.clear();
                light_owner_.clear();
                members_.clear();
                const float inv = 1.0f / cell;
                for(std::size_t i = 0; i < emitters.size(); ++i)
                {
                    const Vector3D& p = emitters[i];
                    const int owner = owner_of(emitter_owner, i);
                    const uint64_t key = owner_key(CellKey(p.x, p.y, p.z, inv), owner);
                    auto it = index.find(key);
                    int light = 0;
                    if(it == index.end())
                    {
                        light = (int)centre_.size();
                        index.emplace(key, light);
                        centre_.push_back({0.0f, 0.0f, 0.0f});
                        light_owner_.push_back(owner);
                        members_.emplace_back();
                    }
                    else
                    {
                        light = it->second;
                    }
                    centre_[(std::size_t)light].x += p.x;
                    centre_[(std::size_t)light].y += p.y;
                    centre_[(std::size_t)light].z += p.z;
                    members_[(std::size_t)light].push_back((int)i);
                    emitter_light_[i] = light;
                }
                if((int)centre_.size() <= kMaxRelayLights)
                {
                    break;
                }
                cell *= 1.4f;
            }
            for(std::size_t k = 0; k < centre_.size(); ++k)
            {
                const float n = (float)std::max<std::size_t>(1, members_[k].size());
                centre_[k].x /= n;
                centre_[k].y /= n;
                centre_[k].z /= n;
            }
        }
        light_count_ = (int)centre_.size();

        /* 2. Receivers near each other (same controller) share shadow tests. */
        receiver_group_.assign(receivers.size(), 0);
        group_centre_.clear();
        group_owner_.clear();
        {
            const float cell = std::max(1e-3f, settings.group_cell);
            const float inv = 1.0f / cell;
            std::unordered_map<uint64_t, int> index;
            std::vector<int> count;
            for(std::size_t i = 0; i < receivers.size(); ++i)
            {
                const Vector3D& p = receivers[i];
                const int owner = owner_of(receiver_owner, i);
                const uint64_t key = owner_key(CellKey(p.x, p.y, p.z, inv), owner);
                auto it = index.find(key);
                int g = 0;
                if(it == index.end())
                {
                    g = (int)group_centre_.size();
                    index.emplace(key, g);
                    group_centre_.push_back({0.0f, 0.0f, 0.0f});
                    group_owner_.push_back(owner);
                    count.push_back(0);
                }
                else
                {
                    g = it->second;
                }
                group_centre_[(std::size_t)g].x += p.x;
                group_centre_[(std::size_t)g].y += p.y;
                group_centre_[(std::size_t)g].z += p.z;
                ++count[(std::size_t)g];
                receiver_group_[i] = g;
            }
            for(std::size_t g = 0; g < group_centre_.size(); ++g)
            {
                const float n = (float)std::max(1, count[g]);
                group_centre_[g].x /= n;
                group_centre_[g].y /= n;
                group_centre_[g].z /= n;
            }
            group_radius_.assign(group_centre_.size(), 0.0f);
            for(std::size_t i = 0; i < receivers.size(); ++i)
            {
                const Vector3D& p = receivers[i];
                const Vector3D& c = group_centre_[(std::size_t)receiver_group_[i]];
                const float d =
                    std::sqrt((p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y) + (p.z - c.z) * (p.z - c.z));
                float& r = group_radius_[(std::size_t)receiver_group_[i]];
                r = std::max(r, d);
            }
        }
        group_count_ = (int)group_centre_.size();
    }

    /* 3. Blockers changed (or new layout): stored shadow results are stale. */
    if(redo_block)
    {
        pass_cache_.assign(group_centre_.size(), {});
    }

    /* 4. Per group: lights within reach of any of its LEDs, with the share of each light
          that gets past the blockers (centre plus the two ends of the light's LEDs, so
          shadows soften). Results are kept per (group, light): changing the reach only
          tests lights that were never tested before. */
    if(redo_reach)
    {
        const bool shadows = settings.occlusion && blocked;
        group_pass_.assign(group_centre_.size(), {});
        const auto build = [&](std::size_t begin, std::size_t end) {
            for(std::size_t g = begin; g < end; ++g)
            {
                const Vector3D& gc = group_centre_[g];
                const float reach_g = reach + group_radius_[g];
                std::vector<Tap>& out = group_pass_[g];
                std::unordered_map<int, float>& cache = pass_cache_[g];
                for(int k = 0; k < light_count_; ++k)
                {
                    const Vector3D& c = centre_[(std::size_t)k];
                    const float dx = gc.x - c.x;
                    const float dy = gc.y - c.y;
                    const float dz = gc.z - c.z;
                    if(dx * dx + dy * dy + dz * dz >= reach_g * reach_g)
                    {
                        continue;
                    }
                    float pass = 1.0f;
                    if(shadows)
                    {
                        auto hit = cache.find(k);
                        if(hit != cache.end())
                        {
                            pass = hit->second;
                        }
                        else
                        {
                            const std::vector<int>& m = members_[(std::size_t)k];
                            Vector3D probes[3] = {c, c, c};
                            int probe_count = 1;
                            if(m.size() >= 2)
                            {
                                probes[1] = emitters_[(std::size_t)m.front()];
                                probes[2] = emitters_[(std::size_t)m.back()];
                                probe_count = 3;
                            }
                            int open = 0;
                            for(int p = 0; p < probe_count; ++p)
                            {
                                if(!blocked(probes[p], light_owner_[(std::size_t)k], gc, group_owner_[g]))
                                {
                                    ++open;
                                }
                            }
                            pass = (float)open / (float)probe_count;
                            cache.emplace(k, pass);
                        }
                    }
                    if(pass > 0.0f)
                    {
                        out.push_back({k, pass});
                    }
                }
            }
        };
        const auto reset = [&](std::size_t begin, std::size_t end) {
            for(std::size_t g = begin; g < end; ++g)
            {
                group_pass_[g].clear();
            }
        };
        parallel(group_centre_.size(),
                 group_centre_.size() * (std::size_t)std::max(1, light_count_) * (shadows ? 8u : 1u),
                 build,
                 reset);
    }

    /* 5. Ambient occlusion per receiver group. */
    if(redo_amb)
    {
        group_ambient_.assign(group_centre_.size(), 1.0f);
        if(settings.ambient_occlusion && ambient)
        {
            const auto build = [&](std::size_t begin, std::size_t end) {
                for(std::size_t g = begin; g < end; ++g)
                {
                    group_ambient_[g] = std::clamp(ambient(group_centre_[g], group_owner_[g]), 0.0f, 1.0f);
                }
            };
            const auto reset = [](std::size_t, std::size_t) {};
            parallel(group_centre_.size(), group_centre_.size() * 64u, build, reset);
        }
    }

    /* 6. Per LED: exact falloff from each light its group can see (no blocker tests). */
    {
        const float core2 = std::max(1e-6f, settings.core2 * r2);
        tap_begin_.assign(receivers_.size() + 1, 0);
        taps_.clear();
        ambient_.assign(receivers_.size(), 1.0f);
        for(std::size_t ri = 0; ri < receivers_.size(); ++ri)
        {
            tap_begin_[ri] = (std::uint32_t)taps_.size();
            const Vector3D& rp = receivers_[ri];
            const int g = receiver_group_[ri];
            ambient_[ri] = (std::size_t)g < group_ambient_.size() ? group_ambient_[(std::size_t)g] : 1.0f;
            for(const Tap& gp : group_pass_[(std::size_t)g])
            {
                const Vector3D& c = centre_[(std::size_t)gp.light];
                const float dx = rp.x - c.x;
                const float dy = rp.y - c.y;
                const float dz = rp.z - c.z;
                const float w = Spill(dx * dx + dy * dy + dz * dz, r2, core2) * gp.weight;
                if(w >= 1e-5f)
                {
                    taps_.push_back({gp.light, w});
                }
            }
        }
        tap_begin_[receivers_.size()] = (std::uint32_t)taps_.size();
    }
    return true;
}

void RelayModel::Shade(const float* emitter_linear, std::vector<float>& out_linear) const
{
    out_linear.assign(receiver_count_ * 3u, 0.0f);
    if(!valid_ || receiver_count_ == 0 || light_count_ == 0 || !emitter_linear)
    {
        return;
    }
    light_sum_.assign((std::size_t)light_count_ * 3u, 0.0f);
    for(std::size_t i = 0; i < emitter_count_; ++i)
    {
        float* s = &light_sum_[(std::size_t)emitter_light_[i] * 3u];
        s[0] += emitter_linear[i * 3u + 0];
        s[1] += emitter_linear[i * 3u + 1];
        s[2] += emitter_linear[i * 3u + 2];
    }
    for(std::size_t ri = 0; ri < receiver_count_; ++ri)
    {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        for(std::uint32_t t = tap_begin_[ri]; t < tap_begin_[ri + 1]; ++t)
        {
            const Tap& tap = taps_[t];
            const float* s = &light_sum_[(std::size_t)tap.light * 3u];
            r += s[0] * tap.weight;
            g += s[1] * tap.weight;
            b += s[2] * tap.weight;
        }
        ToneMap(r, g, b, exposure_);
        const float a = ambient_[ri];
        out_linear[ri * 3u + 0] = r * a;
        out_linear[ri * 3u + 1] = g * a;
        out_linear[ri * 3u + 2] = b * a;
    }
}

} // namespace RoomLight
