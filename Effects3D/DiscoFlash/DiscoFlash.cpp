// SPDX-License-Identifier: GPL-2.0-only

#include "DiscoFlash.h"
#include "SpatialKernelColormap.h"
#include "EffectStratumBlend.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

DiscoFlash::DiscoFlash(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D DiscoFlash::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Disco Flash";
    info.effect_description = "Beat-triggered random colour flashes; optional stratum band tuning";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 1;
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
    info.show_color_controls = false;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void DiscoFlash::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* mode_row = new QHBoxLayout();
    mode_row->addWidget(new QLabel("Mode:"));
    QComboBox* mode_combo = new QComboBox();
    mode_combo->addItem("Beat (audio)");
    mode_combo->addItem("Sparkle (time)");
    mode_combo->setCurrentIndex(std::max(0, std::min(flash_mode, MODE_COUNT - 1)));
    mode_combo->setToolTip("Beat mode needs audio input; Sparkle uses time only.");
    mode_combo->setItemData(0, "Flash on rhythm peaks—threshold and density matter.", Qt::ToolTipRole);
    mode_combo->setItemData(1, "Random twinkles over time, no beat required.", Qt::ToolTipRole);
    mode_row->addWidget(mode_combo);
    layout->addLayout(mode_row);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        flash_mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });

    QHBoxLayout* density_row = new QHBoxLayout();
    density_row->addWidget(new QLabel("Density:"));
    QSlider* density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(5, 100);
    density_slider->setToolTip("How much of the room flashes per hit (Beat mode) or sparkle density (Sparkle mode).");
    density_slider->setValue((int)(flash_density * 100.0f));
    QLabel* density_label = new QLabel(QString::number((int)(flash_density * 100)) + "%");
    density_label->setMinimumWidth(40);
    density_row->addWidget(density_slider);
    density_row->addWidget(density_label);
    layout->addLayout(density_row);

    connect(density_slider, &QSlider::valueChanged, this, [this, density_label](int v){
        flash_density = v / 100.0f;
        density_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* size_row = new QHBoxLayout();
    size_row->addWidget(new QLabel("Flash Size:"));
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(5, 200);
    size_slider->setToolTip("Spatial size of each flash region.");
    size_slider->setValue((int)(flash_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(flash_size * 100)) + "%");
    size_label->setMinimumWidth(40);
    size_row->addWidget(size_slider);
    size_row->addWidget(size_label);
    layout->addLayout(size_row);

    connect(size_slider, &QSlider::valueChanged, this, [this, size_label](int v){
        flash_size = v / 100.0f;
        size_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* decay_row = new QHBoxLayout();
    decay_row->addWidget(new QLabel("Decay:"));
    QSlider* decay_slider = new QSlider(Qt::Horizontal);
    decay_slider->setRange(50, 1000);
    decay_slider->setValue((int)(flash_decay * 100.0f));
    QLabel* decay_label = new QLabel(QString::number(flash_decay, 'f', 1));
    decay_label->setMinimumWidth(36);
    decay_row->addWidget(decay_slider);
    decay_row->addWidget(decay_label);
    layout->addLayout(decay_row);

    connect(decay_slider, &QSlider::valueChanged, this, [this, decay_label](int v){
        flash_decay = v / 100.0f;
        decay_label->setText(QString::number(flash_decay, 'f', 1));
        emit ParametersChanged();
    });

    QHBoxLayout* thresh_row = new QHBoxLayout();
    thresh_row->addWidget(new QLabel("Threshold:"));
    QSlider* thresh_slider = new QSlider(Qt::Horizontal);
    thresh_slider->setRange(0, 95);
    thresh_slider->setToolTip("Beat onset sensitivity in Beat mode (higher = fewer flashes).");
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
    boost_slider->setToolTip("Gain on the monitored band for beat detection and flash strength.");
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
}

void DiscoFlash::UpdateParams(SpatialEffectParams& /*params*/)
{
}

void DiscoFlash::TickFlashes(float time)
{
    if(flash_mode == MODE_SPARKLE)
        return;
    if(std::fabs(time - last_tick_time) < 1e-4f)
    {
        return;
    }
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    float raw = AudioInputManager::instance()->getOnsetLevel();
    onset_smoothed = 0.4f * onset_smoothed + 0.6f * raw;

    if(onset_hold > 0.0f)
    {
        onset_hold = std::max(0.0f, onset_hold - dt);
    }

    if(onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        float strength = std::clamp(onset_smoothed * audio_settings.peak_boost, 0.0f, 1.0f);
        int count = std::max(1, (int)(flash_density * 12.0f * strength));

        std::uniform_real_distribution<float> pos_dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> hue_dist(0.0f, 360.0f);
        std::uniform_real_distribution<float> size_dist(0.5f, 1.5f);

        flashes.reserve(flashes.size() + (size_t)count);
        for(int i = 0; i < count; ++i)
        {
            Flash f;
            f.birth_time = time;
            f.hue  = hue_dist(rng);
            f.nx   = pos_dist(rng);
            f.ny   = pos_dist(rng);
            f.nz   = pos_dist(rng);
            f.size = flash_size * size_dist(rng);
            flashes.push_back(f);
        }
        onset_hold = 0.10f;
    }

    flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
        [&](const Flash& f) {
            float age = time - f.birth_time;
            return std::exp(-flash_decay * age) < 0.004f;
        }), flashes.end());
}

float DiscoFlash::ApplyDiscoFlashRoomHue(float hue_deg,
                                         float x,
                                         float y,
                                         float z,
                                         float time,
                                         const GridContext3D& grid,
                                         const Vector3D& origin,
                                         const Vector3D& rp) const
{
    const float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    const float detail = std::max(0.05f, GetScaledDetail());
    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), detail);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;
    hue_deg = ApplySpatialRainbowHue(hue_deg, coord2, basis, sp, map, time, &grid);
    float p01 = std::fmod(hue_deg / 360.0f, 1.0f);
    if(p01 < 0.0f)
    {
        p01 += 1.0f;
    }
    p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
    return p01 * 360.0f;
}

