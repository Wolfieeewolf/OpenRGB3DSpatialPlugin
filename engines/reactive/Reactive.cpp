// SPDX-License-Identifier: GPL-2.0-only

#include "Reactive.h"
#include "ReactiveInputManager.h"
#include "SpatialKernelColormap.h"
#include "EffectSliderRow.h"
#include "EffectListManager3D.h"
#include "PlayerEngines.h"

#include <QString>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace
{
constexpr int kMaxWaves = 32;
constexpr float kMinLifetime = 0.20f;
constexpr float kMaxLifetime = 1.05f;
constexpr int kMinRepeatDeciHz = 1;
constexpr int kMaxRepeatDeciHz = 100;

float TravelRadius(float travel01, float spacing, float spread, float zone_diag)
{
    const float one = std::max(spacing * 1.15f, 1.0e-4f);
    const float nearby = spacing * 3.5f;
    const float device = std::max(spread * 1.08f, nearby);
    const float room = std::max(zone_diag, device);
    const float t = std::clamp(travel01, 0.0f, 1.0f);
    if(t <= 0.22f)
    {
        const float u = t / 0.22f;
        return one + (nearby - one) * u;
    }
    if(t <= 0.55f)
    {
        const float u = (t - 0.22f) / 0.33f;
        return nearby + (device - nearby) * u;
    }
    const float u = (t - 0.55f) / 0.45f;
    return device + (room - device) * u;
}

float RegularPrismDistance(float dx, float dz, int sides)
{
    const float r = std::hypot(dx, dz);
    if(r < 1.0e-8f || sides < 3)
    {
        return r;
    }
    const float step = 6.28318530718f / static_cast<float>(sides);
    float ang = std::atan2(dz, dx);
    float p = std::fmod(ang + 3.14159265359f, step);
    if(p < 0.0f)
    {
        p += step;
    }
    p -= step * 0.5f;
    return r * std::cos(p);
}

float PolygonalShellDistance(float dx, float dy, float dz, int sides)
{
    const float radial = RegularPrismDistance(dx, dz, sides);
    return std::sqrt(radial * radial + dy * dy);
}

float ShellDistance(float dx, float dy, float dz, int shape)
{
    switch(shape)
    {
    case 1:
        return std::max(std::fabs(dx), std::max(std::fabs(dy), std::fabs(dz)));
    case 2:
        return (std::fabs(dx) + std::fabs(dy) + std::fabs(dz)) * 0.57735027f;
    case 3:
        return std::hypot(dx, dz);
    case 4:
        return PolygonalShellDistance(dx, dy, dz, 3);
    case 5:
        return PolygonalShellDistance(dx, dy, dz, 4);
    case 6:
        return PolygonalShellDistance(dx, dy, dz, 5);
    case 7:
        return PolygonalShellDistance(dx, dy, dz, 6);
    case 0:
    default:
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}

float PlanarFootprint(float u, float v, int shape)
{
    switch(shape)
    {
    case 1:
    case 5:
        return std::max(std::fabs(u), std::fabs(v));
    case 2:
        return (std::fabs(u) + std::fabs(v)) * 0.70710678f;
    case 4:
        return RegularPrismDistance(u, v, 3);
    case 6:
        return RegularPrismDistance(u, v, 5);
    case 7:
        return RegularPrismDistance(u, v, 6);
    case 0:
    case 3:
    default:
        return std::hypot(u, v);
    }
}

float AxisDelta(float dx, float dy, float dz, int axis)
{
    switch(axis)
    {
    case 0:
        return dx;
    case 1:
        return dy;
    default:
        return dz;
    }
}

void PlaneAxes(int orient, float dx, float dy, float dz, float* u, float* v, float* normal)
{
    switch(orient)
    {
    case 0:
        *u = dx;
        *v = dy;
        *normal = dz;
        break;
    case 1:
        *u = dx;
        *v = dz;
        *normal = dy;
        break;
    default:
        *u = dy;
        *v = dz;
        *normal = dx;
        break;
    }
}

void CrossAxes(int orient, int* a0, int* a1)
{
    switch(orient)
    {
    case 0:
        *a0 = 0;
        *a1 = 2;
        break;
    case 1:
        *a0 = 0;
        *a1 = 1;
        break;
    default:
        *a0 = 1;
        *a1 = 2;
        break;
    }
}

float WaveLifetimeSec(float speed01)
{
    const float speed_k = std::clamp(speed01, 0.0f, 1.0f);
    return kMinLifetime + (kMaxLifetime - kMinLifetime) * (1.0f - speed_k);
}

void NormalizeWaveMetrics(float& spread, float& spacing, float zone_diag)
{
    if(spread < 1.0e-4f)
    {
        spread = std::max(0.10f * zone_diag, spacing * 8.0f);
    }
    if(spacing < 1.0e-5f)
    {
        spacing = std::max(spread * 0.07f, 0.008f * zone_diag);
    }
}

float WaveExpandSpeed(float spacing, float spread, float zone_diag, float speed01)
{
    NormalizeWaveMetrics(spread, spacing, zone_diag);
    const float pace = std::max(WaveLifetimeSec(speed01), 1.0e-3f);
    const float local_ref = TravelRadius(0.40f, spacing, spread, zone_diag);
    const float local_expand = local_ref / pace;
    const float snap_expand = std::max(zone_diag, local_ref) / pace;
    const float t = std::clamp(speed01, 0.0f, 1.0f);
    const float boost = (t <= 0.32f) ? 0.0f : (t - 0.32f) / 0.68f;
    const float b = boost * boost;
    return local_expand + (snap_expand - local_expand) * b;
}

struct WaveKinematics
{
    float expand = 0.0f;
    float lifetime = 0.0f;
    float travel = 0.0f;
};

WaveKinematics ComputeWaveKinematics(float spacing,
                                     float spread,
                                     float zone_diag,
                                     float speed01,
                                     int travel_pct,
                                     int look_mode)
{
    NormalizeWaveMetrics(spread, spacing, zone_diag);
    WaveKinematics k;
    k.travel = TravelRadius(travel_pct / 100.0f, spacing, spread, zone_diag);
    k.expand = WaveExpandSpeed(spacing, spread, zone_diag, speed01);
    // Collapsed radius=0 lights the entire along=0 locus (full plane for Axis/Cross).
    // Every pulse starts as a small shell near the key, then expands to travel.
    const float r_min = std::max(spacing * 0.28f, 1.0e-4f);
    if(look_mode == 2)
    {
        k.expand *= 1.85f;
        const float punch = 0.14f + 0.16f * (1.0f - std::clamp(speed01, 0.0f, 1.0f));
        k.lifetime = std::min(std::max(k.travel - r_min, r_min) / std::max(k.expand, 1.0e-6f), punch);
    }
    else if(look_mode == 3)
    {
        const float punch = 0.10f + 0.12f * (1.0f - std::clamp(speed01, 0.0f, 1.0f));
        k.lifetime = punch;
        k.expand = std::max(k.travel - r_min, r_min) / std::max(punch, 1.0e-4f);
    }
    else
    {
        k.lifetime = std::max(k.travel - r_min, r_min) / std::max(k.expand, 1.0e-6f);
    }
    return k;
}

float WaveTravelLifetime(float spacing, float spread, float zone_diag, float speed01, int travel_pct, int look_mode)
{
    return ComputeWaveKinematics(spacing, spread, zone_diag, speed01, travel_pct, look_mode).lifetime;
}

float RingSigma(float width01, float spacing, float spread, float zone_diag)
{
    const float t = std::clamp(width01, 0.0f, 1.0f);
    const float thin = spacing * 0.035f;
    // Cap thickness against spacing so a large device spread cannot turn every new
    // pulse into a device-wide blob that reads as a full restart.
    const float thick_cap = std::max(spacing * 18.0f, zone_diag * 0.08f);
    const float thick = std::min(std::max(spread * 0.55f, spacing * 8.0f), thick_cap);
    return thin + (std::max(thick, thin) - thin) * (t * t);
}

float LookEnergy(float along, float radius, float sigma, int look, float core, float age, float expand)
{
    switch(look)
    {
    case 1:
    {
        const float edge_t = (along - radius) / sigma;
        const float front = std::exp(-2.0f * edge_t * edge_t);
        if(along > radius)
        {
            return std::max(front, core);
        }
        const float inner = 0.42f + 0.58f * std::clamp(along / std::max(radius, 1.0e-4f), 0.0f, 1.0f);
        const float behind = radius - along;
        const float passed = behind / std::max(expand, 1.0e-6f);
        const float clear = std::exp(-passed / 0.22f);
        return std::max(front, std::max(inner * clear, core));
    }
    case 2:
    {
        const float blast_sigma = std::max(sigma * 1.65f, sigma + 1.0e-5f);
        const float edge_t = (along - radius) / blast_sigma;
        const float shock = std::exp(-2.2f * edge_t * edge_t);
        const float u = std::clamp(along / std::max(radius, 1.0e-4f), 0.0f, 1.0f);
        const float fire = (1.0f - u) * std::exp(-age / 0.08f);
        if(along <= radius)
        {
            return std::max(shock, std::max(fire * fire * 1.25f, core));
        }
        return shock * 0.55f;
    }
    case 3:
    {
        const float flash = std::exp(-age / 0.06f);
        const float local = std::exp(-0.5f * (along / std::max(sigma * 1.8f, 1.0e-5f)) *
                                     (along / std::max(sigma * 1.8f, 1.0e-5f)));
        return std::max(flash * local, core);
    }
    case 4:
    {
        const float sep = sigma * 2.8f;
        const auto ring_at = [&](float r) {
            const float edge_t = (along - r) / sigma;
            return std::exp(-2.0f * edge_t * edge_t);
        };
        // No core fold-in: a new pulse must not punch the whole along=0 locus.
        return std::max(ring_at(radius), ring_at(std::max(0.0f, radius - sep)));
    }
    case 0:
    default:
    {
        const float edge_t = (along - radius) / sigma;
        const float edge = std::exp(-2.0f * edge_t * edge_t);
        // Ring is edge-only so concurrent pulses stay distinct traveling shells.
        return edge;
    }
    }
}

float AxisWallEnergy(float dx,
                     float dy,
                     float dz,
                     int axis,
                     float radius,
                     float sigma,
                     int look,
                     float core,
                     float age,
                     float expand)
{
    return LookEnergy(std::fabs(AxisDelta(dx, dy, dz, axis)),
                      radius,
                      sigma,
                      look,
                      core,
                      age,
                      expand);
}

float SpreadAlongDistance(float dx, float dy, float dz, int spread_mode, int spread_orient, int shape)
{
    const int orient = std::clamp(spread_orient, 0, 2);
    switch(spread_mode)
    {
    case 1:
    {
        float pu = 0.0f;
        float pv = 0.0f;
        float pn = 0.0f;
        PlaneAxes(orient, dx, dy, dz, &pu, &pv, &pn);
        return PlanarFootprint(pu, pv, shape);
    }
    case 2:
        return std::fabs(AxisDelta(dx, dy, dz, orient));
    case 3:
    {
        int a0 = 0;
        int a1 = 2;
        CrossAxes(orient, &a0, &a1);
        return std::max(std::fabs(AxisDelta(dx, dy, dz, a0)),
                        std::fabs(AxisDelta(dx, dy, dz, a1)));
    }
    case 4:
        return std::max(std::fabs(dx), std::max(std::fabs(dy), std::fabs(dz)));
    case 0:
    default:
        return ShellDistance(dx, dy, dz, shape);
    }
}

float WaveSampleEnergy(const Vector3D& origin,
                       float spread_in,
                       float spacing_in,
                       float birth_time,
                       float strength,
                       float x,
                       float y,
                       float z,
                       float time,
                       float speed01,
                       int spread_mode,
                       int spread_orient,
                       int look_mode,
                       int shape,
                       int travel_pct,
                       int ring_width_pct,
                       int accel_pct,
                       int distance_fade_pct,
                       int press_glow_pct,
                       int press_glow_deci_sec,
                       float zone_diag)
{
    const float age = std::max(0.0f, time - birth_time);

    float spread = spread_in;
    float spacing = spacing_in;
    NormalizeWaveMetrics(spread, spacing, zone_diag);
    if(spread < 1.0e-4f || spacing < 1.0e-5f)
    {
        return 0.0f;
    }

    const WaveKinematics motion =
        ComputeWaveKinematics(spacing, spread, zone_diag, speed01, travel_pct, look_mode);
    const float expand = motion.expand;
    const float lifetime = motion.lifetime;
    const float travel = std::max(motion.travel, 1.0e-4f);
    if(age > lifetime && press_glow_pct <= 0)
    {
        return 0.0f;
    }

    const float dx = x - origin.x;
    const float dy = y - origin.y;
    const float dz = z - origin.z;
    const float dist3 = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float width01 = std::clamp(ring_width_pct / 100.0f, 0.0f, 1.0f);
    const float sigma = std::max(1.0e-5f, RingSigma(width01, spacing, spread, zone_diag));
    const float r_min = std::max(spacing * 0.28f, 1.0e-4f);

    float energy = 0.0f;
    float expand_inst = expand;
    if(age <= lifetime)
    {
        // Accel: start slow near the key, then pick up speed toward full travel.
        // power 1 = constant; up to ~3 = strong ease-in on distance.
        const float u = std::clamp(age / std::max(lifetime, 1.0e-5f), 0.0f, 1.0f);
        const float accel01 = std::clamp(accel_pct / 100.0f, 0.0f, 1.0f);
        const float power = 1.0f + accel01 * 2.0f;
        const float eased = std::pow(u, power);
        const float radius = std::min(travel, r_min + (travel - r_min) * eased);
        // Instantaneous expand for solid/shock trail clear (dr/dt).
        const float du_dt = 1.0f / std::max(lifetime, 1.0e-5f);
        const float deed_du = power * std::pow(std::max(u, 1.0e-4f), power - 1.0f);
        expand_inst = std::max((travel - r_min) * deed_du * du_dt, expand * 0.15f);

        const float core_sigma = std::max(1.0e-5f, spacing * 0.55f);
        const float core =
            (look_mode == 0 || look_mode == 4)
                ? 0.0f
                : (std::exp(-0.5f * (dist3 / core_sigma) * (dist3 / core_sigma)) *
                   std::exp(-age / 0.08f));

        float envelope = 1.0f;
        if(look_mode == 2)
        {
            const float remain = 1.0f - u;
            envelope = remain * remain;
        }
        else if(look_mode == 3)
        {
            envelope = std::exp(-age / std::max(lifetime * 0.55f, 1.0e-4f));
        }
        else if(look_mode == 1)
        {
            if(u > 0.52f)
            {
                envelope = 1.0f - (u - 0.52f) / 0.48f;
            }
        }
        else if(u > 0.72f)
        {
            envelope = 1.0f - (u - 0.72f) / 0.28f;
        }

        // Fade the traveling front as it gets farther from the press.
        const float fade01 = std::clamp(distance_fade_pct / 100.0f, 0.0f, 1.0f);
        const float progress = std::clamp((radius - r_min) / std::max(travel - r_min, 1.0e-4f), 0.0f, 1.0f);
        const float distance_fade = 1.0f - fade01 * progress * progress;

        const int orient = std::clamp(spread_orient, 0, 2);
        switch(spread_mode)
        {
        case 1:
        {
            float pu = 0.0f;
            float pv = 0.0f;
            float pn = 0.0f;
            PlaneAxes(orient, dx, dy, dz, &pu, &pv, &pn);
            const float slab = std::exp(-0.5f * ((pn * pn) / (sigma * sigma * 6.0f)));
            energy = LookEnergy(PlanarFootprint(pu, pv, shape), radius, sigma, look_mode, core, age, expand_inst) *
                     slab;
            break;
        }
        case 2:
            energy = AxisWallEnergy(dx, dy, dz, orient, radius, sigma, look_mode, core, age, expand_inst);
            break;
        case 3:
        {
            int a0 = 0;
            int a1 = 2;
            CrossAxes(orient, &a0, &a1);
            energy = std::max(AxisWallEnergy(dx, dy, dz, a0, radius, sigma, look_mode, core, age, expand_inst),
                              AxisWallEnergy(dx, dy, dz, a1, radius, sigma, look_mode, core, age, expand_inst));
            break;
        }
        case 4:
            energy = std::max(AxisWallEnergy(dx, dy, dz, 0, radius, sigma, look_mode, core, age, expand_inst),
                              std::max(AxisWallEnergy(dx, dy, dz, 1, radius, sigma, look_mode, core, age, expand_inst),
                                       AxisWallEnergy(dx, dy, dz, 2, radius, sigma, look_mode, core, age, expand_inst)));
            break;
        case 0:
        default:
            energy = LookEnergy(ShellDistance(dx, dy, dz, shape), radius, sigma, look_mode, core, age, expand_inst);
            break;
        }
        energy *= envelope * distance_fade * strength;
    }

    // Optional sticky glow on/near the pressed key (independent of the traveling shell).
    const float glow_amt = std::clamp(press_glow_pct / 100.0f, 0.0f, 1.0f);
    if(glow_amt > 1.0e-4f)
    {
        const float glow_tau = std::clamp(press_glow_deci_sec / 10.0f, 0.1f, 12.0f);
        const float glow_sigma = std::max(spacing * (0.85f + 1.8f * glow_amt), 1.0e-4f);
        const float glow =
            glow_amt * std::exp(-0.5f * (dist3 / glow_sigma) * (dist3 / glow_sigma)) *
            std::exp(-age / glow_tau);
        energy = energy + glow - energy * glow;
    }

    return energy;
}

RGBColor BlendPaletteAlong01(const std::vector<RGBColor>& palette, float color01)
{
    const int n = static_cast<int>(palette.size());
    if(n <= 0)
    {
        return 0xFFFFFF;
    }
    if(n == 1)
    {
        return palette[0];
    }
    const float scaled = std::fmod(color01 + 2.0f, 1.0f) * static_cast<float>(n);
    const int i0 = (static_cast<int>(std::floor(scaled))) % n;
    const int i1 = (i0 + 1) % n;
    const float frac = scaled - std::floor(scaled);
    const RGBColor a = palette[static_cast<size_t>(i0)];
    const RGBColor b = palette[static_cast<size_t>(i1)];
    const int r = static_cast<int>((a & 0xFF) * (1.0f - frac) + (b & 0xFF) * frac);
    const int g = static_cast<int>(((a >> 8) & 0xFF) * (1.0f - frac) + ((b >> 8) & 0xFF) * frac);
    const int bl = static_cast<int>(((a >> 16) & 0xFF) * (1.0f - frac) + ((b >> 16) & 0xFF) * frac);
    return static_cast<RGBColor>((bl << 16) | (g << 8) | r);
}

} // namespace

const char* Reactive::TriggerName(int mode)
{
    switch(mode)
    {
    case TRIGGER_PRESS:      return "On press";
    case TRIGGER_RELEASE:    return "On release";
    case TRIGGER_WHILE_HELD: return "While held";
    default:                 return "On press";
    }
}

const char* Reactive::WaveName(int mode)
{
    switch(mode)
    {
    case WAVE_ONCE:      return "Once";
    case WAVE_PULSE_2:   return "2 pulses";
    case WAVE_PULSE_4:   return "4 pulses";
    case WAVE_CONTINUAL: return "Continual";
    default:             return "Once";
    }
}

const char* Reactive::SpreadName(int mode)
{
    switch(mode)
    {
    case SPREAD_SHELL:  return "Shell (3D)";
    case SPREAD_PLANE:  return "Plane (2D)";
    case SPREAD_AXIS:   return "Axis wall";
    case SPREAD_CROSS:  return "Cross walls";
    case SPREAD_TRIAD:  return "Triad (X+Y+Z)";
    default:            return "Shell (3D)";
    }
}

const char* Reactive::SpreadOrientName(int spread, int orient)
{
    orient = std::clamp(orient, 0, SPREAD_ORIENT_COUNT - 1);
    switch(spread)
    {
    case SPREAD_PLANE:
        switch(orient)
        {
        case 0:
            return "XY";
        case 1:
            return "XZ";
        default:
            return "YZ";
        }
    case SPREAD_AXIS:
        switch(orient)
        {
        case 0:
            return "X";
        case 1:
            return "Y";
        default:
            return "Z";
        }
    case SPREAD_CROSS:
        switch(orient)
        {
        case 0:
            return "X + Z";
        case 1:
            return "X + Y";
        default:
            return "Y + Z";
        }
    default:
        return "—";
    }
}

const char* Reactive::LookName(int mode)
{
    switch(mode)
    {
    case LOOK_RING:    return "Ring";
    case LOOK_SOLID:   return "Solid";
    case LOOK_SHOCK:   return "Shock";
    case LOOK_FLASH:   return "Flash";
    case LOOK_DOUBLE:  return "Double ring";
    default:           return "Ring";
    }
}

const char* Reactive::ShapeName(int mode)
{
    switch(mode)
    {
    case SHAPE_SPHERE:     return "Sphere";
    case SHAPE_CUBE:       return "Cube";
    case SHAPE_OCTAHEDRON: return "Octahedron";
    case SHAPE_CYLINDER:   return "Cylinder";
    case SHAPE_TRIANGLE:   return "Triangle";
    case SHAPE_SQUARE:     return "Square";
    case SHAPE_PENTAGON:   return "Pentagon";
    case SHAPE_HEXAGON:    return "Hexagon";
    default:               return "Sphere";
    }
}

uint64_t Reactive::OriginKey(const Vector3D& p)
{
    const auto q = [](float v) -> uint64_t {
        return static_cast<uint64_t>(static_cast<uint32_t>(std::lround(v * 200.0f)));
    };
    return (q(p.x) << 42) ^ (q(p.y) << 21) ^ q(p.z);
}

uint64_t Reactive::PulseKey(uint64_t source_key, const Vector3D& origin)
{
    return source_key != 0 ? source_key : OriginKey(origin);
}

Reactive::Reactive(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(80);
}

EffectInfo3D Reactive::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Reactive";
    info.effect_description =
        "A pulse from the key or button you press. Mix Spread, Orientation, Shape, and Look. "
        "Rainbow hues follow distance from the press; Size sets rainbow cycles across Travel. "
        "Colours and Patterns (strip colormap) and the global colour gradient are supported. "
        "Input is used only while running.";
    info.category = "Reactive";
    info.effect_type = SPATIAL_EFFECT_REACTIVE;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;
    info.show_thickness_control = true;
    return info;
}


