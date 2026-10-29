// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialEffect3D.h"

#include "SpatialRoom/SpatialRoomDefaults.h"
#include "SpatialRoom/SpatialRoomFrame.h"
#include "Colors.h"
#include "EffectStratumBlend.h"
#include "SpatialKernelColormap.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "AudioReactiveCommon.h"
#include "AudioInputManager.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    SpatialLayerCore::SamplePoint MakeCompassPaletteSamplePoint(const GridContext3D& grid,
                                                                  const SpatialLayerCore::MapperSettings& map,
                                                                  const SpatialLayerCore::SamplePoint& sp)
    {
        SpatialLayerCore::SamplePoint o = sp;
        const float yn = std::clamp(sp.y_norm, 0.0f, 1.0f);
        const float b0 = std::clamp(map.floor_end, 0.08f, 0.55f);
        const float b1 = std::clamp(std::max(b0 + 0.08f, map.upper_end), b0 + 0.08f, 0.92f);
        float y_norm_center;
        if(yn < b0)
        {
            y_norm_center = b0 * 0.5f;
        }
        else if(yn < b1)
        {
            y_norm_center = (b0 + b1) * 0.5f;
        }
        else
        {
            y_norm_center = (b1 + 1.0f) * 0.5f;
        }

        const float room_h = std::max(1e-6f, grid.max_y - grid.min_y);
        o.origin_y = grid.min_y + y_norm_center * room_h;
        return o;
    }
}

bool SpatialEffect3D::EffectGridSampleOutsideVolume(float x, float y, float z, const GridContext3D& grid) const
{
    /* Always world → volume-local (ApplyEffectRotation stays a no-op to avoid doubles). */
    const Vector3D origin_grid = GetEffectOriginGrid(grid);
    const Vector3D local = VolumeWorldDeltaToLocal(x - origin_grid.x,
                                                   y - origin_grid.y,
                                                   z - origin_grid.z);
    return !IsWithinEffectBoundary(local.x, local.y, local.z, grid);
}

void SpatialEffect3D::ApplyAxisScale(float& x, float& y, float& z, const GridContext3D& grid) const
{
    /* Axis % is occupancy (GetVolumeHalfExtents / GPU UV), not a sample stretch.
       A prior rotate±euler shell here was an identity no-op after stretch removal. */
    (void)x;
    (void)y;
    (void)z;
    (void)grid;
}

void SpatialEffect3D::ApplyEffectRotation(float& x, float& y, float& z, const GridContext3D& grid) const
{
    /* No-op: VolumeWorldDeltaToLocal / SampleVolumeLocalUv01 / EffectGridSampleOutsideVolume
       own world→local. Warping here double-transformed FolderVolume/ShaderField. */
    (void)x;
    (void)y;
    (void)z;
    (void)grid;
}

float SpatialEffect3D::ComputeStratumMotion01(const float layer_weights[3],
                                              const GridContext3D& grid,
                                              float x,
                                              float y,
                                              float z,
                                              const Vector3D& origin,
                                              float time_sec) const
{
    const float nx01 = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
    const float ny01 = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    const float nz01 = NormalizeGridAxis01(z, grid.min_z, grid.max_z);
    return EffectStratumBlend::StratumMotionPhase01(GetStratumLayoutMode(),
                                                    layer_weights,
                                                    GetStratumTuning(),
                                                    nx01,
                                                    ny01,
                                                    nz01,
                                                    origin.x,
                                                    origin.z,
                                                    x,
                                                    z,
                                                    time_sec);
}

RGBColor SpatialEffect3D::GetRainbowColor(float hue) const
{
    hue = std::fmod(hue, 360.0f);
    if(hue < 0) hue += 360.0f;

    float c = 1.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));

    float r, g, b;
    if(hue < 60) { r = c; g = x; b = 0; }
    else if(hue < 120) { r = x; g = c; b = 0; }
    else if(hue < 180) { r = 0; g = c; b = x; }
    else if(hue < 240) { r = 0; g = x; b = c; }
    else if(hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    return ((int)(b * 255) << 16) | ((int)(g * 255) << 8) | (int)(r * 255);
}