RGBColor DiscoFlash::SampleFlashField(float x,
                                      float y,
                                      float z,
                                      float nx,
                                      float ny,
                                      float nz,
                                      float time,
                                      const EffectStratumBlend::BandBlendScalars& bb,
                                      float stratum_mot01,
                                      const GridContext3D& grid,
                                      const Vector3D& origin,
                                      const Vector3D& rp)
{
    RGBColor result = ToRGBColor(0, 0, 0);
    float color_cycle = time * GetScaledFrequency() * 12.0f * bb.speed_mul;
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    const float size_m = GetNormalizedSize();

    for(unsigned int i = 0; i < flashes.size(); i++)
    {
        const Flash& f = flashes[i];
        float age = time - f.birth_time;
        if(age < 0.0f) continue;

        float dx = nx - f.nx;
        float dy = ny - f.ny;
        float dz = nz - f.nz;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        float sz = std::max(f.size / tm, 1e-3f);

        float spatial = std::exp(-(dist * dist) / (sz * sz));
        float fade = std::exp(-flash_decay * age);
        float contribution = spatial * fade;

        if(contribution < 0.004f) continue;

        RGBColor flash_color;
        if(UseEffectStripColormap())
        {
            const float ph01 = std::fmod(color_cycle * (1.f / 360.f) + (float)i * 0.017f + f.hue * (1.f / 360.f) +
                                     EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.f,
                                 1.f);
            float p = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
            float hue = std::fmod(p * 360.0f + f.hue * 0.18f, 360.0f);
            hue = ApplyDiscoFlashRoomHue(hue, x, y, z, time, grid, origin, rp);
            flash_color = GetRainbowColor(hue);
        }
        else
        {
            float hue = f.hue + color_cycle + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
            hue = ApplyDiscoFlashRoomHue(hue, x, y, z, time, grid, origin, rp);
            flash_color = GetRainbowColor(hue);
        }
        flash_color = ScaleRGBColor(flash_color, contribution);

        int rr = std::clamp((int)(result & 0xFF)         + (int)(flash_color & 0xFF),         0, 255);
        int rg = std::clamp((int)((result >> 8) & 0xFF)  + (int)((flash_color >> 8) & 0xFF),  0, 255);
        int rb = std::clamp((int)((result >> 16) & 0xFF) + (int)((flash_color >> 16) & 0xFF), 0, 255);
        result = MakeRGBColor(rr, rg, rb);
    }

    return result;
}

