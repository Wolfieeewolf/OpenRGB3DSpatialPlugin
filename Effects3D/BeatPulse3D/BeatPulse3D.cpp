// SPDX-License-Identifier: GPL-2.0-only
// SPDX-License-Identifier: GPL-2.0-only

#include "BeatPulse3D.h"
#include <algorithm>
#include <cmath>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

float BeatPulse3D::EvaluateIntensity(float amplitude, float time)
{
    amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
        float decay = 0.65f + alpha * 0.25f;
        envelope = std::max(envelope * decay, smoothed);
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
        envelope = amplitude;
    }
    return ApplyAudioIntensity(envelope, audio_settings);
}

BeatPulse3D::BeatPulse3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D BeatPulse3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Beat Pulse";
    info.effect_description = "Global brightness pulses with bass";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void BeatPulse3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

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

    QHBoxLayout* smooth_row = new QHBoxLayout();
    smooth_row->addWidget(new QLabel("Smoothing:"));
    QSlider* smooth_slider = new QSlider(Qt::Horizontal);
    smooth_slider->setRange(0, 99);
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

    QHBoxLayout* falloff_row = new QHBoxLayout();
    falloff_row->addWidget(new QLabel("Falloff:"));
    QSlider* falloff_slider = new QSlider(Qt::Horizontal);
    falloff_slider->setRange(20, 500);
    falloff_slider->setValue((int)(audio_settings.falloff * 100.0f));
    QLabel* falloff_label = new QLabel(QString::number(audio_settings.falloff, 'f', 1));
    falloff_label->setMinimumWidth(36);
    falloff_row->addWidget(falloff_slider);
    falloff_row->addWidget(falloff_label);
    layout->addLayout(falloff_row);

    connect(falloff_slider, &QSlider::valueChanged, this, [this, falloff_label](int v){
        audio_settings.falloff = v / 100.0f;
        falloff_label->setText(QString::number(audio_settings.falloff, 'f', 1));
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

void BeatPulse3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor BeatPulse3D::CalculateColor(float x, float y, float z, float time)
{
    AudioInputManager* audio = AudioInputManager::instance();
    float amplitude = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    float phase = CalculateProgress(time);
    float wave_front = std::fmod(phase, 1.0f);
    if(wave_front < 0.0f)
    {
        wave_front += 1.0f;
    }
    float radial_norm = std::clamp(std::sqrt(x * x + y * y + z * z) / 0.75f, 0.0f, 1.0f);
    float height_norm = std::clamp(0.5f + y, 0.0f, 1.0f);
    float distance = std::fabs(radial_norm - wave_front);
    float pulse = std::exp(-distance * distance * 36.0f);
    float tail = std::exp(-std::max(distance - 0.2f, 0.0f) * 6.0f);

    float energy = std::clamp(intensity * (0.55f + 0.45f * (1.0f - height_norm)) * (0.7f * pulse + 0.3f * tail), 0.0f, 1.0f);

    float gradient_pos = std::clamp(radial_norm, 0.0f, 1.0f);
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, energy);
    // Global brightness is applied by PostProcessColorGrid
    color = ScaleRGBColor(color, (0.25f + 0.75f * energy));

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(wave_front * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

RGBColor BeatPulse3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    AudioInputManager* audio = AudioInputManager::instance();
    float amplitude = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    float phase = CalculateProgress(time);
    float wave_front = std::fmod(phase, 1.0f);
    if(wave_front < 0.0f)
    {
        wave_front += 1.0f;
    }
    float dx = x - grid.center_x;
    float dy = y - grid.center_y;
    float dz = z - grid.center_z;
    float max_radius = 0.5f * std::max({grid.width, grid.height, grid.depth});
    float radial_norm = ComputeRadialNormalized(dx, dy, dz, max_radius);
    float height_norm = NormalizeRange(y, grid.min_y, grid.max_y);
    float distance = std::fabs(radial_norm - wave_front);
    float pulse = std::exp(-distance * distance * 36.0f);
    float tail = std::exp(-std::max(distance - 0.2f, 0.0f) * 6.0f);

    float energy = std::clamp(intensity * (0.55f + 0.45f * (1.0f - height_norm)) * (0.7f * pulse + 0.3f * tail), 0.0f, 1.0f);

    float gradient_pos = std::clamp(radial_norm, 0.0f, 1.0f);
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, energy);
    // Global brightness is applied by PostProcessColorGrid
    color = ScaleRGBColor(color, (0.25f + 0.75f * energy));

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(wave_front * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

REGISTER_EFFECT_3D(BeatPulse3D)

nlohmann::json BeatPulse3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    return j;
}

void BeatPulse3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    envelope = 0.0f;
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}
