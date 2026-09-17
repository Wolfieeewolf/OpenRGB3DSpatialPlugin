// SPDX-License-Identifier: GPL-2.0-only

#include "Reactive.h"
#include "ReactiveInputManager.h"
#include "SpatialKernelColormap.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include "EffectCheckRow.h"
#include "EffectSliderRow.h"
#include "EffectLabeledComboRow.h"

#include <QComboBox>
#include <QCheckBox>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <unordered_set>

REGISTER_EFFECT_3D(Reactive);

namespace
{
constexpr int kMaxWaves = 16;
constexpr float kMinLifetime = 0.20f;
constexpr float kMaxLifetime = 1.05f;
constexpr int kMinRepeatDeciHz = 1;
constexpr int kMaxRepeatDeciHz = 100;

QString FormatRepeatRate(int deci_hz)
{
    const int clamped = std::clamp(deci_hz, kMinRepeatDeciHz, kMaxRepeatDeciHz);
    return QString::number(clamped / 10.0, 'f', 1) + QStringLiteral(" Hz");
}

QString FormatTravel(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    const char* label = "Device";
    if(v <= 12)
    {
        label = "1 key";
    }
    else if(v <= 28)
    {
        label = "Nearby";
    }
    else if(v <= 58)
    {
        label = "Device";
    }
    else if(v <= 82)
    {
        label = "Around";
    }
    else
    {
        label = "Room";
    }
    return QString::number(v) + QStringLiteral(" · ") + QString::fromUtf8(label);
}

QString FormatRingWidth(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    const char* label = "Medium";
    if(v <= 8)
    {
        label = "Hairline";
    }
    else if(v <= 22)
    {
        label = "Thin";
    }
    else if(v <= 55)
    {
        label = "Medium";
    }
    else if(v <= 82)
    {
        label = "Thick";
    }
    else
    {
        label = "Solid";
    }
    return QString::number(v) + QStringLiteral(" · ") + QString::fromUtf8(label);
}

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
    if(look_mode == 2)
    {
        k.expand *= 1.85f;
        const float punch = 0.14f + 0.16f * (1.0f - std::clamp(speed01, 0.0f, 1.0f));
        k.lifetime = std::min(k.travel / std::max(k.expand, 1.0e-6f), punch);
    }
    else if(look_mode == 3)
    {
        const float punch = 0.10f + 0.12f * (1.0f - std::clamp(speed01, 0.0f, 1.0f));
        k.lifetime = punch;
        k.expand = k.travel / std::max(punch, 1.0e-4f);
    }
    else
    {
        k.lifetime = k.travel / std::max(k.expand, 1.0e-6f);
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
    const float thick = std::max(spread * 0.92f, std::max(spacing * 14.0f, zone_diag * 0.20f));
    return thin + (thick - thin) * (t * t);
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
        return std::max(std::max(ring_at(radius), ring_at(std::max(0.0f, radius - sep))), core);
    }
    case 0:
    default:
    {
        const float edge_t = (along - radius) / sigma;
        const float edge = std::exp(-2.0f * edge_t * edge_t);
        return std::max(edge, core);
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
                       float zone_diag)
{
    const float age = time - birth_time;
    if(age < 0.0f)
    {
        return 0.0f;
    }

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
    if(age > lifetime)
    {
        return 0.0f;
    }

    const float dx = x - origin.x;
    const float dy = y - origin.y;
    const float dz = z - origin.z;
    const float width01 = std::clamp(ring_width_pct / 100.0f, 0.0f, 1.0f);
    const float sigma = std::max(1.0e-5f, RingSigma(width01, spacing, spread, zone_diag));
    const float radius = expand * age;

    const float dist3 = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float core_sigma = std::max(1.0e-5f, spacing * 0.55f);
    const float core_tau = (look_mode == 0 || look_mode == 4) ? std::max(0.07f, lifetime * 0.16f) : 0.08f;
    const float core = std::exp(-0.5f * (dist3 / core_sigma) * (dist3 / core_sigma)) *
                       std::exp(-age / core_tau);

    float envelope = 1.0f;
    if(look_mode == 2)
    {
        const float remain = 1.0f - std::clamp(age / std::max(lifetime, 1.0e-5f), 0.0f, 1.0f);
        envelope = remain * remain;
    }
    else if(look_mode == 3)
    {
        envelope = std::exp(-age / std::max(lifetime * 0.55f, 1.0e-4f));
    }
    else if(look_mode == 1)
    {
        if(age > lifetime * 0.52f)
        {
            envelope = 1.0f - (age - lifetime * 0.52f) / (lifetime * 0.48f);
        }
    }
    else if(age > lifetime * 0.72f)
    {
        envelope = 1.0f - (age - lifetime * 0.72f) / (lifetime * 0.28f);
    }

    const int orient = std::clamp(spread_orient, 0, 2);
    float energy = 0.0f;
    switch(spread_mode)
    {
    case 1:
    {
        float pu = 0.0f;
        float pv = 0.0f;
        float pn = 0.0f;
        PlaneAxes(orient, dx, dy, dz, &pu, &pv, &pn);
        const float slab = std::exp(-0.5f * ((pn * pn) / (sigma * sigma * 6.0f)));
        energy = LookEnergy(PlanarFootprint(pu, pv, shape), radius, sigma, look_mode, core, age, expand) *
                 slab;
        break;
    }
    case 2:
        energy = AxisWallEnergy(dx, dy, dz, orient, radius, sigma, look_mode, core, age, expand);
        break;
    case 3:
    {
        int a0 = 0;
        int a1 = 2;
        CrossAxes(orient, &a0, &a1);
        energy = std::max(AxisWallEnergy(dx, dy, dz, a0, radius, sigma, look_mode, core, age, expand),
                          AxisWallEnergy(dx, dy, dz, a1, radius, sigma, look_mode, core, age, expand));
        break;
    }
    case 4:
        energy = std::max(AxisWallEnergy(dx, dy, dz, 0, radius, sigma, look_mode, core, age, expand),
                          std::max(AxisWallEnergy(dx, dy, dz, 1, radius, sigma, look_mode, core, age, expand),
                                   AxisWallEnergy(dx, dy, dz, 2, radius, sigma, look_mode, core, age, expand)));
        break;
    case 0:
    default:
        energy = LookEnergy(ShellDistance(dx, dy, dz, shape), radius, sigma, look_mode, core, age, expand);
        break;
    }

    return energy * envelope * strength;
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
        return static_cast<uint64_t>(static_cast<uint32_t>(std::lround(v * 40.0f)));
    };
    return (q(p.x) << 42) ^ (q(p.y) << 21) ^ q(p.z);
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
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_REACTIVE;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;
    return info;
}