bool Reactive::SourceEnabled(ReactiveSourceKind kind) const
{
    switch(kind)
    {
    case ReactiveSourceKind::Keyboard:
        return listen_keyboard_;
    case ReactiveSourceKind::Mouse:
        return listen_mouse_;
    case ReactiveSourceKind::Gamepad:
        return listen_gamepad_;
    }
    return false;
}

bool Reactive::ShouldSpawnOnEdge(bool down) const
{
    switch(trigger_mode_)
    {
    case TRIGGER_PRESS:
        return down;
    case TRIGGER_RELEASE:
        return !down;
    case TRIGGER_WHILE_HELD:
        return down;
    default:
        return down;
    }
}

bool Reactive::UsesHeldRepeats() const
{
    if(trigger_mode_ == TRIGGER_RELEASE)
    {
        return false;
    }
    switch(wave_mode_)
    {
    case WAVE_ONCE:
        return false;
    case WAVE_PULSE_2:
    case WAVE_PULSE_4:
    case WAVE_CONTINUAL:
        return true;
    default:
        return false;
    }
}

int Reactive::PulseBudget() const
{
    switch(wave_mode_)
    {
    case WAVE_ONCE:
        return 1;
    case WAVE_PULSE_2:
        return 2;
    case WAVE_PULSE_4:
        return 4;
    case WAVE_CONTINUAL:
        return 0;
    default:
        return 1;
    }
}

