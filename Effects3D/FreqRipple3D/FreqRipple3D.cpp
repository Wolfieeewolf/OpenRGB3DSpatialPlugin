// SPDX-License-Identifier: GPL-2.0-only

#include "FreqRipple3D.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

FreqRipple3D::FreqRipple3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
    ripples.reserve(64);
}

EffectInfo3D FreqRipple3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Frequency Ripple";
    info.effect_description = "Beat-triggered expanding ring from origin";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
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
    info.show_color_controls = true;
    return info;
}

void FreqRipple3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* speed_row = new QHBoxLayout();
    speed_row->addWidget(new QLabel("Expand Speed:"));
    QSlider* speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(20, 400);
    speed_slider->setValue((int)(expand_speed * 100.0f));
    QLabel* speed_label = new QLabel(QString::number(expand_speed, 'f', 1));
    speed_label->setMinimumWidth(36);
    speed_row->addWidget(speed_slider);
    speed_row->addWidget(speed_label);
    layout->addLayout(speed_row);

    connect(speed_slider, &QSlider::valueChanged, this, [this, speed_label](int v){
        expand_speed = v / 100.0f;
        speed_label->setText(QString::number(expand_speed, 'f', 1));
        emit ParametersChanged();
    });

    QHBoxLayout* width_row = new QHBoxLayout();
    width_row->addWidget(new QLabel("Ring Width:"));
    QSlider* width_slider = new QSlider(Qt::Horizontal);
    width_slider->setRange(2, 50);
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

void FreqRipple3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

void FreqRipple3D::TickRipples(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-4f)
    {
        return;
    }
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    float onset_raw = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
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

RGBColor FreqRipple3D::ComputeRippleColor(float dist_norm, float time) const
{
    RGBColor result = ToRGBColor(0, 0, 0);

    for(unsigned int i = 0; i < ripples.size(); i++)
    {
        const Ripple& r = ripples[i];
        float age = time - r.birth_time;
        if(age < 0.0f) continue;

        float front = expand_speed * age;
        float ring_dist = std::fabs(dist_norm - front);
        float half_w = std::max(trail_width * 0.5f, 1e-3f);

        float ring_bright = std::exp(-(ring_dist * ring_dist) / (half_w * half_w));

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

RGBColor FreqRipple3D::CalculateColor(float x, float y, float z, float time)
{
    TickRipples(time);

    Vector3D origin = GetEffectOrigin();
    float dx = x - origin.x;
    float dy = y - origin.y;
    float dz = z - origin.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    RGBColor color = ComputeRippleColor(dist, time);

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(CalculateProgress(time) * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

RGBColor FreqRipple3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    TickRipples(time);

    Vector3D origin = GetEffectOriginGrid(grid);
    float dx = x - origin.x;
    float dy = y - origin.y;
    float dz = z - origin.z;
    float max_radius = 0.5f * std::max({grid.width, grid.height, grid.depth});
    float dist_norm = std::clamp(std::sqrt(dx*dx + dy*dy + dz*dz) / std::max(max_radius, 1e-5f), 0.0f, 2.0f);

    RGBColor color = ComputeRippleColor(dist_norm, time);

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(dist_norm * 180.0f + CalculateProgress(time) * 50.0f)
        : GetColorAtPosition(std::min(dist_norm * 0.5f, 1.0f));
    return ModulateRGBColors(color, user_color);
}

nlohmann::json FreqRipple3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["expand_speed"]     = expand_speed;
    j["trail_width"]      = trail_width;
    j["decay_rate"]       = decay_rate;
    j["onset_threshold"]  = onset_threshold;
    return j;
}

void FreqRipple3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("expand_speed"))    expand_speed    = settings["expand_speed"].get<float>();
    if(settings.contains("trail_width"))     trail_width     = settings["trail_width"].get<float>();
    if(settings.contains("decay_rate"))      decay_rate      = settings["decay_rate"].get<float>();
    if(settings.contains("onset_threshold")) onset_threshold = settings["onset_threshold"].get<float>();
    ripples.clear();
    last_tick_time = std::numeric_limits<float>::lowest();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
}

REGISTER_EFFECT_3D(FreqRipple3D)