RGBColor SpatialEffect3D::GetColorAtPosition(float position) const
{
    position = std::clamp(position, 0.0f, 1.0f);

    if(rainbow_mode)
    {
        return GetRainbowColor(position * 360.0f);
    }

    if(colors.empty())
    {
        return COLOR_WHITE;
    }

    if(colors.size() == 1)
    {
        return colors[0];
    }

    float scaled_pos = position * (colors.size() - 1);
    int index = (int)scaled_pos;
    float frac = scaled_pos - index;

    if(index >= (int)colors.size() - 1)
    {
        return colors.back();
    }

    RGBColor color1 = colors[index];
    RGBColor color2 = colors[index + 1];

    int b1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int r1 = color1 & 0xFF;

    int b2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int r2 = color2 & 0xFF;

    int r = (int)(r1 + (r2 - r1) * frac);
    int g = (int)(g1 + (g2 - g1) * frac);
    int b = (int)(b1 + (b2 - b1) * frac);

    return (b << 16) | (g << 8) | r;
}

RGBColor SpatialEffect3D::GetPerBeatPulseColor(uint32_t color_slot) const
{
    if(GetRainbowMode())
    {
        /* Fixed hue step — do not divide 360 by the user stop count (default 2
           stops previously forced red + aqua only). */
        constexpr float kRainbowHueStepDeg = 30.0f;
        const float hue = std::fmod((float)color_slot * kRainbowHueStepDeg + 360.0f, 360.0f);
        return GetRainbowColor(hue);
    }

    if(colors.empty())
    {
        return COLOR_WHITE;
    }
    if(colors.size() == 1)
    {
        return colors[0];
    }
    return colors[color_slot % colors.size()];
}

RGBColor SpatialEffect3D::ResolveAudioReactiveColor(const AudioReactiveSettings3D& cfg,
                                                    const AudioReactiveColorParams& p) const
{
    const float gradient_pos = std::clamp(p.gradient_pos01, 0.0f, 1.0f);
    auto mode = static_cast<AudioPulseColorMode>(cfg.pulse_color_mode);

    if(GetRainbowMode()
       && mode != AudioPulseColorMode::PerBeatCycle
       && mode != AudioPulseColorMode::Uniform)
    {
        mode = AudioPulseColorMode::SpatialAlongRing;
    }

    if(mode == AudioPulseColorMode::FollowNotes)
    {
        float hue01 = 0.0f;
        if(AudioInputManager* audio = AudioInputManager::instance())
            hue01 = audio->getDominantNoteHue01();
        return GetRainbowColor(std::clamp(hue01, 0.0f, 1.0f) * 360.0f);
    }

    if(mode != AudioPulseColorMode::PerBeatCycle
       && mode != AudioPulseColorMode::Uniform
       && mode != AudioPulseColorMode::SpatialAlongRing)
    {
        mode = AudioPulseColorMode::SpatialAlongRing;
    }

    uint32_t slot = p.beat_color_slot;
    if(mode == AudioPulseColorMode::Uniform)
    {
        slot = 0;
    }

    if(mode == AudioPulseColorMode::PerBeatCycle || mode == AudioPulseColorMode::Uniform)
    {
        if(UseEffectStripColormap() && p.grid)
        {
            const size_t n = std::max(GetColors().size(), size_t(1));
            const float ph01 = std::fmod((float)slot / (float)n, 1.0f);
            float p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                   GetEffectStripColormapUnfold(),
                                                   GetEffectStripColormapDirectionDeg(),
                                                   ph01,
                                                   p.time,
                                                   *p.grid,
                                                   GetNormalizedSize(),
                                                   p.origin,
                                                   p.rotated_pos);
            return ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), p01, p.time);
        }
        return GetPerBeatPulseColor(slot);
    }

    if(!p.grid || !p.band_scalars)
    {
        if(GetRainbowMode() || colors.size() <= 2)
            return GetRainbowColor(gradient_pos * 360.0f);
        return GetPerBeatPulseColor(slot);
    }

    const EffectStratumBlend::BandBlendScalars& bb = *p.band_scalars;
    float detail = std::max(0.05f, GetScaledDetail());
    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), detail);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = p.grid_x;
    sp.grid_y = p.grid_y;
    sp.grid_z = p.grid_z;
    sp.origin_x = p.origin.x;
    sp.origin_y = p.origin.y;
    sp.origin_z = p.origin.z;
    sp.y_norm = p.y_norm01;

    RGBColor spatial_color;
    if(UseEffectStripColormap())
    {
        float ph01 = std::fmod(gradient_pos + CalculateProgress(p.time) * 0.25f
                                   + p.time * GetColorCycleHz() * bb.speed_mul + 1.0f,
                               1.0f);
        float p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               ph01,
                                               p.time,
                                               *p.grid,
                                               GetNormalizedSize(),
                                               p.origin,
                                               p.rotated_pos);
        spatial_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), std::min(p01, 1.0f), p.time);
    }
    else if(GetRainbowMode())
    {
        float hue = gradient_pos * 360.0f + CalculateProgress(p.time) * 40.0f * bb.speed_mul
                    + p.time * GetColorCycleHz() * 360.0f * bb.speed_mul
                    + EffectStratumBlend::CombinedPhase01(bb, p.stratum_mot01) * 50.0f;
        hue = ApplySpatialRainbowHue(hue, gradient_pos, basis, sp, map, p.time, p.grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        spatial_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float pal = ApplySpatialPalette01(gradient_pos, basis, sp, map, p.time, p.grid);
        /* Default effect colors are red+blue — RGB lerp midtones as purple.
           With ≤2 stops, use HSV spectrum so audio effects have real color range. */
        if(colors.size() <= 2)
        {
            spatial_color = GetRainbowColor(std::clamp(pal, 0.0f, 1.0f) * 360.0f);
        }
        else
        {
            spatial_color = GetColorAtPosition(std::min(pal, 1.0f));
        }
    }

    return spatial_color;
}

