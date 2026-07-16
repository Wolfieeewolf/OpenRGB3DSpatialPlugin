// SPDX-License-Identifier: GPL-2.0-only

#include "Bubbles.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(Bubbles);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Bubbles::Bubbles(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetFrequency(50);
}

EffectInfo3D Bubbles::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Bubbles";
    info.effect_description =
        "Rising expanding spheres (like OpenRGB Bubbles); optional floor/mid/ceiling band tuning for motion and shell detail";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
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

void Bubbles::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("BubblesEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    auto bind_int = [&](const char* name, const QString& caption, int min, int max, int value,
                        const QString& tip, auto apply, auto format) {
        EffectSliderRow* row = EffectUiRows::AppendSliderRow(layout, caption, min, max, value, tip);
        row->setObjectName(name);
        row->bindValueChanged(this, apply, format, on_changed);
    };

    bind_int("maxBubblesRow", QStringLiteral("Max bubbles:"), 4, 100, max_bubbles,
             QStringLiteral("How many bubble centers are simulated (internally capped for performance)."),
             [this](int v) { max_bubbles = v; }, [](int v) { return QString::number(v); });
    bind_int("ringThicknessRow", QStringLiteral("Ring thickness:"), 2, 100, (int)(bubble_thickness * 100.0f),
             QStringLiteral("Shell thickness of each bubble as a fraction of room scale."),
             [this](int v) { bubble_thickness = v / 100.0f; }, pct_format);
    bind_int("riseSpeedRow", QStringLiteral("Rise speed:"), 20, 800, (int)(rise_speed * 100.0f),
             QStringLiteral("How fast bubbles drift upward through the volume."),
             [this](int v) { rise_speed = v / 100.0f; },
             [this](int) { return QString::number(rise_speed, 'f', 2); });
    bind_int("spawnRateRow", QStringLiteral("Spawn rate:"), 8, 200, (int)(spawn_interval * 100.0f),
             QStringLiteral("Spacing between bubble phases (lower = busier, more motion)."),
             [this](int v) { spawn_interval = v / 100.0f; },
             [this](int) { return QString::number(spawn_interval, 'f', 2); });
    bind_int("bubbleSizeRow", QStringLiteral("Bubble size:"), 50, 350, (int)(max_radius * 100.0f),
             QStringLiteral("Maximum bubble shell radius (higher fills more room volume)."),
             [this](int v) { max_radius = v / 100.0f; },
             [this](int) { return QString::number(max_radius, 'f', 2); });
    bind_int("horizontalFillRow", QStringLiteral("Horizontal fill:"), 50, 180, (int)(horizontal_fill * 100.0f),
             QStringLiteral("How widely bubble centers spread across X/Z."),
             [this](int v) { horizontal_fill = v / 100.0f; }, pct_format);
    bind_int("minSpacingRow", QStringLiteral("Min spacing:"), 10, 100, (int)(overlap_spacing * 100.0f),
             QStringLiteral("Minimum separation between bubble centers to reduce stacking."),
             [this](int v) { overlap_spacing = v / 100.0f; }, pct_format);
    bind_int("launchRandomnessRow", QStringLiteral("Launch randomness:"), 0, 100,
             (int)std::lround(launch_randomness * 100.0f),
             QStringLiteral("0 = even cadence, 100 = natural staggered launch intervals."),
             [this](int v) { launch_randomness = v / 100.0f; }, pct_format);

    AddWidgetToParent(w, parent);
}

