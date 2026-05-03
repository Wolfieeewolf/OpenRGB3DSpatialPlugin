// SPDX-License-Identifier: GPL-2.0-only

#include "Spiral.h"
#include "EffectStratumBlend.h"
#include "SpatialLayerCore.h"

REGISTER_EFFECT_3D(Spiral);
#include <algorithm>
#include <array>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
static void LoadBandInt3FromJson(const nlohmann::json& settings,
                                 const char* key,
                                 std::array<int, 3>& out,
                                 int lo,
                                 int hi)
{
    if(!settings.contains(key) || !settings[key].is_array())
    {
        return;
    }
    const nlohmann::json& a = settings[key];
    for(size_t i = 0; i < 3 && i < a.size(); i++)
    {
        if(a[i].is_number_integer())
        {
            out[i] = std::clamp(a[i].get<int>(), lo, hi);
        }
    }
}
}

Spiral::Spiral(QWidget* parent) : SpatialEffect3D(parent)
{
    arms_slider = nullptr;
    arms_label = nullptr;
    pattern_combo = nullptr;
    gap_slider = nullptr;
    gap_label = nullptr;
    num_arms = 3;
    pattern_type = 0;
    gap_size = 30;
    progress = 0.0f;
    spiral_layout_mode = 0;
    band_speed_pct = {100, 100, 100};
    band_tight_pct = {100, 100, 100};
    band_phase_deg = {0, 0, 0};

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

Spiral::~Spiral() = default;

EffectInfo3D Spiral::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Spiral";
    info.effect_description = "Spiral pattern with arms/gap; optional per-height-band speed, tightness, and phase";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SPIRAL;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = true;
    info.needs_frequency = false;

    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 40.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;

    return info;
}

