// SPDX-License-Identifier: GPL-2.0-only

#include "AudioLevel.h"
#include "Colors.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

float AudioLevel::EvaluateIntensity(float amplitude, float time)
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

AudioLevel::AudioLevel(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioLevel::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Audio Level";
    info.effect_description = "Level drives a 3D fill surface (like a water level) with optional wave on the boundary";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    return info;
}

void AudioLevel::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

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

    QHBoxLayout* wave_row = new QHBoxLayout();
    wave_row->addWidget(new QLabel("Boundary wave:"));
    QSlider* wave_slider = new QSlider(Qt::Horizontal);
    wave_slider->setRange(0, 100);
    wave_slider->setValue((int)(wave_amount * 100.0f));
    QLabel* wave_label = new QLabel(QString::number((int)(wave_amount * 100)) + "%");
    wave_label->setMinimumWidth(40);
    wave_row->addWidget(wave_slider);
    wave_row->addWidget(wave_label);
    layout->addLayout(wave_row);
    connect(wave_slider, &QSlider::valueChanged, this, [this, wave_label](int v){
        wave_amount = v / 100.0f;
        wave_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* edge_row = new QHBoxLayout();
    edge_row->addWidget(new QLabel("Edge softness:"));
    QSlider* edge_slider = new QSlider(Qt::Horizontal);
    edge_slider->setRange(2, 100);
    edge_slider->setValue((int)(edge_soft * 100.0f));
    QLabel* edge_label = new QLabel(QString::number((int)(edge_soft * 100)) + "%");
    edge_label->setMinimumWidth(40);
    edge_row->addWidget(edge_slider);
    edge_row->addWidget(edge_label);
    layout->addLayout(edge_row);
    connect(edge_slider, &QSlider::valueChanged, this, [this, edge_label](int v){
        edge_soft = v / 100.0f;
        edge_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
}

void AudioLevel::UpdateParams(SpatialEffectParams& /*params*/)
{
}


RGBColor AudioLevel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float amplitude = AudioInputManager::instance()->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float fill_level = EvaluateIntensity(amplitude, time);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float ax = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    float ay = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    float az = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);
    int fax = GetPathAxis();
    float axis_pos = (fax == 0) ? ax : ((fax == 1) ? ay : az);
    float axis_other = (fax == 0) ? ay : ((fax == 1) ? ax : ay);
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float wave = wave_amount * std::sin(time * 4.0f * std::max(0.2f, GetScaledFrequency() * 0.15f) + axis_other * 6.283185f * (0.6f + 0.4f * detail));
    float fill_boundary = std::clamp(fill_level + wave, 0.0f, 1.0f);
    float edge = std::max(edge_soft, 0.01f) / std::max(0.35f, size_m);
    float blend = std::clamp((fill_boundary - axis_pos) / edge + 0.5f, 0.0f, 1.0f);
    float intensity = blend;

    float dx = rotated_pos.x - origin.x, dy = rotated_pos.y - origin.y, dz = rotated_pos.z - origin.z;
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float radial_norm = ComputeRadialNormalized(dx, dy, dz, max_radius);
    float gradient_pos = std::clamp(0.65f * axis_pos + 0.35f * (1.0f - radial_norm), 0.0f, 1.0f);
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    color = ScaleRGBColor(color, (0.35f + 0.65f * intensity));
    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(gradient_pos * 360.0f + CalculateProgress(time) * 40.0f + time * GetScaledFrequency() * 12.0f)
        : GetColorAtPosition(gradient_pos);
    return ModulateRGBColors(color, user_color);
}

REGISTER_EFFECT_3D(AudioLevel)

nlohmann::json AudioLevel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["wave_amount"] = wave_amount;
    j["edge_soft"] = edge_soft;
    return j;
}

void AudioLevel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("wave_amount")) wave_amount = std::clamp(settings["wave_amount"].get<float>(), 0.0f, 0.5f);
    if(settings.contains("edge_soft"))   edge_soft   = std::clamp(settings["edge_soft"].get<float>(), 0.02f, 0.5f);
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}
