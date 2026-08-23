// SPDX-License-Identifier: GPL-2.0-only

#include "HarmonicPulse.h"
#include "HarmonicPulseVolumeFieldGlsl.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

#include <QColor>
#include <QComboBox>
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(HarmonicPulse);

const char* HarmonicPulse::ColorModeName(int m)
{
    switch(m)
    {
    case COLOR_MONO: return "One color (brightness pulse)";
    case COLOR_DUO_SNAP: return "Two colors (snap A↔B)";
    case COLOR_DUO_BLEND: return "Two colors (soft A↔B)";
    case COLOR_MULTI: return "Multi color (chase)";
    default: return "Two colors (snap A↔B)";
    }
}

RGBColor HarmonicPulse::Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    float r = 0, g = 0, b = 0;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch(i % 6)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return (RGBColor)((std::clamp((int)std::lround(b * 255.0f), 0, 255) << 16) |
                      (std::clamp((int)std::lround(g * 255.0f), 0, 255) << 8) |
                      std::clamp((int)std::lround(r * 255.0f), 0, 255));
}

RGBColor HarmonicPulse::ScaleColor(RGBColor c, float bright)
{
    bright = std::clamp(bright, 0.0f, 1.0f);
    int r = std::clamp((int)((float)(c & 0xFF) * bright), 0, 255);
    int g = std::clamp((int)((float)((c >> 8) & 0xFF) * bright), 0, 255);
    int b = std::clamp((int)((float)((c >> 16) & 0xFF) * bright), 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

RGBColor HarmonicPulse::LerpColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF;
    int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;
    int r = (int)((float)ar + ((float)br - (float)ar) * t);
    int g = (int)((float)ag + ((float)bg - (float)ag) * t);
    int bl = (int)((float)ab + ((float)bb - (float)ab) * t);
    return (RGBColor)((std::clamp(bl, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
}

HarmonicPulse::HarmonicPulse(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(50);
    SetSpeed(45);
    SetRainbowMode(false);
    std::vector<RGBColor> cols = {
        0x000000FF, // red
        0x00FF0000  // blue
    };
    if(GetColors().empty())
        SetColors(cols);
    volume_assist_.setFragmentBody(QString::fromUtf8(HarmonicPulseVolumeFieldGlsl()));
    volume_assist_.setResolution(18);
}

HarmonicPulse::~HarmonicPulse() = default;

EffectInfo3D HarmonicPulse::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Harmonic Pulse";
    info.effect_description =
        "Room-wide beat with optional spatial waves. Color modes: one-color brightness, two-color snap/soft A↔B, "
        "or multi-color chase. Speed sets the pulse rate; Detail densifies the waves; Size zooms the field.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_HARMONIC_PULSE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_speed_scale = 22.0f;
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

void HarmonicPulse::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* color_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Color pulse:"));
    color_row->setObjectName(QStringLiteral("colorPulseRow"));
    color_mode_combo = color_row->combo();
    for(int m = 0; m < COLOR_COUNT; m++)
        color_mode_combo->addItem(QString::fromUtf8(ColorModeName(m)));
    color_mode_combo->setCurrentIndex(std::clamp(color_mode, 0, COLOR_COUNT - 1));
    color_mode_combo->setToolTip(QStringLiteral(
        "One color: brightness breathes.\n"
        "Two snap: hard A↔B with the beat (clearest duo look).\n"
        "Two soft: blends A↔B with the beat.\n"
        "Multi: walks your palette with the beat."));
    connect(color_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        color_mode = std::clamp(idx, 0, COLOR_COUNT - 1);
        emit ParametersChanged();
    });

    EffectSliderRow* spatial_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Spatial waves:"),
        0,
        100,
        (int)std::lround(spatial_amount * 100.0f),
        QStringLiteral("0 = whole-room beat only; higher mixes in traveling harmonic waves (uses Detail)."));
    spatial_row->setObjectName(QStringLiteral("spatialWavesRow"));
    spatial_slider = spatial_row->slider();
    spatial_row->bindValueChanged(
        this, [this](int v) { spatial_amount = std::clamp(v / 100.0f, 0.0f, 1.0f); }, pct_format, on_changed);

    EffectSliderRow* flow_amount_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pulse energy:"),
        40,
        250,
        (int)std::lround(flow_amount * 100.0f),
        QStringLiteral("Extra multiplier on Speed/Frequency so the beat stays lively."));
    flow_amount_row->setObjectName(QStringLiteral("flowAmountRow"));
    flow_slider = flow_amount_row->slider();
    flow_amount_row->bindValueChanged(
        this,
        [this](int v) { flow_amount = std::clamp(v / 100.0f, 0.4f, 2.5f); },
        pct_format,
        on_changed);

    EffectSliderRow* pulse_contrast_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pulse contrast:"),
        35,
        200,
        (int)std::lround(pulse_contrast * 100.0f),
        QStringLiteral("Lower = softer breathing; higher = sharper on/off peaks."));
    pulse_contrast_row->setObjectName(QStringLiteral("pulseContrastRow"));
    contrast_slider = pulse_contrast_row->slider();
    pulse_contrast_row->bindValueChanged(
        this,
        [this](int v) { pulse_contrast = std::clamp(v / 100.0f, 0.35f, 2.0f); },
        pct_format,
        on_changed);

    EffectSliderRow* zoom_wobble_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Zoom wobble:"),
        0,
        300,
        (int)std::lround(zoom_wobble_strength * 100.0f),
        QStringLiteral("Gently breathes the spatial pattern scale (0 = stable)."));
    zoom_wobble_row->setObjectName(QStringLiteral("zoomWobbleRow"));
    wobble_slider = zoom_wobble_row->slider();
    zoom_wobble_row->bindValueChanged(
        this,
        [this](int v) { zoom_wobble_strength = std::clamp(v / 100.0f, 0.0f, 3.0f); },
        [this](int) { return QString::number(zoom_wobble_strength, 'f', 2); },
        on_changed);

    AddWidgetToParent(w, parent);
}