bool SpatialEffect3D::SkipsSpatialSampleWarp() const
{
    return SpatialRoom::DefaultCapabilitiesForMode(GetSpatialRoomMode())
        .has(SpatialRoom::CapSkipSampleWarp);
}

Vector3D SpatialEffect3D::GetEffectOrigin() const
{
    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_WORLD_ORIGIN:
        case REF_MODE_LED_CENTROID:
        case REF_MODE_TARGET_ZONE_CENTER:
        case REF_MODE_ROOM_CENTER:
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

Vector3D SpatialEffect3D::GetReferencePointGrid(const GridContext3D& grid) const
{
    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_TARGET_ZONE_CENTER:
            if(grid.has_anchor_override)
            {
                return {grid.anchor_override_x, grid.anchor_override_y, grid.anchor_override_z};
            }
            return {grid.center_x, grid.center_y, grid.center_z};
        case REF_MODE_WORLD_ORIGIN:
            return {0.0f, 0.0f, 0.0f};
        case REF_MODE_LED_CENTROID:
            /* Baked controller-centre hub on the grid (RefreshMappedLightsAnchor). */
            if(grid.has_led_centroid)
            {
                return {grid.led_centroid_x, grid.led_centroid_y, grid.led_centroid_z};
            }
            return {grid.center_x, grid.center_y, grid.center_z};
        case REF_MODE_ROOM_CENTER:
        default:
            return {grid.center_x, grid.center_y, grid.center_z};
    }
}

Vector3D SpatialEffect3D::GetEffectOriginGrid(const GridContext3D& grid) const
{
    /* Gizmo writes an absolute origin while unlocked so the stack Spatial Anchor
       cannot yank the lit region off the wireframe every render tick. */
    if(HasVolumeWorldOrigin())
    {
        return effect_volume_world_origin_;
    }
    Vector3D base = GetReferencePointGrid(grid);
    float half_w = grid.width * 0.5f;
    float half_h = grid.height * 0.5f;
    float half_d = grid.depth * 0.5f;
    base.x += (effect_offset_x / 100.0f) * half_w;
    base.y += (effect_offset_y / 100.0f) * half_h;
    base.z += (effect_offset_z / 100.0f) * half_d;
    return base;
}

float SpatialEffect3D::GetNormalizedSpeed() const
{
    float normalized = std::clamp(effect_speed / 200.0f, 0.0f, 1.0f);
    return std::pow(normalized, 1.35f);
}

float SpatialEffect3D::GetNormalizedFrequency() const
{
    float normalized = std::clamp(effect_frequency / 200.0f, 0.0f, 1.0f);
    return std::pow(normalized, 1.35f);
}

float SpatialEffect3D::GetNormalizedDetail() const
{
    float normalized = std::clamp(effect_detail / 200.0f, 0.0f, 1.0f);
    return std::pow(normalized, 1.35f);
}