void Reactive::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("ReactiveEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    if(!layout)
    {
        return;
    }
    const auto on_changed = [this]() { emit ParametersChanged(); };

    EffectUiRows::AppendInfoLabel(
        layout,
        QStringLiteral("Listens only while this effect is running. Presses are not saved or shown."));

    EffectCheckRow* keyboard_row = EffectUiRows::AppendCheckRow(
        layout,
        QStringLiteral("Keyboard"),
        listen_keyboard_,
        QStringLiteral("Waves from the named LED on the keyboard or keypad that sent the press."));
    keyboard_row->setObjectName(QStringLiteral("listenKeyboardRow"));
    if(keyboard_row->checkBox())
    {
        keyboard_row->checkBox()->setObjectName(QStringLiteral("listenKeyboardCheck"));
    }
    keyboard_row->bindToggled(this, [this](bool on) { listen_keyboard_ = on; }, on_changed);

    EffectCheckRow* mouse_row = EffectUiRows::AppendCheckRow(
        layout,
        QStringLiteral("Mouse"),
        listen_mouse_,
        QStringLiteral("Waves from the left, right, or middle mouse-button LED in the 3D layout."));
    mouse_row->setObjectName(QStringLiteral("listenMouseRow"));
    if(mouse_row->checkBox())
    {
        mouse_row->checkBox()->setObjectName(QStringLiteral("listenMouseCheck"));
    }
    mouse_row->bindToggled(this, [this](bool on) { listen_mouse_ = on; }, on_changed);

    EffectCheckRow* pad_row = EffectUiRows::AppendCheckRow(
        layout,
        QStringLiteral("Gamepad"),
        listen_gamepad_,
        QStringLiteral("Waves from the pressed controller button LED in the 3D layout."));
    pad_row->setObjectName(QStringLiteral("listenGamepadRow"));
    if(pad_row->checkBox())
    {
        pad_row->checkBox()->setObjectName(QStringLiteral("listenGamepadCheck"));
    }
    pad_row->bindToggled(this, [this](bool on) { listen_gamepad_ = on; }, on_changed);

    EffectLabeledComboRow* trigger_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Trigger:"));
    trigger_row->setObjectName(QStringLiteral("triggerRow"));
    QComboBox* trigger_combo = trigger_row->combo();
    for(int m = 0; m < TRIGGER_COUNT; ++m)
    {
        trigger_combo->addItem(QString::fromUtf8(TriggerName(m)));
    }
    trigger_combo->setCurrentIndex(std::clamp(trigger_mode_, 0, TRIGGER_COUNT - 1));
    trigger_combo->setToolTip(QStringLiteral(
        "On press: fire when the key or button goes down.\n"
        "On release: fire when it comes up.\n"
        "While held: treat the down state as the trigger (pairs with 2 pulses, 4 pulses, or Continual)."));
    connect(trigger_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        trigger_mode_ = std::clamp(idx, 0, TRIGGER_COUNT - 1);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* wave_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Wave:"));
    wave_row->setObjectName(QStringLiteral("waveRow"));
    QComboBox* wave_combo = wave_row->combo();
    for(int m = 0; m < WAVE_COUNT; ++m)
    {
        wave_combo->addItem(QString::fromUtf8(WaveName(m)));
    }
    wave_combo->setCurrentIndex(std::clamp(wave_mode_, 0, WAVE_COUNT - 1));
    wave_combo->setToolTip(QStringLiteral(
        "Once: a single expanding shell from the pressed key or button.\n"
        "2 pulses / 4 pulses: extra shells while you hold, at the pulse rate.\n"
        "Continual: keep pulsing from that button for as long as it is held."));

    EffectSliderRow* rate_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pulse rate:"),
        kMinRepeatDeciHz,
        kMaxRepeatDeciHz,
        repeat_rate_deci_hz_,
        QStringLiteral(
            "How fast extra shells spawn while a key or button is held (0.1–10 Hz). "
            "Used for 2 pulses, 4 pulses, and Continual. Once ignores this."));
    rate_row->setObjectName(QStringLiteral("repeatRateRow"));
    rate_row->bindValueChanged(
        this,
        [this](int v) { repeat_rate_deci_hz_ = std::clamp(v, kMinRepeatDeciHz, kMaxRepeatDeciHz); },
        FormatRepeatRate,
        on_changed);
    rate_row->setEnabled(wave_mode_ != WAVE_ONCE);
    connect(wave_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, rate_row](int idx) {
        wave_mode_ = std::clamp(idx, 0, WAVE_COUNT - 1);
        rate_row->setEnabled(wave_mode_ != WAVE_ONCE);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* spread_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Spread:"));
    spread_row->setObjectName(QStringLiteral("spreadRow"));
    QComboBox* spread_combo = spread_row->combo();
    for(int m = 0; m < SPREAD_COUNT; ++m)
    {
        spread_combo->addItem(QString::fromUtf8(SpreadName(m)));
    }
    spread_combo->setCurrentIndex(std::clamp(spread_mode_, 0, SPREAD_COUNT - 1));
    spread_combo->setToolTip(QStringLiteral(
        "How distance is measured from the press.\n"
        "Shell: full 3D (sphere, cube, …).\n"
        "Plane: 2D ripple in XY, XZ, or YZ.\n"
        "Axis wall: thin wall traveling on X, Y, or Z.\n"
        "Cross: two walls (X+Z, X+Y, or Y+Z).\n"
        "Triad: all three axis walls at once."));

    EffectLabeledComboRow* orient_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Orientation:"));
    orient_row->setObjectName(QStringLiteral("orientRow"));
    QComboBox* orient_combo = orient_row->combo();
    orient_combo->setToolTip(QStringLiteral(
        "Plane: XY (vertical sheet), XZ (keyboard/desk), YZ (side wall).\n"
        "Axis: travel along X, Y, or Z.\n"
        "Cross: pick the wall pair. Shell and Triad ignore this."));

    EffectLabeledComboRow* look_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Look:"));
    look_row->setObjectName(QStringLiteral("lookRow"));
    QComboBox* look_combo = look_row->combo();
    for(int m = 0; m < LOOK_COUNT; ++m)
    {
        look_combo->addItem(QString::fromUtf8(LookName(m)));
    }
    look_combo->setCurrentIndex(std::clamp(look_mode_, 0, LOOK_COUNT - 1));
    look_combo->setToolTip(QStringLiteral(
        "How the front is filled.\n"
        "Ring: traveling band only.\n"
        "Solid: filled behind the front, then clears.\n"
        "Shock: fast blast and fireball.\n"
        "Flash: bright pop at the key, little travel.\n"
        "Double ring: two nested bands."));

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Shape:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    QComboBox* shape_combo = shape_row->combo();
    for(int m = 0; m < SHAPE_COUNT; ++m)
    {
        shape_combo->addItem(QString::fromUtf8(ShapeName(m)));
    }
    shape_combo->setCurrentIndex(std::clamp(footprint_shape_, 0, SHAPE_COUNT - 1));
    shape_combo->setToolTip(QStringLiteral(
        "Footprint for Shell (3D) and Plane (2D): sphere/circle, cube/square, hex, etc."));

    EffectSliderRow* travel_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Travel:"),
        0,
        100,
        travel_pct_,
        QStringLiteral(
            "How far the pulse travels. Expansion speed follows the global Speed slider, "
            "not this control. 1 key stays on the pressed button, Nearby is two or three keys, "
            "Device covers that controller, Room fills the layer."));
    travel_row->setObjectName(QStringLiteral("travelRow"));
    travel_row->bindValueChanged(
        this,
        [this](int v) { travel_pct_ = std::clamp(v, 0, 100); },
        FormatTravel,
        on_changed);

    EffectSliderRow* ring_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Thickness:"),
        0,
        100,
        ring_width_pct_,
        QStringLiteral(
            "Width of the glowing band or wall. Hairline is a thin pulse; Solid is so wide "
            "it almost fills in. Applies to every spread and look."));
    ring_row->setObjectName(QStringLiteral("ringWidthRow"));
    ring_row->bindValueChanged(
        this,
        [this](int v) { ring_width_pct_ = std::clamp(v, 0, 100); },
        FormatRingWidth,
        on_changed);

    const auto repopulate_orient = [orient_combo, this]() {
        orient_combo->blockSignals(true);
        orient_combo->clear();
        const int spread = std::clamp(spread_mode_, 0, SPREAD_COUNT - 1);
        if(spread == SPREAD_PLANE || spread == SPREAD_AXIS || spread == SPREAD_CROSS)
        {
            for(int o = 0; o < SPREAD_ORIENT_COUNT; ++o)
            {
                orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, o)));
            }
            orient_combo->setCurrentIndex(std::clamp(spread_orient_, 0, SPREAD_ORIENT_COUNT - 1));
            orient_combo->setEnabled(true);
        }
        else
        {
            orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, spread_orient_)));
            orient_combo->setCurrentIndex(0);
            orient_combo->setEnabled(false);
        }
        orient_combo->blockSignals(false);
    };
    const auto sync_shape_enabled = [shape_row, this]() {
        shape_row->setEnabled(spread_mode_ == SPREAD_SHELL || spread_mode_ == SPREAD_PLANE);
    };
    connect(spread_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, repopulate_orient, sync_shape_enabled](int idx) {
        spread_mode_ = std::clamp(idx, 0, SPREAD_COUNT - 1);
        repopulate_orient();
        sync_shape_enabled();
        emit ParametersChanged();
    });
    connect(orient_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        spread_orient_ = std::clamp(idx, 0, SPREAD_ORIENT_COUNT - 1);
        emit ParametersChanged();
    });
    connect(look_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        look_mode_ = std::clamp(idx, 0, LOOK_COUNT - 1);
        emit ParametersChanged();
    });
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        footprint_shape_ = std::clamp(idx, 0, SHAPE_COUNT - 1);
        emit ParametersChanged();
    });
    repopulate_orient();
    sync_shape_enabled();

    AddWidgetToParent(w, parent);
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
                         float spacing)
{
    WaveImpact wave{};
    wave.origin = origin;
    wave.birth_time = time;
    wave.strength = std::clamp(strength, 0.05f, 1.0f);
    wave.spread = std::max(0.0f, spread);
    wave.spacing = std::max(0.0f, spacing);
    wave.color_slot = next_color_slot_++;
    if(static_cast<int>(waves_.size()) >= kMaxWaves)
    {
        waves_.erase(waves_.begin());
    }
    waves_.push_back(wave);
    last_spawn_time_[OriginKey(origin)] = time;
}