float Reactive::RepeatIntervalSec() const
{
    const int deci = std::clamp(repeat_rate_deci_hz_, kMinRepeatDeciHz, kMaxRepeatDeciHz);
    return 10.0f / static_cast<float>(deci);
}

void Reactive::SpawnWave(const Vector3D& origin,
                         float time,
                         float strength,
                         float spread,
                         float spacing,
                         uint64_t source_key,
                         float zone_diag)
{
    // Always append a fully independent pulse snapshot. Never rewrite an in-flight wave.
    WaveImpact wave{};
    wave.origin = origin;
    wave.birth_time = time;
    wave.strength = std::clamp(strength, 0.05f, 1.0f);
    wave.spread = std::max(0.0f, spread);
    wave.spacing = std::max(0.0f, spacing);
    wave.speed01 = GetNormalizedSpeed();
    wave.spread_mode = std::clamp(spread_mode_, 0, SPREAD_COUNT - 1);
    wave.spread_orient = std::clamp(spread_orient_, 0, SPREAD_ORIENT_COUNT - 1);
    wave.look_mode = std::clamp(look_mode_, 0, LOOK_COUNT - 1);
    wave.footprint_shape = std::clamp(footprint_shape_, 0, SHAPE_COUNT - 1);
    wave.travel_pct = std::clamp(travel_pct_, 0, 100);
    wave.ring_width_pct = std::clamp(GetBandThickness(), 0, 100);
    wave.accel_pct = std::clamp(accel_pct_, 0, 100);
    wave.distance_fade_pct = std::clamp(distance_fade_pct_, 0, 100);
    wave.press_glow_pct = std::clamp(press_glow_pct_, 0, 100);
    wave.press_glow_deci_sec = std::clamp(press_glow_deci_sec_, 1, 120);
    wave.color_slot = next_color_slot_++;
    wave.source_key = source_key;
    if(static_cast<int>(waves_.size()) >= kMaxWaves)
    {
        // Drop the wave nearest finishing — not blindly the oldest FIFO slot —
        // so a spam of new pulses doesn't cancel a still-traveling mid-life shell.
        size_t drop = 0;
        float best_frac = -1.0f;
        for(size_t i = 0; i < waves_.size(); ++i)
        {
            const WaveImpact& w = waves_[i];
            const float age = std::max(0.0f, time - w.birth_time);
            float keep = WaveTravelLifetime(
                w.spacing, w.spread, zone_diag, w.speed01, w.travel_pct, w.look_mode);
            if(w.press_glow_pct > 0)
            {
                keep = std::max(keep, (w.press_glow_deci_sec / 10.0f) * 3.5f);
            }
            const float frac = age / std::max(keep, 1.0e-4f);
            if(frac > best_frac)
            {
                best_frac = frac;
                drop = i;
            }
        }
        waves_.erase(waves_.begin() + static_cast<std::ptrdiff_t>(drop));
    }
    waves_.push_back(wave);
    last_spawn_time_[PulseKey(source_key, origin)] = time;
}