float SpatialEffect3D::GetNormalizedSize() const
{
    /* Feature size inside the effect (ring width, blob radius, shader zoom). Not occupancy.
     * Slider 100 = 1.0, 200 = 2.0. */
    return std::max(0.05f, effect_size / 100.0f);
}

float SpatialEffect3D::GetNormalizedScale() const
{
    /* Occupancy in the room/layout. 100 = fill the grid from the Spatial Anchor. */
    float normalized = effect_scale / 100.0f;

    if(scale_inverted)
    {
        if(normalized <= 1.0f)
        {
            normalized = 1.0f - normalized;
        }
        else
        {
            float extra = normalized - 1.0f;
            normalized = std::max(0.0f, 1.0f - extra);
        }
    }

    return std::max(0.0f, normalized);
}

unsigned int SpatialEffect3D::CombineMediaSampling(unsigned int local_detail_percent) const
{
    const unsigned int g = std::min(100u, effect_sampling_resolution);
    const unsigned int l = std::min(100u, local_detail_percent);
    return (unsigned int)std::clamp((int)((l * g + 50u) / 100u), 0, 100);
}

namespace
{
void FillCompassSpinFromPreset(int preset, int out_spin[3])
{
    static const int kSpins[4][3] = {
        {1, 1, 1},
        {-1, -1, -1},
        {1, -1, 1},
        {-1, 1, -1},
    };
    const int p = std::clamp(preset, 0, 3);
    for(int i = 0; i < 3; i++)
    {
        out_spin[i] = kSpins[p][i];
    }
}

} // namespace

void ApplySpatialSamplingQuantization(float& x, float& y, float& z, const GridContext3D& grid, unsigned int resolution_pct)
{
    if(resolution_pct >= 100u)
    {
        return;
    }
    const float sx = std::max(1e-6f, grid.max_x - grid.min_x);
    const float sy = std::max(1e-6f, grid.max_y - grid.min_y);
    const float sz = std::max(1e-6f, grid.max_z - grid.min_z);
    float nx = std::clamp((x - grid.min_x) / sx, 0.0f, 1.0f);
    float ny = std::clamp((y - grid.min_y) / sy, 0.0f, 1.0f);
    float nz = std::clamp((z - grid.min_z) / sz, 0.0f, 1.0f);
    constexpr int kVirtualCells = 128;
    Geometry3D::QuantizeNormalizedAxis01(nx, resolution_pct, kVirtualCells);
    Geometry3D::QuantizeNormalizedAxis01(ny, resolution_pct, kVirtualCells);
    Geometry3D::QuantizeNormalizedAxis01(nz, resolution_pct, kVirtualCells);
    x = grid.min_x + nx * sx;
    y = grid.min_y + ny * sy;
    z = grid.min_z + nz * sz;
}

void SpatialEffect3D::PrepareTrueGpuSamplePoint(float& x, float& y, float& z, const GridContext3D& grid) const
{
    if(!SkipsSpatialSampleWarp())
    {
        ApplyEffectRotation(x, y, z, grid);
        ApplyAxisScale(x, y, z, grid);
    }
    if(UsesSpatialSamplingQuantization())
    {
        ApplySpatialSamplingQuantization(x, y, z, grid, GetSamplingResolution());
    }
}

static thread_local const SpatialEffect3D* g_tls_eval_effect = nullptr;

RGBColor SpatialEffect3D::EvaluateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    const SpatialEffect3D* prev_eval_effect = g_tls_eval_effect;
    g_tls_eval_effect = this;

    if(effect_room_output_role_ == SpatialRoom::SpatialRoomOutputRole::EmitterRelay)
    {
        if(SpatialRoom::IsRoomGridOverlayPass())
        {
            RGBColor relay = ShadeRelayReceiversAt(x, y, z, grid);
            g_tls_eval_effect = prev_eval_effect;
            return relay;
        }
        const int shading_ctrl = SpatialLightingSceneProvider::instance()->shadingControllerIndex();
        if(shading_ctrl >= 0 && isRoomReceiverController(shading_ctrl) && !isRoomEmitterController(shading_ctrl))
        {
            RGBColor relay = ShadeRelayReceiversAt(x, y, z, grid);
            g_tls_eval_effect = prev_eval_effect;
            return relay;
        }
        if(shading_ctrl >= 0 && isRoomEmitterController(shading_ctrl))
        {
            RGBColor pattern = CalculateColorGrid(x, y, z, time, grid);
            g_tls_eval_effect = prev_eval_effect;
            return pattern;
        }
        g_tls_eval_effect = prev_eval_effect;
        return 0x00000000;
    }

    if(UsesSpatialSamplingQuantization())
    {
        ApplySpatialSamplingQuantization(x, y, z, grid, GetSamplingResolution());
    }
    RGBColor base = CalculateColorGrid(x, y, z, time, grid);
    g_tls_eval_effect = prev_eval_effect;
    return base;
}

