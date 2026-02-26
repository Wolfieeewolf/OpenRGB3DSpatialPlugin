// SPDX-License-Identifier: GPL-2.0-only

#include "BandScan3D.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace
{
    inline int MapHzToBandIndex(float hz, int bands, float f_min, float f_max)
    {
        float clamped = std::clamp(hz, f_min, f_max);
        float denom = std::log(f_max / f_min);
        if(std::abs(denom) < 1e-6f)
        {
            return 0;
        }
        float t = std::log(clamped / f_min) / denom;
        int idx = static_cast<int>(std::floor(t * bands));
        return std::clamp(idx, 0, bands - 1);
    }
}

BandScan3D::BandScan3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
    RefreshBandRange();
}

EffectInfo3D BandScan3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Band Scan";
    info.effect_description = "Single moving band of spectrum energy across the room";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_color_controls = true;
    return info;
}

void BandScan3D::SetupCustomUI(QWidget* parent)
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

void BandScan3D::UpdateParams(SpatialEffectParams& /*params*/)
{
    RefreshBandRange();
}

RGBColor BandScan3D::CalculateColor(float x, float y, float z, float time)
{
    EnsureSpectrumCache(time);
    float axis_pos = ResolveCoordinateNormalized(nullptr, x, y, z);
    float height_norm = ResolveHeightNormalized(nullptr, x, y, z);
    float radial_norm = ResolveRadialNormalized(nullptr, x, y, z);
    bool rainbow_mode = GetRainbowMode();
    RGBColor axis_color = rainbow_mode
        ? GetRainbowColor(axis_pos * 360.0f)
        : GetColorAtPosition(std::clamp(axis_pos, 0.0f, 1.0f));
    return ComposeColor(axis_pos, height_norm, radial_norm, time, 1.0f, axis_color, rainbow_mode);
}

RGBColor BandScan3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    EnsureSpectrumCache(time);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float axis_pos = ResolveCoordinateNormalized(&grid, rotated_pos.x, rotated_pos.y, rotated_pos.z);
    float height_norm = ResolveHeightNormalized(&grid, rotated_pos.x, rotated_pos.y, rotated_pos.z);
    float radial_norm = ResolveRadialNormalized(&grid, rotated_pos.x, rotated_pos.y, rotated_pos.z);
    bool rainbow_mode = GetRainbowMode();
    RGBColor axis_color = rainbow_mode
        ? GetRainbowColor(axis_pos * 360.0f)
        : GetColorAtPosition(std::clamp(axis_pos, 0.0f, 1.0f));
    return ComposeColor(axis_pos, height_norm, radial_norm, time, 1.0f, axis_color, rainbow_mode);
}

nlohmann::json BandScan3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    return j;
}

void BandScan3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);

    RefreshBandRange();
    last_sample_time = std::numeric_limits<float>::lowest();
}

void BandScan3D::RefreshBandRange()
{
    AudioInputManager* audio = AudioInputManager::instance();
    int total_bands = audio->getBandsCount();
    if(total_bands <= 0)
    {
        total_bands = static_cast<int>(audio->getBands().size());
        if(total_bands <= 0)
        {
            total_bands = 16;
        }
    }

    float sample_rate = static_cast<float>(audio->getSampleRate());
    if(sample_rate <= 0.0f)
    {
        sample_rate = 48000.0f;
    }
    int fft_size = audio->getFFTSize();
    if(fft_size <= 0)
    {
        fft_size = 1024;
    }

    float f_min = std::max(1.0f, sample_rate / std::max(1, fft_size));
    float f_max = sample_rate * 0.5f;
    if(f_max <= f_min)
    {
        f_max = f_min + 1.0f;
    }

    int start = MapHzToBandIndex((float)audio_settings.low_hz, total_bands, f_min, f_max);
    int end   = MapHzToBandIndex((float)audio_settings.high_hz, total_bands, f_min, f_max);
    if(end < start)
    {
        std::swap(end, start);
    }

    band_start = std::clamp(start, 0, total_bands - 1);
    band_end   = std::clamp(end, band_start, total_bands - 1);

    int count = std::max(1, band_end - band_start + 1);
    if(static_cast<int>(smoothed_bands.size()) != count)
    {
        smoothed_bands.assign(count, 0.0f);
    }
}

void BandScan3D::EnsureSpectrumCache(float time)
{
    const float epsilon = 1e-4f;
    if(last_sample_time != std::numeric_limits<float>::lowest())
    {
        if(std::fabs(time - last_sample_time) <= epsilon)
        {
            return;
        }
    }

    last_sample_time = time;
    UpdateSmoothedBands(AudioInputManager::instance()->getBands());
}

