// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall.h"
#include "BouncingBallVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include "EffectUiRows.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace
{

float HashFloat01(unsigned int seed)
{
    unsigned int value = seed ^ 0x27D4EB2D;
    value = (value ^ 61U) ^ (value >> 16U);
    value = value + (value << 3U);
    value = value ^ (value >> 4U);
    value = value * 0x27D4EB2D;
    value = value ^ (value >> 15U);
    return (value & 0xFFFFU) / 65535.0f;
}

void IntegrateBall(float& pos_x, float& pos_y, float& pos_z,
                   float& vel_x, float& vel_y, float& vel_z,
                   float dt, float gravity, float e,
                   float floor_bounce_vy_up,
                   float xmin, float xmax, float ymin, float ymax, float zmin, float zmax)
{
    vel_y -= gravity * dt;

    pos_x += vel_x * dt;
    pos_y += vel_y * dt;
    pos_z += vel_z * dt;

    constexpr float k_horiz_damp = 0.992f;

    if(pos_x <= xmin)
    {
        pos_x = xmin;
        vel_x = -vel_x * e * k_horiz_damp;
    }
    else if(pos_x >= xmax)
    {
        pos_x = xmax;
        vel_x = -vel_x * e * k_horiz_damp;
    }

    if(pos_y <= ymin)
    {
        pos_y = ymin;
        // Prefer a strong hop: max of fixed bounce energy and elastic rebound.
        const float rebound = -vel_y * e;
        vel_y = fmaxf(floor_bounce_vy_up, rebound);
    }
    else if(pos_y >= ymax)
    {
        pos_y = ymax;
        vel_y = -vel_y * e;
    }

    if(pos_z <= zmin)
    {
        pos_z = zmin;
        vel_z = -vel_z * e * k_horiz_damp;
    }
    else if(pos_z >= zmax)
    {
        pos_z = zmax;
        vel_z = -vel_z * e * k_horiz_damp;
    }
}

void ClampBallSpeed(float& vx, float& vy, float& vz, float v_max_horiz, float v_max_vert)
{
    const float h2 = vx * vx + vz * vz;
    const float hm2 = v_max_horiz * v_max_horiz;
    if(h2 > hm2 && h2 > 1e-12f)
    {
        const float inv = v_max_horiz / sqrtf(h2);
        vx *= inv;
        vz *= inv;
    }
    if(vy > v_max_vert)
        vy = v_max_vert;
    else if(vy < -v_max_vert)
        vy = -v_max_vert;
}

void SeedBall(unsigned int k,
              float xmin, float ymin, float zmin,
              float span_x, float span_y, float span_z,
              float motion, float gravity, float radius_basis,
              CachedBall3D& b)
{
    const float hy = HashFloat01(k * 313U + 5U);
    const float hx = HashFloat01(k * 131U);
    const float hz = HashFloat01(k * 919U);

    b.px = xmin + hx * span_x;
    b.py = ymin + (0.18f + hy * 0.72f) * span_y;
    b.pz = zmin + hz * span_z;

    const float drop_h = fmaxf(span_y * (0.35f + 0.55f * HashFloat01(k * 419U + 11U)),
                               radius_basis * 0.08f);
    b.floor_bounce_vy = sqrtf(2.0f * gravity * drop_h) * 1.05f;

    // Keep horizontal drift secondary to vertical bounce.
    const float horiz = (0.12f + 0.45f * motion) * radius_basis;
    b.vx = (HashFloat01(k * 733U) * 2.0f - 1.0f) * horiz;
    b.vz = (HashFloat01(k * 829U) * 2.0f - 1.0f) * horiz;
    b.vy = (0.55f + HashFloat01(k * 577U) * 0.55f) * b.floor_bounce_vy;
}

}

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

void BouncingBall::OnBallParameterChanged()
{
    if(count_slider)
        ball_count = (unsigned int)std::clamp(count_slider->value(), 1, (int)kMaxGpuBalls);
    emit ParametersChanged();
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
    // Unit-cube ball radius — Size scales glow footprint; floor keeps sparse LEDs lit.
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
        strip_p01_bb = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                  GetEffectStripColormapRepeats(),
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
    ball_last_integrated_wall_time = -1e9f;
}

REGISTER_EFFECT_3D(BouncingBall);
