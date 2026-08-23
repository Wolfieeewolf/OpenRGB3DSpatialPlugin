// SPDX-License-Identifier: GPL-2.0-only

#include "DepthTone.h"
#include "DepthToneVolumeFieldGlsl.h"
#include "EffectHelpers.h"

#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

#include <QColor>
#include <QComboBox>
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(DepthTone);

namespace
{
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
}

const char* DepthTone::AxisName(int a)
{
    switch(a)
    {
    case AXIS_X: return "Width (X)";
    case AXIS_Y: return "Height (Y)";
    case AXIS_Z: return "Depth (Z)";
    default: return "Depth (Z)";
    }
}

const char* DepthTone::LayoutName(int L)
{
    switch(L)
    {
    case LAYOUT_LINEAR: return "Linear (axis)";
    case LAYOUT_CENTER: return "From room center";
    case LAYOUT_REF: return "From ref point";
    default: return "Linear (axis)";
    }
}

DepthTone::DepthTone(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(45);
    SetFrequency(30);
    volume_assist_.setFragmentBody(QString::fromUtf8(DepthToneVolumeFieldGlsl()));
    volume_assist_.setResolution(18);
}

DepthTone::~DepthTone() = default;

EffectInfo3D DepthTone::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Depth Tone";
    info.effect_description =
        "Hue mapped along an axis or radiating from room center / ref point, with optional center dimming. "
        "Speed scrolls hue; Frequency adds a second drift; Size zooms the gradient; Detail sharpens tone steps.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_DEPTH_TONE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_frequency = true;
    info.default_speed_scale = 18.0f;
    info.default_frequency_scale = 14.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_strip_colormap = true;

    return info;
}

void DepthTone::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    EffectLabeledComboRow* axis_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Axis:"));
    axis_row->setObjectName(QStringLiteral("axisRow"));
    axis_combo = axis_row->combo();
    for(int a = 0; a < AXIS_COUNT; a++)
        axis_combo->addItem(QString::fromUtf8(AxisName(a)));
    axis_combo->setCurrentIndex(std::clamp(depth_axis, 0, AXIS_COUNT - 1));
    axis_combo->setToolTip(QStringLiteral("Which room axis carries the tone when Layout is Linear."));
    axis_combo->setEnabled(depth_layout == LAYOUT_LINEAR);
    connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        depth_axis = std::clamp(idx, 0, AXIS_COUNT - 1);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* layout_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Layout:"));
    layout_row->setObjectName(QStringLiteral("layoutRow"));
    layout_combo = layout_row->combo();
    for(int L = 0; L < LAYOUT_COUNT; L++)
        layout_combo->addItem(QString::fromUtf8(LayoutName(L)));
    layout_combo->setCurrentIndex(std::clamp(depth_layout, 0, LAYOUT_COUNT - 1));
    layout_combo->setToolTip(QStringLiteral(
        "Linear: tones along the chosen axis.\n"
        "From room center / ref point: tones radiate by distance (Axis is unused)."));
    connect(layout_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        depth_layout = std::clamp(idx, 0, LAYOUT_COUNT - 1);
        if(axis_combo)
            axis_combo->setEnabled(depth_layout == LAYOUT_LINEAR);
        emit ParametersChanged();
    });

    EffectSliderRow* depth_tones_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Depth tones:"),
        2,
        32,
        std::clamp(depth_tone_count, 2, 32),
        QStringLiteral(
            "How many tone steps span the layout. 2 ~ complementary pair; higher = more hues across depth."));
    depth_tones_row->setObjectName(QStringLiteral("depthTonesRow"));
    depth_tones_slider = depth_tones_row->slider();
    depth_tones_row->bindValueChanged(
        this,
        [this](int v) { depth_tone_count = std::clamp(v, 2, 32); },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* dim_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Center dim:"),
        0,
        100,
        (int)std::lround(dim_amount * 100.0f),
        QStringLiteral("Darkens toward the middle of the mapped range (0 = flat brightness)."));
    dim_row->setObjectName(QStringLiteral("centerDimRow"));
    dim_slider = dim_row->slider();
    dim_row->bindValueChanged(
        this,
        [this](int v) { dim_amount = std::clamp(v / 100.0f, 0.0f, 1.0f); },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);

    AddWidgetToParent(w, parent);
}