void Spiral::SetupCustomUI(QWidget* parent)
{
    QWidget* spiral_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spiral_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Smooth Spiral");
    pattern_combo->addItem("Pinwheel");
    pattern_combo->addItem("Sharp Blades");
    pattern_combo->addItem("Swirl Circles");
    pattern_combo->addItem("Hypnotic");
    pattern_combo->addItem("Simple Spin");
    pattern_combo->setCurrentIndex(std::clamp(pattern_type, 0, kSpiralPatternCount - 1));
    pattern_combo->setToolTip(
        "Spiral field recipe (changes both shape and rainbow hue layout). "
        "Arms and Gap matter for pinwheel and blade styles; use Scale and zone bounds on sparse layouts.");
    pattern_combo->setItemData(0, "Classic logarithmic spiral—smooth color roll-off.", Qt::ToolTipRole);
    pattern_combo->setItemData(1, "Radial wedges like a pinwheel; pair with Arms.", Qt::ToolTipRole);
    pattern_combo->setItemData(2, "High-contrast blades; Gap Size sets dark spacing.", Qt::ToolTipRole);
    pattern_combo->setItemData(3, "Concentric rings with twist—strong center read.", Qt::ToolTipRole);
    pattern_combo->setItemData(4, "Multi-frequency twist; busy, hypnotic motion.", Qt::ToolTipRole);
    pattern_combo->setItemData(5, "Lightweight angular spin—fewer features, very legible.", Qt::ToolTipRole);
    layout->addWidget(pattern_combo, 0, 1);
    pattern_type = pattern_combo->currentIndex();

    layout->addWidget(new QLabel("Arms:"), 1, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(2, 8);
    arms_slider->setValue(num_arms);
    arms_slider->setToolTip("Number of spiral arms");
    layout->addWidget(arms_slider, 1, 1);
    arms_label = new QLabel(QString::number(num_arms));
    arms_label->setMinimumWidth(30);
    layout->addWidget(arms_label, 1, 2);

    layout->addWidget(new QLabel("Gap Size:"), 2, 0);
    gap_slider = new QSlider(Qt::Horizontal);
    gap_slider->setRange(10, 80);
    gap_slider->setValue(gap_size);
    gap_slider->setToolTip("Gap size between blades");
    layout->addWidget(gap_slider, 2, 1);
    gap_label = new QLabel(QString::number(gap_size));
    gap_label->setMinimumWidth(30);
    layout->addWidget(gap_label, 2, 2);

    layout->addWidget(new QLabel("Height bands:"), 3, 0);
    spiral_layout_combo = new QComboBox();
    spiral_layout_combo->addItem("Single field", 0);
    spiral_layout_combo->addItem("Per band (floor · mid · ceiling)", 1);
    spiral_layout_combo->setToolTip(
        "Single: one spiral everywhere. Per band: each stratum blends its own speed, tightness, and phase "
        "(same origin; soft transitions at floor/mid/ceiling boundaries).");
    for(int i = 0; i < spiral_layout_combo->count(); i++)
    {
        if(spiral_layout_combo->itemData(i).toInt() == spiral_layout_mode)
        {
            spiral_layout_combo->setCurrentIndex(i);
            break;
        }
    }
    layout->addWidget(spiral_layout_combo, 3, 1, 1, 2);
    connect(spiral_layout_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if(spiral_layout_combo)
        {
            spiral_layout_mode = std::clamp(spiral_layout_combo->currentData().toInt(), 0, 1);
        }
        if(layered_band_widget)
        {
            layered_band_widget->setVisible(spiral_layout_mode == 1);
        }
        emit ParametersChanged();
    });

    layered_band_widget = new QGroupBox(QStringLiteral("Band tuning"));
    layered_band_widget->setToolTip(
        "Speed % scales motion in that band. Tightness % scales pattern detail. Phase ° twists the spiral.");
    QGridLayout* band_grid = new QGridLayout(layered_band_widget);
    band_grid->addWidget(new QLabel(QString()), 0, 0);
    band_grid->addWidget(new QLabel(QStringLiteral("Speed %")), 0, 1);
    band_grid->addWidget(new QLabel(QStringLiteral("Tight %")), 0, 3);
    band_grid->addWidget(new QLabel(QStringLiteral("Phase °")), 0, 5);
    for(int i = 0; i < 3; i++)
    {
        const int r = i + 1;
        band_grid->addWidget(new QLabel(QString("%1:").arg(EffectStratumBlend::BandNameUi(i))), r, 0);

        QSlider* sp = new QSlider(Qt::Horizontal);
        sp->setRange(0, 200);
        sp->setValue(band_speed_pct[(size_t)i]);
        QLabel* spl = new QLabel(QString::number(band_speed_pct[(size_t)i]));
        spl->setMinimumWidth(28);
        band_speed_slider[i] = sp;
        band_speed_lbl[i] = spl;
        band_grid->addWidget(sp, r, 1);
        band_grid->addWidget(spl, r, 2);
        connect(sp, &QSlider::valueChanged, this, [this, i, spl](int v) {
            band_speed_pct[(size_t)i] = std::clamp(v, 0, 200);
            spl->setText(QString::number(band_speed_pct[(size_t)i]));
            emit ParametersChanged();
        });

        QSlider* ti = new QSlider(Qt::Horizontal);
        ti->setRange(25, 300);
        ti->setValue(band_tight_pct[(size_t)i]);
        QLabel* til = new QLabel(QString::number(band_tight_pct[(size_t)i]));
        til->setMinimumWidth(28);
        band_tight_slider[i] = ti;
        band_tight_lbl[i] = til;
        band_grid->addWidget(ti, r, 3);
        band_grid->addWidget(til, r, 4);
        connect(ti, &QSlider::valueChanged, this, [this, i, til](int v) {
            band_tight_pct[(size_t)i] = std::clamp(v, 25, 300);
            til->setText(QString::number(band_tight_pct[(size_t)i]));
            emit ParametersChanged();
        });

        QSlider* ph = new QSlider(Qt::Horizontal);
        ph->setRange(-180, 180);
        ph->setValue(band_phase_deg[(size_t)i]);
        QLabel* phl = new QLabel(QString::number(band_phase_deg[(size_t)i]));
        phl->setMinimumWidth(32);
        band_phase_slider[i] = ph;
        band_phase_lbl[i] = phl;
        band_grid->addWidget(ph, r, 5);
        band_grid->addWidget(phl, r, 6);
        connect(ph, &QSlider::valueChanged, this, [this, i, phl](int v) {
            band_phase_deg[(size_t)i] = std::clamp(v, -180, 180);
            phl->setText(QString::number(band_phase_deg[(size_t)i]));
            emit ParametersChanged();
        });
    }
    layered_band_widget->setVisible(spiral_layout_mode == 1);
    layout->addWidget(layered_band_widget, 4, 0, 1, 3);

    AddWidgetToParent(spiral_widget, parent);

    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Spiral::OnSpiralParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, this, &Spiral::OnSpiralParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, arms_label, [this](int value) {
        arms_label->setText(QString::number(value));
    });
    connect(gap_slider, &QSlider::valueChanged, this, &Spiral::OnSpiralParameterChanged);
    connect(gap_slider, &QSlider::valueChanged, gap_label, [this](int value) {
        gap_label->setText(QString::number(value));
    });
}