const SpatialEffect3D* SpatialEffect3D::GetEvaluatingEffect()
{
    return g_tls_eval_effect;
}

float SpatialEffect3D::ApplySpatialPalette01(float base_pos01,
                                            const SpatialLayerCore::Basis& basis,
                                            const SpatialLayerCore::SamplePoint& sp,
                                            const SpatialLayerCore::MapperSettings& map,
                                            float time,
                                            const GridContext3D* grid) const
{
    const float infl = std::clamp((float)effect_sampler_influence_centi / 100.0f, 0.0f, 2.5f);
    SpatialLayerCore::MapperSettings m = map;
    m.compass_azimuth_offset_rad = (float)effect_sampler_compass_north_offset_deg * 0.01745329251f;
    m.discrete_compass_zones = effect_compass_discrete_zones;

    switch(spatial_mapping_mode)
    {
    case SpatialMappingMode::Off:
        return base_pos01;
    case SpatialMappingMode::SubtleTint:
    {
        const float h = SpatialLayerCore::CompassStratumHueOffsetDegrees(basis, sp, m, 3) * infl;
        const float drift_deg = time * GetColorCycleHz() * 360.0f * infl;
        const float h_use = h + drift_deg;
        return SpatialLayerCore::ShiftGradient01WithCompassHue(base_pos01, h_use);
    }
    case SpatialMappingMode::CompassPalette:
    {
        SpatialLayerCore::SamplePoint sp_map = sp;
        if(grid)
        {
            sp_map = MakeCompassPaletteSamplePoint(*grid, m, sp);
        }
        SpatialLayerCore::CompassStratumSample css{};
        if(!SpatialLayerCore::MapPositionToCompassStrata(basis, sp_map, m, 3, css) || !css.valid)
        {
            return base_pos01;
        }
        int spins[3];
        FillCompassSpinFromPreset(compass_layer_spin_preset, spins);
        const float scroll =
            time * GetColorCycleHz() * infl;
        const float plane_mix = 0.58f * std::clamp(GetScaledDetail(), 0.05f, 1.f) * infl;
        return SpatialLayerCore::CompassStratumPalettePosition01(css, spins, scroll, plane_mix, base_pos01);
    }
    }
    return base_pos01;
}

float SpatialEffect3D::ApplySpatialRainbowHue(float hue_deg,
                                               float plane_pos01,
                                               const SpatialLayerCore::Basis& basis,
                                               const SpatialLayerCore::SamplePoint& sp,
                                               const SpatialLayerCore::MapperSettings& map,
                                               float time,
                                               const GridContext3D* grid) const
{
    const float infl = std::clamp((float)effect_sampler_influence_centi / 100.0f, 0.0f, 2.5f);
    SpatialLayerCore::MapperSettings m = map;
    m.compass_azimuth_offset_rad = (float)effect_sampler_compass_north_offset_deg * 0.01745329251f;
    m.discrete_compass_zones = effect_compass_discrete_zones;

    switch(spatial_mapping_mode)
    {
    case SpatialMappingMode::Off:
        return hue_deg;
    case SpatialMappingMode::SubtleTint:
    {
        const float h = SpatialLayerCore::CompassStratumHueOffsetDegrees(basis, sp, m, 3) * infl;
        const float drift_deg = time * GetColorCycleHz() * 360.0f * infl;
        return hue_deg + h + drift_deg;
    }
    case SpatialMappingMode::CompassPalette:
    {
        SpatialLayerCore::SamplePoint sp_map = sp;
        if(grid)
        {
            sp_map = MakeCompassPaletteSamplePoint(*grid, m, sp);
        }
        SpatialLayerCore::CompassStratumSample css{};
        if(!SpatialLayerCore::MapPositionToCompassStrata(basis, sp_map, m, 3, css) || !css.valid)
        {
            return hue_deg;
        }
        int spins[3];
        FillCompassSpinFromPreset(compass_layer_spin_preset, spins);
        const float scroll =
            time * GetColorCycleHz() * infl;
        const float plane_mix = 0.52f * std::clamp(GetScaledDetail(), 0.05f, 1.f) * infl;
        const float pos01 =
            SpatialLayerCore::CompassStratumPalettePosition01(css, spins, scroll, plane_mix, plane_pos01);
        return pos01 * 360.0f + hue_deg * 0.22f;
    }
    }
    return hue_deg;
}

