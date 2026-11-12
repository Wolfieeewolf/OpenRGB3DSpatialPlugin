// SPDX-License-Identifier: GPL-2.0-only

#include "GameWrapperCapture.h"
#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include "LEDPosition3D.h"
#include "SpatialEffect3D.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <vector>

namespace GameWrapperCapture
{
namespace
{

static RGBColor MakeRgb(float r, float g, float b)
{
    return (RGBColor)(((unsigned)std::lround(std::clamp(b, 0.0f, 1.0f) * 255.0f) << 16) |
                      ((unsigned)std::lround(std::clamp(g, 0.0f, 1.0f) * 255.0f) << 8) |
                      (unsigned)std::lround(std::clamp(r, 0.0f, 1.0f) * 255.0f));
}

static RGBColor MaxRgb(RGBColor a, RGBColor b)
{
    const unsigned char ar = (unsigned char)(a & 0xFF);
    const unsigned char ag = (unsigned char)((a >> 8) & 0xFF);
    const unsigned char ab = (unsigned char)((a >> 16) & 0xFF);
    const unsigned char br = (unsigned char)(b & 0xFF);
    const unsigned char bg = (unsigned char)((b >> 8) & 0xFF);
    const unsigned char bb = (unsigned char)((b >> 16) & 0xFF);
    return MakeRgb(std::max(ar, br) / 255.0f, std::max(ag, bg) / 255.0f, std::max(ab, bb) / 255.0f);
}

static const ControllerTransform* CurrentShadingController()
{
    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    if(!provider)
    {
        return nullptr;
    }
    const int idx = provider->shadingControllerIndex();
    const auto* transforms = provider->controllers();
    if(!transforms || idx < 0 || idx >= (int)transforms->size())
    {
        return nullptr;
    }
    return (*transforms)[(size_t)idx].get();
}

static void ControllerLocalUv01(const ControllerTransform* ctrl,
                                float world_x,
                                float world_y,
                                float world_z,
                                float& out_u,
                                float& out_v)
{
    out_u = 0.5f;
    out_v = 0.5f;
    if(!ctrl || ctrl->led_positions.empty())
    {
        return;
    }

    Vector3D local_min{};
    Vector3D local_max{};
    ControllerLayout3D::CalculateControllerLocalBounds(ctrl, local_min, local_max);
    const Vector3D center = ControllerLayout3D::GetLedLocalCenter(ctrl);
    const Vector3D world{world_x, world_y, world_z};
    const Vector3D rel = Geometry3D::TransformWorldToLocalScaled(world, ctrl->transform);
    const Vector3D local{rel.x + center.x, rel.y + center.y, rel.z + center.z};

    const float sx = local_max.x - local_min.x;
    const float sy = local_max.y - local_min.y;
    const float sz = local_max.z - local_min.z;

    out_u = (std::fabs(sx) > 1e-6f) ? std::clamp((local.x - local_min.x) / sx, 0.0f, 1.0f) : 0.5f;
    if(std::fabs(sz) >= std::fabs(sy) && std::fabs(sz) > 1e-6f)
    {
        out_v = std::clamp((local.z - local_min.z) / sz, 0.0f, 1.0f);
    }
    else if(std::fabs(sy) > 1e-6f)
    {
        out_v = std::clamp((local.y - local_min.y) / sy, 0.0f, 1.0f);
    }
}

static void SampleDeviceColor(const GameLightingBridge::DeviceSample& sample,
                              float u01,
                              float v01,
                              float& out_r,
                              float& out_g,
                              float& out_b)
{
    if(!sample.has_grid || sample.grid_rows == 0 || sample.grid_cols == 0)
    {
        out_r = sample.r;
        out_g = sample.g;
        out_b = sample.b;
        return;
    }

    const int rows = (int)sample.grid_rows;
    const int cols = (int)sample.grid_cols;
    const float u = std::clamp(u01, 0.0f, 1.0f);
    const float v = std::clamp(v01, 0.0f, 1.0f);

    int col = (int)std::lround(u * (float)(cols - 1));
    int row = 0;
    if(rows > 1)
    {
        row = (int)std::lround((1.0f - v) * (float)(rows - 1));
    }
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);

    const std::size_t off = ((std::size_t)row * (std::size_t)cols + (std::size_t)col) * 3u;
    out_r = sample.grid_rgb[off + 0] / 255.0f;
    out_g = sample.grid_rgb[off + 1] / 255.0f;
    out_b = sample.grid_rgb[off + 2] / 255.0f;
}

} // namespace

