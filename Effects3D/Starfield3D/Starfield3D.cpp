// SPDX-License-Identifier: GPL-2.0-only

#include "Starfield3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

REGISTER_EFFECT_3D(Starfield3D);

static float hash_float(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

Starfield3D::Starfield3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D Starfield3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Starfield";
    info.effect_description = "Moving stars (Mega-Cube style): points in 3D, move along Z, wrap, rotate";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 15.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void Starfield3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Star count:"), 0, 0);
    QSlider* count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(40, 120);
    count_slider->setValue(num_stars);
    QLabel* count_label = new QLabel(QString::number(num_stars));
    count_label->setMinimumWidth(36);
    layout->addWidget(count_slider, 0, 1);
    layout->addWidget(count_label, 0, 2);
    connect(count_slider, &QSlider::valueChanged, this, [this, count_label](int v){
        num_stars = v;
        if(count_label) count_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    layout->addWidget(new QLabel("Star size:"), 1, 0);
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(2, 100);
    size_slider->setValue((int)(star_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(star_size * 100)) + "%");
    size_label->setMinimumWidth(36);
    layout->addWidget(size_slider, 1, 1);
    layout->addWidget(size_label, 1, 2);
    connect(size_slider, &QSlider::valueChanged, this, [this, size_label](int v){
        star_size = v / 100.0f;
        if(size_label) size_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    layout->addWidget(new QLabel("Drift:"), 2, 0);
    QSlider* drift_slider = new QSlider(Qt::Horizontal);
    drift_slider->setRange(0, 100);
    drift_slider->setValue((int)(drift_amount * 100.0f));
    QLabel* drift_label = new QLabel(QString::number((int)(drift_amount * 100)) + "%");
    drift_label->setMinimumWidth(36);
    layout->addWidget(drift_slider, 2, 1);
    layout->addWidget(drift_label, 2, 2);
    connect(drift_slider, &QSlider::valueChanged, this, [this, drift_label](int v){
        drift_amount = v / 100.0f;
        if(drift_label) drift_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    layout->addWidget(new QLabel("Twinkle:"), 3, 0);
    QSlider* twinkle_slider = new QSlider(Qt::Horizontal);
    twinkle_slider->setRange(0, 100);
    twinkle_slider->setValue((int)(twinkle_speed * 100.0f));
    QLabel* twinkle_label = new QLabel(QString::number((int)(twinkle_speed * 100)) + "%");
    twinkle_label->setMinimumWidth(36);
    layout->addWidget(twinkle_slider, 3, 1);
    layout->addWidget(twinkle_label, 3, 2);
    connect(twinkle_slider, &QSlider::valueChanged, this, [this, twinkle_label](int v){
        twinkle_speed = v / 100.0f;
        if(twinkle_label) twinkle_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    layout->addWidget(new QLabel("Speed mult:"), 4, 0);
    QSlider* speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(50, 200);
    speed_slider->setValue((int)(star_speed_mult * 100.0f));
    QLabel* speed_label = new QLabel(QString::number((int)(star_speed_mult * 100)) + "%");
    speed_label->setMinimumWidth(36);
    layout->addWidget(speed_slider, 4, 1);
    layout->addWidget(speed_label, 4, 2);
    connect(speed_slider, &QSlider::valueChanged, this, [this, speed_label](int v){
        star_speed_mult = v / 100.0f;
        if(speed_label) speed_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void Starfield3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Starfield3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor Starfield3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float half = 0.5f * std::max(grid.width, std::max(grid.height, grid.depth)) * GetNormalizedScale();
    if(half < 1e-5f) half = 1.0f;
    float speed_mult = std::max(0.5f, std::min(2.0f, star_speed_mult));
    float speed = GetScaledSpeed() * 0.5f * speed_mult;
    float sigma = std::max(star_size * 0.5f, 0.02f);
    float sigma_sq = sigma * sigma * half * half;
    const float d2_cutoff = 9.0f * sigma_sq;

    const int n_stars = std::max(1, std::min(200, num_stars));

    // Recompute star positions only when time changes (once per frame) — major FPS win
    if(star_positions_cached.size() != (size_t)n_stars || fabsf(time - star_cache_time) > 0.001f)
    {
        star_cache_time = time;
        star_cache_count = n_stars;
        star_positions_cached.resize(n_stars);
        float drift = std::max(0.0f, std::min(1.0f, drift_amount));
        for(int i = 0; i < n_stars; i++)
        {
            float sx = hash_float((unsigned int)i, 1u);
            float sy = hash_float((unsigned int)i, 2u);
            float sz0 = hash_float((unsigned int)i, 3u);
            float sz = fmodf(sz0 + time * speed, 2.0f) - 1.0f;
            float sx_d = sx + drift * 0.3f * sinf(time * 2.0f + (float)i * 0.1f);
            float sy_d = sy + drift * 0.3f * cosf(time * 1.7f + (float)i * 0.07f);
            Vector3D star_local{sx_d * half + origin.x, sy_d * half + origin.y, sz * half + origin.z};
            star_positions_cached[i] = TransformPointByRotation(star_local.x, star_local.y, star_local.z, origin);
        }
    }

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    float sum_intensity = 0.0f;

    for(int i = 0; i < n_stars; i++)
    {
        const Vector3D& star_rot = star_positions_cached[i];
        float dx = x - star_rot.x, dy = y - star_rot.y, dz = z - star_rot.z;
        float d2 = dx*dx + dy*dy + dz*dz;
        if(d2 > d2_cutoff) continue;
        float intensity = expf(-d2 / sigma_sq);
        float twinkle = std::max(0.0f, std::min(1.0f, twinkle_speed));
        if(twinkle > 0.01f)
            intensity *= 0.5f + 0.5f * sinf(time * (3.0f + twinkle * 5.0f) + (float)i);
        if(intensity < 0.01f) continue;

        float hue = fmodf((float)i * 2.0f + time * 30.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition((float)i / (float)n_stars);
        sum_r += ((c & 0xFF) / 255.0f) * intensity;
        sum_g += (((c >> 8) & 0xFF) / 255.0f) * intensity;
        sum_b += (((c >> 16) & 0xFF) / 255.0f) * intensity;
        sum_intensity += intensity;
    }

    if(sum_intensity < 1e-6f) return 0x00000000;
    float scale = 1.0f / (sum_intensity > 1.0f ? sum_intensity : 1.0f);
    scale = fminf(1.0f, scale * 1.5f);
    int r_ = std::min(255, std::max(0, (int)(sum_r * scale * 255.0f)));
    int g_ = std::min(255, std::max(0, (int)(sum_g * scale * 255.0f)));
    int b_ = std::min(255, std::max(0, (int)(sum_b * scale * 255.0f)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json Starfield3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["star_size"] = star_size;
    j["num_stars"] = num_stars;
    j["drift_amount"] = drift_amount;
    j["twinkle_speed"] = twinkle_speed;
    j["star_speed_mult"] = star_speed_mult;
    return j;
}

void Starfield3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("star_size") && settings["star_size"].is_number())
        star_size = std::max(0.02f, std::min(1.0f, settings["star_size"].get<float>()));
    if(settings.contains("num_stars") && settings["num_stars"].is_number())
        num_stars = std::max(40, std::min(200, settings["num_stars"].get<int>()));
    if(settings.contains("drift_amount") && settings["drift_amount"].is_number())
        drift_amount = std::max(0.0f, std::min(1.0f, settings["drift_amount"].get<float>()));
    if(settings.contains("twinkle_speed") && settings["twinkle_speed"].is_number())
        twinkle_speed = std::max(0.0f, std::min(1.0f, settings["twinkle_speed"].get<float>()));
    if(settings.contains("star_speed_mult") && settings["star_speed_mult"].is_number())
        star_speed_mult = std::max(0.5f, std::min(2.0f, settings["star_speed_mult"].get<float>()));
}
