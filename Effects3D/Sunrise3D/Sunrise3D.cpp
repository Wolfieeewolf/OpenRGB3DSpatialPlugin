// SPDX-License-Identifier: GPL-2.0-only

#include "Sunrise3D.h"
#include <cmath>
#include <ctime>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>

REGISTER_EFFECT_3D(Sunrise3D);

const char* Sunrise3D::ModeName(int m)
{
    switch(m) {
    case MODE_MANUAL: return "Manual (animated)";
    case MODE_REALTIME: return "Real-time (system clock)";
    case MODE_SIMULATED: return "Simulated day";
    default: return "Manual";
    }
}

const char* Sunrise3D::PresetName(int p)
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

void Sunrise3D::ApplyPreset(int preset)
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

float Sunrise3D::GetTimeOfDayProgress(float time) const
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

Sunrise3D::Sunrise3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    ApplyPreset(PRESET_DAYTIME);
}

EffectInfo3D Sunrise3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Realtime Environment";
    info.effect_description = "Sky gradient with optional weather. Real-time clock, simulated day, or manual. Toggle rain, fog, cloudy, lightning.";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 4;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 8.0f;
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

void Sunrise3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;

    layout->addWidget(new QLabel("Time mode:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(time_mode, MODE_COUNT - 1)));
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
    rain_cb->setChecked(weather_rain);
    layout->addWidget(rain_cb, row, 0);
    connect(rain_cb, &QCheckBox::toggled, this, [this](bool on){ weather_rain = on; emit ParametersChanged(); });
    row++;

    QCheckBox* fog_cb = new QCheckBox("Fog");
    fog_cb->setChecked(weather_fog);
    layout->addWidget(fog_cb, row, 0);
    connect(fog_cb, &QCheckBox::toggled, this, [this](bool on){ weather_fog = on; emit ParametersChanged(); });
    row++;

    QCheckBox* cloudy_cb = new QCheckBox("Cloudy");
    cloudy_cb->setChecked(weather_cloudy);
    layout->addWidget(cloudy_cb, row, 0);
    connect(cloudy_cb, &QCheckBox::toggled, this, [this](bool on){ weather_cloudy = on; emit ParametersChanged(); });
    row++;

    QCheckBox* lightning_cb = new QCheckBox("Lightning");
    lightning_cb->setChecked(weather_lightning);
    layout->addWidget(lightning_cb, row, 0);
    connect(lightning_cb, &QCheckBox::toggled, this, [this](bool on){ weather_lightning = on; emit ParametersChanged(); });
    row++;

    AddWidgetToParent(w, parent);
}

void Sunrise3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Sunrise3D::CalculateColor(float, float, float, float) { return 0x00000000; }

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

RGBColor Sunrise3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = GetTimeOfDayProgress(time);
    float spd = std::max(0.5f, std::min(3.0f, 0.5f + GetScaledSpeed() * 0.3f));

    float norm_y = (grid.height > 0.001f) ? ((y - grid.min_y) / grid.height) : 0.5f;
    norm_y = std::max(0.0f, std::min(1.0f, norm_y));

    const std::vector<RGBColor>& cols = GetColors();
    RGBColor c0 = (cols.size() > 0) ? cols[0] : 0x00FFCC66;
    RGBColor c1 = (cols.size() > 1) ? cols[1] : 0x00FFFFFF;
    RGBColor c2 = (cols.size() > 2) ? cols[2] : 0x0000AA44;
    RGBColor c3 = (cols.size() > 3) ? cols[3] : 0x00006622;

    if(GetRainbowMode())
    {
        float hue = progress * 60.0f + norm_y * 40.0f;
        c0 = GetRainbowColor(hue);
        c1 = GetRainbowColor(hue + 30.0f);
        c2 = GetRainbowColor(hue + 60.0f);
        c3 = GetRainbowColor(hue + 90.0f);
    }

    float horizon_y = 0.12f + 0.28f * powf(progress, 0.15f * spd);
    float sun_y = horizon_y + 0.15f + 0.35f * powf(progress, 0.12f * spd);
    horizon_y = std::min(0.95f, horizon_y);
    sun_y = std::min(0.98f, std::max(horizon_y + 0.05f, sun_y));

    RGBColor result;
    if(norm_y <= horizon_y)
        result = lerp_color(c3, c2, norm_y / std::max(0.001f, horizon_y));
    else if(norm_y <= sun_y)
        result = lerp_color(c2, c1, (norm_y - horizon_y) / std::max(0.001f, sun_y - horizon_y));
    else
        result = lerp_color(c1, c0, (norm_y - sun_y) / std::max(0.001f, 1.0f - sun_y));

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
        float rain_noise = fmodf(sinf(x * 47.0f + z * 31.0f + time * 8.0f) * 0.5f + 0.5f, 1.0f);
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
        float flash = fmodf(time * 0.3f, 4.0f);
        if(flash < 0.08f)
        {
            float t = flash / 0.08f;
            int r = result & 0xFF, g = (result >> 8) & 0xFF, b = (result >> 16) & 0xFF;
            int wr = 255, wg = 255, wb = 255;
            r = (int)(r + (wr - r) * t); g = (int)(g + (wg - g) * t); b = (int)(b + (wb - b) * t);
            result = (RGBColor)((std::min(255,b) << 16) | (std::min(255,g) << 8) | std::min(255,r));
        }
    }

    return result;
}

nlohmann::json Sunrise3D::SaveSettings() const
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

void Sunrise3D::LoadSettings(const nlohmann::json& settings)
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