float SpatialEffect3D::GetMotionHz() const
{
    if(effect_speed == 0)
    {
        return 0.0f;
    }
    return std::max(0.02f, GetNormalizedSpeed()) * 1.06f;
}

float SpatialEffect3D::GetColorCycleHz() const
{
    if(effect_frequency == 0)
    {
        return 0.0f;
    }
    return std::max(0.02f, GetNormalizedFrequency()) * 0.28f;
}

bool SpatialEffect3D::RequiresWorldSpaceCoordinates() const
{
    if(GetSpatialRoomMode() == SpatialRoom::SpatialRoomMode::EmissiveRelay)
    {
        return false;
    }
    return true;
}

bool SpatialEffect3D::UseZoneGrid() const
{
    return (effect_bounds_mode == (int)BOUNDS_MODE_TARGET_ZONE);
}

bool SpatialEffect3D::UseWorldGridBounds() const
{
    if(effect_bounds_mode == (int)BOUNDS_MODE_TARGET_ZONE)
    {
        return RequiresWorldSpaceCoordinates();
    }
    return RequiresWorldSpaceGridBounds();
}

float SpatialEffect3D::GetScaledDetail() const
{
    return GetNormalizedDetail() * 8.0f;
}

float SpatialEffect3D::GetHueBandDensity() const
{
    /* Spatial hue/ring density from Detail + Frequency only (no time/flash).
     * Both at 0 → 1.0 (soft single blend). High → many overlapping radial bands. */
    const float d01 = std::clamp(effect_detail / 200.0f, 0.0f, 1.0f);
    const float f01 = std::clamp(effect_frequency / 200.0f, 0.0f, 1.0f);
    return 1.0f + d01 * 40.0f + f01 * 24.0f + d01 * f01 * 16.0f;
}

float SpatialEffect3D::CalculateProgress(float time) const
{
    return time * GetMotionHz();
}

float SpatialEffect3D::GetMotionCycle01(float time) const
{
    return std::fmod(CalculateProgress(time) + 1000.0f, 1.0f);
}

RGBColor SpatialEffect3D::PostProcessColorGrid(RGBColor color) const
{
    float intensity_normalized = effect_intensity / 200.0f;
    float intensity_mul = std::pow(intensity_normalized, 0.7f) * 1.7f;
    float brightness_mul = effect_brightness / 100.0f;
    float factor = intensity_mul * brightness_mul;
    if(factor <= 0.0f) return 0x00000000;

    unsigned char r = color & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = (color >> 16) & 0xFF;
    int rr = (int)(r * factor); if(rr > 255) rr = 255;
    int gg = (int)(g * factor); if(gg > 255) gg = 255;
    int bb = (int)(b * factor); if(bb > 255) bb = 255;

    if(effect_sharpness != 0)
    {
        // 0 = passthrough. Higher = crisper / more contrast.
        // Old values below 100 darkened everything — that side of the scale is removed.
        const float gamma = std::pow(2.0f, effect_sharpness / 100.0f);
        float rf = (float)rr;
        float gf = (float)gg;
        float bf = (float)bb;
        float lum = 0.299f * rf + 0.587f * gf + 0.114f * bf;
        if(lum > 0.25f)
        {
            float lum_new = std::pow(lum / 255.0f, gamma) * 255.0f;
            float scale = lum_new / lum;
            rf = std::clamp(rf * scale, 0.0f, 255.0f);
            gf = std::clamp(gf * scale, 0.0f, 255.0f);
            bf = std::clamp(bf * scale, 0.0f, 255.0f);
            rr = (int)(rf + 0.5f);
            gg = (int)(gf + 0.5f);
            bb = (int)(bf + 0.5f);
            if(rr > 255) rr = 255;
            if(gg > 255) gg = 255;
            if(bb > 255) bb = 255;
        }
    }

    return (bb << 16) | (gg << 8) | rr;
}

