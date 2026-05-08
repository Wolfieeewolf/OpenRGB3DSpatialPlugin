// SPDX-License-Identifier: GPL-2.0-only

#include "SharpPulse.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(SharpPulse);

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}

inline float Wave01(float x01)
{
    return 0.5f + 0.5f * std::sin(kTwoPi * x01);
}

inline float Triangle01(float x01)
{
    const float f = x01 - std::floor(x01);
    return 1.0f - std::fabs(2.0f * f - 1.0f);
}

inline RGBColor Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    const float hf = h * 6.0f;
    const int i = (int)std::floor(hf) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch(i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}
} // namespace

SharpPulse::SharpPulse(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(55);
    SetFrequency(20);
}

SharpPulse::~SharpPulse() = default;

EffectInfo3D SharpPulse::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Sharp Pulse";
    info.effect_description =
        "Fast high-contrast 3D pulse field from moving XYZ wave sums.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SHARP_PULSE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_speed_scale = 55.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void SharpPulse::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(sharppulse_strip_cmap_on,
                                            sharppulse_strip_cmap_kernel,
                                            sharppulse_strip_cmap_rep,
                                            sharppulse_strip_cmap_unfold,
                                            sharppulse_strip_cmap_dir,
                                            sharppulse_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &SharpPulse::SyncStripColormapFromPanel);
    AddWidgetToParent(w, parent);
}

void SharpPulse::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    sharppulse_strip_cmap_on = strip_cmap_panel->useStripColormap();
    sharppulse_strip_cmap_kernel = strip_cmap_panel->kernelId();
    sharppulse_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    sharppulse_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    sharppulse_strip_cmap_dir = strip_cmap_panel->directionDeg();
    sharppulse_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void SharpPulse::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SHARP_PULSE;
}

RGBColor SharpPulse::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float spd = std::max(0.08f, GetScaledSpeed() / 100.0f);
    const float rate = std::max(0.08f, GetScaledFrequency() / 50.0f);
    const float motion = spd * rate;

    const float t1 = Phase01(time, 10.0f, motion); // time(.1)
    const float r1 = std::sin(kTwoPi * Phase01(time, 10.0f, motion));
    const float r2 = std::sin(kTwoPi * Phase01(time, 20.0f, motion));
    const float r3 = std::sin(kTwoPi * Phase01(time, 14.285714f, motion));

    const float size_mul = std::max(0.2f, GetNormalizedSize());
    float v = Triangle01(std::fmod(3.0f * Wave01(t1) + (nx * r1 + ny * r2 + nz * r3) * size_mul + 10.0f, 1.0f));
    v = v * v * v * v * v;
    const float s = (v < 0.8f) ? 1.0f : 0.0f;

    RGBColor c = 0x00000000;
    if(sharppulse_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(sharppulse_strip_cmap_kernel,
                                               sharppulse_strip_cmap_rep,
                                               sharppulse_strip_cmap_unfold,
                                               sharppulse_strip_cmap_dir,
                                               t1,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rot);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        c = ResolveStripKernelFinalColor(*this,
                                         SpatialPatternKernelClamp(sharppulse_strip_cmap_kernel),
                                         p01,
                                         sharppulse_strip_cmap_color_style,
                                         time,
                                         rate * 12.0f);
    }
    else if(GetRainbowMode())
    {
        c = Hsv01ToBgr(t1, s, 1.0f);
    }
    else
    {
        c = GetColorAtPosition(t1);
    }

    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json SharpPulse::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    StripColormapSaveJson(j,
                          "sharppulse",
                          sharppulse_strip_cmap_on,
                          sharppulse_strip_cmap_kernel,
                          sharppulse_strip_cmap_rep,
                          sharppulse_strip_cmap_unfold,
                          sharppulse_strip_cmap_dir,
                          sharppulse_strip_cmap_color_style);
    return j;
}

void SharpPulse::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    StripColormapLoadJson(settings,
                          "sharppulse",
                          sharppulse_strip_cmap_on,
                          sharppulse_strip_cmap_kernel,
                          sharppulse_strip_cmap_rep,
                          sharppulse_strip_cmap_unfold,
                          sharppulse_strip_cmap_dir,
                          sharppulse_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(sharppulse_strip_cmap_on,
                                                sharppulse_strip_cmap_kernel,
                                                sharppulse_strip_cmap_rep,
                                                sharppulse_strip_cmap_unfold,
                                                sharppulse_strip_cmap_dir,
                                                sharppulse_strip_cmap_color_style);
    }
}
