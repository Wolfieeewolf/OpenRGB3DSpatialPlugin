// SPDX-License-Identifier: GPL-2.0-only

#include "ColorWheel.h"
#include "EffectStratumBlend.h"
#include "EffectHelpers.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(ColorWheel);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    const char* kBandName(int i)
    {
        static const char* n[] = {"Floor", "Mid", "Ceiling"};
        return (i >= 0 && i < 3) ? n[i] : "?";
    }
}

ColorWheel::ColorWheel(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
}

EffectInfo3D ColorWheel::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Color Wheel";
    info.effect_description = "Rotating rainbow from center; optional independent floor / mid / ceiling wheels";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
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
    info.show_plane_control = true;
    return info;
}

void ColorWheel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    outer->addLayout(layout);

    int row = 0;
    layout->addWidget(new QLabel("Direction:"), row, 0);
    QComboBox* dir_combo = new QComboBox();
    dir_combo->addItem("Clockwise");
    dir_combo->addItem("Counter-clockwise");
    dir_combo->setCurrentIndex(direction);
    dir_combo->setToolTip("Hue progression around the effect origin in the active plane (see Plane in common controls).");
    dir_combo->setItemData(0, "Increasing angle follows clock motion when viewed from the plane normal.", Qt::ToolTipRole);
    dir_combo->setItemData(1, "Reverses hue sweep—useful to match other rotating effects.", Qt::ToolTipRole);
    layout->addWidget(dir_combo, row, 1, 1, 2);
    connect(dir_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        direction = std::max(0, std::min(1, idx));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel(tr("Hue geometry:")), row, 0);
    QComboBox* geom_combo = new QComboBox();
    geom_combo->addItem(tr("Radial (classic)"), 0);
    geom_combo->addItem(tr("Shear (no focal point)"), 1);
    geom_combo->setToolTip(
        tr("Radial: hue wraps around the effect origin (see Effect origin in the main tab).\n"
           "Shear: moving rainbow bands from a rotating planar gradient—no single center, but still \"wheels\" in time."));
    for(int i = 0; i < geom_combo->count(); i++)
    {
        if(geom_combo->itemData(i).toInt() == hue_geometry_mode)
        {
            geom_combo->setCurrentIndex(i);
            break;
        }
    }
    layout->addWidget(geom_combo, row, 1, 1, 2);
    connect(geom_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, geom_combo](int) {
        hue_geometry_mode = std::clamp(geom_combo->currentData().toInt(), 0, 1);
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Wheel layout:"), row, 0);
    QComboBox* layout_combo = new QComboBox();
    layout_combo->addItem("Single wheel", 0);
    layout_combo->addItem("Per height band (floor · mid · ceiling)", 1);
    layout_combo->setToolTip(
        "Single: one wheel for the whole room. Per band: each vertical stratum blends its own speed, tightness, and phase "
        "(same geometry origin; acts like stacked independent wheels at soft band boundaries).");
    for(int i = 0; i < layout_combo->count(); i++)
    {
        if(layout_combo->itemData(i).toInt() == wheel_layout_mode)
        {
            layout_combo->setCurrentIndex(i);
            break;
        }
    }
    layout->addWidget(layout_combo, row, 1, 1, 2);
    wheel_layout_combo = layout_combo;
    connect(layout_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, layout_combo](int) {
        wheel_layout_mode = std::clamp(layout_combo->currentData().toInt(), 0, 1);
        if(layered_settings_widget)
        {
            layered_settings_widget->setVisible(wheel_layout_mode == 1);
        }
        emit ParametersChanged();
    });
    row++;

    layered_settings_widget = new QGroupBox(QStringLiteral("Band tuning (floor · mid · ceiling)"));
    layered_settings_widget->setToolTip(
        "Speed % scales spin from global Frequency in that band. Tightness % scales how many hue cycles fit in space "
        "(higher = tighter wheel). Phase offsets hue per band. Values blend smoothly at band edges.");
    QGridLayout* band_grid = new QGridLayout(layered_settings_widget);
    band_grid->addWidget(new QLabel(QString()), 0, 0);
    band_grid->addWidget(new QLabel(QStringLiteral("Speed %")), 0, 1);
    band_grid->addWidget(new QLabel(QStringLiteral("Tight %")), 0, 3);
    band_grid->addWidget(new QLabel(QStringLiteral("Phase °")), 0, 5);

    for(int i = 0; i < 3; i++)
    {
        const int r = i + 1;
        band_grid->addWidget(new QLabel(QString("%1:").arg(kBandName(i))), r, 0);

        QSlider* sp_sl = new QSlider(Qt::Horizontal);
        sp_sl->setRange(0, 200);
        sp_sl->setValue(band_speed_pct[(size_t)i]);
        sp_sl->setToolTip(QStringLiteral("Band speed vs global Frequency (0–200%)."));
        QLabel* sp_lbl = new QLabel(QString::number(band_speed_pct[(size_t)i]));
        sp_lbl->setMinimumWidth(28);
        band_speed_value_lbl[i] = sp_lbl;
        band_speed_slider[i] = sp_sl;
        band_grid->addWidget(sp_sl, r, 1);
        band_grid->addWidget(sp_lbl, r, 2);
        connect(sp_sl, &QSlider::valueChanged, this, [this, i, sp_lbl](int v) {
            band_speed_pct[(size_t)i] = std::clamp(v, 0, 200);
            sp_lbl->setText(QString::number(band_speed_pct[(size_t)i]));
            emit ParametersChanged();
        });

        QSlider* sz_sl = new QSlider(Qt::Horizontal);
        sz_sl->setRange(25, 300);
        sz_sl->setValue(band_size_pct[(size_t)i]);
        sz_sl->setToolTip(QStringLiteral("Hue tightness in this band (25–300%). Higher = more cycles across the room."));
        QLabel* sz_lbl = new QLabel(QString::number(band_size_pct[(size_t)i]));
        sz_lbl->setMinimumWidth(28);
        band_size_value_lbl[i] = sz_lbl;
        band_size_slider[i] = sz_sl;
        band_grid->addWidget(sz_sl, r, 3);
        band_grid->addWidget(sz_lbl, r, 4);
        connect(sz_sl, &QSlider::valueChanged, this, [this, i, sz_lbl](int v) {
            band_size_pct[(size_t)i] = std::clamp(v, 25, 300);
            sz_lbl->setText(QString::number(band_size_pct[(size_t)i]));
            emit ParametersChanged();
        });

        QSlider* ph_sl = new QSlider(Qt::Horizontal);
        ph_sl->setRange(-180, 180);
        ph_sl->setValue(band_phase_deg[(size_t)i]);
        ph_sl->setToolTip(QStringLiteral("Hue offset for this band (−180°…180°)."));
        QLabel* ph_lbl = new QLabel(QString::number(band_phase_deg[(size_t)i]));
        ph_lbl->setMinimumWidth(32);
        band_phase_value_lbl[i] = ph_lbl;
        band_phase_slider[i] = ph_sl;
        band_grid->addWidget(ph_sl, r, 5);
        band_grid->addWidget(ph_lbl, r, 6);
        connect(ph_sl, &QSlider::valueChanged, this, [this, i, ph_lbl](int v) {
            band_phase_deg[(size_t)i] = std::clamp(v, -180, 180);
            ph_lbl->setText(QString::number(band_phase_deg[(size_t)i]));
            emit ParametersChanged();
        });
    }

    layered_settings_widget->setVisible(wheel_layout_mode == 1);
    outer->addWidget(layered_settings_widget);

    AddWidgetToParent(w, parent);
}

