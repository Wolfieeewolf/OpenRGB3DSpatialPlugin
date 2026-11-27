// SPDX-License-Identifier: GPL-2.0-or-later

#include "RoomLight.h"

#include <cstring>
#include <thread>

namespace RoomLight
{

namespace
{

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

/* Run fn(begin, end) over [0, count) on worker threads; a worker that fails or cannot start
   leaves its range to the calling thread, so a thread error never takes the app down. */
void Parallel(std::size_t count, std::size_t work, const std::function<void(std::size_t, std::size_t)>& fn)
{
    unsigned int threads = std::min<unsigned int>(8u, std::max(1u, std::thread::hardware_concurrency()));
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
        fn(range.first, range.second);
    }
}

uint64_t CellKey(float x, float y, float z, float inv, int owner)
{
    const int64_t ix = (int64_t)std::floor(x * inv);
    const int64_t iy = (int64_t)std::floor(y * inv);
    const int64_t iz = (int64_t)std::floor(z * inv);
    const uint64_t key = ((uint64_t)(ix & 0x1FFFFF) << 42) | ((uint64_t)(iy & 0x1FFFFF) << 21) | (uint64_t)(iz & 0x1FFFFF);
    return key ^ ((uint64_t)(owner + 2) * 0x9E3779B97F4A7C15ull);
}

constexpr int kMaxRelayLights = 768;

} // namespace

float RayVisibility(const float* depth, const Vector3D& dn, float dist, float tolerance)
{
    if(!depth)
    {
        return 1.0f;
    }
    const float ax = std::fabs(dn.x);
    const float ay = std::fabs(dn.y);
    const float az = std::fabs(dn.z);
    int face = 0;
    float s = 0.0f;
    float t = 0.0f;
    if(ax >= ay && ax >= az)
    {
        if(ax < 1e-9f)
        {
            return 1.0f;
        }
        face = dn.x >= 0.0f ? 0 : 1;
        s = dn.y / ax;
        t = dn.z / ax;
    }
    else if(ay >= az)
    {
        face = dn.y >= 0.0f ? 2 : 3;
        s = dn.x / ay;
        t = dn.z / ay;
    }
    else
    {
        face = dn.z >= 0.0f ? 4 : 5;
        s = dn.x / az;
        t = dn.y / az;
    }
    const float n = (float)kRayMapRes;
    const float fu = std::clamp((s + 1.0f) * 0.5f * n - 0.5f, 0.0f, n - 1.0f);
    const float fv = std::clamp((t + 1.0f) * 0.5f * n - 0.5f, 0.0f, n - 1.0f);
    const int u0 = (int)std::floor(fu);
    const int v0 = (int)std::floor(fv);
    const int u1 = std::min(u0 + 1, kRayMapRes - 1);
    const int v1 = std::min(v0 + 1, kRayMapRes - 1);
    const float wu = fu - (float)u0;
    const float wv = fv - (float)v0;
    const int base = face * kRayMapRes * kRayMapRes;
    const auto open = [&](int u, int v) { return dist <= depth[base + v * kRayMapRes + u] + tolerance ? 1.0f : 0.0f; };
    const float a = open(u0, v0) + (open(u1, v0) - open(u0, v0)) * wu;
    const float b = open(u0, v1) + (open(u1, v1) - open(u0, v1)) * wu;
    return a + (b - a) * wv;
}

void BuildLights(const std::vector<Vector3D>& pos,
                 const std::vector<int>& owner,
                 const std::vector<char>& housed,
                 const std::vector<int>& led_id,
                 float cell,
                 int max_lights,
                 std::vector<Light>& lights,
                 std::vector<int>& led_light)
{
    std::unordered_map<uint64_t, int> index;
    std::vector<int> count;
    cell = std::max(1e-3f, cell);
    for(int attempt = 0; attempt < 12; ++attempt)
    {
        lights.clear();
        index.clear();
        count.clear();
        led_light.assign(pos.size(), 0);
        const float inv = 1.0f / cell;
        for(std::size_t i = 0; i < pos.size(); ++i)
        {
            const int o = i < owner.size() ? owner[i] : -1;
            const bool solo = i < housed.size() && housed[i];
            const int id = i < led_id.size() ? led_id[i] : (int)i;
            const uint64_t key = solo ? (0xB0C5000000000000ull ^ (((uint64_t)(uint32_t)o << 24) | (uint64_t)(uint32_t)id))
                                      : CellKey(pos[i].x, pos[i].y, pos[i].z, inv, o);
            auto it = index.find(key);
            int k = 0;
            if(it == index.end())
            {
                k = (int)lights.size();
                index.emplace(key, k);
                Light l;
                l.key = key;
                l.owner = o;
                lights.push_back(l);
                count.push_back(0);
            }
            else
            {
                k = it->second;
            }
            lights[(std::size_t)k].pos.x += pos[i].x;
            lights[(std::size_t)k].pos.y += pos[i].y;
            lights[(std::size_t)k].pos.z += pos[i].z;
            ++count[(std::size_t)k];
            led_light[i] = k;
        }
        if((int)lights.size() <= max_lights)
        {
            break;
        }
        cell *= 1.5f;
    }
    for(std::size_t k = 0; k < lights.size(); ++k)
    {
        const float n = (float)std::max(1, count[k]);
        lights[k].pos.x /= n;
        lights[k].pos.y /= n;
        lights[k].pos.z /= n;
    }
}