bool IsPeripheralTarget(TargetDevice target)
{
    switch(target)
    {
        case TargetDevice::Keyboard:
        case TargetDevice::Mouse:
        case TargetDevice::Mousepad:
        case TargetDevice::Headset:
        case TargetDevice::Keypad:
        case TargetDevice::RoomLights:
            return true;
        default:
            return false;
    }
}

bool IsSpatialTarget(TargetDevice target)
{
    switch(target)
    {
        case TargetDevice::WholeRoom:
        case TargetDevice::Desk:
        case TargetDevice::Floor:
        case TargetDevice::Ceiling:
        case TargetDevice::Left:
        case TargetDevice::Right:
            return true;
        default:
            return false;
    }
}

bool ControllerMatchesTarget(device_type type, TargetDevice target)
{
    switch(target)
    {
        case TargetDevice::Off:
            return false;
        case TargetDevice::Keyboard:
            return type == DEVICE_TYPE_KEYBOARD || type == DEVICE_TYPE_LAPTOP;
        case TargetDevice::Mouse:
            return type == DEVICE_TYPE_MOUSE;
        case TargetDevice::Mousepad:
            return type == DEVICE_TYPE_MOUSEMAT;
        case TargetDevice::Headset:
            return type == DEVICE_TYPE_HEADSET || type == DEVICE_TYPE_HEADSET_STAND;
        case TargetDevice::Keypad:
            return type == DEVICE_TYPE_KEYPAD;
        case TargetDevice::RoomLights:
            (void)type;
            return true; /* any controller */
        default:
            return false;
    }
}

bool SpatialTargetApplies(TargetDevice target, float x01, float y01)
{
    switch(target)
    {
        case TargetDevice::WholeRoom:
            return true;
        case TargetDevice::Desk:
            return y01 >= 0.28f && y01 <= 0.72f;
        case TargetDevice::Floor:
            return y01 < 0.33f;
        case TargetDevice::Ceiling:
            return y01 > 0.66f;
        case TargetDevice::Left:
            return x01 < 0.45f;
        case TargetDevice::Right:
            return x01 > 0.55f;
        default:
            return false;
    }
}

std::string NormalizeGameKey(const std::string& source)
{
    if(source.empty())
    {
        return {};
    }
    std::string s = source;
    const auto slash = s.find_last_of("/\\");
    if(slash != std::string::npos)
    {
        s = s.substr(slash + 1);
    }
    while(!s.empty() && (s.back() == '\0' || std::isspace((unsigned char)s.back())))
    {
        s.pop_back();
    }
    return s;
}

void SyncActiveGameProfile(Settings& settings,
                           const std::string& live_source,
                           const GameLightingBridge::LightingSnapshot* snap_for_new)
{
    const std::string key = NormalizeGameKey(live_source);
    if(key.empty())
    {
        return;
    }
    if(settings.active_game == key)
    {
        settings.game_profiles[key] = settings.devices;
        return;
    }
    if(!settings.active_game.empty())
    {
        settings.game_profiles[settings.active_game] = settings.devices;
    }
    settings.active_game = key;
    const auto it = settings.game_profiles.find(key);
    if(it != settings.game_profiles.end())
    {
        settings.devices = it->second;
    }
    else
    {
        if(snap_for_new)
        {
            ApplySeenChannelsDefault(settings.devices, *snap_for_new);
        }
        else
        {
            Settings::ApplyIdentityMap(settings.devices);
        }
        settings.game_profiles[key] = settings.devices;
    }
}

void ApplySeenChannelsDefault(DeviceMapArray& out, const GameLightingBridge::LightingSnapshot& snap)
{
    Settings::ApplyIdentityMap(out);
    bool any_seen = false;
    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        const bool seen = snap.has_devices && snap.devices[i].active;
        out[i].enabled = seen;
        out[i].targets = TargetBit(TargetDevice::WholeRoom);
        any_seen = any_seen || seen;
    }
    if(!any_seen)
    {
        Settings::ApplyIdentityMap(out);
    }
}

void ApplyPresetDefault(Settings& s)
{
    const auto profiles = s.game_profiles;
    const auto active = s.active_game;
    s = Settings{};
    s.game_profiles = profiles;
    s.active_game = active;
    Settings::ApplyIdentityMap(s.devices);
    if(!s.active_game.empty())
    {
        s.game_profiles[s.active_game] = s.devices;
    }
}