void Reactive::TickWaves(float time, const GridContext3D& grid)
{
    ReactiveInputManager* mgr = ReactiveInputManager::instance();
    if(!mgr || !mgr->isRunning())
    {
        waves_.clear();
        last_spawn_time_.clear();
        spawn_counts_.clear();
        have_tick_time_ = false;
        return;
    }

    if(have_tick_time_ && std::fabs(time - last_tick_time_) < 1e-5f)
    {
        return;
    }
    last_tick_time_ = time;
    have_tick_time_ = true;

    const float speed01 = GetNormalizedSpeed();
    const float zone_diag = std::sqrt(grid.width * grid.width + grid.height * grid.height + grid.depth * grid.depth);

    std::vector<ReactiveOriginEvent> edges;
    mgr->DrainOriginEdges(edges);
    for(const ReactiveOriginEvent& edge : edges)
    {
        if(!SourceEnabled(edge.kind))
        {
            continue;
        }
        if(ShouldSpawnOnEdge(edge.down))
        {
            SpawnWave(edge.room_position, time, 1.0f, edge.device_spread, edge.led_spacing);
            if(edge.down)
            {
                spawn_counts_[OriginKey(edge.room_position)] = 1;
            }
        }
    }

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
            const uint64_t key = OriginKey(h.room_position);
            held_keys.insert(key);
            if(budget > 0)
            {
                const auto count_found = spawn_counts_.find(key);
                if(count_found != spawn_counts_.end() && count_found->second >= budget)
                {
                    continue;
                }
            }
            const auto found = last_spawn_time_.find(key);
            if(found != last_spawn_time_.end() && (time - found->second) < interval)
            {
                continue;
            }
            SpawnWave(h.room_position, time, 0.85f, h.device_spread, h.led_spacing);
            spawn_counts_[key] += 1;
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
                                [time, speed01, zone_diag, this](const WaveImpact& w) {
                                    const float life = WaveTravelLifetime(
                                        w.spacing, w.spread, zone_diag, speed01, travel_pct_, look_mode_);
                                    return (time - w.birth_time) > life;
                                }),
                 waves_.end());
}