void RayMapCache::Begin(std::uint64_t scene_signature, float max_dist)
{
    if(scene_signature != signature_ || max_dist != max_dist_)
    {
        maps_.clear();
        signature_ = scene_signature;
        max_dist_ = max_dist;
    }
}

void RayMapCache::Ensure(const std::vector<Light>& lights,
                         const RayTestFn& own_housing,
                         const RayTestFn& scene,
                         std::vector<const float*>& out)
{
    std::vector<const Light*> todo;
    for(const Light& l : lights)
    {
        auto it = maps_.find(l.key);
        if(it == maps_.end() || it->second.owner != l.owner || std::fabs(it->second.pos.x - l.pos.x) > 1e-4f
           || std::fabs(it->second.pos.y - l.pos.y) > 1e-4f || std::fabs(it->second.pos.z - l.pos.z) > 1e-4f)
        {
            todo.push_back(&l);
        }
    }
    if(!todo.empty())
    {
        const float reach = max_dist_;
        std::vector<Entry> built(todo.size());
        Parallel(todo.size(), todo.size() * (std::size_t)kRayMapSize * 4u, [&](std::size_t begin, std::size_t end) {
            for(std::size_t i = begin; i < end; ++i)
            {
                const Light& l = *todo[i];
                Entry& e = built[i];
                e.pos = l.pos;
                e.owner = l.owner;
                const Vector3D& p = l.pos;
                for(int d = 0; d < kRayMapSize; ++d)
                {
                    const Vector3D dir = RayMapDirection(d);
                    const Vector3D q = {p.x + dir.x * reach, p.y + dir.y * reach, p.z + dir.z * reach};
                    /* Its own housing first: a blocked direction gets no light at all. */
                    if(own_housing && own_housing(p, l.owner, q))
                    {
                        e.depth[d] = 0.0f;
                        continue;
                    }
                    if(!scene || !scene(p, l.owner, q))
                    {
                        e.depth[d] = reach;
                        continue;
                    }
                    /* Something is in the way: find where (to ~0.2% of the range). */
                    float lo = 0.0f;
                    float hi = reach;
                    for(int step = 0; step < 9; ++step)
                    {
                        const float mid = 0.5f * (lo + hi);
                        const Vector3D m = {p.x + dir.x * mid, p.y + dir.y * mid, p.z + dir.z * mid};
                        if(scene(p, l.owner, m))
                        {
                            hi = mid;
                        }
                        else
                        {
                            lo = mid;
                        }
                    }
                    e.depth[d] = lo;
                }
            }
        });
        for(std::size_t i = 0; i < todo.size(); ++i)
        {
            maps_[todo[i]->key] = built[i];
        }
    }
    /* Keep the cache bounded (devices come and go): rebuilt next call if it overflows. */
    if(maps_.size() > 8192)
    {
        std::unordered_map<std::uint64_t, Entry> keep;
        for(const Light& l : lights)
        {
            auto it = maps_.find(l.key);
            if(it != maps_.end())
            {
                keep.emplace(it->first, it->second);
            }
        }
        maps_.swap(keep);
    }
    out.resize(lights.size());
    for(std::size_t i = 0; i < lights.size(); ++i)
    {
        auto it = maps_.find(lights[i].key);
        out[i] = it != maps_.end() ? it->second.depth : nullptr;
    }
}

RayMapCache& SharedRayMaps()
{
    static RayMapCache cache;
    return cache;
}