void HarmonicPulse::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    const float spd = std::max(0.05f, GetScaledSpeed());
    const float freq = std::max(0.05f, GetScaledFrequency());
    const float detail = std::max(0.05f, GetNormalizedDetail());
    const float size_m = std::max(0.25f, GetNormalizedSize());
    const float flow = std::clamp(flow_amount, 0.4f, 2.5f);
    const float motion = std::clamp((0.12f * spd + 0.05f * freq) * flow, 0.08f, 6.0f);
    const float spatial_freq = std::clamp(1.1f + detail * 4.5f * size_m * GetNormalizedScale(), 0.6f, 9.0f);
    const float pulse_mix = std::clamp(spatial_amount, 0.0f, 1.0f);
    const float contrast = std::clamp(pulse_contrast, 0.35f, 2.0f);
    const float wobble = std::clamp(zoom_wobble_strength, 0.0f, 3.0f);
    const float size_density = std::clamp(0.75f + 0.55f * size_m, 0.4f, 2.2f);
    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, GetEffectOriginGrid(grid), &ox, &oy, &oz);
    const float vp[9] = {motion, spatial_freq, wobble, contrast, size_density, pulse_mix, ox, oy, oz};
    volume_assist_.prepare(render_sequence, time_sec, vp, 9);
}