void Reactive::TickWaves(float time, const GridContext3D& grid, std::uint64_t frame_key)
{
    ReactiveInputManager* mgr = ReactiveInputManager::instance();
    if(!mgr || !mgr->isRunning())
    {
        waves_.clear();
        last_spawn_time_.clear();
        spawn_counts_.clear();
        have_tick_time_ = false;
        have_edges_frame_key_ = false;
        return;
    }

    // Only wipe on a real clock rewind (effect restart). Tiny float jitter must not
    // destroy in-flight rings or they look like the pulse "reset" to the center.
    if(have_tick_time_ && time + 0.25f < last_tick_time_)
    {
        waves_.clear();
        last_spawn_time_.clear();
        spawn_counts_.clear();
        have_edges_frame_key_ = false;
    }

    const float zone_diag = std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    const std::uint64_t key = frame_key != 0 ? frame_key
        : (static_cast<std::uint64_t>(std::llround(time * 240.0)) | (1ull << 63));
    const bool same_time = have_tick_time_ && std::fabs(time - last_tick_time_) < 1e-5f;
    const bool apply_edges = !have_edges_frame_key_ || last_edges_frame_key_ != key;

    // Apply edges even when time is unchanged (new render frame) so presses are not dropped.
    if(apply_edges)
    {
        last_edges_frame_key_ = key;
        have_edges_frame_key_ = true;

        std::vector<ReactiveOriginEvent> edges;
        mgr->CopyOriginEdgesForFrame(key, edges);
        for(const ReactiveOriginEvent& edge : edges)
        {
            if(!SourceEnabled(edge.kind))
            {
                continue;
            }
            if(ShouldSpawnOnEdge(edge.down))
            {
                const uint64_t pulse_key = PulseKey(edge.source_key, edge.room_position);
                SpawnWave(edge.room_position,
                          time,
                          1.0f,
                          edge.device_spread,
                          edge.led_spacing,
                          edge.source_key,
                          zone_diag);
                if(edge.down)
                {
                    spawn_counts_[pulse_key] = 1;
                }
                else
                {
                    spawn_counts_.erase(pulse_key);
                }
            }
        }
    }

    if(same_time)
    {
        return;
    }
    last_tick_time_ = time;
    have_tick_time_ = true;

    if(UsesHeldRepeats())
    {
        std::vector<ReactiveHeldOrigin> held;
        mgr->CopyHeldOrigins(held);
        std::unordered_set<uint64_t> held_keys;
        const float interval = RepeatIntervalSec();
        const int budget = PulseBudget();
        for(const ReactiveHeldOrigin& h : held)
        {
            if(!SourceEnabled(h.kind))
            {
                continue;
            }
            const uint64_t key_h = PulseKey(h.source_key, h.room_position);
            held_keys.insert(key_h);
            if(budget > 0)
            {
                const auto count_found = spawn_counts_.find(key_h);
                if(count_found != spawn_counts_.end() && count_found->second >= budget)
                {
                    continue;
                }
            }
            const auto found = last_spawn_time_.find(key_h);
            if(found != last_spawn_time_.end() && (time - found->second) < interval)
            {
                continue;
            }
            SpawnWave(h.room_position, time, 0.85f, h.device_spread, h.led_spacing, h.source_key, zone_diag);
            spawn_counts_[key_h] += 1;
        }
        for(auto it = spawn_counts_.begin(); it != spawn_counts_.end();)
        {
            if(held_keys.find(it->first) == held_keys.end())
            {
                it = spawn_counts_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    waves_.erase(std::remove_if(waves_.begin(),
                                waves_.end(),
                                [time, zone_diag](const WaveImpact& w) {
                                    const float age = time - w.birth_time;
                                    if(age < 0.0f)
                                    {
                                        return false;
                                    }
                                    const float life = WaveTravelLifetime(
                                        w.spacing, w.spread, zone_diag, w.speed01, w.travel_pct, w.look_mode);
                                    float keep = life;
                                    if(w.press_glow_pct > 0)
                                    {
                                        keep = std::max(keep, (w.press_glow_deci_sec / 10.0f) * 3.5f);
                                    }
                                    return age > keep;
                                }),
                 waves_.end());
}

void Reactive::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    TickWaves(time_sec, grid, render_sequence);
}

RGBColor Reactive::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    TickWaves(time, grid, grid.render_sequence);

    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    if(waves_.empty())
    {
        return 0x00000000;
    }

    float energy_or = 0.0f;
    float weight_sum = 0.0f;
    float along_acc = 0.0f;
    float travel_acc = 0.0f;
    float slot_phase_acc = 0.0f;
    const float zone_diag =
        std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);
    for(const WaveImpact& w : waves_)
    {
        const float energy = WaveSampleEnergy(w.origin,
                                              w.spread,
                                              w.spacing,
                                              w.birth_time,
                                              w.strength,
                                              x,
                                              y,
                                              z,
                                              time,
                                              w.speed01,
                                              w.spread_mode,
                                              w.spread_orient,
                                              w.look_mode,
                                              w.footprint_shape,
                                              w.travel_pct,
                                              w.ring_width_pct,
                                              w.accel_pct,
                                              w.distance_fade_pct,
                                              w.press_glow_pct,
                                              w.press_glow_deci_sec,
                                              zone_diag);
        if(energy <= 1.0e-4f)
        {
            continue;
        }
        // Soft-OR so concentric shells from the same key all stay visible.
        energy_or = energy_or + energy - energy_or * energy;
        const float dx = x - w.origin.x;
        const float dy = y - w.origin.y;
        const float dz = z - w.origin.z;
        const float along = SpreadAlongDistance(dx, dy, dz, w.spread_mode, w.spread_orient, w.footprint_shape);
        float spread_m = w.spread;
        float spacing_m = w.spacing;
        NormalizeWaveMetrics(spread_m, spacing_m, zone_diag);
        const float travel = ComputeWaveKinematics(spacing_m,
                                                   spread_m,
                                                   zone_diag,
                                                   w.speed01,
                                                   w.travel_pct,
                                                   w.look_mode)
                                 .travel;
        // Energy-weighted color drivers so a newborn pulse cannot steal hue from an
        // older outer shell (winner-take-all looked like a full restart).
        weight_sum += energy;
        along_acc += energy * along;
        travel_acc += energy * travel;
        slot_phase_acc += energy * (static_cast<float>(w.color_slot % 8u) / 8.0f);
    }

    if(energy_or <= 0.02f || weight_sum <= 1.0e-5f)
    {
        return 0x00000000;
    }

    const float bright = std::clamp(GetBrightness() / 100.0f, 0.0f, 1.0f);
    const float intensity = std::min(1.0f, energy_or * bright);

    const float mean_along = along_acc / weight_sum;
    const float mean_travel = std::max(travel_acc / weight_sum, 1.0e-4f);
    const float radial01 = std::clamp(mean_along / mean_travel, 0.0f, 1.0f);
    const float rainbow_cycles = std::clamp(GetNormalizedSize(), 0.25f, 2.5f);
    const float slot_phase = slot_phase_acc / weight_sum;
    const float color_driver =
        std::fmod(radial01 * rainbow_cycles + slot_phase + GetColorCycleHz() * time + 2.0f, 1.0f);

    Vector3D anchor = GetEffectOriginGrid(grid);
    Vector3D sample_pos{x, y, z};
    const float coord_y01 = SampleStratumYNorm01(y, grid, anchor);
    SpatialLayerCore::Basis basis{};
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map{};
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = 0.12f;
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = 1.1f;
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = anchor.x;
    sp.origin_y = anchor.y;
    sp.origin_z = anchor.z;
    sp.y_norm = coord_y01;

    RGBColor c;
    if(UseEffectStripColormap())
    {
        const float strip_p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                            GetEffectStripColormapUnfold(),
                                                            GetEffectStripColormapDirectionDeg(),
                                                            color_driver,
                                                            time,
                                                            grid,
                                                            GetNormalizedSize(),
                                                            anchor,
                                                            sample_pos);
        c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
    }
    else if(GetRainbowMode())
    {
        float hue = std::fmod(radial01 * 360.0f * rainbow_cycles +
                                  slot_phase * 8.0f * 47.0f +
                                  GetColorCycleHz() * time * 360.0f + 720.0f,
                              360.0f);
        hue = ApplySpatialRainbowHue(hue, radial01, basis, sp, map, time, &grid);
        c = GetRainbowColor(hue);
    }
    else
    {
        const std::vector<RGBColor> palette = GetColors();
        if(!palette.empty())
        {
            c = BlendPaletteAlong01(palette, color_driver);
        }
        else
        {
            const float p = ApplySpatialPalette01(color_driver, basis, sp, map, time, &grid);
            c = GetColorAtPosition(p);
        }
    }

    const int r = std::clamp(static_cast<int>((c & 0xFF) * intensity), 0, 255);
    const int g = std::clamp(static_cast<int>(((c >> 8) & 0xFF) * intensity), 0, 255);
    const int b = std::clamp(static_cast<int>(((c >> 16) & 0xFF) * intensity), 0, 255);
    return static_cast<RGBColor>((b << 16) | (g << 8) | r);
}

