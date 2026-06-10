// SPDX-License-Identifier: GPL-2.0-only

#include "RoomCampfireEffect3D.h"

#include "RoomSpatialLightingUi.h"
#include "RoomCampfireSettingsPanel.h"
#include "EffectHelpers.h"
#include "GridSpaceUtils.h"

#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(RoomCampfireEffect3D);

namespace
{

RGBColor LerpRgb(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const int ar = a & 0xFF;
    const int ag = (a >> 8) & 0xFF;
    const int ab = (a >> 16) & 0xFF;
    const int br = b & 0xFF;
    const int bg = (b >> 8) & 0xFF;
    const int bb = (b >> 16) & 0xFF;
    const int r = (int)(ar + (br - ar) * t);
    const int g = (int)(ag + (bg - ag) * t);
    const int bch = (int)(ab + (bb - ab) * t);
    return (bch << 16) | (g << 8) | r;
}

float Hash01(float x, float y, float z, float t)
{
    const float n = std::sin(x * 12.9898f + y * 78.233f + z * 37.719f + t * 19.17f) * 43758.5453f;
    return n - std::floor(n);
}

float Fract(float v)
{
    return v - std::floor(v);
}

// Smooth gradient-interpolated value noise in [0,1]. Coherent in space, so it
// produces flowing structure instead of per-LED twinkle.
float ValueNoise3(float x, float y, float z)
{
    const float xi = std::floor(x);
    const float yi = std::floor(y);
    const float zi = std::floor(z);
    const float xf = x - xi;
    const float yf = y - yi;
    const float zf = z - zi;

    auto fade = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const float u = fade(xf);
    const float v = fade(yf);
    const float w = fade(zf);

    auto h = [](float X, float Y, float Z) {
        const float n = std::sin(X * 127.1f + Y * 311.7f + Z * 74.7f) * 43758.5453f;
        return n - std::floor(n);
    };

    const float c000 = h(xi, yi, zi),         c100 = h(xi + 1, yi, zi);
    const float c010 = h(xi, yi + 1, zi),     c110 = h(xi + 1, yi + 1, zi);
    const float c001 = h(xi, yi, zi + 1),     c101 = h(xi + 1, yi, zi + 1);
    const float c011 = h(xi, yi + 1, zi + 1), c111 = h(xi + 1, yi + 1, zi + 1);

    const float x00 = c000 + (c100 - c000) * u;
    const float x10 = c010 + (c110 - c010) * u;
    const float x01 = c001 + (c101 - c001) * u;
    const float x11 = c011 + (c111 - c011) * u;
    const float y0 = x00 + (x10 - x00) * v;
    const float y1 = x01 + (x11 - x01) * v;
    return y0 + (y1 - y0) * w;
}

// Fractal sum -> turbulent detail. Returns roughly [0,1].
float Fbm3(float x, float y, float z)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for(int i = 0; i < 3; ++i)
    {
        sum += amp * ValueNoise3(x * freq, y * freq, z * freq);
        amp *= 0.5f;
        freq *= 2.03f;
    }
    return sum / 0.875f;
}

} // namespace

RoomCampfireEffect3D::RoomCampfireEffect3D(QWidget* parent) : RoomSpatialLightingEffect3D(parent)
{
    SetReferenceMode(REF_MODE_USER_POSITION);
    SetRainbowMode(false);

    room_light_.glow_radius_mm = 95.0f;
    room_light_.light_reach_mm = 680.0f;
    room_light_.room_fill = 0.0f;
    room_light_.ao_strength = 42.0f;
    campfire_.spill_fill = 22.0f;

    std::vector<RGBColor> fire_palette;
    fire_palette.push_back(ToRGBColor(255, 245, 200));  // white-hot core
    fire_palette.push_back(ToRGBColor(255, 120, 20));   // orange body
    fire_palette.push_back(ToRGBColor(120, 18, 6));    // char edge
    SetColors(fire_palette);
}

void RoomCampfireEffect3D::ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const
{
    RoomSpatialLightingEffect3D::ApplyLiveShadeSettings(scene);
    scene.shade.ambient_level = 0.03f;
    const float ember_halo =
        (campfire_.spill_fill / 100.0f) * 0.16f * (effect_brightness / 100.0f);
    scene.shade.room_fill_strength = ember_halo;
    scene.shade.direct_falloff = 0.82f;
}

float RoomCampfireEffect3D::RoomLightEmissiveMul() const
{
    return 2.6f;
}

float RoomCampfireEffect3D::RoomLightDirectMul() const
{
    return 1.55f;
}