void BandScan3D::UpdateSmoothedBands(const std::vector<float>& spectrum)
{
    RefreshBandRange();
    int count = band_end - band_start + 1;
    if(count <= 0)
    {
        smoothed_bands.clear();
        return;
    }
    if(static_cast<int>(smoothed_bands.size()) != count)
    {
        smoothed_bands.assign(count, 0.0f);
    }

    float smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    for(int i = 0; i < count; ++i)
    {
        int idx = band_start + i;
        float sample = 0.0f;
        if(idx >= 0 && idx < static_cast<int>(spectrum.size()))
        {
            sample = std::clamp(spectrum[idx], 0.0f, 1.0f);
        }
        smoothed_bands[i] = smooth * smoothed_bands[i] + (1.0f - smooth) * sample;
    }
}

static float normalize_linear(float value, float min, float max)
{
    float range = max - min;
    if(range <= 1e-5f) return 0.0f;
    return (value - min) / range;
}

float BandScan3D::ResolveCoordinateNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    (void)y;
    (void)z;
    float normalized = 0.0f;
    if(grid)
    {
        normalized = normalize_linear(x, grid->min_x, grid->max_x);
    }
    else
    {
        normalized = std::fmod(std::fabs(x), 1.0f);
    }

    normalized = std::clamp(normalized, 0.0f, 1.0f);
    return normalized;
}

float BandScan3D::ResolveHeightNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    (void)x;
    (void)z;
    if(grid)
    {
        return NormalizeRange(y, grid->min_y, grid->max_y);
    }
    else
    {
        return std::clamp(0.5f + y, 0.0f, 1.0f);
    }
}

float BandScan3D::ResolveRadialNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    if(grid)
    {
        float dx = x - grid->center_x;
        float dy = y - grid->center_y;
        float dz = z - grid->center_z;
        float max_radius = 0.5f * std::max({grid->width, grid->height, grid->depth});
        return ComputeRadialNormalized(dx, dy, dz, max_radius);
    }
    return std::clamp(std::sqrt(x * x + y * y + z * z) / 0.75f, 0.0f, 1.0f);
}

float BandScan3D::WrapDistance(float a, float b, int modulo) const
{
    float diff = std::fabs(a - b);
    if(modulo <= 0)
    {
        return diff;
    }
    float wrapped = std::fmod(diff, static_cast<float>(modulo));
    return std::min(wrapped, static_cast<float>(modulo) - wrapped);
}

RGBColor BandScan3D::ComposeColor(float axis_pos, float height_norm, float radial_norm, float time, float brightness, const RGBColor& axis_color, bool rainbow_mode) const
{
    if(smoothed_bands.empty())
    {
        (void)brightness;
        RGBColor base = ComposeAudioGradientColor(audio_settings, axis_pos, 0.0f);
        return ModulateRGBColors(base, axis_color);
    }

    int count = static_cast<int>(smoothed_bands.size());
    float scaled = axis_pos * count;
    int idx_local = std::clamp(static_cast<int>(std::floor(scaled)), 0, count - 1);
    float frac = scaled - std::floor(scaled);
    int idx_next = std::min(idx_local + 1, count - 1);
    float band_value = smoothed_bands[idx_local] + (smoothed_bands[idx_next] - smoothed_bands[idx_local]) * frac;
    band_value = std::clamp(band_value, 0.0f, 1.0f);

    float phase = CalculateProgress(time);
    float scan_phase = std::fmod(phase, 1.0f);
    if(scan_phase < 0.0f)
    {
        scan_phase += 1.0f;
    }
    float scan_index = scan_phase * count;
    float distance = WrapDistance(scaled, scan_index, count);
    float highlight = std::exp(-distance * 1.35f);
    float trail = std::exp(-std::max(distance - 0.6f, 0.0f) * 2.5f);

    float height_profile = std::pow(std::clamp(height_norm, 0.0f, 1.0f), 1.3f);
    float radial_profile = std::clamp(1.0f - radial_norm, 0.0f, 1.0f);
    float energy = band_value * (0.55f + 0.45f * height_profile) * (0.45f + 0.55f * radial_profile);
    energy *= (0.65f * highlight + 0.35f * trail);
    energy = std::clamp(energy, 0.0f, 1.0f);
    float intensity = ApplyAudioIntensity(energy, audio_settings);

    float gradient_pos = (count > 1) ? (float)idx_local / (float)(count - 1) : axis_pos;
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    color = ScaleRGBColor(color, (0.35f + 0.65f * intensity));

    RGBColor modulation = axis_color;
    if(rainbow_mode)
    {
        auto* mutable_self = const_cast<BandScan3D*>(this);
        modulation = mutable_self->GetRainbowColor(scan_phase * 360.0f);
    }

    color = ModulateRGBColors(color, modulation);
    return color;
}

REGISTER_EFFECT_3D(BandScan3D)
