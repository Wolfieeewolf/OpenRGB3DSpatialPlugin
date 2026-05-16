// SPDX-License-Identifier: GPL-2.0-only

#include "FreqRipple.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

FreqRipple::FreqRipple(QWidget* parent)
    : SpatialEffect3D(parent)
{
    ripples.reserve(64);
}

EffectInfo3D FreqRipple::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Frequency Ripple";
    info.effect_description = "Beat-triggered expanding ring from origin; optional stratum band tuning";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 4.0f;
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

void FreqRipple::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* width_row = new QHBoxLayout();
    width_row->addWidget(new QLabel("Ring Width:"));
    QSlider* width_slider = new QSlider(Qt::Horizontal);
    width_slider->setRange(5, 200);
    width_slider->setValue((int)(trail_width * 100.0f));
    QLabel* width_label = new QLabel(QString::number((int)(trail_width * 100)) + "%");
    width_label->setMinimumWidth(40);
    width_row->addWidget(width_slider);
    width_row->addWidget(width_label);
    layout->addLayout(width_row);

    connect(width_slider, &QSlider::valueChanged, this, [this, width_label](int v){
        trail_width = v / 100.0f;
        width_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* decay_row = new QHBoxLayout();
    decay_row->addWidget(new QLabel("Decay:"));
    QSlider* decay_slider = new QSlider(Qt::Horizontal);
    decay_slider->setRange(50, 800);
    decay_slider->setValue((int)(decay_rate * 100.0f));
    QLabel* decay_label = new QLabel(QString::number(decay_rate, 'f', 1));
    decay_label->setMinimumWidth(36);
    decay_row->addWidget(decay_slider);
    decay_row->addWidget(decay_label);
    layout->addLayout(decay_row);

    connect(decay_slider, &QSlider::valueChanged, this, [this, decay_label](int v){
        decay_rate = v / 100.0f;
        decay_label->setText(QString::number(decay_rate, 'f', 1));
        emit ParametersChanged();
    });

    QHBoxLayout* thresh_row = new QHBoxLayout();
    thresh_row->addWidget(new QLabel("Threshold:"));
    QSlider* thresh_slider = new QSlider(Qt::Horizontal);
    thresh_slider->setRange(0, 95);
    thresh_slider->setValue((int)(onset_threshold * 100.0f));
    QLabel* thresh_label = new QLabel(QString::number((int)(onset_threshold * 100)) + "%");
    thresh_label->setMinimumWidth(40);
    thresh_row->addWidget(thresh_slider);
    thresh_row->addWidget(thresh_label);
    layout->addLayout(thresh_row);

    connect(thresh_slider, &QSlider::valueChanged, this, [this, thresh_label](int v){
        onset_threshold = v / 100.0f;
        thresh_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* boost_row = new QHBoxLayout();
    boost_row->addWidget(new QLabel("Peak Boost:"));
    QSlider* boost_slider = new QSlider(Qt::Horizontal);
    boost_slider->setRange(50, 500);
    boost_slider->setValue((int)(audio_settings.peak_boost * 100.0f));
    QLabel* boost_label = new QLabel(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
    boost_label->setMinimumWidth(44);
    boost_row->addWidget(boost_slider);
    boost_row->addWidget(boost_label);
    layout->addLayout(boost_row);

    connect(boost_slider, &QSlider::valueChanged, this, [this, boost_label](int v){
        audio_settings.peak_boost = v / 100.0f;
        boost_label->setText(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
        emit ParametersChanged();
    });

    QHBoxLayout* edge_row = new QHBoxLayout();
    edge_row->addWidget(new QLabel("Edge:"));
    QComboBox* edge_combo = new QComboBox();
    edge_combo->addItem("Round");
    edge_combo->addItem("Sharp");
    edge_combo->addItem("Square");
    edge_combo->setCurrentIndex(std::clamp(ripple_edge_shape, 0, 2));
    edge_combo->setToolTip("Cross-section of the expanding beat ring.");
    edge_combo->setItemData(0, "Soft falloff—wider apparent ring.", Qt::ToolTipRole);
    edge_combo->setItemData(1, "Narrow hard ring.", Qt::ToolTipRole);
    edge_combo->setItemData(2, "Flat ring top with steep sides.", Qt::ToolTipRole);
    edge_row->addWidget(edge_combo);
    layout->addLayout(edge_row);
    connect(edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        ripple_edge_shape = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });
}

float FreqRipple::smoothstep(float edge0, float edge1, float x) const
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void FreqRipple::UpdateParams(SpatialEffectParams& /*params*/)
{
}

void FreqRipple::TickRipples(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-4f)
    {
        return;
    }
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    float onset_raw = AudioInputManager::instance()->getOnsetLevel();
    onset_smoothed = 0.5f * onset_smoothed + 0.5f * onset_raw;

    if(onset_hold > 0.0f)
    {
        onset_hold = std::max(0.0f, onset_hold - dt);
    }

    if(onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        Ripple r;
        r.birth_time = time;
        r.strength   = std::clamp(onset_smoothed * audio_settings.peak_boost, 0.0f, 1.0f);
        ripples.push_back(r);
        onset_hold = 0.12f;
    }

    ripples.erase(std::remove_if(ripples.begin(), ripples.end(),
        [&](const Ripple& r) {
            float age = time - r.birth_time;
            float alive = r.strength * std::exp(-decay_rate * age);
            return alive < 0.004f;
        }), ripples.end());
}

RGBColor FreqRipple::ComputeRippleColor(float dist_norm,
                                        float time,
                                        const EffectStratumBlend::BandBlendScalars& bb,
                                        float stratum_mot01) const
{
    RGBColor result = ToRGBColor(0, 0, 0);
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    float phase_shift = EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);

    for(unsigned int i = 0; i < ripples.size(); i++)
    {
        const Ripple& r = ripples[i];
        float age = time - r.birth_time;
        if(age < 0.0f) continue;

        float expand = (0.2f + GetScaledSpeed() * 0.95f) * bb.speed_mul;
        float front = expand * age + phase_shift * 0.35f;
        float ring_dist = std::fabs(dist_norm - front);
        float half_w = std::max(trail_width * 0.5f * size_m, 1e-3f) / ((0.6f + 0.4f * detail) * tm);

        float ring_bright = 0.0f;
        switch(ripple_edge_shape)
        {
        case 0:
            ring_bright = 1.0f - smoothstep(0.0f, half_w, ring_dist);
            break;
        case 1:
            ring_bright = ring_dist < half_w * 0.5f ? 1.0f : 0.0f;
            break;
        case 2:
        default:
            ring_bright = ring_dist < half_w ? 1.0f : 0.0f;
            break;
        }

        float fade = r.strength * std::exp(-decay_rate * age);
        float contribution = ring_bright * fade;

        if(contribution < 0.004f) continue;

        RGBColor ring_color = ComposeAudioGradientColor(audio_settings, dist_norm, contribution);
        ring_color = ScaleRGBColor(ring_color, contribution);

        int rr = std::clamp((int)(result & 0xFF)         + (int)(ring_color & 0xFF),         0, 255);
        int rg = std::clamp((int)((result >> 8) & 0xFF)  + (int)((ring_color >> 8) & 0xFF),  0, 255);
        int rb = std::clamp((int)((result >> 16) & 0xFF) + (int)((ring_color >> 16) & 0xFF), 0, 255);
        result = MakeRGBColor(rr, rg, rb);
    }

    return result;
}

RGBColor FreqRipple::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    TickRipples(time);

    Vector3D origin = GetEffectOriginGrid(grid);
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

    float dx = rp.x - origin.x;
    float dy = rp.y - origin.y;
    float dz = rp.z - origin.z;
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float dist_norm = std::clamp(std::sqrt(dx*dx + dy*dy + dz*dz) / std::max(max_radius, 1e-5f), 0.0f, 2.0f);

    RGBColor color = ComputeRippleColor(dist_norm, time, bb, stratum_mot01);

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

    RGBColor user_color;
    if(UseEffectStripColormap())
    {
        const float size_m = GetNormalizedSize();
        const float ph01 =
            std::fmod(CalculateProgress(time) * 0.37f + dist_norm * 0.21f +
                          time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) + 1.f,
                      1.f);
        float p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        user_color = ResolveStripKernelFinalColor(*this, GetEffectStripColormapKernel(), std::min(p01, 1.0f),
                                                  GetEffectStripColormapColorStyle(), time,
                                                  GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        float hue =
            dist_norm * 180.0f + CalculateProgress(time) * 50.0f + time * GetScaledFrequency() * 12.0f * bb.speed_mul;
        hue = ApplySpatialRainbowHue(hue, std::min(dist_norm * 0.5f, 1.0f), basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        user_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(std::min(dist_norm * 0.5f, 1.0f), basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        user_color = GetColorAtPosition(std::min(p, 1.0f));
    }
    return ModulateRGBColors(color, user_color);
}

nlohmann::json FreqRipple::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["trail_width"]      = trail_width;
    j["decay_rate"]       = decay_rate;
    j["onset_threshold"]  = onset_threshold;
    j["ripple_edge_shape"] = ripple_edge_shape;
return j;
}

void FreqRipple::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("trail_width"))     trail_width     = settings["trail_width"].get<float>();
    if(settings.contains("decay_rate"))      decay_rate      = settings["decay_rate"].get<float>();
    if(settings.contains("onset_threshold")) onset_threshold = settings["onset_threshold"].get<float>();
    if(settings.contains("ripple_edge_shape")) ripple_edge_shape = std::clamp(settings["ripple_edge_shape"].get<int>(), 0, 2);
ripples.clear();
    last_tick_time = std::numeric_limits<float>::lowest();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
}

REGISTER_EFFECT_3D(FreqRipple)