void ColorWheel::SyncLayeredSliderWidgets()
{
    for(int i = 0; i < 3; i++)
    {
        if(band_speed_slider[i])
        {
            QSignalBlocker b(band_speed_slider[i]);
            band_speed_slider[i]->setValue(band_speed_pct[(size_t)i]);
        }
        if(band_speed_value_lbl[i])
        {
            band_speed_value_lbl[i]->setText(QString::number(band_speed_pct[(size_t)i]));
        }
        if(band_size_slider[i])
        {
            QSignalBlocker b(band_size_slider[i]);
            band_size_slider[i]->setValue(band_size_pct[(size_t)i]);
        }
        if(band_size_value_lbl[i])
        {
            band_size_value_lbl[i]->setText(QString::number(band_size_pct[(size_t)i]));
        }
        if(band_phase_slider[i])
        {
            QSignalBlocker b(band_phase_slider[i]);
            band_phase_slider[i]->setValue(band_phase_deg[(size_t)i]);
        }
        if(band_phase_value_lbl[i])
        {
            band_phase_value_lbl[i]->setText(QString::number(band_phase_deg[(size_t)i]));
        }
    }
}

void ColorWheel::UpdateParams(SpatialEffectParams& params) { (void)params; }


RGBColor ColorWheel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    float detail = std::max(0.05f, GetScaledDetail());
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = rot.x - origin.x, ly = rot.y - origin.y, lz = rot.z - origin.z;

    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);

    const float y_norm = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    float layer_w[SpatialLayerCore::kMaxLayerCount]{};
    SpatialLayerCore::ComputeVerticalStratumWeights(y_norm, map, 3, layer_w);

    EffectStratumBlend::BandTuningPct wheel_bt{};
    wheel_bt.speed = band_speed_pct;
    wheel_bt.tight = band_size_pct;
    wheel_bt.phase_deg = band_phase_deg;
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(wheel_layout_mode, layer_w, wheel_bt);
    float spd_mul = bb.speed_mul;
    float sz_mul = bb.tight_mul;
    float ph_blend = bb.phase_deg;

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    e.hw /= sz_mul;
    e.hh /= sz_mul;
    e.hd /= sz_mul;

    int pl = GetPlane();
    const float dir = (direction == 0) ? 1.0f : -1.0f;
    float angle = 0.0f;
    if(hue_geometry_mode == 1)
    {
        float u = 0.0f;
        float v = 0.0f;
        if(pl == 0)
        {
            u = lx / e.hw;
            v = lz / e.hd;
        }
        else if(pl == 1)
        {
            u = lx / e.hw;
            v = ly / e.hh;
        }
        else
        {
            u = lz / e.hd;
            v = ly / e.hh;
        }
        const float spin = progress * 6.2831855f * dir + time * GetScaledFrequency() * 0.12f * spd_mul;
        const float cu = std::cos(spin);
        const float su = std::sin(spin);
        angle = (u * cu + v * su) * (float)M_PI * (0.5f + 0.5f * detail);
    }
    else
    {
        if(pl == 0) angle = atan2f(lz / e.hd, lx / e.hw);
        else if(pl == 1) angle = atan2f(lx / e.hw, ly / e.hh);
        else angle = atan2f(lz / e.hd, ly / e.hh);
    }

    float hue_plane = fmodf(angle * (180.0f / (float)M_PI) * (0.5f + 0.5f * detail) + progress * 360.0f * dir +
                            time * GetScaledFrequency() * 12.0f * spd_mul + ph_blend,
                        360.0f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = y_norm;

    if(hue_plane < 0.0f) hue_plane += 360.0f;
    const float plane01 = hue_plane / 360.0f;
    float mapped_hue = ApplySpatialRainbowHue(hue_plane, plane01, basis, sp, map, time, &grid);
    float palette01 = std::fmod(mapped_hue / 360.0f, 1.0f);
    if(palette01 < 0.0f)
    {
        palette01 += 1.0f;
    }
    palette01 = ApplyVoxelDriveToPalette01(palette01, x, y, z, time, grid);

    return GetRainbowMode() ? GetRainbowColor(palette01 * 360.0f) : GetColorAtPosition(palette01);
}

