// SPDX-License-Identifier: GPL-2.0-only

#include "Starfield.h"
#include "EffectHelpers.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(Starfield);

const char* Starfield::ModeName(int m)
{
    switch(m) { case MODE_STARFIELD: return "Starfield"; case MODE_TWINKLE: return "Twinkle"; default: return "Starfield"; }
}

static float hash_float(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

Starfield::Starfield(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D Starfield::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Starfield";
    info.effect_description =
        "Moving stars (Mega-Cube style): points in 3D, move along Z, wrap, rotate; optional floor/mid/ceiling band tuning";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 15.0f;
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
    return info;
}

void Starfield::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);
    int row = 0;
    layout->addWidget(new QLabel("Mode:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(this->mode, MODE_COUNT - 1)));
    mode_combo->setToolTip("Motion style for star points. Size and drift still apply in both modes.");
    mode_combo->setItemData(0, "Classic drift along depth with wrap—good volumetric read.", Qt::ToolTipRole);
    mode_combo->setItemData(1, "Stars brighten and dim in place; less directional motion.", Qt::ToolTipRole);
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        this->mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Star count:"), row, 0);
    QSlider* count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(40, 120);
    count_slider->setToolTip("Number of star points simulated in the field.");
    count_slider->setValue(num_stars);
    QLabel* count_label = new QLabel(QString::number(num_stars));
    count_label->setMinimumWidth(36);
    layout->addWidget(count_slider, row, 1);
    layout->addWidget(count_label, row, 2);
    connect(count_slider, &QSlider::valueChanged, this, [this, count_label](int v){
        num_stars = v;
        if(count_label) count_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Star size:"), row, 0);
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(2, 100);
    size_slider->setToolTip("Apparent size of each star streak or point.");
    size_slider->setValue((int)(star_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(star_size * 100)) + "%");
    size_label->setMinimumWidth(36);
    layout->addWidget(size_slider, row, 1);
    layout->addWidget(size_label, row, 2);
    connect(size_slider, &QSlider::valueChanged, this, [this, size_label](int v){
        star_size = v / 100.0f;
        if(size_label) size_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Drift:"), row, 0);
    QSlider* drift_slider = new QSlider(Qt::Horizontal);
    drift_slider->setRange(0, 100);
    drift_slider->setValue((int)(drift_amount * 100.0f));
    QLabel* drift_label = new QLabel(QString::number((int)(drift_amount * 100)) + "%");
    drift_label->setMinimumWidth(36);
    layout->addWidget(drift_slider, row, 1);
    layout->addWidget(drift_label, row, 2);
    connect(drift_slider, &QSlider::valueChanged, this, [this, drift_label](int v){
        drift_amount = v / 100.0f;
        if(drift_label) drift_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Twinkle:"), row, 0);
    QSlider* twinkle_slider = new QSlider(Qt::Horizontal);
    twinkle_slider->setRange(0, 100);
    twinkle_slider->setToolTip("Brightness modulation; strongest in Twinkle mode, subtle shimmer in Starfield when raised.");
    twinkle_slider->setValue((int)(twinkle_speed * 100.0f));
    QLabel* twinkle_label = new QLabel(QString::number((int)(twinkle_speed * 100)) + "%");
    twinkle_label->setMinimumWidth(36);
    layout->addWidget(twinkle_slider, row, 1);
    layout->addWidget(twinkle_label, row, 2);
    connect(twinkle_slider, &QSlider::valueChanged, this, [this, twinkle_label](int v){
        twinkle_speed = v / 100.0f;
        if(twinkle_label) twinkle_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    outer->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Starfield::OnStratumBandChanged);
    OnStratumBandChanged();
    AddWidgetToParent(w, parent);
}

void Starfield::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void Starfield::UpdateParams(SpatialEffectParams& params) { (void)params; }


RGBColor Starfield::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);
    const bool strat_on = (stratum_layout_mode == 1);

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float h_scale = std::max({e.hw, e.hh, e.hd});
    float speed_base = GetScaledSpeed() * 0.5f;
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float sigma = std::max(star_size * 0.5f * size_m / std::max(0.25f, strat_on ? bb.tight_mul : 1.0f), 0.02f);
    float sigma_sq = sigma * sigma * h_scale * h_scale;
    const float d2_cutoff = 9.0f * sigma_sq;
    float color_cycle = time * GetScaledFrequency() * 12.0f;
    if(strat_on)
    {
        color_cycle = color_cycle * bb.speed_mul + bb.phase_deg;
    }

    const int n_stars = std::max(1, std::min(200, num_stars));

    if(!strat_on)
    {
        if(star_positions_cached.size() != (size_t)n_stars || fabsf(time - star_cache_time) > 0.001f)
        {
            star_cache_time = time;
            star_cache_count = n_stars;
            star_positions_cached.resize(n_stars);
            float drift = std::max(0.0f, std::min(1.0f, drift_amount));
            float margin = 3.0f * sigma * h_scale;
            float min_x = 1e9f, min_y = 1e9f, min_z = 1e9f;
            float max_x = -1e9f, max_y = -1e9f, max_z = -1e9f;
            for(int i = 0; i < n_stars; i++)
            {
                float sx = hash_float((unsigned int)i, 1u);
                float sy = hash_float((unsigned int)i, 2u);
                float sz0 = hash_float((unsigned int)i, 3u);
                float sz = fmodf(sz0 + time * speed_base, 2.0f) - 1.0f;
                float sx_d = sx + drift * 0.3f * sinf(time * 2.0f + (float)i * 0.1f);
                float sy_d = sy + drift * 0.3f * cosf(time * 1.7f + (float)i * 0.07f);
                Vector3D star_local{sx_d * e.hw + origin.x, sy_d * e.hh + origin.y, sz * e.hd + origin.z};
                Vector3D rot = TransformPointByRotation(star_local.x, star_local.y, star_local.z, origin);
                star_positions_cached[i] = rot;
                if(rot.x < min_x) min_x = rot.x; if(rot.x > max_x) max_x = rot.x;
                if(rot.y < min_y) min_y = rot.y; if(rot.y > max_y) max_y = rot.y;
                if(rot.z < min_z) min_z = rot.z; if(rot.z > max_z) max_z = rot.z;
            }
            star_aabb_min_x = min_x - margin; star_aabb_max_x = max_x + margin;
            star_aabb_min_y = min_y - margin; star_aabb_max_y = max_y + margin;
            star_aabb_min_z = min_z - margin; star_aabb_max_z = max_z + margin;
        }

        if(x < star_aabb_min_x || x > star_aabb_max_x ||
           y < star_aabb_min_y || y > star_aabb_max_y ||
           z < star_aabb_min_z || z > star_aabb_max_z)
            return 0x00000000;
    }

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    float sum_intensity = 0.0f;
    float drift = std::max(0.0f, std::min(1.0f, drift_amount));
    const float twinkle_time_scale = strat_on ? bb.speed_mul : 1.0f;

    for(int i = 0; i < n_stars; i++)
    {
        Vector3D star_rot;
        if(strat_on)
        {
            float sx = hash_float((unsigned int)i, 1u);
            float sy = hash_float((unsigned int)i, 2u);
            float sz0 = hash_float((unsigned int)i, 3u);
            float sz = fmodf(sz0 + time * speed_base * bb.speed_mul, 2.0f) - 1.0f;
            float sx_d = sx + drift * 0.3f * sinf(time * 2.0f * twinkle_time_scale + (float)i * 0.1f);
            float sy_d = sy + drift * 0.3f * cosf(time * 1.7f * twinkle_time_scale + (float)i * 0.07f);
            Vector3D star_local{sx_d * e.hw + origin.x, sy_d * e.hh + origin.y, sz * e.hd + origin.z};
            star_rot = TransformPointByRotation(star_local.x, star_local.y, star_local.z, origin);
        }
        else
        {
            star_rot = star_positions_cached[i];
        }
        float dx = x - star_rot.x, dy = y - star_rot.y, dz = z - star_rot.z;
        float d2 = dx*dx + dy*dy + dz*dz;
        if(d2 > d2_cutoff) continue;
        float intensity = expf(-d2 / sigma_sq);
        float twinkle = std::max(0.0f, std::min(1.0f, twinkle_speed));
        if(this->mode == MODE_TWINKLE) twinkle = std::max(0.5f, twinkle);
        if(twinkle > 0.01f)
        {
            float tw_ph = time * (3.0f + twinkle * (this->mode == MODE_TWINKLE ? 8.0f : 5.0f)) * (0.6f + 0.06f * GetScaledFrequency()) * twinkle_time_scale + (float)i;
            if(strat_on)
            {
                tw_ph += bb.phase_deg * 0.05f;
            }
            intensity *= 0.5f + 0.5f * sinf(tw_ph);
        }
        if(intensity < 0.01f) continue;

        float hue = fmodf((float)i * 2.0f * (0.6f + 0.4f * detail) + color_cycle, 360.0f);
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

nlohmann::json Starfield::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "starfield_stratum_layout_mode",
                                           sm,
                                           st,
                                           "starfield_stratum_band_speed_pct",
                                           "starfield_stratum_band_tight_pct",
                                           "starfield_stratum_band_phase_deg");
    j["mode"] = this->mode;
    j["star_size"] = star_size;
    j["num_stars"] = num_stars;
    j["drift_amount"] = drift_amount;
    j["twinkle_speed"] = twinkle_speed;
    return j;
}

void Starfield::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "starfield_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "starfield_stratum_band_speed_pct",
                                            "starfield_stratum_band_tight_pct",
                                            "starfield_stratum_band_phase_deg");
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        this->mode = std::max(0, std::min(settings["mode"].get<int>(), MODE_COUNT - 1));
    if(settings.contains("star_size") && settings["star_size"].is_number())
        star_size = std::max(0.02f, std::min(1.0f, settings["star_size"].get<float>()));
    if(settings.contains("num_stars") && settings["num_stars"].is_number())
        num_stars = std::max(40, std::min(200, settings["num_stars"].get<int>()));
    if(settings.contains("drift_amount") && settings["drift_amount"].is_number())
        drift_amount = std::max(0.0f, std::min(1.0f, settings["drift_amount"].get<float>()));
    if(settings.contains("twinkle_speed") && settings["twinkle_speed"].is_number())
        twinkle_speed = std::max(0.0f, std::min(1.0f, settings["twinkle_speed"].get<float>()));
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
