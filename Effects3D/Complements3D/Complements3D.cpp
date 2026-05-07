// SPDX-License-Identifier: GPL-2.0-only

#include "Complements3D.h"

#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StripShellPattern/StripShellPatternKernels.h"

#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Complements3D);

namespace
{
inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}

inline RGBColor Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    const float hf = h * 6.0f;
    const int i = (int)std::floor(hf) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch(i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }

    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}
} // namespace

Complements3D::Complements3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(45);
}

Complements3D::~Complements3D() = default;

EffectInfo3D Complements3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Complements 3D";
    info.effect_description =
        "Rotating hues along room depth (Z) with center dimming; choose how many tone steps span front to back, "
        "optional strip kernel palette for the pattern.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_COMPLEMENTS_3D;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = false;
    info.default_speed_scale = 45.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void Complements3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);
    QGridLayout* g = new QGridLayout();
    g->setContentsMargins(0, 0, 0, 0);

    g->addWidget(new QLabel("Depth tones:"), 0, 0);
    depth_tones_slider = new QSlider(Qt::Horizontal);
    depth_tones_slider->setRange(2, 32);
    depth_tones_slider->setToolTip(
        "How many tone steps span front to back (Z). 2 ~ complementary pair; higher = more hues across depth.");
    depth_tones_slider->setValue(std::clamp(depth_tone_count, 2, 32));
    depth_tones_label = new QLabel(QString::number(depth_tones_slider->value()));
    depth_tones_label->setMinimumWidth(28);
    g->addWidget(depth_tones_slider, 0, 1);
    g->addWidget(depth_tones_label, 0, 2);
    connect(depth_tones_slider, &QSlider::valueChanged, this, [this](int v) {
        depth_tone_count = std::clamp(v, 2, 32);
        if(depth_tones_label)
            depth_tones_label->setText(QString::number(depth_tone_count));
        emit ParametersChanged();
    });

    vbox->addLayout(g);

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(depth_tone_strip_cmap_on,
                                            depth_tone_strip_cmap_kernel,
                                            depth_tone_strip_cmap_rep,
                                            depth_tone_strip_cmap_unfold,
                                            depth_tone_strip_cmap_dir,
                                            depth_tone_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &Complements3D::SyncStripColormapFromPanel);

    AddWidgetToParent(w, parent);
}

void Complements3D::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    depth_tone_strip_cmap_on = strip_cmap_panel->useStripColormap();
    depth_tone_strip_cmap_kernel = strip_cmap_panel->kernelId();
    depth_tone_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    depth_tone_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    depth_tone_strip_cmap_dir = strip_cmap_panel->directionDeg();
    depth_tone_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void Complements3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_COMPLEMENTS_3D;
}

RGBColor Complements3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);
    float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = ny;

    const float spd = std::max(0.08f, GetScaledSpeed() / 100.0f);
    const float pos = Phase01(time, 10.0f, spd);

    const int dc = std::clamp(depth_tone_count, 2, 32);
    const float hue_span = (float)(dc - 1) / (float)dc;
    float hue01 = std::fmod(pos + nz * hue_span + 1.0f, 1.0f);

    const float percent_dim = 0.7f;
    float v = 1.0f;
    if(nz < 0.5f)
        v *= (1.0f - (nz * 2.0f * percent_dim));
    else
        v *= ((1.0f - percent_dim) + ((nz - 0.5f) * (nz * 2.0f * percent_dim)));
    v = std::clamp(v, 0.0f, 1.0f);

    const float size_m = std::max(0.2f, GetNormalizedSize());
    const float rainbow_rate = spd * 12.0f;

    if(depth_tone_strip_cmap_on)
    {
        const float ph01 =
            std::fmod(pos * 0.45f + nz * (0.2f + 0.55f * hue_span) + time * rainbow_rate * 0.012f + 1.0f, 1.0f);
        float pal01 = SampleStripKernelPalette01(depth_tone_strip_cmap_kernel,
                                                 depth_tone_strip_cmap_rep,
                                                 depth_tone_strip_cmap_unfold,
                                                 depth_tone_strip_cmap_dir,
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rot);
        pal01 = ApplySpatialPalette01(pal01, basis, sp, map, time, &grid);
        pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
        const int kid = StripShellKernelClamp(depth_tone_strip_cmap_kernel);
        RGBColor c = ResolveStripKernelFinalColor(*this,
                                                  kid,
                                                  std::clamp(pal01, 0.0f, 1.0f),
                                                  depth_tone_strip_cmap_color_style,
                                                  time,
                                                  rainbow_rate * 0.02f);
        const int cr = (int)(c & 0xFF);
        const int cg = (int)((c >> 8) & 0xFF);
        const int cb = (int)((c >> 16) & 0xFF);
        QColor qc = QColor::fromRgb(cr, cg, cb);
        const QColor hsv = qc.toHsv();
        const float ch = static_cast<float>(hsv.hueF());
        const float cs = static_cast<float>(hsv.saturationF());
        const float cv = static_cast<float>(hsv.valueF());
        const float h_use = (ch >= 0.0f) ? std::fmod(ch + 1.0f, 1.0f) : hue01;
        return Hsv01ToBgr(h_use, cs, std::clamp(v * cv, 0.0f, 1.0f));
    }

    if(GetRainbowMode())
    {
        float hue = hue01 * 360.0f;
        hue = ApplySpatialRainbowHue(hue, nz, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f) p01 += 1.0f;
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        return Hsv01ToBgr(p01, 1.0f, v);
    }

    float p = ApplySpatialPalette01(hue01, basis, sp, map, time, &grid);
    p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
    RGBColor c = GetColorAtPosition(p);
    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Complements3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["depth_tone_count"] = depth_tone_count;
    StripColormapSaveJson(j,
                          "depth_tone",
                          depth_tone_strip_cmap_on,
                          depth_tone_strip_cmap_kernel,
                          depth_tone_strip_cmap_rep,
                          depth_tone_strip_cmap_unfold,
                          depth_tone_strip_cmap_dir,
                          depth_tone_strip_cmap_color_style);
    return j;
}

void Complements3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("depth_tone_count") && settings["depth_tone_count"].is_number_integer())
        depth_tone_count = std::clamp(settings["depth_tone_count"].get<int>(), 2, 32);

    StripColormapLoadJson(settings,
                          "depth_tone",
                          depth_tone_strip_cmap_on,
                          depth_tone_strip_cmap_kernel,
                          depth_tone_strip_cmap_rep,
                          depth_tone_strip_cmap_unfold,
                          depth_tone_strip_cmap_dir,
                          depth_tone_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(depth_tone_strip_cmap_on,
                                                depth_tone_strip_cmap_kernel,
                                                depth_tone_strip_cmap_rep,
                                                depth_tone_strip_cmap_unfold,
                                                depth_tone_strip_cmap_dir,
                                                depth_tone_strip_cmap_color_style);
    }
    if(depth_tones_slider)
    {
        depth_tones_slider->setValue(depth_tone_count);
        if(depth_tones_label)
            depth_tones_label->setText(QString::number(depth_tone_count));
    }
}