void Spiral::SyncLayeredBandWidgets()
{
    for(int i = 0; i < 3; i++)
    {
        if(band_speed_slider[i])
        {
            QSignalBlocker b(band_speed_slider[i]);
            band_speed_slider[i]->setValue(band_speed_pct[(size_t)i]);
        }
        if(band_speed_lbl[i])
        {
            band_speed_lbl[i]->setText(QString::number(band_speed_pct[(size_t)i]));
        }
        if(band_tight_slider[i])
        {
            QSignalBlocker b(band_tight_slider[i]);
            band_tight_slider[i]->setValue(band_tight_pct[(size_t)i]);
        }
        if(band_tight_lbl[i])
        {
            band_tight_lbl[i]->setText(QString::number(band_tight_pct[(size_t)i]));
        }
        if(band_phase_slider[i])
        {
            QSignalBlocker b(band_phase_slider[i]);
            band_phase_slider[i]->setValue(band_phase_deg[(size_t)i]);
        }
        if(band_phase_lbl[i])
        {
            band_phase_lbl[i]->setText(QString::number(band_phase_deg[(size_t)i]));
        }
    }
}

void Spiral::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SPIRAL;
}

void Spiral::OnSpiralParameterChanged()
{
    if(pattern_combo)
        pattern_type = std::clamp(pattern_combo->currentIndex(), 0, kSpiralPatternCount - 1);
    if(arms_slider)
    {
        num_arms = arms_slider->value();
        if(arms_label) arms_label->setText(QString::number(num_arms));
    }
    if(gap_slider)
    {
        gap_size = gap_slider->value();
        if(gap_label) gap_label->setText(QString::number(gap_size));
    }
    emit ParametersChanged();
}