RGBColor Bubbles::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    const bool strat_on = (GetStratumLayoutMode() == 1);

    EffectGridAxisHalfExtents e_room = MakeEffectGridAxisHalfExtents(grid, 1.0f);
    float h_scale = std::max({e_room.hw, e_room.hh, e_room.hd});
    float speed_scale = GetScaledSpeed() * 0.015f;
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float color_cycle = time * GetScaledFrequency() * 12.0f;
    if(strat_on)
    {
        color_cycle = color_cycle * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
    }
    int n_bub = std::max(8, std::min(80, max_bubbles));
    float thick = std::max(0.02f, std::min(4.0f, bubble_thickness * h_scale)) / std::max(0.35f, detail);
    if(strat_on)
    {
        thick /= std::max(0.25f, bb.tight_mul);
    }
    float rise = std::max(0.2f, std::min(8.0f, rise_speed)) * speed_scale * e_room.hh;
    float interval = std::max(0.08f, std::min(2.0f, spawn_interval));
    float max_r = std::max(0.5f, std::min(3.5f, max_radius)) * h_scale * size_m * 0.50f;

    const float bias_limit_x = e_room.hw * 0.5f;
    const float bias_limit_z = e_room.hd * 0.5f;
    const float bias_x = std::clamp(origin.x - grid.center_x, -bias_limit_x, bias_limit_x);
    const float bias_z = std::clamp(origin.z - grid.center_z, -bias_limit_z, bias_limit_z);
    const float fill = std::clamp(horizontal_fill, 0.5f, 1.8f);
    const float lateral_spread_x = e_room.hw * 0.5f * fill;
    const float lateral_spread_z = e_room.hd * 0.5f * fill;
    const float min_sep_norm = std::clamp(overlap_spacing, 0.10f, 1.0f);
    const float launch_jitter = std::clamp(launch_randomness, 0.0f, 1.0f);
    constexpr float kGoldenAngle = 2.39996323f;
    auto hash01 = [](unsigned int seed, unsigned int salt) -> float
    {
        unsigned int v = seed * 73856093u ^ salt * 19349663u;
        v = (v << 13u) ^ v;
        v = v * (v * v * 15731u + 789221u) + 1376312589u;
        return (v & 0xFFFFu) / 65535.0f;
    };

    if(!strat_on)
    {
        if(bubble_centers_cached.size() != (size_t)n_bub || fabsf(time - bubble_cache_time) > 0.001f)
        {
            bubble_cache_time = time;
            bubble_centers_cached.resize(n_bub);
            for(int i = 0; i < n_bub; i++)
            {
                const unsigned int seed = (unsigned int)(i * 2654435761u + 1013904223u);
                const float cycle_mul_min = 1.0f + 0.35f * launch_jitter;
                const float cycle_mul_span = 2.0f * launch_jitter;
                const float cycle_mul = cycle_mul_min + cycle_mul_span * hash01(seed, 11u);
                const float active_base = 0.52f - 0.26f * launch_jitter;
                const float active_span = 0.06f + 0.22f * launch_jitter;
                const float active_frac = active_base + active_span * hash01(seed, 12u);
                const float cycle_i = std::max(0.12f, interval * cycle_mul);
                const float active_window = std::max(0.04f, cycle_i * active_frac);
                const float offset_i = cycle_i * hash01(seed, 13u);
                const float phase_i = fmodf(time * rise * 0.9f + offset_i, cycle_i);
                if(phase_i > active_window)
                {
                    bubble_centers_cached[i] = {0.0f, 0.0f, 0.0f, -1.0f};
                    continue;
                }
                const float radius_phase = phase_i / active_window;
                float radius = (0.18f + 0.82f * radius_phase) * max_r * 0.55f;
                float shell_sep = std::max(1e-3f, radius + thick);
                float min_sep = shell_sep * min_sep_norm;
                float ring = sqrtf(((float)i + 0.5f) / (float)n_bub);
                float cx = grid.center_x + bias_x + cosf((float)i * kGoldenAngle) * ring * lateral_spread_x;
                float cy = grid.min_y + radius_phase * std::max(1e-3f, grid.height);
                float cz = grid.center_z + bias_z + sinf((float)i * kGoldenAngle) * ring * lateral_spread_z;
                cx = std::clamp(cx, grid.min_x, grid.max_x);
                cz = std::clamp(cz, grid.min_z, grid.max_z);
                for(int j = 0; j < i; ++j)
                {
                    const BubbleCenter3D& prev = bubble_centers_cached[(size_t)j];
                    if(prev.radius <= 0.0f)
                    {
                        continue;
                    }
                    float dx2 = cx - prev.cx;
                    float dz2 = cz - prev.cz;
                    float d2 = dx2 * dx2 + dz2 * dz2;
                    if(d2 < min_sep * min_sep && d2 > 1e-6f)
                    {
                        float d = sqrtf(d2);
                        float push = (min_sep - d) / d;
                        cx += dx2 * push * 0.5f;
                        cz += dz2 * push * 0.5f;
                        cx = std::clamp(cx, grid.min_x, grid.max_x);
                        cz = std::clamp(cz, grid.min_z, grid.max_z);
                    }
                }
                bubble_centers_cached[i] = {cx, cy, cz, radius};
            }
        }
    }

    float max_intensity = 0.0f;
    float best_hue = 0.0f;

    for(int i = 0; i < n_bub; i++)
    {
        BubbleCenter3D b;
        if(strat_on)
        {
            const unsigned int seed = (unsigned int)(i * 2654435761u + 1013904223u);
            const float cycle_mul_min = 1.0f + 0.35f * launch_jitter;
            const float cycle_mul_span = 2.0f * launch_jitter;
            const float cycle_mul = cycle_mul_min + cycle_mul_span * hash01(seed, 11u);
            const float active_base = 0.52f - 0.26f * launch_jitter;
            const float active_span = 0.06f + 0.22f * launch_jitter;
            const float active_frac = active_base + active_span * hash01(seed, 12u);
            const float cycle_i = std::max(0.12f, interval * cycle_mul);
            const float active_window = std::max(0.04f, cycle_i * active_frac);
            const float offset_i = cycle_i * hash01(seed, 13u);
            const float phase_i = fmodf(time * rise * 0.9f * bb.speed_mul + offset_i, cycle_i);
            if(phase_i > active_window)
            {
                continue;
            }
            const float radius_phase = phase_i / active_window;
            b.radius = (0.18f + 0.82f * radius_phase) * max_r * 0.55f;
            float ring = sqrtf(((float)i + 0.5f) / (float)n_bub);
            b.cx = std::clamp(grid.center_x + bias_x + cosf((float)i * kGoldenAngle) * ring * lateral_spread_x,
                              grid.min_x,
                              grid.max_x);
            b.cy = grid.min_y + radius_phase * std::max(1e-3f, grid.height);
            b.cz = std::clamp(grid.center_z + bias_z + sinf((float)i * kGoldenAngle) * ring * lateral_spread_z,
                              grid.min_z,
                              grid.max_z);
        }
        else
        {
            b = bubble_centers_cached[i];
            if(b.radius <= 0.0f)
            {
                continue;
            }
        }
        float dx = x - b.cx;
        float dy = y - b.cy;
        float dz = z - b.cz;
        float dist_sq = dx*dx + dy*dy + dz*dz;
        float far = b.radius + thick * 4.0f;
        if(dist_sq > far * far) continue;
        float dist = sqrtf(dist_sq);
        float shallow = fabsf(dist - b.radius) / thick;
        float value = (shallow < 0.01f) ? 1.0f : 1.0f / (1.0f + shallow * shallow);
        value = fmaxf(0.0f, fminf(1.0f, value));

        if(value > max_intensity)
        {
            max_intensity = value;
            best_hue = fmodf((float)i * 40.0f + color_cycle, 360.0f);
            if(best_hue < 0.0f) best_hue += 360.0f;
        }
    }

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    RGBColor final_color;
    if(UseEffectStripColormap())
    {
        const float ph01 = std::fmod(color_cycle * (1.f / 360.f) + best_hue * (1.f / 360.f) + 1.f, 1.f);
        float pal01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
        pal01 = ApplySpatialPalette01(pal01, basis, sp, map, time, &grid);
        final_color = ResolveStripKernelFinalColor(*this,
                                                   GetEffectStripColormapKernel(),
                                                   std::clamp(pal01, 0.0f, 1.0f),
                                                   GetEffectStripColormapColorStyle(),
                                                   time,
                                                   GetScaledFrequency() * 12.0f * (strat_on ? bb.speed_mul : 1.0f));
    }
    else
    {
        if(GetRainbowMode())
        {
            float hue = ApplySpatialRainbowHue(best_hue,
                                               std::fmod(best_hue * (1.0f / 360.0f) + 1.0f, 1.0f),
                                               basis,
                                               sp,
                                               map,
                                               time,
                                               &grid);
            float p01 = std::fmod(hue / 360.0f, 1.0f);
            if(p01 < 0.0f) p01 += 1.0f;
            final_color = GetRainbowColor(p01 * 360.0f);
        }
        else
        {
            float p = ApplySpatialPalette01(0.5f, basis, sp, map, time, &grid);
            final_color = GetColorAtPosition(p);
        }
    }
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Bubbles::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["max_bubbles"] = max_bubbles;
    j["bubble_thickness"] = bubble_thickness;
    j["rise_speed"] = rise_speed;
    j["spawn_interval"] = spawn_interval;
    j["max_radius"] = max_radius;
    j["horizontal_fill"] = horizontal_fill;
    j["overlap_spacing"] = overlap_spacing;
    j["launch_randomness"] = launch_randomness;