RGBColor DiscoFlash::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    Vector3D o = GetEffectOriginGrid(grid);
    Vector3D rot = TransformPointByRotation(x, y, z, o);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, o, time);


    int mode = std::max(0, std::min(flash_mode, MODE_COUNT - 1));
    if(mode == MODE_SPARKLE)
    {
        float detail = std::max(0.05f, GetScaledDetail());
        float rate = GetScaledFrequency();
        float nx = (grid.width  > 1e-5f) ? (x - o.x) / (grid.width  * 0.5f) : 0.0f;
        float ny = (grid.height > 1e-5f) ? (y - o.y) / (grid.height * 0.5f) : 0.0f;
        float nz = (grid.depth  > 1e-5f) ? (z - o.z) / (grid.depth  * 0.5f) : 0.0f;
        unsigned int h = (unsigned int)((nx * 1000 + ny * 2000 + nz * 3000) * 1000);
        h = h * 73856093u ^ (unsigned int)(time * 100.f * bb.speed_mul) * 19349663u;
        h = (h << 13) ^ h;
        float sparkle = ((h & 0xFFFFu) / 65535.0f);
        float phase = fmodf(time * bb.speed_mul * (3.0f + sparkle * 5.0f) + sparkle * 6.28f, 6.28f);
        float intensity = (phase < 1.0f) ? (0.3f + 0.7f * phase) : (phase > 5.28f) ? (1.0f - (phase - 5.28f)) : 1.0f;
        intensity = std::max(0.0f, std::min(1.0f, intensity));
        if(intensity < 0.01f) return ToRGBColor(0, 0, 0);
        float hue = fmodf(sparkle * 360.0f * (0.6f + 0.4f * detail * bb.tight_mul) + time * rate * 12.0f * bb.speed_mul
                           + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f,
                      360.0f);
        if(hue < 0.0f) hue += 360.0f;
        if(UseEffectStripColormap())
        {
            const float size_m = GetNormalizedSize();
            const float ph01 = std::fmod(time * rate * 12.0f * bb.speed_mul * (1.f / 360.f) + sparkle * 0.11f + 1.f, 1.f);
            hue = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                             GetEffectStripColormapRepeats(),
                                             GetEffectStripColormapUnfold(),
                                             GetEffectStripColormapDirectionDeg(),
                                             ph01,
                                             time,
                                             grid,
                                             size_m,
                                             o,
                                             rot) *
                  360.0f;
        }
        hue = ApplyDiscoFlashRoomHue(hue, x, y, z, time, grid, o, rot);
        RGBColor c = GetRainbowColor(hue);
        unsigned char r = (unsigned char)((c & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    TickFlashes(time);

    float nx = (grid.width  > 1e-5f) ? (x - o.x) / (grid.width  * 0.5f) : 0.0f;
    float ny = (grid.height > 1e-5f) ? (y - o.y) / (grid.height * 0.5f) : 0.0f;
    float nz = (grid.depth  > 1e-5f) ? (z - o.z) / (grid.depth  * 0.5f) : 0.0f;

    return SampleFlashField(x, y, z, nx, ny, nz, time, bb, stratum_mot01, grid, o, rot);
}

nlohmann::json DiscoFlash::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["flash_decay"]      = flash_decay;
    j["flash_density"]    = flash_density;
    j["flash_size"]       = flash_size;
    j["onset_threshold"]  = onset_threshold;
    j["flash_mode"]       = flash_mode;
return j;
}

void DiscoFlash::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("flash_decay"))      flash_decay      = settings["flash_decay"].get<float>();
    if(settings.contains("flash_density"))    flash_density    = settings["flash_density"].get<float>();
    if(settings.contains("flash_size"))       flash_size       = settings["flash_size"].get<float>();
    if(settings.contains("onset_threshold"))  onset_threshold  = settings["onset_threshold"].get<float>();
    if(settings.contains("flash_mode") && settings["flash_mode"].is_number_integer())
        flash_mode = std::max(0, std::min(settings["flash_mode"].get<int>(), MODE_COUNT - 1));
    flashes.clear();
    last_tick_time = std::numeric_limits<float>::lowest();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
}

REGISTER_EFFECT_3D(DiscoFlash)
