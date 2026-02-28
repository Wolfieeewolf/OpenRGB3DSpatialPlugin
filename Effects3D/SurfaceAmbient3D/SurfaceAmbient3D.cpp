// SPDX-License-Identifier: GPL-2.0-only

#include "SurfaceAmbient3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(SurfaceAmbient3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const char* SurfaceAmbient3D::StyleName(int s)
{
    switch(s) {
    case STYLE_FIRE: return "Fire";
    case STYLE_WATER: return "Water";
    case STYLE_SLIME: return "Slime";
    case STYLE_LAVA: return "Lava";
    case STYLE_EMBER: return "Ember (soft fire)";
    case STYLE_OCEAN: return "Ocean (deep water)";
    case STYLE_STEAM: return "Steam";
    default: return "Fire";
    }
}

float SurfaceAmbient3D::EvalPlasmaStyle(int style, float u, float v, float dist_norm, float time, float freq, float speed)
{
    float t = time * speed * (float)(2.0 * M_PI) * 0.5f;
    float f = freq * 8.0f;
    float val = 0.0f;
    switch(style)
    {
    case STYLE_FIRE:
        val = sinf(u * f + t) + sinf(v * f * 1.3f + t * 1.2f) +
              sinf((u + v) * f * 0.8f + t * 0.9f) + cosf((u - v) * f * 0.7f - t * 1.1f) +
              sinf(sqrtf(u*u + v*v) * f * 0.6f + t * 1.5f) * (1.0f - dist_norm * 0.5f);
        break;
    case STYLE_WATER:
        val = sinf(u * f + t * 0.8f) + cosf(v * f * 1.1f + t * 1.0f) +
              sinf((u * 0.7f + v * 1.2f) * f + t * 0.6f) +
              cosf(sqrtf((u-0.5f)*(u-0.5f) + (v-0.5f)*(v-0.5f)) * f * 1.2f - t * 1.2f);
        break;
    case STYLE_SLIME:
        val = sinf(u * f * 0.6f + sinf(v * f + t) * 0.5f + t * 0.4f) +
              cosf(v * f * 0.7f + cosf(u * f * 1.1f + t * 1.2f) * 0.5f) +
              sinf((u + v) * f * 0.5f + t * 0.3f);
        break;
    case STYLE_LAVA:
        val = sinf(u * f * 1.2f + t * 1.5f) + cosf(v * f * 0.9f - t * 1.3f) +
              sinf(sqrtf(u*u + v*v) * f * 1.0f + t * 2.0f) * (1.0f - dist_norm * 0.3f) +
              cosf((u - v) * f * 0.8f + t * 0.7f);
        break;
    case STYLE_EMBER:
        val = sinf(u * f * 0.5f + t * 0.6f) + sinf(v * f * 0.6f + t * 0.5f) +
              sinf((u + v) * f * 0.4f + t * 0.4f) * 0.7f +
              cosf(sqrtf(u*u + v*v) * f * 0.3f + t * 0.8f) * (1.0f - dist_norm * 0.6f);
        break;
    case STYLE_OCEAN:
        val = sinf(u * f * 1.0f + t * 0.7f) + cosf(v * f * 1.2f + t * 0.9f) +
              sinf((u * 0.8f + v * 1.1f) * f + t * 0.5f) * 0.8f +
              cosf(sqrtf((u-0.5f)*(u-0.5f) + (v-0.5f)*(v-0.5f)) * f * 1.5f - t * 1.0f) * 0.6f;
        break;
    case STYLE_STEAM:
        val = sinf(u * f * 0.4f + sinf(v * f * 0.5f + t * 0.3f) + t * 0.2f) +
              cosf(v * f * 0.45f + cosf(u * f * 0.6f + t * 0.4f)) +
              sinf((u + v) * f * 0.35f + t * 0.25f) * 0.5f;
        break;
    default:
        val = sinf(u * f + t) + sinf(v * f + t);
    }
    val = (val + 4.0f) / 8.0f;
    return std::max(0.0f, std::min(1.0f, val));
}