bool RelayModel::Prepare(const std::vector<Light>& lights,
                         const std::vector<const float*>& ray_maps,
                         std::uint64_t ray_sig,
                         const std::vector<Vector3D>& receivers,
                         const std::vector<int>& receiver_owner,
                         const std::vector<char>& receiver_housed,
                         const RelaySettings& settings,
                         const RayTestFn& receiver_housing,
                         const AmbientFn& ambient)
{
    exposure_ = settings.exposure;

    uint64_t tap_sig = 1469598103934665603ull;
    HashValue(tap_sig, lights.size());
    for(const Light& l : lights)
    {
        HashValue(tap_sig, l.key);
        HashValue(tap_sig, l.pos);
    }
    HashValue(tap_sig, receivers.size());
    if(!receivers.empty())
    {
        HashBytes(tap_sig, receivers.data(), receivers.size() * sizeof(Vector3D));
    }
    if(!receiver_owner.empty())
    {
        HashBytes(tap_sig, receiver_owner.data(), receiver_owner.size() * sizeof(int));
    }
    if(!receiver_housed.empty())
    {
        HashBytes(tap_sig, receiver_housed.data(), receiver_housed.size());
    }
    HashValue(tap_sig, settings.reach);
    HashValue(tap_sig, settings.core2);
    HashValue(tap_sig, settings.occlusion);
    if(settings.occlusion)
    {
        HashValue(tap_sig, ray_sig);
    }

    uint64_t amb_sig = tap_sig;
    HashValue(amb_sig, settings.ambient_occlusion);
    if(settings.ambient_occlusion)
    {
        HashValue(amb_sig, settings.ambient_strength);
        HashValue(amb_sig, settings.ambient_span);
        HashValue(amb_sig, settings.ambient_centre);
        HashValue(amb_sig, ray_sig);
    }

    const bool redo_taps = !valid_ || tap_sig != tap_sig_;
    const bool redo_amb = redo_taps || amb_sig != amb_sig_;
    if(!redo_taps && !redo_amb)
    {
        return false;
    }
    tap_sig_ = tap_sig;
    amb_sig_ = amb_sig;
    valid_ = true;
    receiver_count_ = receivers.size();
    light_count_ = std::min((int)lights.size(), kMaxRelayLights);

    if(redo_taps)
    {
        /* Per receiver: each light within reach, weighted by distance and, with blockers on,
           by how much of that light its ray map lets through and its own housing lets in. */
        const float reach = std::max(1e-3f, settings.reach);
        const float r2 = reach * reach;
        const float core2 = std::max(1e-6f, settings.core2 * r2);
        const float tolerance = 0.02f * reach;
        std::vector<std::vector<Tap>> per(receivers.size());
        Parallel(receivers.size(), receivers.size() * (std::size_t)std::max(1, light_count_),
                 [&](std::size_t begin, std::size_t end) {
                     for(std::size_t ri = begin; ri < end; ++ri)
                     {
                         const Vector3D& rp = receivers[ri];
                         const int owner = ri < receiver_owner.size() ? receiver_owner[ri] : -1;
                         const bool housed = ri < receiver_housed.size() && receiver_housed[ri];
                         for(int k = 0; k < light_count_; ++k)
                         {
                             const Vector3D& lp = lights[(std::size_t)k].pos;
                             const Vector3D d = {rp.x - lp.x, rp.y - lp.y, rp.z - lp.z};
                             const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                             float w = Spill(d2, r2, core2);
                             if(w < 1e-5f)
                             {
                                 continue;
                             }
                             if(settings.occlusion)
                             {
                                 const float dist = std::sqrt(d2);
                                 if(dist > 1e-5f && (std::size_t)k < ray_maps.size())
                                 {
                                     const Vector3D dn = {d.x / dist, d.y / dist, d.z / dist};
                                     w *= RayVisibility(ray_maps[(std::size_t)k], dn, dist, tolerance);
                                 }
                                 if(w > 0.0f && housed && receiver_housing && receiver_housing(rp, owner, lp))
                                 {
                                     w = 0.0f;
                                 }
                             }
                             if(w >= 1e-5f)
                             {
                                 per[ri].push_back({k, w});
                             }
                         }
                     }
                 });
        tap_begin_.assign(receivers.size() + 1, 0);
        taps_.clear();
        for(std::size_t ri = 0; ri < receivers.size(); ++ri)
        {
            tap_begin_[ri] = (std::uint32_t)taps_.size();
            taps_.insert(taps_.end(), per[ri].begin(), per[ri].end());
        }
        tap_begin_[receivers.size()] = (std::uint32_t)taps_.size();
    }

    if(redo_amb)
    {
        ambient_.assign(receivers.size(), 1.0f);
        if(settings.ambient_occlusion && ambient)
        {
            Parallel(receivers.size(), receivers.size() * 64u, [&](std::size_t begin, std::size_t end) {
                for(std::size_t ri = begin; ri < end; ++ri)
                {
                    const int owner = ri < receiver_owner.size() ? receiver_owner[ri] : -1;
                    ambient_[ri] = std::clamp(ambient(receivers[ri], owner), 0.0f, 1.0f);
                }
            });
        }
    }
    return true;
}

void RelayModel::Shade(const std::vector<float>& light_linear, std::vector<float>& out_linear) const
{
    out_linear.assign(receiver_count_ * 3u, 0.0f);
    if(!valid_ || receiver_count_ == 0 || light_count_ == 0 || light_linear.size() < (std::size_t)light_count_ * 3u)
    {
        return;
    }
    for(std::size_t ri = 0; ri < receiver_count_; ++ri)
    {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        for(std::uint32_t t = tap_begin_[ri]; t < tap_begin_[ri + 1]; ++t)
        {
            const Tap& tap = taps_[t];
            const float* s = &light_linear[(std::size_t)tap.light * 3u];
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