EffectInfo3D RoomCampfireEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Room campfire";
    info.effect_description =
        "Flame tongues in room space (partial coverage) with localized light and sparks. "
        "Room fill is off; use Ember glow only for a faint halo. Speed = flicker.";
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
    info.user_colors = 3;
    info.supports_strip_colormap = false;
    info.supports_height_bands = false;
    info.show_room_output_control = false;
    info.default_speed_scale = 12.0f;
    return info;
}

void RoomCampfireEffect3D::SetupCustomUI(QWidget* parent)
{
    auto* panel = new RoomCampfireSettingsPanel();
    panel->setObjectName(QStringLiteral("RoomCampfireSettings"));
    const auto on_light_changed = [this]() {
        MarkRoomLightPlacementDirty();
        emit ParametersChanged();
    };
    const auto on_tune_changed = [this]() {
        InvalidateLightingScene();
        emit ParametersChanged();
    };
    panel->bind(this, room_light_, campfire_, on_light_changed, on_tune_changed);
    AddWidgetToParent(panel, parent);
}

void RoomCampfireEffect3D::SyncCampfireSettingsPanel()
{
    if(auto* panel = findChild<RoomCampfireSettingsPanel*>(QStringLiteral("RoomCampfireSettings")))
    {
        panel->syncFromState(this, room_light_, campfire_);
    }
}

void RoomCampfireEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

float RoomCampfireEffect3D::FlameHeightUnits(const GridContext3D& grid) const
{
    const float rise = campfire_.flame_rise / 100.0f;
    const float reach_u = MMToGridUnits(room_light_.light_reach_mm, grid.grid_scale_mm);
    const float room_v = std::max(grid.height, 0.5f);
    // Tall, clearly visible flame scaled to the room; Light reach can push taller.
    const float h = std::max(reach_u * (0.55f + rise * 1.10f), room_v * (0.42f + rise * 0.55f));
    return std::clamp(h, 0.05f, room_v * 1.25f);
}

float RoomCampfireEffect3D::FlameBaseRadiusUnits(const GridContext3D& grid) const
{
    const float glow_u = MMToGridUnits(room_light_.glow_radius_mm, grid.grid_scale_mm);
    const float room_h = std::max(0.5f * (grid.width + grid.depth), 0.5f);
    const float r = std::max(glow_u * 1.55f, room_h * 0.15f);
    return std::clamp(r, 0.05f, room_h * 0.42f);
}

float RoomCampfireEffect3D::ComputeFlameTongueMask(float x,
                                                   float y,
                                                   float z,
                                                   float time,
                                                   const SpatialLighting::Vec3& fire_pos,
                                                   const GridContext3D& grid) const
{
    // Y is the vertical (up) axis in this engine; X/Z form the ground plane.
    const float dx = x - fire_pos.x;
    const float up = y - fire_pos.y;
    const float dz = z - fire_pos.z;
    const float horiz = std::sqrt(dx * dx + dz * dz);

    const float speed_norm = GetNormalizedSpeed();
    const float turb = campfire_.flame_turbulence / 100.0f;

    const float flame_height = FlameHeightUnits(grid);
    const float base_radius = FlameBaseRadiusUnits(grid);

    // Cheap bounding reject (generous so turbulent edges aren't clipped).
    if(up < -base_radius * 0.9f || horiz > base_radius * 1.8f || up > flame_height * 1.45f)
    {
        return 0.0f;
    }

    // Normalized height: 0 at the logs, 1 at the nominal flame tip.
    const float v = up / std::max(flame_height, 0.05f);

    // Upward-flowing turbulence: sample a noise field whose vertical coordinate
    // scrolls down over time so the crests appear to rise like real flame.
    const float flow = time * (0.6f + speed_norm * 2.6f) * 1.6f;
    const float freq = (2.4f / std::max(base_radius, 0.1f)) * (0.85f + turb * 0.7f);
    const float fb = Fbm3(dx * freq, up * freq - flow, dz * freq);
    const float turbn = (fb - 0.5f) * 2.0f;                 // ~[-1,1]

    // A finer, faster-rising octave adds the "lick" detail in the body.
    const float detail = Fbm3(dx * freq * 2.1f + 11.0f,
                              up * freq * 2.1f - flow * 1.7f,
                              dz * freq * 2.1f + 5.0f);

    // Cone that narrows with height; turbulence distorts the edge so the
    // silhouette ripples instead of being a smooth ellipse.
    float cone_r = base_radius * (1.0f - 0.62f * std::clamp(v, 0.0f, 1.0f));
    cone_r = std::max(cone_r, base_radius * 0.18f);
    float r_norm = horiz / cone_r;
    r_norm += turbn * turb * 0.65f * (0.35f + std::clamp(v, 0.0f, 1.2f));

    const float radial = 1.0f - smoothstep(0.28f, 1.05f, r_norm);

    // Vertical profile: solid at the base, flickering tip whose height wobbles.
    const float tip = std::max(0.40f, 1.0f + turbn * turb * 0.45f);
    const float vert = (1.0f - smoothstep(0.0f, 1.0f, v / tip)) *
                       (1.0f - smoothstep(0.0f, 0.55f, -v));   // fade below the logs

    float flame = radial * vert;

    // Rounded ember pool hugging the logs.
    const float pool = std::exp(-(horiz * horiz) / std::max(base_radius * base_radius * 0.45f, 0.01f)) *
                       (1.0f - smoothstep(-0.2f, 0.5f, v));
    flame = std::max(flame, pool * 0.95f);

    // Modulate the body with smooth spatial detail (NOT random per-LED), so the
    // flame breaks into tongues without twinkling.
    flame *= (0.72f + 0.42f * detail);

    return std::clamp(flame, 0.0f, 1.0f);
}