RGBColor Spiral::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time);

    float size_multiplier = GetNormalizedSize();

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_z = rotated_pos.z - origin.z;

    float angle = atan2(rot_rel_z, rot_rel_x);
    EffectGridAxisHalfExtents ex = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float r_xz = EffectGridHorizontalRadialNormXZ(rot_rel_x, rot_rel_z, ex.hw, ex.hd);
    float norm_radius = EffectGridHorizontalRadialNorm01(r_xz);
    norm_radius = fmaxf(0.0f, fminf(1.0f, norm_radius));

    float norm_twist = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);

    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    float layer_w[SpatialLayerCore::kMaxLayerCount]{};
    SpatialLayerCore::ComputeVerticalStratumWeights(norm_twist, strat_map, 3, layer_w);

    EffectStratumBlend::BandTuningPct strat_bt{};
    strat_bt.speed = band_speed_pct;
    strat_bt.tight = band_tight_pct;
    strat_bt.phase_deg = band_phase_deg;
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(spiral_layout_mode, layer_w, strat_bt);
    float spd_mul = bb.speed_mul;
    float tight_mul = bb.tight_mul;
    float ph_deg = bb.phase_deg;

    const float detail_e = detail * tight_mul;
    const float rate_e = rate * spd_mul;
    const float progress_e = progress * spd_mul;
    const float freq_scale_e = detail_e * 0.15f / fmax(0.1f, size_multiplier);
    float z_twist = norm_twist * (0.35f + 0.25f * detail_e);
    float spiral_angle =
        angle * (float)num_arms + norm_radius * (detail_e * 6.5f) + z_twist - progress_e * 1.35f;
    spiral_angle += ph_deg * ((float)M_PI / 180.0f);

    float spiral_value;
    float gap_factor = gap_size / 100.0f;

    switch(pattern_type)
    {
        case 0:
            spiral_value = sin(spiral_angle) * (1.0f + 0.4f * cos(norm_twist * freq_scale_e * 3.0f + progress_e * 0.7f));
            spiral_value += 0.3f * cos(spiral_angle * 0.5f + norm_twist * freq_scale_e * 4.5f + progress_e * 1.2f);
            spiral_value = (spiral_value + 1.5f) / 3.0f;
            spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            break;
        case 1:
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);
                if(arm_angle < blade_width)
                {
                    float blade_position = arm_angle / blade_width;
                    spiral_value = 0.5f + 0.5f * cos(blade_position * 3.14159f);
                }
                else
                {
                    spiral_value = 0.0f;
                }
                float radial_fade = 0.4f + 0.6f * (1.0f - exp(-norm_radius * (detail_e * 0.8f)));
                spiral_value = spiral_value * radial_fade + 0.1f * radial_fade;
            }
            break;
        case 2:
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);
                if(arm_angle < blade_width)
                {
                    float blade_position = fabs(arm_angle - blade_width * 0.5f) / (blade_width * 0.5f);
                    spiral_value = 1.0f - blade_position * blade_position;
                }
                else
                {
                    spiral_value = 0.0f;
                }
                float energy_pulse = 0.2f * sin(norm_radius * (detail_e * 1.2f) - progress_e * 2.0f);
                spiral_value = fmax(0.0f, spiral_value + energy_pulse);
                float radial_fade = 0.4f + 0.6f * (1.0f - exp(-norm_radius * (detail_e * 0.8f)));
                spiral_value *= radial_fade;
            }
            break;
        case 3:
            {
                float circle_angle = atan2(rot_rel_z, rot_rel_x) + progress_e * 2.0f;
                float ring_phase = norm_radius * (detail_e * 8.0f) * (float)num_arms - circle_angle * (float)num_arms;
                spiral_value = 0.5f + 0.5f * sin(ring_phase) * (1.0f - norm_radius * 0.3f);
                spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            }
            break;
        case 4:
            {
                float hyp_angle = atan2(rot_rel_z, rot_rel_x) - progress_e * 3.0f;
                float hyp_radius = norm_radius * (detail_e * 4.0f);
                spiral_value = 0.5f + 0.5f * sin(hyp_angle * 2.0f + hyp_radius - progress_e * 2.0f) * cos(norm_twist * freq_scale_e * 3.0f + progress_e);
                spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            }
            break;
        case 5:
            {
                float period = 6.28318f / (float)num_arms;
                float arm_angle = fmod(spiral_angle, period);
                if(arm_angle < 0.0f) arm_angle += period;
                float blade_width = 0.4f * period;
                float blade_core = (arm_angle < blade_width) ? (1.0f - arm_angle / blade_width) : 0.0f;
                float blade_glow = 0.0f;
                if(arm_angle < blade_width * 1.5f)
                {
                    float glow_dist = fabsf(arm_angle - blade_width * 0.5f) / (blade_width * 0.5f);
                    blade_glow = 0.3f * (1.0f - glow_dist);
                }
                spiral_value = fmin(1.0f, blade_core + blade_glow);
                float radial_fade = 0.35f + 0.65f * (1.0f - fmin(1.0f, norm_radius) * 0.6f);
                spiral_value = spiral_value * radial_fade + 0.08f * radial_fade;
            }
            break;
        default:
            spiral_value = 0.5f;
            break;
    }

    SpatialLayerCore::Basis compass_basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), compass_basis);
    SpatialLayerCore::MapperSettings compass_map;
    compass_map.floor_end = 0.30f;
    compass_map.desk_end = 0.55f;
    compass_map.upper_end = 0.78f;
    compass_map.blend_softness =
        std::clamp(0.08f + 0.05f * (1.0f - detail), 0.05f, 0.20f);
    compass_map.center_size = std::clamp(0.10f + 0.22f * size_multiplier, 0.06f, 0.50f);
    compass_map.directional_sharpness = std::clamp(1.0f + detail * 0.15f, 0.85f, 2.4f);

    SpatialLayerCore::SamplePoint compass_sample{};
    compass_sample.grid_x = x;
    compass_sample.grid_y = y;
    compass_sample.grid_z = z;
    compass_sample.origin_x = origin.x;
    compass_sample.origin_y = origin.y;
    compass_sample.origin_z = origin.z;
    compass_sample.y_norm = norm_twist;

    RGBColor final_color;
    if((pattern_type == 1 || pattern_type == 2 || pattern_type == 5) && !GetRainbowMode())
    {
        float arm_index = fmod(spiral_angle / (6.28318f / num_arms), (float)num_arms);
        if(arm_index < 0) arm_index += num_arms;
        float pos = fmodf((arm_index / (float)num_arms) + time * rate_e * 0.02f, 1.0f);
        if(pos < 0.0f) pos += 1.0f;
        float p = ApplySpatialPalette01(pos, compass_basis, compass_sample, compass_map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        final_color = GetColorAtPosition(p);
    }
    else if(GetRainbowMode())
    {
        float hue = spiral_angle * 57.2958f + spiral_value * 200.0f + norm_twist * 40.0f + time * rate_e * 12.0f;
        hue = ApplySpatialRainbowHue(hue, fmodf(spiral_value + 0.25f, 1.0f), compass_basis, compass_sample, compass_map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float pos = fmodf(spiral_value + time * rate_e * 0.02f, 1.0f);
        if(pos < 0.0f) pos += 1.0f;
        float p = ApplySpatialPalette01(pos, compass_basis, compass_sample, compass_map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        final_color = GetColorAtPosition(p);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Spiral::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["num_arms"] = num_arms;
    j["pattern_type"] = pattern_type;
    j["gap_size"] = gap_size;
    j["spiral_layout_mode"] = spiral_layout_mode;
    j["spiral_band_speed_pct"] = nlohmann::json::array({band_speed_pct[0], band_speed_pct[1], band_speed_pct[2]});
    j["spiral_band_tight_pct"] = nlohmann::json::array({band_tight_pct[0], band_tight_pct[1], band_tight_pct[2]});
    j["spiral_band_phase_deg"] = nlohmann::json::array({band_phase_deg[0], band_phase_deg[1], band_phase_deg[2]});
    return j;
}

void Spiral::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("num_arms"))
    {
        num_arms = settings["num_arms"].get<unsigned int>();
        if(arms_slider)
        {
            arms_slider->setValue((int)num_arms);
        }
    }
    if(settings.contains("pattern_type") && settings["pattern_type"].is_number_integer())
    {
        pattern_type = std::clamp(settings["pattern_type"].get<int>(), 0, kSpiralPatternCount - 1);
        if(pattern_combo)
        {
            pattern_combo->setCurrentIndex(pattern_type);
        }
    }
    if(settings.contains("gap_size"))
    {
        gap_size = settings["gap_size"].get<unsigned int>();
        if(gap_slider)
        {
            gap_slider->setValue((int)gap_size);
        }
    }
    if(settings.contains("spiral_layout_mode") && settings["spiral_layout_mode"].is_number_integer())
    {
        spiral_layout_mode = std::clamp(settings["spiral_layout_mode"].get<int>(), 0, 1);
    }

    LoadBandInt3FromJson(settings, "spiral_band_speed_pct", band_speed_pct, 0, 200);
    LoadBandInt3FromJson(settings, "spiral_band_tight_pct", band_tight_pct, 25, 300);
    LoadBandInt3FromJson(settings, "spiral_band_phase_deg", band_phase_deg, -180, 180);

    if(spiral_layout_combo)
    {
        QSignalBlocker b(spiral_layout_combo);
        for(int i = 0; i < spiral_layout_combo->count(); i++)
        {
            if(spiral_layout_combo->itemData(i).toInt() == spiral_layout_mode)
            {
                spiral_layout_combo->setCurrentIndex(i);
                break;
            }
        }
    }
    if(layered_band_widget)
    {
        layered_band_widget->setVisible(spiral_layout_mode == 1);
    }
    SyncLayeredBandWidgets();
}