void DepthTone::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const float spd = std::max(0.05f, GetScaledSpeed());
    const float freq = std::max(0.05f, GetScaledFrequency());
    const float pos = std::fmod(time_sec * spd * 0.085f + time_sec * freq * 0.028f + 1000.0f, 1.0f);
    const int dc = std::clamp(depth_tone_count, 2, 32);
    const float hue_span = (float)(dc - 1) / (float)dc;
    const float percent_dim = std::clamp(dim_amount, 0.0f, 1.0f);
    const int axis = std::clamp(depth_axis, 0, AXIS_COUNT - 1);
    const int layout_i = std::clamp(depth_layout, 0, LAYOUT_COUNT - 1);
    const float size_zoom = std::clamp(GetNormalizedSize() * (0.65f + 0.55f * GetNormalizedScale()), 0.15f, 2.5f);
    const float detail_norm = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float vp[7] = {
        pos,
        hue_span,
        percent_dim,
        (float)axis,
        (float)layout_i,
        size_zoom,
        detail_norm
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 7);
}

RGBColor DepthTone::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot{x, y, z};
    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rot.x, rot.y, rot.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);

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
    sp.y_norm = SampleStratumYNorm01(rot.y, grid, origin);

    // Speed = primary hue scroll; Frequency = secondary drift (both clearly audible in motion).
    const float spd = std::max(0.05f, GetScaledSpeed());
    const float freq = std::max(0.05f, GetScaledFrequency());
    const float pos = std::fmod(time * spd * 0.085f + time * freq * 0.028f + 1000.0f, 1.0f);

    float hue01 = 0.0f;
    float v = 1.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        const float hue_rad = std::atan2(samp.y() * 2.0f - 1.0f, samp.x() * 2.0f - 1.0f);
        hue01 = std::fmod(hue_rad / TWO_PI + 1.0f, 1.0f);
        v = std::clamp(samp.z(), 0.0f, 1.0f);
    }
    else
        return 0x00000000;

    const float size_m = std::max(0.2f, GetNormalizedSize());
    const float rainbow_rate = spd * 0.35f + freq * 0.12f;

    if(UseEffectStripColormap())
    {
        const float ph01 =
            std::fmod(pos * 0.45f + hue01 * 0.55f + time * rainbow_rate * 0.01f + 1.f, 1.f);
        float pal01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rot);
        pal01 = ApplySpatialPalette01(pal01, basis, sp, map, time, &grid);
        const int kid = SpatialPatternKernelClamp(GetEffectStripColormapKernel());
        RGBColor c = ResolveStripKernelFinalColor(kid, std::clamp(pal01, 0.0f, 1.0f), time);
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
        hue = ApplySpatialRainbowHue(hue, hue01, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
            p01 += 1.0f;
        return Hsv01ToBgr(p01, 1.0f, v);
    }

    float p = ApplySpatialPalette01(hue01, basis, sp, map, time, &grid);
    RGBColor c = GetColorAtPosition(p);
    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json DepthTone::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["depth_tone_count"] = depth_tone_count;
    j["depth_axis"] = depth_axis;
    j["depth_layout"] = depth_layout;
    j["depth_dim_amount"] = dim_amount;
    return j;
}

void DepthTone::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("depth_tone_count") && settings["depth_tone_count"].is_number_integer())
        depth_tone_count = std::clamp(settings["depth_tone_count"].get<int>(), 2, 32);
    if(settings.contains("depth_axis") && settings["depth_axis"].is_number_integer())
        depth_axis = std::clamp(settings["depth_axis"].get<int>(), 0, AXIS_COUNT - 1);
    if(settings.contains("depth_layout") && settings["depth_layout"].is_number_integer())
        depth_layout = std::clamp(settings["depth_layout"].get<int>(), 0, LAYOUT_COUNT - 1);
    if(settings.contains("depth_dim_amount") && settings["depth_dim_amount"].is_number())
        dim_amount = std::clamp(settings["depth_dim_amount"].get<float>(), 0.0f, 1.0f);
    if(depth_tones_slider)
        depth_tones_slider->setValue(depth_tone_count);
    if(dim_slider)
        dim_slider->setValue((int)std::lround(dim_amount * 100.0f));
    if(axis_combo)
    {
        axis_combo->setCurrentIndex(std::clamp(depth_axis, 0, AXIS_COUNT - 1));
        axis_combo->setEnabled(depth_layout == LAYOUT_LINEAR);
    }
    if(layout_combo)
        layout_combo->setCurrentIndex(std::clamp(depth_layout, 0, LAYOUT_COUNT - 1));
}