nlohmann::json Reactive::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["listen_keyboard"] = listen_keyboard_;
    j["listen_mouse"] = listen_mouse_;
    j["listen_gamepad"] = listen_gamepad_;
    j["trigger_mode"] = trigger_mode_;
    j["wave_mode"] = wave_mode_;
    j["repeat_rate_deci_hz"] = repeat_rate_deci_hz_;
    j["spread_mode"] = spread_mode_;
    j["spread_orient"] = spread_orient_;
    j["look_mode"] = look_mode_;
    j["footprint_shape"] = footprint_shape_;
    j["travel_pct"] = travel_pct_;
    j["accel_pct"] = accel_pct_;
    j["distance_fade_pct"] = distance_fade_pct_;
    j["press_glow_pct"] = press_glow_pct_;
    j["press_glow_deci_sec"] = press_glow_deci_sec_;
    return j;
}

void Reactive::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("listen_keyboard"))
    {
        listen_keyboard_ = settings["listen_keyboard"].get<bool>();
    }
    if(settings.contains("listen_mouse"))
    {
        listen_mouse_ = settings["listen_mouse"].get<bool>();
    }
    if(settings.contains("listen_gamepad"))
    {
        listen_gamepad_ = settings["listen_gamepad"].get<bool>();
    }
    if(settings.contains("trigger_mode"))
    {
        trigger_mode_ = std::clamp(settings["trigger_mode"].get<int>(), 0, TRIGGER_COUNT - 1);
    }
    if(settings.contains("wave_mode"))
    {
        wave_mode_ = std::clamp(settings["wave_mode"].get<int>(), 0, WAVE_COUNT - 1);
    }
    if(settings.contains("repeat_rate_deci_hz"))
    {
        repeat_rate_deci_hz_ = std::clamp(settings["repeat_rate_deci_hz"].get<int>(),
                                          kMinRepeatDeciHz,
                                          kMaxRepeatDeciHz);
    }
    if(settings.contains("spread_mode"))
    {
        spread_mode_ = std::clamp(settings["spread_mode"].get<int>(), 0, SPREAD_COUNT - 1);
    }
    if(settings.contains("spread_orient"))
    {
        spread_orient_ = std::clamp(settings["spread_orient"].get<int>(), 0, SPREAD_ORIENT_COUNT - 1);
    }
    if(settings.contains("look_mode"))
    {
        look_mode_ = std::clamp(settings["look_mode"].get<int>(), 0, LOOK_COUNT - 1);
    }
    if(settings.contains("footprint_shape"))
    {
        footprint_shape_ = std::clamp(settings["footprint_shape"].get<int>(), 0, SHAPE_COUNT - 1);
    }
    if(settings.contains("travel_pct"))
    {
        travel_pct_ = std::clamp(settings["travel_pct"].get<int>(), 0, 100);
    }
    if(settings.contains("accel_pct"))
    {
        accel_pct_ = std::clamp(settings["accel_pct"].get<int>(), 0, 100);
    }
    if(settings.contains("distance_fade_pct"))
    {
        distance_fade_pct_ = std::clamp(settings["distance_fade_pct"].get<int>(), 0, 100);
    }
    if(settings.contains("press_glow_pct"))
    {
        press_glow_pct_ = std::clamp(settings["press_glow_pct"].get<int>(), 0, 100);
    }
    if(settings.contains("press_glow_deci_sec"))
    {
        press_glow_deci_sec_ = std::clamp(settings["press_glow_deci_sec"].get<int>(), 1, 120);
    }
    if(thickness_slider)
    {
        thickness_slider->setValue((int)effect_band_thickness);
    }
    SyncCustomUiFromModel();
}

void RegisterReactiveEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "Reactive",
        "Reactive",
        "Reactive",
        "",
        "",
        []() { return new Reactive; });
}