static float smoothstep_edge(float edge0, float edge1, float x)
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float SpatialEffect3D::ApplyEdgeToIntensity(float normalized_dist) const
{
    float thick = 0.05f + 0.45f * (effect_edge_thickness / 100.0f);
    float glow = 0.15f * (effect_glow_level / 100.0f);
    int profile = std::clamp(effect_edge_profile, 0, 4);

    float mult = 0.0f;
    switch(profile)
    {
    case 0:
        mult = (normalized_dist < 1.0f) ? 1.0f : 0.0f;
        break;
    case 1:
        mult = (normalized_dist <= 1.0f) ? 1.0f : 0.0f;
        break;
    case 2:
        mult = 1.0f - smoothstep_edge(1.0f - thick, 1.0f, normalized_dist);
        break;
    case 3:
        mult = 1.0f - smoothstep_edge(1.0f - thick * 1.5f, 1.0f + thick * 0.5f, normalized_dist);
        break;
    case 4:
        {
            float margin = 0.08f * (1.0f - effect_edge_thickness / 100.0f);
            mult = (normalized_dist < 1.0f - margin) ? 1.0f : (1.0f - smoothstep_edge(1.0f - margin, 1.0f, normalized_dist));
        }
        break;
    default:
        mult = (normalized_dist < 1.0f) ? 1.0f : 0.0f;
        break;
    }
    if(normalized_dist > 1.0f && glow > 0.0f)
    {
        float fall = std::exp(-(normalized_dist - 1.0f) * 2.5f);
        mult = std::max(mult, glow * fall);
    }
    return std::clamp(mult, 0.0f, 1.0f);
}

float SpatialEffect3D::GetBoundaryMultiplier(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    const float nd = VolumeNormalizedDistance(rel_x, rel_y, rel_z, grid);
    /* Tight volume: no exterior soft buffer / glow past the shape surface. */
    if(nd > 1.0f)
    {
        return 0.0f;
    }
    return ApplyEdgeToIntensity(nd);
}

Vector3D SpatialEffect3D::VolumeWorldDeltaToLocal(float dx, float dy, float dz) const
{
    /* Inverse of EffectVolume3D::LocalToWorld — same Euler matrix as controllers
       (Geometry3D::ComputeRotationMatrix), then undo shear. */
    Rotation3D rot;
    rot.x = effect_rotation_pitch;
    rot.y = effect_rotation_yaw;
    rot.z = effect_rotation_roll;
    float m[9];
    Geometry3D::ComputeRotationMatrix(rot, m);
    /* R^T * delta (column-vector convention used by RotateVector). */
    float lx = m[0] * dx + m[3] * dy + m[6] * dz;
    float ly = m[1] * dx + m[4] * dy + m[7] * dz;
    float lz = m[2] * dx + m[5] * dy + m[8] * dz;

    if(effect_volume_tools_unlocked)
    {
        ly -= effect_shear_yz * lz;
        lx -= effect_shear_xy * ly + effect_shear_xz * lz;
    }
    return {lx, ly, lz};
}

void SpatialEffect3D::GetVolumeHalfExtents(const GridContext3D& grid, float& hx, float& hy, float& hz) const
{
    const Vector3D origin = GetEffectOriginGrid(grid);
    const float cover_x = std::max(1e-4f, std::max(origin.x - grid.min_x, grid.max_x - origin.x));
    const float cover_y = std::max(1e-4f, std::max(origin.y - grid.min_y, grid.max_y - origin.y));
    const float cover_z = std::max(1e-4f, std::max(origin.z - grid.min_z, grid.max_z - origin.z));
    /* Volume tools treat Scale as absolute occupancy. scale_inverted is a pattern
       semantic and must not fight the gizmo OBB. */
    const float scale_pct = effect_volume_tools_unlocked
        ? std::max(0.01f, effect_scale / 100.0f)
        : std::max(0.01f, GetNormalizedScale());
    hx = std::max(1e-4f, cover_x * scale_pct * ((float)effect_scale_x / 100.0f));
    hy = std::max(1e-4f, cover_y * scale_pct * ((float)effect_scale_y / 100.0f));
    hz = std::max(1e-4f, cover_z * scale_pct * ((float)effect_scale_z / 100.0f));
}

