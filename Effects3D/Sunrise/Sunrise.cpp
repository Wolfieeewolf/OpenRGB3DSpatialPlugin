// SPDX-License-Identifier: GPL-2.0-only

#include "Sunrise.h"
#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "EffectStratumBlend.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(Sunrise);

const char* Sunrise::ModeName(int m)
{
    switch(m) {
    case MODE_MANUAL: return "Manual (animated)";
    case MODE_REALTIME: return "Real-time (system clock)";
    case MODE_SIMULATED: return "Simulated day";
    default: return "Manual";
    }
}

const char* Sunrise::PresetName(int p)
{
    switch(p) {
    case PRESET_REALISTIC_SUNRISE: return "Realistic Sunrise";
    case PRESET_REALISTIC_SUNSET: return "Realistic Sunset";
    case PRESET_DAYTIME: return "Daytime (blue sky, green grass)";
    case PRESET_NIGHT: return "Night";
    case PRESET_CUSTOM: return "Custom (use color pickers)";
    default: return "Daytime";
    }
}

void Sunrise::ApplyPreset(int preset)
{
    std::vector<RGBColor> cols;
    switch(preset)
    {
    case PRESET_REALISTIC_SUNRISE:
        cols.push_back(0x00CC6600);
        cols.push_back(0x0000C0FF);
        cols.push_back(0x00804080);
        cols.push_back(0x00080800);
        break;
    case PRESET_REALISTIC_SUNSET:
        cols.push_back(0x00804080);
        cols.push_back(0x0000A0FF);
        cols.push_back(0x000000FF);
        cols.push_back(0x00080800);
        break;
    case PRESET_DAYTIME:
        cols.push_back(0x00FFCC66);
        cols.push_back(0x00FFFFFF);
        cols.push_back(0x0000AA44);
        cols.push_back(0x00006622);
        break;
    case PRESET_NIGHT:
        cols.push_back(0x00332211);
        cols.push_back(0x00221810);
        cols.push_back(0x00110800);
        cols.push_back(0x00000000);
        break;
    case PRESET_CUSTOM:
    default:
        return;
    }
    SetColors(cols);
}

float Sunrise::GetTimeOfDayProgress(float time) const
{
    if(time_mode == MODE_MANUAL)
    {
        float progress = fmodf(CalculateProgress(time), 1.0f);
        if(progress < 0.0f) progress += 1.0f;
        return progress;
    }
    if(time_mode == MODE_REALTIME)
    {
        time_t now = ::time(nullptr);
        struct tm* t = localtime(&now);
        if(!t) return 0.5f;
        float hour = (float)t->tm_hour + (float)t->tm_min / 60.0f + (float)t->tm_sec / 3600.0f;
        if(hour >= 24.0f) hour -= 24.0f;
        float hour_norm = hour / 24.0f;
        float progress;
        if(hour_norm < 5.0f/24.0f)       progress = 0.0f;
        else if(hour_norm < 7.0f/24.0f)  progress = (hour_norm - 5.0f/24.0f) / (2.0f/24.0f) * 0.4f;
        else if(hour_norm < 17.0f/24.0f) progress = 0.4f + (hour_norm - 7.0f/24.0f) / (10.0f/24.0f) * 0.5f;
        else if(hour_norm < 19.0f/24.0f) progress = 0.9f - (hour_norm - 17.0f/24.0f) / (2.0f/24.0f) * 0.5f;
        else if(hour_norm < 21.0f/24.0f) progress = 0.4f - (hour_norm - 19.0f/24.0f) / (2.0f/24.0f) * 0.4f;
        else                             progress = 0.0f;
        return progress;
    }
    if(time_mode == MODE_SIMULATED)
    {
        float mins_per_cycle = std::max(1.0f, std::min(120.0f, day_length_minutes));
        float cycle_sec = mins_per_cycle * 60.0f;
        float hour = fmodf(time / cycle_sec * 24.0f, 24.0f);
        if(hour < 0.0f) hour += 24.0f;
        float t = hour / 24.0f;
        float progress;
        if(t < 5.0f/24.0f)       progress = 0.0f;
        else if(t < 7.0f/24.0f)  progress = (t - 5.0f/24.0f) / (2.0f/24.0f) * 0.4f;
        else if(t < 17.0f/24.0f) progress = 0.4f + (t - 7.0f/24.0f) / (10.0f/24.0f) * 0.5f;
        else if(t < 19.0f/24.0f) progress = 0.9f - (t - 17.0f/24.0f) / (2.0f/24.0f) * 0.5f;
        else if(t < 21.0f/24.0f) progress = 0.4f - (t - 19.0f/24.0f) / (2.0f/24.0f) * 0.4f;
        else                     progress = 0.0f;
        return progress;
    }
    return 0.5f;
}