void Reactive::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    (void)render_sequence;
    TickWaves(time_sec, grid);
}

RGBColor Reactive::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    TickWaves(time, grid);

    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    Vector3D anchor = GetEffectOriginGrid(grid);
    const float rel_x = x - anchor.x;
    const float rel_y = y - anchor.y;
    const float rel_z = z - anchor.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    if(waves_.empty())
    {
        return 0x00000000;
    }

    float best = 0.0f;
    float best_along = 0.0f;
    float best_travel = 1.0f;
    uint32_t best_slot = 0;
    const float speed01 = GetNormalizedSpeed();
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
                                              speed01,
                                              spread_mode_,
                                              spread_orient_,
                                              look_mode_,
                                              footprint_shape_,
                                              travel_pct_,
                                              ring_width_pct_,
                                              zone_diag);
        if(energy > best)
        {
            best = energy;
            best_slot = w.color_slot;
            const float dx = x - w.origin.x;
            const float dy = y - w.origin.y;
            const float dz = z - w.origin.z;
            best_along = SpreadAlongDistance(dx, dy, dz, spread_mode_, spread_orient_, footprint_shape_);
            float spread_m = w.spread;
            float spacing_m = w.spacing;
            NormalizeWaveMetrics(spread_m, spacing_m, zone_diag);
            best_travel = ComputeWaveKinematics(spacing_m,
                                                spread_m,
                                                zone_diag,
                                                speed01,
                                                travel_pct_,
                                                look_mode_)
                              .travel;
        }
    }

    if(best <= 0.02f)
    {
        return 0x00000000;
    }

    const float bright = std::clamp(GetBrightness() / 100.0f, 0.0f, 1.0f);
    const float intensity = std::min(1.0f, best * (0.85f + 0.55f * bright));

    const float radial01 =
        std::clamp(best_along / std::max(best_travel, 1.0e-4f), 0.0f, 1.0f);
    const float rainbow_cycles = std::clamp(GetNormalizedSize(), 0.25f, 2.5f);
    const float slot_phase = static_cast<float>(best_slot % 8u) / 8.0f;
    const float color_driver =
        std::fmod(radial01 * rainbow_cycles + slot_phase + GetColorCycleHz() * time + 2.0f, 1.0f);

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
                                  static_cast<float>(best_slot % 8u) * 47.0f +
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
    j["ring_width_pct"] = ring_width_pct_;
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
    if(settings.contains("ring_width_pct"))
    {
        ring_width_pct_ = std::clamp(settings["ring_width_pct"].get<int>(), 0, 100);
    }

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "ReactiveEffectSettings"))
        {
            EffectUiSync::setCheckBox(fx, "listenKeyboardCheck", listen_keyboard_);
            EffectUiSync::setCheckBox(fx, "listenMouseCheck", listen_mouse_);
            EffectUiSync::setCheckBox(fx, "listenGamepadCheck", listen_gamepad_);
            EffectUiSync::setComboIndex(fx, "triggerRow", trigger_mode_);
            EffectUiSync::setComboIndex(fx, "waveRow", wave_mode_);
            EffectUiSync::setComboIndex(fx, "spreadRow", spread_mode_);
            EffectUiSync::setComboIndex(fx, "lookRow", look_mode_);
            EffectUiSync::setComboIndex(fx, "shapeRow", footprint_shape_);
            EffectUiSync::setSliderValue(fx, "repeatRateRow", repeat_rate_deci_hz_, FormatRepeatRate);
            EffectUiSync::setSliderValue(fx, "travelRow", travel_pct_, FormatTravel);
            EffectUiSync::setSliderValue(fx, "ringWidthRow", ring_width_pct_, FormatRingWidth);
            if(EffectSliderRow* rate_row = EffectUiSync::sliderRow(fx, "repeatRateRow"))
            {
                rate_row->setEnabled(wave_mode_ != WAVE_ONCE);
            }
            if(EffectLabeledComboRow* orient_row = EffectUiSync::comboRow(fx, "orientRow"))
            {
                QComboBox* orient_combo = orient_row->combo();
                if(orient_combo)
                {
                    orient_combo->blockSignals(true);
                    orient_combo->clear();
                    const int spread = std::clamp(spread_mode_, 0, SPREAD_COUNT - 1);
                    if(spread == SPREAD_PLANE || spread == SPREAD_AXIS || spread == SPREAD_CROSS)
                    {
                        for(int o = 0; o < SPREAD_ORIENT_COUNT; ++o)
                        {
                            orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, o)));
                        }
                        orient_combo->setCurrentIndex(std::clamp(spread_orient_, 0, SPREAD_ORIENT_COUNT - 1));
                        orient_combo->setEnabled(true);
                    }
                    else
                    {
                        orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, spread_orient_)));
                        orient_combo->setCurrentIndex(0);
                        orient_combo->setEnabled(false);
                    }
                    orient_combo->blockSignals(false);
                }
            }
            if(EffectLabeledComboRow* shape_row = EffectUiSync::comboRow(fx, "shapeRow"))
            {
                shape_row->setEnabled(spread_mode_ == SPREAD_SHELL || spread_mode_ == SPREAD_PLANE);
            }
        }
    }
}