RGBColor HarmonicPulse::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot{x, y, z};
    float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    // Real motion — do not divide ScaledSpeed into oblivion.
    const float spd = std::max(0.05f, GetScaledSpeed());
    const float freq = std::max(0.05f, GetScaledFrequency());
    const float size_m = std::max(0.25f, GetNormalizedSize());
    const float flow = std::clamp(flow_amount, 0.4f, 2.5f);
    const float motion = std::clamp((0.12f * spd + 0.05f * freq) * flow, 0.08f, 6.0f);
    const float pulse_mix = std::clamp(spatial_amount, 0.0f, 1.0f);

    float val = 0.0f;
    float phase01 = 0.0f;
    float master = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        val = std::clamp(samp.x(), 0.0f, 1.0f);
        phase01 = std::clamp(samp.y(), 0.0f, 1.0f);
        master = std::clamp(samp.z(), 0.0f, 1.0f);
    }


    if(UseEffectStripColormap())
    {
        const float ph01 = std::fmod(phase01 + master * 0.25f + 1.f, 1.f);
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
        return ScaleColor(ResolveStripKernelFinalColor(SpatialPatternKernelClamp(GetEffectStripColormapKernel()), std::clamp(pal01, 0.0f, 1.0f), time),
                          val);
    }

    if(GetRainbowMode())
    {
        float hh = std::fmod(phase01 + time * motion * 0.35f + 1.0f, 1.0f);
        return Hsv01ToBgr(hh, 1.0f, val);
    }

    const std::vector<RGBColor>& cols = GetColors();
    const int mode = std::clamp(color_mode, 0, COLOR_COUNT - 1);

    if(mode == COLOR_MONO || cols.empty())
    {
        RGBColor c = cols.empty() ? (RGBColor)0x000000FF : cols[0];
        return ScaleColor(c, val);
    }

    if(mode == COLOR_DUO_SNAP || mode == COLOR_DUO_BLEND)
    {
        RGBColor a = cols[0];
        RGBColor b = (cols.size() >= 2) ? cols[1] : ScaleColor(cols[0], 0.25f);
        // Drive A↔B from the master beat so the whole room reads as a color pulse.
        const float drive = (pulse_mix < 0.35f) ? master : (0.55f * master + 0.45f * val);
        if(mode == COLOR_DUO_SNAP)
        {
            RGBColor pick = (drive >= 0.5f) ? b : a;
            // Keep some brightness motion so snap isn't a flat cut.
            return ScaleColor(pick, 0.35f + 0.65f * val);
        }
        return ScaleColor(LerpColor(a, b, drive), 0.40f + 0.60f * val);
    }

    // Multi chase through palette.
    const int n = std::max(1, (int)cols.size());
    float u = std::fmod(master + phase01 * 0.35f + 1.0f, 1.0f) * (float)n;
    int i0 = std::clamp((int)std::floor(u), 0, n - 1);
    int i1 = (i0 + 1) % n;
    float frac = u - std::floor(u);
    return ScaleColor(LerpColor(cols[(size_t)i0], cols[(size_t)i1], frac), val);
}

nlohmann::json HarmonicPulse::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["harmonic_color_mode"] = color_mode;
    j["harmonic_zoom_wobble"] = zoom_wobble_strength;
    j["harmonic_flow_amount"] = flow_amount;
    j["harmonic_pulse_contrast"] = pulse_contrast;
    j["harmonic_spatial_amount"] = spatial_amount;
    return j;
}

void HarmonicPulse::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("harmonic_color_mode") && settings["harmonic_color_mode"].is_number_integer())
        color_mode = std::clamp(settings["harmonic_color_mode"].get<int>(), 0, COLOR_COUNT - 1);
    if(settings.contains("harmonic_zoom_wobble") && settings["harmonic_zoom_wobble"].is_number())
        zoom_wobble_strength = std::clamp(settings["harmonic_zoom_wobble"].get<float>(), 0.0f, 3.0f);
    if(settings.contains("harmonic_flow_amount") && settings["harmonic_flow_amount"].is_number())
        flow_amount = std::clamp(settings["harmonic_flow_amount"].get<float>(), 0.4f, 2.5f);
    if(settings.contains("harmonic_pulse_contrast") && settings["harmonic_pulse_contrast"].is_number())
        pulse_contrast = std::clamp(settings["harmonic_pulse_contrast"].get<float>(), 0.35f, 2.0f);
    if(settings.contains("harmonic_spatial_amount") && settings["harmonic_spatial_amount"].is_number())
        spatial_amount = std::clamp(settings["harmonic_spatial_amount"].get<float>(), 0.0f, 1.0f);

    if(color_mode_combo)
        color_mode_combo->setCurrentIndex(color_mode);
    if(wobble_slider)
        wobble_slider->setValue((int)std::lround(zoom_wobble_strength * 100.0f));
    if(flow_slider)
        flow_slider->setValue((int)std::lround(flow_amount * 100.0f));
    if(contrast_slider)
        contrast_slider->setValue((int)std::lround(pulse_contrast * 100.0f));
    if(spatial_slider)
        spatial_slider->setValue((int)std::lround(spatial_amount * 100.0f));
}
