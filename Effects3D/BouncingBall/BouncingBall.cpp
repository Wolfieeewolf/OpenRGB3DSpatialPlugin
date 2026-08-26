// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall.h"
#include "BouncingBallVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "EffectUiRows.h"
#include <cmath>
#include <algorithm>

BouncingBall::BouncingBall(QWidget* parent) : SpatialEffect3D(parent)
{
    count_slider = nullptr;
    ball_count = 4;
    SetRainbowMode(true);
    volume_assist_.setFragmentBody(QString::fromUtf8(BouncingBallVolumeFieldGlsl()));
    volume_assist_.setResolution(20);
}

BouncingBall::~BouncingBall() = default;

EffectInfo3D BouncingBall::GetEffectInfo() const
{
    EffectInfo3D info;
    info.effect_name = "Bouncing Ball";
    info.effect_description =
        "Independent balls bouncing in the room with GPU assist (no ball–ball collisions). "
        "Speed sets motion rate; Frequency scrolls hue; Size sets ball radius.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_BOUNCING_BALL;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 16.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void BouncingBall::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    EffectSliderRow* ball_count_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Balls:"),
        1,
        (int)kMaxGpuBalls,
        (int)std::clamp(ball_count, 1u, kMaxGpuBalls),
        QStringLiteral("Number of balls (GPU atlas evaluates the full set once per frame)."));
    ball_count_row->setObjectName(QStringLiteral("ballCountRow"));
    count_slider = ball_count_row->slider();
    ball_count_row->bindValueChanged(
        this,
        [this](int v) { ball_count = (unsigned int)std::clamp(v, 1, (int)kMaxGpuBalls); },
        [](int v) { return QString::number(v); },
        [this]() { emit ParametersChanged(); });

    AddWidgetToParent(w, parent);
}

void BouncingBall::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float speed_lin = fmaxf(0.02f, fminf(1.0f, GetSpeed() / 200.0f));
    const float motion = speed_lin * speed_lin * 0.28f + speed_lin * 0.72f;
    const float size_m = GetNormalizedSize();
    const float detail = std::max(0.05f, GetScaledDetail());
    const float radius01 = std::clamp(0.045f + 0.12f * size_m * GetNormalizedScale(), 0.03f, 0.26f);
    const float sim_phase_rate = (0.28f + motion * 2.35f) * bb.speed_mul;
    const float sim_t = time_sec * sim_phase_rate;
    const float hue_scroll =
        std::fmod(time_sec * GetScaledFrequency() * 0.033f * bb.speed_mul + 1000.0f, 1.0f);
    const float vp[7] = {
        sim_t,
        (float)std::clamp(ball_count == 0 ? 1u : ball_count, 1u, kMaxGpuBalls),
        radius01,
        1.0f,
        motion,
        hue_scroll,
        detail
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 7);
}

RGBColor BouncingBall::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rp{x, y, z};
    const float coord2 = SampleStratumYNorm01(rp.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    const float size_m = GetNormalizedSize();
    const float color_cycle = time * GetScaledFrequency() * 12.0f * bb.speed_mul;
    const float detail = std::max(0.05f, GetScaledDetail()) * std::max(0.25f, bb.tight_mul);

    float max_intensity = 0.0f;
    float hue_for_max = 120.0f;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rp.x, rp.y, rp.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);
    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
    max_intensity = samp.x();
    hue_for_max = std::fmod(samp.y() * 360.0f + color_cycle * 0.25f
                                + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f
                                + 720.0f,
                            360.0f);
    if(GetStratumLayoutMode() == 1)
        max_intensity = EffectStratumBlend::ApplyMotionToUnit01(max_intensity, stratum_mot01, 0.18f);

    if(max_intensity <= 1e-5f)
        return 0x00000000;

    float strip_p01_bb = 0.f;
    if(UseEffectStripColormap())
    {
        const float ph01 =
            std::fmod(color_cycle * (1.f / 360.f) + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.f, 1.f);
        strip_p01_bb = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                  GetEffectStripColormapUnfold(),
                                                  GetEffectStripColormapDirectionDeg(),
                                                  ph01,
                                                  time,
                                                  grid,
                                                  size_m,
                                                  origin,
                                                  rp);
    }
    const float pos_driver = fmodf(0.28f + hue_for_max * (1.0f / 360.0f) + 1.0f, 1.0f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.10f, 0.05f, 0.22f);
    map.center_size = std::clamp(0.11f + 0.24f * GetNormalizedScale(), 0.06f, 0.52f);
    map.directional_sharpness = std::clamp(1.05f + std::max(0.05f, detail) * 0.08f, 0.9f, 2.3f);

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
        float p01v = strip_p01_bb;
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), p01v, time);
    }
    else if(GetRainbowMode())
    {
        float hue = ApplySpatialRainbowHue(hue_for_max, pos_driver, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(pos_driver, basis, sp, map, time, &grid);
        final_color = GetColorAtPosition(p);
    }
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json BouncingBall::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ball_count"] = ball_count;
    return j;
}

void BouncingBall::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("ball_count"))
        ball_count = std::clamp(settings["ball_count"].get<unsigned int>(), 1u, kMaxGpuBalls);
    if(count_slider)
        count_slider->setValue((int)ball_count);
}

REGISTER_EFFECT_3D(BouncingBall);