SurfaceAmbient3D::SurfaceAmbient3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D SurfaceAmbient3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Surface Fire/Water/Slime";
    info.effect_description = "Fire, water, slime, lava, ember, ocean, or steam on floor, ceiling, or walls";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 8.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void SurfaceAmbient3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;

    layout->addWidget(new QLabel("Style:"), row, 0);
    QComboBox* style_combo = new QComboBox();
    for(int s = 0; s < STYLE_COUNT; s++) style_combo->addItem(StyleName(s));
    style_combo->setCurrentIndex(std::max(0, std::min(style, STYLE_COUNT - 1)));
    layout->addWidget(style_combo, row, 1, 1, 2);
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Height:"), row, 0);
    QSlider* height_slider = new QSlider(Qt::Horizontal);
    height_slider->setRange(5, 100);
    height_slider->setValue((int)(height_pct * 100.0f));
    QLabel* height_label = new QLabel(QString::number((int)(height_pct * 100)) + "%");
    height_label->setMinimumWidth(36);
    layout->addWidget(height_slider, row, 1);
    layout->addWidget(height_label, row, 2);
    connect(height_slider, &QSlider::valueChanged, this, [this, height_label](int v){
        height_pct = v / 100.0f;
        if(height_label) height_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 50);
    thick_slider->setValue((int)(thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    AddWidgetToParent(w, parent);
}

void SurfaceAmbient3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor SurfaceAmbient3D::CalculateColor(float, float, float, float) { return 0x00000000; }

static void eval_surface(int surf, const GridContext3D& grid, float x, float y, float z,
    float& dist, float& u, float& v, float& extent)
{
    dist = 0.0f; u = 0.0f; v = 0.0f; extent = 0.0f;
    switch(surf)
    {
    case 1: extent = grid.height; dist = y - grid.min_y; u = (x - grid.min_x) / std::max(0.001f, grid.width); v = (z - grid.min_z) / std::max(0.001f, grid.depth); break;
    case 2: extent = grid.height; dist = grid.max_y - y; u = (x - grid.min_x) / std::max(0.001f, grid.width); v = (z - grid.min_z) / std::max(0.001f, grid.depth); break;
    case 4: extent = grid.width; dist = x - grid.min_x; u = (y - grid.min_y) / std::max(0.001f, grid.height); v = (z - grid.min_z) / std::max(0.001f, grid.depth); break;
    case 8: extent = grid.width; dist = grid.max_x - x; u = (y - grid.min_y) / std::max(0.001f, grid.height); v = (z - grid.min_z) / std::max(0.001f, grid.depth); break;
    case 16: extent = grid.depth; dist = z - grid.min_z; u = (x - grid.min_x) / std::max(0.001f, grid.width); v = (y - grid.min_y) / std::max(0.001f, grid.height); break;
    case 32: extent = grid.depth; dist = grid.max_z - z; u = (x - grid.min_x) / std::max(0.001f, grid.width); v = (y - grid.min_y) / std::max(0.001f, grid.height); break;
    default: break;
    }
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
}

RGBColor SurfaceAmbient3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    float h_pct = std::max(0.05f, std::min(1.0f, height_pct));
    float sigma = std::max(thickness * 0.5f, 0.02f);
    float freq = std::max(0.3f, std::min(3.0f, 0.3f + GetScaledFrequency() * 0.27f));
    float speed = std::max(0.0f, std::min(2.0f, GetScaledSpeed() / 4.0f));
    int mask = GetSurfaceMask();
    if(mask == 0) mask = 1;

    float best_intensity = 0.0f;
    float best_plasma = 0.0f;

    for(int bit = 1; bit <= 32; bit <<= 1)
    {
        if(!(mask & bit)) continue;
        float dist, u, v, extent;
        eval_surface(bit, grid, x, y, z, dist, u, v, extent);
        float height_ext = h_pct * extent;
        if(dist < 0.0f || dist > height_ext) continue;
        float dist_norm = (extent > 0.001f) ? (dist / height_ext) : 0.0f;
        float d_sigma = sigma * extent;
        float intensity = expf(-dist * dist / (d_sigma * d_sigma));
        float plasma = EvalPlasmaStyle(style, u, v, dist_norm, time, freq, speed);
        if(intensity > best_intensity) { best_intensity = intensity; best_plasma = plasma; }
    }

    if(best_intensity < 0.01f) return 0x00000000;

    float hue;
    if(GetRainbowMode() && style != STYLE_STEAM)
    {
        hue = fmodf(best_plasma * 360.0f + progress * 60.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
    }
    else if(style != STYLE_STEAM)
    {
        switch(style)
        {
        case STYLE_FIRE: hue = 20.0f + best_plasma * 40.0f; break;
        case STYLE_WATER: hue = 190.0f + best_plasma * 40.0f; break;
        case STYLE_SLIME: hue = 100.0f + best_plasma * 30.0f; break;
        case STYLE_LAVA: hue = 25.0f + best_plasma * 35.0f; break;
        case STYLE_EMBER: hue = 15.0f + best_plasma * 25.0f; break;
        case STYLE_OCEAN: hue = 200.0f + best_plasma * 30.0f; break;
        default: hue = best_plasma * 360.0f;
        }
        hue = fmodf(hue + progress * 20.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
    }

    RGBColor c;
    if(style == STYLE_STEAM)
    {
        unsigned char gv = (unsigned char)(180 + (int)(best_plasma * 75));
        c = (RGBColor)((gv << 16) | (gv << 8) | gv);
    }
    else if(GetRainbowMode())
        c = GetRainbowColor(hue);
    else
        c = GetColorAtPosition(best_plasma);
    float mult = best_intensity;
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * mult)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * mult)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * mult)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json SurfaceAmbient3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["style"] = style;
    j["height_pct"] = height_pct;
    j["thickness"] = thickness;
    return j;
}

void SurfaceAmbient3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("style") && settings["style"].is_number_integer())
        style = std::max(0, std::min(settings["style"].get<int>(), (int)STYLE_COUNT - 1));
    if(settings.contains("height_pct") && settings["height_pct"].is_number())
        height_pct = std::max(0.05f, std::min(1.0f, settings["height_pct"].get<float>()));
    if(settings.contains("thickness") && settings["thickness"].is_number())
        thickness = std::max(0.02f, std::min(0.5f, settings["thickness"].get<float>()));
}