bool SpatialEffect3D::SampleVolumeLocalUv01(float world_x, float world_y, float world_z,
                                           const GridContext3D& grid,
                                           float* u, float* v, float* w) const
{
    const Vector3D origin = GetEffectOriginGrid(grid);
    const Vector3D local = VolumeWorldDeltaToLocal(world_x - origin.x,
                                                   world_y - origin.y,
                                                   world_z - origin.z);
    float hx = 1.0f, hy = 1.0f, hz = 1.0f;
    GetVolumeHalfExtents(grid, hx, hy, hz);
    const float uu = 0.5f + 0.5f * (local.x / hx);
    const float vv = 0.5f + 0.5f * (local.y / hy);
    const float ww = 0.5f + 0.5f * (local.z / hz);
    constexpr float kEps = 1e-4f;
    if(uu < -kEps || uu > 1.0f + kEps || vv < -kEps || vv > 1.0f + kEps || ww < -kEps || ww > 1.0f + kEps)
    {
        return false;
    }
    if(u) *u = std::clamp(uu, 0.0f, 1.0f);
    if(v) *v = std::clamp(vv, 0.0f, 1.0f);
    if(w) *w = std::clamp(ww, 0.0f, 1.0f);
    return VolumeNormalizedDistance(local.x, local.y, local.z, grid) <= 1.0f;
}

float SpatialEffect3D::VolumeNormalizedDistance(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    /* rel is volume-local (VolumeWorldDeltaToLocal already applied). */
    float hx = 1.0f, hy = 1.0f, hz = 1.0f;
    GetVolumeHalfExtents(grid, hx, hy, hz);
    const float nx = rel_x / hx;
    const float ny = rel_y / hy;
    const float nz = rel_z / hz;

    const int shape = effect_volume_tools_unlocked ? effect_volume_shape : (int)VOLUME_SHAPE_BOX;
    switch(shape)
    {
        case VOLUME_SHAPE_SPHERE:
            return std::sqrt(nx * nx + ny * ny + nz * nz);
        case VOLUME_SHAPE_CYLINDER:
            return std::max(std::sqrt(nx * nx + nz * nz), std::fabs(ny));
        case VOLUME_SHAPE_CONE:
        {
            /* Apex at +Y, base at -Y. */
            const float t = std::clamp(0.5f * (1.0f - ny), 0.0f, 1.0f);
            if(ny > 1.0f)
            {
                return 1.0f + (ny - 1.0f);
            }
            if(ny < -1.0f)
            {
                return 1.0f + (-1.0f - ny);
            }
            const float radial = std::sqrt(nx * nx + nz * nz);
            const float allow = std::max(t, 1e-4f);
            return radial / allow;
        }
        case VOLUME_SHAPE_TETRA:
        {
            /* Regular-ish tetra inscribed in the unit cube (tight planes). */
            const float d0 =  nx +  ny +  nz;
            const float d1 =  nx -  ny -  nz;
            const float d2 = -nx +  ny -  nz;
            const float d3 = -nx -  ny +  nz;
            const float lim = 1.0f;
            return std::max(std::max(d0, d1), std::max(d2, d3)) / lim;
        }
        case VOLUME_SHAPE_BOX:
        default:
            return std::max(std::fabs(nx), std::max(std::fabs(ny), std::fabs(nz)));
    }
}

bool SpatialEffect3D::IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    return VolumeNormalizedDistance(rel_x, rel_y, rel_z, grid) <= 1.0f;
}

bool SpatialEffect3D::IsPointOnActiveSurface(float x, float y, float z, const GridContext3D& grid) const
{
    if((effect_surface_mask & SURF_ALL) == SURF_ALL)
        return true;
    float d_floor = y - grid.min_y;
    float d_ceil = grid.max_y - y;
    float d_wxm = x - grid.min_x;
    float d_wxp = grid.max_x - x;
    float d_wzm = z - grid.min_z;
    float d_wzp = grid.max_z - z;
    float best = d_floor;
    int surf = SURF_FLOOR;
    if(d_ceil < best) { best = d_ceil; surf = SURF_CEIL; }
    if(d_wxm < best) { best = d_wxm; surf = SURF_WALL_XM; }
    if(d_wxp < best) { best = d_wxp; surf = SURF_WALL_XP; }
    if(d_wzm < best) { best = d_wzm; surf = SURF_WALL_ZM; }
    if(d_wzp < best) { surf = SURF_WALL_ZP; }
    return (effect_surface_mask & surf) != 0;
}