return j;
}

void Bubbles::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("max_bubbles") && settings["max_bubbles"].is_number_integer())
        max_bubbles = std::max(4, std::min(80, settings["max_bubbles"].get<int>()));
    if(settings.contains("bubble_thickness") && settings["bubble_thickness"].is_number())
        bubble_thickness = std::max(0.02f, std::min(1.0f, settings["bubble_thickness"].get<float>()));
    if(settings.contains("rise_speed") && settings["rise_speed"].is_number())
        rise_speed = std::max(0.2f, std::min(8.0f, settings["rise_speed"].get<float>()));
    if(settings.contains("spawn_interval") && settings["spawn_interval"].is_number())
        spawn_interval = std::max(0.08f, std::min(2.0f, settings["spawn_interval"].get<float>()));
    if(settings.contains("max_radius") && settings["max_radius"].is_number())
        max_radius = std::max(0.5f, std::min(3.5f, settings["max_radius"].get<float>()));
    if(settings.contains("horizontal_fill") && settings["horizontal_fill"].is_number())
        horizontal_fill = std::max(0.5f, std::min(1.8f, settings["horizontal_fill"].get<float>()));
    if(settings.contains("overlap_spacing") && settings["overlap_spacing"].is_number())
        overlap_spacing = std::max(0.10f, std::min(1.0f, settings["overlap_spacing"].get<float>()));
    if(settings.contains("launch_randomness") && settings["launch_randomness"].is_number())
        launch_randomness = std::max(0.0f, std::min(1.0f, settings["launch_randomness"].get<float>()));

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "BubblesEffectSettings"))
        {
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "maxBubblesRow", max_bubbles, [](int v) { return QString::number(v); });
            EffectUiSync::setSliderValue(fx, "ringThicknessRow", (int)(bubble_thickness * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "riseSpeedRow", (int)(rise_speed * 100.0f),
                                          [this](int) { return QString::number(rise_speed, 'f', 2); });
            EffectUiSync::setSliderValue(fx, "spawnRateRow", (int)(spawn_interval * 100.0f),
                                          [this](int) { return QString::number(spawn_interval, 'f', 2); });
            EffectUiSync::setSliderValue(fx, "bubbleSizeRow", (int)(max_radius * 100.0f),
                                          [this](int) { return QString::number(max_radius, 'f', 2); });
            EffectUiSync::setSliderValue(fx, "horizontalFillRow", (int)(horizontal_fill * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "minSpacingRow", (int)(overlap_spacing * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "launchRandomnessRow", (int)std::lround(launch_randomness * 100.0f), pct);
        }
    }
}