Sunrise::Sunrise(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    ApplyPreset(PRESET_DAYTIME);
}

EffectInfo3D Sunrise::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Sunrise";
    info.effect_description = "Sky gradient with optional weather. Real-time clock, simulated day, or manual. Toggle rain, fog, cloudy, lightning.";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 4;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 8.0f;
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
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void Sunrise::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);
    int row = 0;

    layout->addWidget(new QLabel("Time mode:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(time_mode, MODE_COUNT - 1)));
    mode_combo->setToolTip("What drives the sun/sky cycle. Simulated uses Day length; Real-time uses your PC clock.");
    mode_combo->setItemData(0, "Effect speed slider drives the day phase.", Qt::ToolTipRole);
    mode_combo->setItemData(1, "Phase follows local time of day.", Qt::ToolTipRole);
    mode_combo->setItemData(2, "Compresses a full day into Day length minutes.", Qt::ToolTipRole);
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        time_mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Color preset:"), row, 0);
    QComboBox* preset_combo = new QComboBox();
    for(int p = 0; p < PRESET_COUNT; p++) preset_combo->addItem(PresetName(p));
    preset_combo->setCurrentIndex(std::max(0, std::min(color_preset, PRESET_COUNT - 1)));
    preset_combo->setToolTip("Loads sky/horizon/ground key colors. Custom leaves your color pickers as-is.");
    preset_combo->setItemData(0, "Warm dawn palette.", Qt::ToolTipRole);
    preset_combo->setItemData(1, "Evening oranges and deep blues.", Qt::ToolTipRole);
    preset_combo->setItemData(2, "Bright midday look.", Qt::ToolTipRole);
    preset_combo->setItemData(3, "Dim night tones.", Qt::ToolTipRole);
    preset_combo->setItemData(4, "No automatic color load—edit colors manually.", Qt::ToolTipRole);
    layout->addWidget(preset_combo, row, 1, 1, 2);
    connect(preset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        color_preset = std::max(0, std::min(idx, PRESET_COUNT - 1));
        ApplyPreset(color_preset);
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Day length (min):"), row, 0);
    QSlider* day_slider = new QSlider(Qt::Horizontal);
    day_slider->setRange(1, 120);
    day_slider->setToolTip("Length of one full day/night cycle in Simulated time mode (minutes).");
    day_slider->setValue((int)day_length_minutes);
    QLabel* day_label = new QLabel(QString::number((int)day_length_minutes));
    day_label->setMinimumWidth(36);
    layout->addWidget(day_slider, row, 1);
    layout->addWidget(day_label, row, 2);
    connect(day_slider, &QSlider::valueChanged, this, [this, day_label](int v){
        day_length_minutes = (float)v;
        day_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;

    QCheckBox* rain_cb = new QCheckBox("Rain");
    rain_cb->setToolTip("Adds blue-violet streaks in the upper volume.");
    rain_cb->setChecked(weather_rain);
    layout->addWidget(rain_cb, row, 0);
    connect(rain_cb, &QCheckBox::toggled, this, [this](bool on){ weather_rain = on; emit ParametersChanged(); });
    row++;

    QCheckBox* fog_cb = new QCheckBox("Fog");
    fog_cb->setToolTip("Softens horizon contrast and lifts low tones.");
    fog_cb->setChecked(weather_fog);
    layout->addWidget(fog_cb, row, 0);
    connect(fog_cb, &QCheckBox::toggled, this, [this](bool on){ weather_fog = on; emit ParametersChanged(); });
    row++;

    QCheckBox* cloudy_cb = new QCheckBox("Cloudy");
    cloudy_cb->setToolTip("Broadens cloud band and mutes the sun disk.");
    cloudy_cb->setChecked(weather_cloudy);
    layout->addWidget(cloudy_cb, row, 0);
    connect(cloudy_cb, &QCheckBox::toggled, this, [this](bool on){ weather_cloudy = on; emit ParametersChanged(); });
    row++;

    QCheckBox* lightning_cb = new QCheckBox("Lightning");
    lightning_cb->setToolTip("Random brief flashes during stormy parts of the cycle.");
    lightning_cb->setChecked(weather_lightning);
    layout->addWidget(lightning_cb, row, 0);
    connect(lightning_cb, &QCheckBox::toggled, this, [this](bool on){ weather_lightning = on; emit ParametersChanged(); });
    row++;
AddWidgetToParent(w, parent);
}

void Sunrise::UpdateParams(SpatialEffectParams& params) { (void)params; }

static RGBColor lerp_color(RGBColor a, RGBColor b, float t)
{
    t = std::max(0.0f, std::min(1.0f, t));
    int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF;
    int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;
    int r = (int)(ar + (br - ar) * t);
    int g = (int)(ag + (bg - ag) * t);
    int b_ = (int)(ab + (bb - ab) * t);
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b_ = std::max(0, std::min(255, b_));
    return (RGBColor)((b_ << 16) | (g << 8) | r);
}

RGBColor Sunrise::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);


    float progress = GetTimeOfDayProgress(time);
    float spd = std::max(0.5f, std::min(3.0f, 0.5f + GetScaledSpeed() * 0.3f)) * bb.speed_mul;
    float size_m = GetNormalizedSize();
    float weather_freq = std::max(0.2f, std::min(8.0f, GetScaledFrequency() * 0.15f));
    float detail = std::max(0.05f, GetScaledDetail());
    const float time_w = time * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);

    float norm_y = NormalizeGridAxis01(y, grid.min_y, grid.max_y);

    const std::vector<RGBColor>& cols = GetColors();
    RGBColor c0 = (cols.size() > 0) ? cols[0] : 0x00FFCC66;
    RGBColor c1 = (cols.size() > 1) ? cols[1] : 0x00FFFFFF;
    RGBColor c2 = (cols.size() > 2) ? cols[2] : 0x0000AA44;
    RGBColor c3 = (cols.size() > 3) ? cols[3] : 0x00006622;

    if(GetRainbowMode())
    {
        SpatialLayerCore::Basis basis;
        SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
        SpatialLayerCore::MapperSettings smap;
        smap.floor_end = 0.30f;
        smap.desk_end = 0.55f;
        smap.upper_end = 0.78f;
        smap.blend_softness = 0.10f;
        smap.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
        smap.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
        SpatialLayerCore::SamplePoint sp{};
        sp.grid_x = x;
        sp.grid_y = y;
        sp.grid_z = z;
        sp.origin_x = origin.x;
        sp.origin_y = origin.y;
        sp.origin_z = origin.z;
        sp.y_norm = coord2;
        float hue = progress * 60.0f + norm_y * 40.0f * (0.6f + 0.4f * detail) + time_w * weather_freq * 12.0f + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
        hue = ApplySpatialRainbowHue(hue, norm_y, basis, sp, smap, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        const float hub = p01 * 360.0f;
        c0 = GetRainbowColor(hub);
        c1 = GetRainbowColor(hub + 30.0f);
        c2 = GetRainbowColor(hub + 60.0f);
        c3 = GetRainbowColor(hub + 90.0f);
    }

    SpatialLayerCore::Basis basis_sky;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis_sky);
    SpatialLayerCore::MapperSettings map_sky;
    EffectStratumBlend::InitStratumBreaks(map_sky);
    map_sky.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map_sky.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map_sky.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
    SpatialLayerCore::SamplePoint sp_sky{};
    sp_sky.grid_x = x;
    sp_sky.grid_y = y;
    sp_sky.grid_z = z;
    sp_sky.origin_x = origin.x;
    sp_sky.origin_y = origin.y;
    sp_sky.origin_z = origin.z;
    sp_sky.y_norm = coord2;

    float sky_y = norm_y;
    if(!GetRainbowMode())
    {
        sky_y = ApplySpatialPalette01(norm_y, basis_sky, sp_sky, map_sky, time, &grid);
        sky_y = ApplyVoxelDriveToPalette01(sky_y, x, y, z, time, grid);
        sky_y = std::clamp(sky_y, 0.0f, 1.0f);
    }

    float exp_h = (0.15f * spd) / std::max(0.25f, bb.tight_mul);
    float exp_s = (0.12f * spd) / std::max(0.25f, bb.tight_mul);
    float horizon_y = 0.12f + 0.28f * powf(progress, exp_h);
    float sun_y = horizon_y + 0.15f / std::max(0.25f, bb.tight_mul) + 0.35f * powf(progress, exp_s);
    float y_scale = 0.6f + 0.4f * size_m;
    horizon_y *= y_scale;
    sun_y *= y_scale;
    horizon_y = std::min(0.95f, horizon_y);
    sun_y = std::min(0.98f, std::max(horizon_y + 0.05f, sun_y));

    RGBColor result;
    if(sky_y <= horizon_y)
        result = lerp_color(c3, c2, sky_y / std::max(0.001f, horizon_y));
    else if(sky_y <= sun_y)
        result = lerp_color(c2, c1, (sky_y - horizon_y) / std::max(0.001f, sun_y - horizon_y));
    else
        result = lerp_color(c1, c0, (sky_y - sun_y) / std::max(0.001f, 1.0f - sun_y));

    if(weather_cloudy)
    {
        int r = result & 0xFF, g = (result >> 8) & 0xFF, b = (result >> 16) & 0xFF;
        r = (int)(r * 0.6f); g = (int)(g * 0.6f); b = (int)(b * 0.7f);
        result = (RGBColor)((b << 16) | (g << 8) | r);
    }

    if(weather_fog)
    {
        float fog_blend = 0.4f + 0.3f * norm_y;
        RGBColor fog_col = 0x00C0C0C0;
        result = lerp_color(result, fog_col, fog_blend);
    }

    if(weather_rain)
    {
        float rain_noise = fmodf(sinf(x * 47.0f + z * 31.0f + time_w * 8.0f * weather_freq) * 0.5f + 0.5f, 1.0f);
        if(rain_noise > 0.92f)
        {
            int r = result & 0xFF, g = (result >> 8) & 0xFF, b = (result >> 16) & 0xFF;
            int add = (int)(80.0f * (rain_noise - 0.92f) / 0.08f);
            r = std::min(255, r + add); g = std::min(255, g + add); b = std::min(255, b + add);
            result = (RGBColor)((b << 16) | (g << 8) | r);
        }
    }

    if(weather_lightning)
    {
        float flash = fmodf(time_w * 0.3f * weather_freq, 4.0f);
        if(flash < 0.08f)
        {
            float t = flash / 0.08f;
            int r = result & 0xFF, g = (result >> 8) & 0xFF, b = (result >> 16) & 0xFF;
            int wr = 255, wg = 255, wb = 255;
            r = (int)(r + (wr - r) * t); g = (int)(g + (wg - g) * t); b = (int)(b + (wb - b) * t);
            result = (RGBColor)((std::min(255,b) << 16) | (std::min(255,g) << 8) | std::min(255,r));
        }
    }

    if(UseEffectStripColormap())
    {
        float ph01 = std::fmod(progress + norm_y * 0.35f + time_w * weather_freq * 0.02f + 1.0f, 1.0f);
        ph01 = ApplySpatialPalette01(ph01, basis_sky, sp_sky, map_sky, time, &grid);
        float p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               ph01,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        RGBColor pat = ResolveStripKernelFinalColor(*this,
                                                    SpatialPatternKernelClamp(GetEffectStripColormapKernel()),
                                                    p01,
                                                    GetEffectStripColormapColorStyle(),
                                                    time,
                                                    weather_freq * 12.0f);
        const int rr = (result & 0xFF) * (pat & 0xFF) / 255;
        const int rg = ((result >> 8) & 0xFF) * ((pat >> 8) & 0xFF) / 255;
        const int rb = ((result >> 16) & 0xFF) * ((pat >> 16) & 0xFF) / 255;
        result = (RGBColor)((rb << 16) | (rg << 8) | rr);
    }

    return result;
}

nlohmann::json Sunrise::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["time_mode"] = time_mode;
    j["color_preset"] = color_preset;
    j["day_length_minutes"] = day_length_minutes;
    j["weather_rain"] = weather_rain;
    j["weather_fog"] = weather_fog;
    j["weather_cloudy"] = weather_cloudy;
    j["weather_lightning"] = weather_lightning;
return j;
}

void Sunrise::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("time_mode") && settings["time_mode"].is_number_integer())
        time_mode = std::max(0, std::min(settings["time_mode"].get<int>(), MODE_COUNT - 1));
    if(settings.contains("color_preset") && settings["color_preset"].is_number_integer())
        color_preset = std::max(0, std::min(settings["color_preset"].get<int>(), PRESET_COUNT - 1));
    if(settings.contains("day_length_minutes") && settings["day_length_minutes"].is_number())
        day_length_minutes = std::max(1.0f, std::min(120.0f, settings["day_length_minutes"].get<float>()));
    if(settings.contains("weather_rain") && settings["weather_rain"].is_boolean())
        weather_rain = settings["weather_rain"].get<bool>();
    if(settings.contains("weather_fog") && settings["weather_fog"].is_boolean())
        weather_fog = settings["weather_fog"].get<bool>();
    if(settings.contains("weather_cloudy") && settings["weather_cloudy"].is_boolean())
        weather_cloudy = settings["weather_cloudy"].get<bool>();
    if(settings.contains("weather_lightning") && settings["weather_lightning"].is_boolean())
        weather_lightning = settings["weather_lightning"].get<bool>();
}