RoomCampfireEffect3D::FireAppearance RoomCampfireEffect3D::SampleFireAppearance(
    float x,
    float y,
    float z,
    float time,
    const SpatialLighting::Vec3& fire_pos,
    const GridContext3D& grid) const
{
    FireAppearance out{};

    const std::vector<RGBColor> palette = GetColors();
    const RGBColor hot = palette.empty() ? ToRGBColor(255, 245, 200) : palette[0];
    const RGBColor mid = palette.size() > 1 ? palette[1] : hot;
    const RGBColor cool = palette.size() > 2 ? palette[2] : mid;

    const float dx = x - fire_pos.x;
    const float up = y - fire_pos.y;
    const float dz = z - fire_pos.z;
    const float horiz = std::sqrt(dx * dx + dz * dz);

    const float speed_norm = GetNormalizedSpeed();
    const float turb = campfire_.flame_turbulence / 100.0f;
    const float flame_height = FlameHeightUnits(grid);
    const float base_radius = FlameBaseRadiusUnits(grid);

    // Same advected field as the coverage mask so color tracks the moving flame.
    const float flow = time * (0.6f + speed_norm * 2.6f) * 1.6f;
    const float freq = (2.4f / std::max(base_radius, 0.1f)) * (0.85f + turb * 0.7f);
    const float detail = Fbm3(dx * freq * 2.1f + 11.0f,
                              up * freq * 2.1f - flow * 1.7f,
                              dz * freq * 2.1f + 5.0f);

    // Height the flame coordinate sees: turbulence lets color licks rise too.
    const float v = std::clamp(up / std::max(flame_height, 0.05f) +
                                   (detail - 0.5f) * turb * 0.30f,
                               -0.25f, 1.30f);
    float cone_r = base_radius * (1.0f - 0.62f * std::clamp(v, 0.0f, 1.0f));
    cone_r = std::max(cone_r, base_radius * 0.18f);
    const float r_norm = std::clamp(horiz / cone_r, 0.0f, 1.6f);

    // Real-fire vertical gradient: white-hot base -> orange body -> deep-red tip.
    RGBColor body;
    if(v < 0.32f)
    {
        body = LerpRgb(hot, mid, std::clamp(v / 0.32f, 0.0f, 1.0f));
    }
    else
    {
        body = LerpRgb(mid, cool, smoothstep(0.32f, 1.05f, v));
    }
    // Edges run cooler than the centre of the column.
    body = LerpRgb(body, cool, smoothstep(0.35f, 1.0f, r_norm) * 0.55f);
    // Concentrated white-hot heart right at the logs.
    const float heart = (1.0f - smoothstep(0.0f, 0.30f, v)) * (1.0f - smoothstep(0.0f, 0.55f, r_norm));
    body = LerpRgb(body, hot, heart * 0.7f);

    out.source_color = body;

    // Brightness varies smoothly through the flowing field (no per-LED twinkle),
    // hottest low and centre, dimming toward the cooler tips.
    float bright = 0.80f + 0.45f * detail;
    bright *= 1.0f - 0.35f * smoothstep(0.45f, 1.15f, v);   // tips burn lower
    bright *= 1.0f + 0.25f * heart;                          // glowing core
    // Very subtle slow breathing of the whole bed; intentionally gentle.
    bright *= 0.95f + 0.05f * std::sin(time * 1.6f);
    out.brightness = std::clamp(bright, 0.35f, 1.6f);

    // Embers: short-lived points that loft upward out of the bed.
    if(campfire_.spark_amount > 0.5f && up > -base_radius * 0.2f && horiz < base_radius * 1.2f)
    {
        const float spark_rate = 2.8f + speed_norm * 8.0f;
        const float seed = Hash01(std::floor(dx * 4.0f), std::floor(up * 4.0f), std::floor(dz * 4.0f), 0.0f);
        const float cell_t = Fract(time * spark_rate * (0.6f + seed * 0.8f) + seed * 17.0f);
        const float spark_gate = (campfire_.spark_amount / 100.0f) * (0.4f + speed_norm * 0.5f);
        const float window = spark_gate * 0.18f;
        if(cell_t < window)
        {
            const float spark_life = cell_t / std::max(window, 0.001f);
            const float loft = std::clamp(0.4f + v * 0.9f, 0.2f, 1.3f);   // brighter higher up
            out.spark_add = std::sin(spark_life * static_cast<float>(M_PI)) * 3.2f * loft * spark_gate;
        }
    }

    return out;
}