nlohmann::json ColorWheel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["direction"] = direction;
    j["hue_geometry_mode"] = hue_geometry_mode;
    j["wheel_layout_mode"] = wheel_layout_mode;
    j["band_speed_pct"] = nlohmann::json::array({band_speed_pct[0], band_speed_pct[1], band_speed_pct[2]});
    j["band_size_pct"] = nlohmann::json::array({band_size_pct[0], band_size_pct[1], band_size_pct[2]});
    j["band_phase_deg"] = nlohmann::json::array({band_phase_deg[0], band_phase_deg[1], band_phase_deg[2]});
    return j;
}

void ColorWheel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("direction") && settings["direction"].is_number_integer())
        direction = std::max(0, std::min(1, settings["direction"].get<int>()));
    if(settings.contains("hue_geometry_mode") && settings["hue_geometry_mode"].is_number_integer())
        hue_geometry_mode = std::clamp(settings["hue_geometry_mode"].get<int>(), 0, 1);
    if(settings.contains("wheel_layout_mode") && settings["wheel_layout_mode"].is_number_integer())
        wheel_layout_mode = std::clamp(settings["wheel_layout_mode"].get<int>(), 0, 1);

    auto load_int3 = [&settings](const char* key, std::array<int, 3>& out, int lo, int hi) {
        if(!settings.contains(key) || !settings[key].is_array())
            return;
        const auto& a = settings[key];
        for(size_t i = 0; i < 3 && i < a.size(); i++)
        {
            if(a[i].is_number_integer())
                out[i] = std::clamp(a[i].get<int>(), lo, hi);
        }
    };
    load_int3("band_speed_pct", band_speed_pct, 0, 200);
    load_int3("band_size_pct", band_size_pct, 25, 300);
    load_int3("band_phase_deg", band_phase_deg, -180, 180);

    if(wheel_layout_combo)
    {
        QSignalBlocker b(wheel_layout_combo);
        for(int i = 0; i < wheel_layout_combo->count(); i++)
        {
            if(wheel_layout_combo->itemData(i).toInt() == wheel_layout_mode)
            {
                wheel_layout_combo->setCurrentIndex(i);
                break;
            }
        }
    }
    if(layered_settings_widget)
    {
        layered_settings_widget->setVisible(wheel_layout_mode == 1);
    }

    SyncLayeredSliderWidgets();
}
