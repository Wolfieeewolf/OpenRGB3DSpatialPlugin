// SPDX-License-Identifier: GPL-2.0-only

#include "DiscoFlash3D.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

DiscoFlash3D::DiscoFlash3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D DiscoFlash3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Disco Flash";
    info.effect_description = "Beat-triggered random colour flashes — chaotic disco effect";
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
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_fps_control = false;
    info.show_axis_control = false;
    info.show_color_controls = false; // uses random hues
    return info;
}

void DiscoFlash3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    // Hz range
    QHBoxLayout* hz_row = new QHBoxLayout();
    hz_row->addWidget(new QLabel("Low Hz:"));
    QSpinBox* low_spin = new QSpinBox();
    low_spin->setRange(1, 20000);
    low_spin->setValue(audio_settings.low_hz);
    hz_row->addWidget(low_spin);
    hz_row->addWidget(new QLabel("High Hz:"));
    QSpinBox* high_spin = new QSpinBox();
    high_spin->setRange(1, 20000);
    high_spin->setValue(audio_settings.high_hz);
    hz_row->addWidget(high_spin);
    layout->addLayout(hz_row);

    connect(low_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        audio_settings.low_hz = v;
        emit ParametersChanged();
    });
    connect(high_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        audio_settings.high_hz = v;
        emit ParametersChanged();
    });

    // Flash density
    QHBoxLayout* density_row = new QHBoxLayout();
    density_row->addWidget(new QLabel("Density:"));
    QSlider* density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(5, 100);
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

    // Flash size
    QHBoxLayout* size_row = new QHBoxLayout();
    size_row->addWidget(new QLabel("Flash Size:"));
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(3, 60);
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

    // Decay
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

    // Threshold
    QHBoxLayout* thresh_row = new QHBoxLayout();
    thresh_row->addWidget(new QLabel("Threshold:"));
    QSlider* thresh_slider = new QSlider(Qt::Horizontal);
    thresh_slider->setRange(10, 95);
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
    boost_slider->setRange(50, 400);
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

void DiscoFlash3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

void DiscoFlash3D::TickFlashes(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-4f)
    {
        return;
    }
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    float raw = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    onset_smoothed = 0.4f * onset_smoothed + 0.6f * raw;

    if(onset_hold > 0.0f)
    {
        onset_hold = std::max(0.0f, onset_hold - dt);
    }

    if(onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        // Number of flashes to spawn scales with density and audio strength
        float strength = std::clamp(onset_smoothed * audio_settings.peak_boost, 0.0f, 1.0f);
        int count = std::max(1, (int)(flash_density * 12.0f * strength));

        std::uniform_real_distribution<float> pos_dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> hue_dist(0.0f, 360.0f);
        std::uniform_real_distribution<float> size_dist(0.5f, 1.5f);

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

    // Remove dead flashes
    flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
        [&](const Flash& f) {
            float age = time - f.birth_time;
            return std::exp(-flash_decay * age) < 0.004f;
        }), flashes.end());
}

RGBColor DiscoFlash3D::SampleFlashField(float nx, float ny, float nz, float time) const
{
    RGBColor result = ToRGBColor(0, 0, 0);

    for(const Flash& f : flashes)
    {
        float age = time - f.birth_time;
        if(age < 0.0f) continue;

        float dx = nx - f.nx;
        float dy = ny - f.ny;
        float dz = nz - f.nz;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        float sz = std::max(f.size, 1e-3f);

        float spatial = std::exp(-(dist * dist) / (sz * sz));
        float fade = std::exp(-flash_decay * age);
        float contribution = spatial * fade;

        if(contribution < 0.004f) continue;

        RGBColor flash_color = GetRainbowColor(f.hue);
        flash_color = ScaleRGBColor(flash_color, contribution);

        int rr = std::clamp((int)(result & 0xFF)         + (int)(flash_color & 0xFF),         0, 255);
        int rg = std::clamp((int)((result >> 8) & 0xFF)  + (int)((flash_color >> 8) & 0xFF),  0, 255);
        int rb = std::clamp((int)((result >> 16) & 0xFF) + (int)((flash_color >> 16) & 0xFF), 0, 255);
        result = MakeRGBColor(rr, rg, rb);
    }

    return result;
}

RGBColor DiscoFlash3D::CalculateColor(float x, float y, float z, float time)
{
    TickFlashes(time);
    // Normalise to [-1,1] roughly (using a 2-unit world)
    return SampleFlashField(x * 0.5f, y * 0.5f, z * 0.5f, time);
}

RGBColor DiscoFlash3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    TickFlashes(time);

    float nx = (grid.width  > 1e-5f) ? (x - grid.center_x) / (grid.width  * 0.5f) : 0.0f;
    float ny = (grid.height > 1e-5f) ? (y - grid.center_y) / (grid.height * 0.5f) : 0.0f;
    float nz = (grid.depth  > 1e-5f) ? (z - grid.center_z) / (grid.depth  * 0.5f) : 0.0f;

    return SampleFlashField(nx, ny, nz, time);
}

nlohmann::json DiscoFlash3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["flash_decay"]      = flash_decay;
    j["flash_density"]    = flash_density;
    j["flash_size"]       = flash_size;
    j["onset_threshold"]  = onset_threshold;
    return j;
}

void DiscoFlash3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("flash_decay"))      flash_decay      = settings["flash_decay"].get<float>();
    if(settings.contains("flash_density"))    flash_density    = settings["flash_density"].get<float>();
    if(settings.contains("flash_size"))       flash_size       = settings["flash_size"].get<float>();
    if(settings.contains("onset_threshold"))  onset_threshold  = settings["onset_threshold"].get<float>();
    flashes.clear();
    last_tick_time = std::numeric_limits<float>::lowest();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
}

REGISTER_EFFECT_3D(DiscoFlash3D)