RGBColor RoomCampfireEffect3D::ApplySparkTint(RGBColor base, float spark_add) const
{
    if(spark_add <= 0.001f)
    {
        return base;
    }

    const RGBColor ember = ToRGBColor(255, 220, 140);
    const float t = std::clamp(spark_add, 0.0f, 1.0f);
    return LerpRgb(base, ember, t);
}

RGBColor RoomCampfireEffect3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    const SpatialLighting::Vec3 fire_pos = ResolvedRoomLightPosition(grid);
    const float flame_mask = ComputeFlameTongueMask(x, y, z, time, fire_pos, grid);

    constexpr RGBColor kChar = ToRGBColor(9, 4, 2);
    if(flame_mask < 0.015f)
    {
        return kChar;
    }

    const FireAppearance fire = SampleFireAppearance(x, y, z, time, fire_pos, grid);
    RGBColor base = ShadeRoomLightAt(x, y, z, grid, fire.source_color);

    // Keep contrast: the spaces between tongues stay dark, cores stay bright.
    const float presence = std::pow(flame_mask, 0.60f);
    const float gain = fire.brightness * (0.30f + 0.70f * presence) * 1.30f;

    const float br = std::clamp(static_cast<float>(base & 0xFF) * gain, 0.0f, 255.0f);
    const float bg = std::clamp(static_cast<float>((base >> 8) & 0xFF) * gain, 0.0f, 255.0f);
    const float bb = std::clamp(static_cast<float>((base >> 16) & 0xFF) * gain, 0.0f, 255.0f);
    base = ToRGBColor((int)br, (int)bg, (int)bb);

    // Soft char fade at the flame's outer edge so tongues melt into darkness.
    if(flame_mask < 0.18f)
    {
        const float edge_t = flame_mask / 0.18f;
        base = LerpRgb(kChar, base, edge_t);
    }

    return ApplySparkTint(base, fire.spark_add * presence);
}

nlohmann::json RoomCampfireEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    RoomSpatialLightingUi::SaveParamsToJson(j, "room_campfire", room_light_);
    j["room_campfire"]["flame_rise"] = campfire_.flame_rise;
    j["room_campfire"]["flame_turbulence"] = campfire_.flame_turbulence;
    j["room_campfire"]["spark_amount"] = campfire_.spark_amount;
    j["room_campfire"]["spill_fill"] = campfire_.spill_fill;
    return j;
}

void RoomCampfireEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    RoomSpatialLightingUi::LoadParamsFromJson(settings, "room_campfire", nullptr, room_light_);
    if(settings.contains("room_campfire") && settings["room_campfire"].is_object())
    {
        const auto& rc = settings["room_campfire"];
        if(rc.contains("flame_rise"))
        {
            campfire_.flame_rise = rc["flame_rise"].get<float>();
        }
        if(rc.contains("flame_turbulence"))
        {
            campfire_.flame_turbulence = rc["flame_turbulence"].get<float>();
        }
        if(rc.contains("spark_amount"))
        {
            campfire_.spark_amount = rc["spark_amount"].get<float>();
        }
        if(rc.contains("spill_fill"))
        {
            campfire_.spill_fill = rc["spill_fill"].get<float>();
        }
        else if(rc.contains("room_fill"))
        {
            campfire_.spill_fill = rc["room_fill"].get<float>();
        }
    }
    room_light_.room_fill = 0.0f;
    MarkRoomLightPlacementDirty();
    SyncCampfireSettingsPanel();
}
