// SPDX-License-Identifier: GPL-2.0-only

#include "FreqFill.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

float FreqFill::EvaluateIntensity(float amplitude, float time)
{
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
    }
    return ApplyAudioIntensity(smoothed, audio_settings);
}

FreqFill::FreqFill(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D FreqFill::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Frequency Fill";
    info.effect_description = "Fills room along an axis like a VU meter; optional stratum band tuning";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 1.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    return info;
}

void FreqFill::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* edge_row = new QHBoxLayout();
    edge_row->addWidget(new QLabel("Edge width:"));
    QSlider* edge_slider = new QSlider(Qt::Horizontal);
    edge_slider->setRange(0, 100);
    edge_slider->setToolTip("Width of the transition from lit to dark along the fill axis (Path axis in common controls).");
    edge_slider->setValue((int)(edge_width * 100.0f));
    QLabel* edge_label = new QLabel(QString::number((int)(edge_width * 100)) + "%");
    edge_label->setMinimumWidth(40);
    edge_row->addWidget(edge_slider);
    edge_row->addWidget(edge_label);
    layout->addLayout(edge_row);

    connect(edge_slider, &QSlider::valueChanged, this, [this, edge_label](int v){
        edge_width = v / 100.0f;
        edge_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* smooth_row = new QHBoxLayout();
    smooth_row->addWidget(new QLabel("Smoothing:"));
    QSlider* smooth_slider = new QSlider(Qt::Horizontal);
    smooth_slider->setRange(0, 99);
    smooth_slider->setToolTip("Smooths band energy before mapping to fill position.");
    smooth_slider->setValue((int)(audio_settings.smoothing * 100.0f));
    QLabel* smooth_label = new QLabel(QString::number(audio_settings.smoothing, 'f', 2));
    smooth_label->setMinimumWidth(36);
    smooth_row->addWidget(smooth_slider);
    smooth_row->addWidget(smooth_label);
    layout->addLayout(smooth_row);

    connect(smooth_slider, &QSlider::valueChanged, this, [this, smooth_label](int v){
        audio_settings.smoothing = v / 100.0f;
        smooth_label->setText(QString::number(audio_settings.smoothing, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* boost_row = new QHBoxLayout();
    boost_row->addWidget(new QLabel("Peak boost:"));
    QSlider* boost_slider = new QSlider(Qt::Horizontal);
    boost_slider->setRange(50, 500);
    boost_slider->setToolTip("Boosts quiet audio so the fill reaches farther on subtle tracks.");
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

    strip_cmap_panel = new StripKernelColormapPanel(parent);
    strip_cmap_panel->mirrorStateFromEffect(freqfill_strip_cmap_on,
                                            freqfill_strip_cmap_kernel,
                                            freqfill_strip_cmap_rep,
                                            freqfill_strip_cmap_unfold,
                                            freqfill_strip_cmap_dir,
                                            freqfill_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &FreqFill::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(parent);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &FreqFill::OnStratumBandChanged);
    OnStratumBandChanged();
}

void FreqFill::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    freqfill_strip_cmap_on = strip_cmap_panel->useStripColormap();
    freqfill_strip_cmap_kernel = strip_cmap_panel->kernelId();
    freqfill_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    freqfill_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    freqfill_strip_cmap_dir = strip_cmap_panel->directionDeg();
    freqfill_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void FreqFill::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void FreqFill::UpdateParams(SpatialEffectParams& /*params*/)
{
}

static float AxisPosition(int axis, float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z)
{
    float val = 0.0f, lo = 0.0f, hi = 1.0f;
    switch(axis)
    {
        case 0: val = x; lo = min_x; hi = max_x; break;
        case 2: val = z; lo = min_z; hi = max_z; break;
        default: val = y; lo = min_y; hi = max_y; break;
    }
    float range = hi - lo;
    if(range < 1e-5f) return 0.5f;
    return std::clamp((val - lo) / range, 0.0f, 1.0f);
}


RGBColor FreqFill::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float fill_level = EvaluateIntensity(amplitude, time);

    Vector3D o = GetEffectOriginGrid(grid);
    Vector3D rot = TransformPointByRotation(x, y, z, o);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float pos = AxisPosition(GetPathAxis(), rot.x, rot.y, rot.z,
                             grid.min_x, grid.max_x,
                             grid.min_y, grid.max_y,
                             grid.min_z, grid.max_z);

    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    float edge = std::max(edge_width, 1e-3f) / std::max(0.35f, size_m * tm);
    float blend = std::clamp((fill_level - pos) / edge + 0.5f, 0.0f, 1.0f);

    float pos_color =
        fmodf(pos * (0.6f + 0.4f * detail * tm) + time * GetScaledFrequency() * 0.02f * bb.speed_mul + bb.phase_deg * (1.0f / 360.0f),
              1.0f);
    if(pos_color < 0.0f) pos_color += 1.0f;

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), detail);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = o.x;
    sp.origin_y = o.y;
    sp.origin_z = o.z;
    sp.y_norm = coord2;

    RGBColor lit_color;
    if(freqfill_strip_cmap_on)
    {
        const float ph01 =
            std::fmod(CalculateProgress(time) * 0.28f + pos * 0.15f + pos_color * 0.12f + 1.f, 1.f);
        float p01 = SampleStripKernelPalette01(freqfill_strip_cmap_kernel,
                                                 freqfill_strip_cmap_rep,
                                                 freqfill_strip_cmap_unfold,
                                                 freqfill_strip_cmap_dir,
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 o,
                                                 rot);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        lit_color = ResolveStripKernelFinalColor(*this, freqfill_strip_cmap_kernel, p01, freqfill_strip_cmap_color_style, time,
                                                 GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        float hue = pos_color * 360.0f + time * GetScaledFrequency() * 12.0f * bb.speed_mul;
        hue = ApplySpatialRainbowHue(hue, pos_color, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        lit_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        lit_color = GetColorAtPosition(p);
    }
    RGBColor dark_color = GetColorAtPosition(1.0f);

    RGBColor color = BlendRGBColors(dark_color, lit_color, blend);
    return ScaleRGBColor(color, 0.1f + 0.9f * blend);
}

nlohmann::json FreqFill::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["edge_width"] = edge_width;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "freqfill_stratum_layout_mode",
                                           sm,
                                           st,
                                           "freqfill_stratum_band_speed_pct",
                                           "freqfill_stratum_band_tight_pct",
                                           "freqfill_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "freqfill",
                          freqfill_strip_cmap_on,
                          freqfill_strip_cmap_kernel,
                          freqfill_strip_cmap_rep,
                          freqfill_strip_cmap_unfold,
                          freqfill_strip_cmap_dir,
                          freqfill_strip_cmap_color_style);
    return j;
}

void FreqFill::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "freqfill_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "freqfill_stratum_band_speed_pct",
                                            "freqfill_stratum_band_tight_pct",
                                            "freqfill_stratum_band_phase_deg");
    if(settings.contains("edge_width")) edge_width = settings["edge_width"].get<float>();
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
    StripColormapLoadJson(settings,
                          "freqfill",
                          freqfill_strip_cmap_on,
                          freqfill_strip_cmap_kernel,
                          freqfill_strip_cmap_rep,
                          freqfill_strip_cmap_unfold,
                          freqfill_strip_cmap_dir,
                          freqfill_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(freqfill_strip_cmap_on,
                                                freqfill_strip_cmap_kernel,
                                                freqfill_strip_cmap_rep,
                                                freqfill_strip_cmap_unfold,
                                                freqfill_strip_cmap_dir,
                                                freqfill_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(FreqFill)