void ApplyPresetPeripheralIdentity(Settings& s)
{
    ApplyPresetDefault(s);
    auto set_one = [&](GameLightingFormat::DeviceId id, TargetDevice dest) {
        DeviceMapping& m = s.devices[static_cast<size_t>(id)];
        m.enabled = true;
        m.ClearTargets();
        m.SetTarget(dest, true);
    };
    set_one(GameLightingFormat::DeviceId::Keyboard, TargetDevice::Keyboard);
    set_one(GameLightingFormat::DeviceId::Mouse, TargetDevice::Mouse);
    set_one(GameLightingFormat::DeviceId::Mousepad, TargetDevice::Mousepad);
    set_one(GameLightingFormat::DeviceId::Headset, TargetDevice::Headset);
    set_one(GameLightingFormat::DeviceId::Keypad, TargetDevice::Keypad);
    set_one(GameLightingFormat::DeviceId::ChromaLink, TargetDevice::RoomLights);
    if(!s.active_game.empty())
    {
        s.game_profiles[s.active_game] = s.devices;
    }
}

void ApplyPresetAllWholeRoom(Settings& s)
{
    ApplyPresetDefault(s);
    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        s.devices[i].enabled = true;
        s.devices[i].ClearTargets();
        s.devices[i].SetTarget(TargetDevice::WholeRoom, true);
    }
    if(!s.active_game.empty())
    {
        s.game_profiles[s.active_game] = s.devices;
    }
}

RGBColor RenderColor(const GameLightingBridge::LightingSnapshot& snap,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& settings)
{
    if(!snap.has_devices)
    {
        return (RGBColor)0x00000000;
    }

    const ControllerTransform* ctrl = CurrentShadingController();
    device_type ctrl_type = DEVICE_TYPE_UNKNOWN;
    float local_u = 0.5f;
    float local_v = 0.5f;
    if(ctrl && ctrl->controller)
    {
        ctrl_type = ctrl->controller->GetDeviceType();
        ControllerLocalUv01(ctrl, grid_x, grid_y, grid_z, local_u, local_v);
    }

    const float x01 = NormalizeGridAxis01(grid_x, grid.min_x, grid.max_x);
    const float y01 = SampleStratumYNorm01(grid_y, grid, Vector3D{origin_x, origin_y, origin_z});

    RGBColor out = 0;
    bool any = false;
    RGBColor wash = 0;
    bool have_wash = false;
    int wash_cells = -1;

    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        const GameLightingBridge::DeviceSample& sample = snap.devices[i];
        if(!sample.active)
        {
            continue;
        }

        /* Failsafe wash: prefer densest grid (keyboard over ChromaLink). */
        const int cells = sample.has_grid ? (int)sample.grid_rows * (int)sample.grid_cols : 1;
        if(cells >= wash_cells)
        {
            RGBColor candidate = MakeRgb(sample.r, sample.g, sample.b);
            if(candidate == 0 && sample.has_grid)
            {
                float wr, wg, wb;
                SampleDeviceColor(sample, 0.5f, 0.5f, wr, wg, wb);
                candidate = MakeRgb(wr, wg, wb);
            }
            if(candidate != 0 || !have_wash)
            {
                wash = candidate;
                have_wash = (candidate != 0);
                wash_cells = cells;
            }
        }

        const DeviceMapping& map = settings.devices[i];
        if(!map.enabled || !map.HasAnyTarget())
        {
            continue;
        }

        bool hit_peripheral = false;
        bool hit_spatial = false;
        for(TargetDevice dest : map.ListedTargets())
        {
            if(IsPeripheralTarget(dest))
            {
                if(ctrl && ctrl->controller && ControllerMatchesTarget(ctrl_type, dest))
                {
                    hit_peripheral = true;
                }
            }
            else if(IsSpatialTarget(dest))
            {
                if(SpatialTargetApplies(dest, x01, y01))
                {
                    hit_spatial = true;
                }
            }
        }
        if(!hit_peripheral && !hit_spatial)
        {
            continue;
        }

        float sample_u = local_u;
        float sample_v = local_v;
        if(hit_spatial)
        {
            sample_u = x01;
            sample_v = y01;
        }

        float r, g, b;
        SampleDeviceColor(sample, sample_u, sample_v, r, g, b);
        const RGBColor c = MakeRgb(r, g, b);
        if(!any)
        {
            out = c;
            any = true;
        }
        else if(settings.max_blend)
        {
            out = MaxRgb(out, c);
        }
    }

    if(any)
    {
        return out;
    }
    if(have_wash)
    {
        return wash;
    }
    return (RGBColor)0x00000000;
}

} // namespace GameWrapperCapture
