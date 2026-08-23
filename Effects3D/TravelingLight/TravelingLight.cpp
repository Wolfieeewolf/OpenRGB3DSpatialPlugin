// SPDX-License-Identifier: GPL-2.0-only

#include "TravelingLight.h"
#include "TravelingLightVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "../EffectHelpers.h"
#include <QComboBox>
#include <QVector3D>
#include <QVBoxLayout>
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned char screen_blend(unsigned char a, unsigned char b)
{
    return (unsigned char)(255 - ((255 - a) * (255 - b) / 255));
}

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

const char* TravelingLight::ModeName(int m)
{
    switch(m) {
    case MODE_COMET:        return "Comet";
    case MODE_CHASE:        return "Chase (multi)";
    case MODE_MARQUEE:      return "Marquee (band)";
    case MODE_ZIGZAG:       return "ZigZag (snake)";
    case MODE_KITT:         return "KITT Scanner";
    case MODE_WIPE:         return "Wipe";
    case MODE_MOVING_PANES: return "Moving Panes";
    case MODE_CROSSING:     return "Crossing Beams";
    case MODE_ROTATING:     return "Rotating Beam";
    case MODE_WAVE_FRONTS:  return "Wave Fronts";
    default: return "Comet";
    }
}

float TravelingLight::smoothstep(float edge0, float edge1, float x) const
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

TravelingLight::TravelingLight(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
    volume_assist_.setFragmentBody(QString::fromUtf8(TravelingLightVolumeFieldGlsl()));
    volume_assist_.setResolution(22);
}

void TravelingLight::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    float progress = CalculateProgress(time_sec) * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, 0.0f);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;

    const float size_scale = GetNormalizedSize() / 1.5f;
    const float tight_inv = 1.0f / std::max(0.25f, bb.tight_mul);
    const float freq_n = std::min(6.0f, std::max(0.02f, GetScaledFrequency() * 0.065f));

    float vp[13] = {
        (float)std::clamp(mode, 0, MODE_COUNT - 1),
        progress,
        size_scale,
        tight_inv,
        (float)GetPathAxis(),
        (float)GetPlane(),
        std::clamp(glow, 0.1f, 1.0f),
        (float)std::clamp(wipe_edge_shape, 0, 2),
        (float)std::clamp(num_divisions, 2, 16),
        (float)std::clamp(front_shape, 0, 3),
        (float)std::clamp(front_edge, 0, 2),
        std::clamp(front_thickness, 5, 100) / 100.0f,
        freq_n
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 13);
}

EffectInfo3D TravelingLight::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Traveling Light";
    info.effect_description =
        "Comet, Chase, Marquee, ZigZag, KITT, Wipe, Moving Panes, Crossing Beams, Rotating Beam, or Wave Fronts. "
        "Wave Fronts are soft traveling rings/bands (Circles, Squares, Lines, Diagonal).";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_COMET;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.needs_frequency = true;
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

void TravelingLight::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("TravelingLightEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* style_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Style:"));
    style_row->setObjectName(QStringLiteral("styleRow"));
    QComboBox* mode_combo = style_row->combo();
    for(int m = 0; m < MODE_COUNT; m++)
    {
        mode_combo->addItem(ModeName(m));
    }
    mode_combo->setCurrentIndex(std::max(0, std::min(this->mode, MODE_COUNT - 1)));
    mode_combo->setToolTip(QStringLiteral(
        "How the light path moves through the grid. Wipe uses Wipe edge; Wave Fronts uses Shape/Edge/Thickness; beams use Glow."));
    mode_combo->setItemData(0, QStringLiteral("Single bright head with tail."), Qt::ToolTipRole);
    mode_combo->setItemData(1, QStringLiteral("Several heads spaced along the path."), Qt::ToolTipRole);
    mode_combo->setItemData(2, QStringLiteral("Wide lit band that travels."), Qt::ToolTipRole);
    mode_combo->setItemData(3, QStringLiteral("Snake path that folds across axes."), Qt::ToolTipRole);
    mode_combo->setItemData(4, QStringLiteral("Back-and-forth scanner bars."), Qt::ToolTipRole);
    mode_combo->setItemData(5, QStringLiteral("Plane that sweeps across the volume."), Qt::ToolTipRole);
    mode_combo->setItemData(6, QStringLiteral("Multiple parallel sheets in motion."), Qt::ToolTipRole);
    mode_combo->setItemData(7, QStringLiteral("Two crossing beams with glow control."), Qt::ToolTipRole);
    mode_combo->setItemData(8, QStringLiteral("One beam rotating around the vertical axis."), Qt::ToolTipRole);
    mode_combo->setItemData(9, QStringLiteral(
        "Soft traveling bands: Circles/Squares expand from the origin; Lines/Diagonal sweep across the room."),
        Qt::ToolTipRole);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        this->mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        UpdateWaveFrontsControlsVisible();
        emit ParametersChanged();
    });

    wave_fronts_panel = new QWidget();
    wave_fronts_panel->setObjectName(QStringLiteral("waveFrontsPanel"));
    QVBoxLayout* fronts_layout = new QVBoxLayout(wave_fronts_panel);
    fronts_layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(wave_fronts_panel);

    EffectLabeledComboRow* front_shape_row = EffectUiRows::AppendComboRow(fronts_layout, QStringLiteral("Front shape:"));
    front_shape_row->setObjectName(QStringLiteral("frontShapeRow"));
    front_shape_combo = front_shape_row->combo();
    front_shape_combo->addItem(QStringLiteral("Circles"));
    front_shape_combo->addItem(QStringLiteral("Squares"));
    front_shape_combo->addItem(QStringLiteral("Lines"));
    front_shape_combo->addItem(QStringLiteral("Diagonal"));
    front_shape_combo->setCurrentIndex(std::clamp(front_shape, 0, 3));
    front_shape_combo->setToolTip(QStringLiteral(
        "Circles/Squares: expanding rings from the origin. Lines/Diagonal: soft bands sweeping across the room."));
    front_shape_combo->setItemData(0, QStringLiteral("Expanding circular rings from the effect origin."), Qt::ToolTipRole);
    front_shape_combo->setItemData(1, QStringLiteral("Expanding square rings (Chebyshev) from the origin."), Qt::ToolTipRole);
    front_shape_combo->setItemData(2, QStringLiteral("Soft vertical band sweeping along X."), Qt::ToolTipRole);
    front_shape_combo->setItemData(3, QStringLiteral("Soft band sweeping on the X+Z diagonal."), Qt::ToolTipRole);
    connect(front_shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        front_shape = std::clamp(idx, 0, 3);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* front_edge_row = EffectUiRows::AppendComboRow(fronts_layout, QStringLiteral("Front edge:"));
    front_edge_row->setObjectName(QStringLiteral("frontEdgeRow"));
    front_edge_combo = front_edge_row->combo();
    front_edge_combo->addItem(QStringLiteral("Round"));
    front_edge_combo->addItem(QStringLiteral("Sharp"));
    front_edge_combo->addItem(QStringLiteral("Square"));
    front_edge_combo->setCurrentIndex(std::clamp(front_edge, 0, 2));
    front_edge_combo->setToolTip(QStringLiteral("Cross-section of the traveling front band."));
    front_edge_combo->setItemData(0, QStringLiteral("Soft cosine falloff at the band edges."), Qt::ToolTipRole);
    front_edge_combo->setItemData(1, QStringLiteral("Hard edge—crisp on-off band."), Qt::ToolTipRole);
    front_edge_combo->setItemData(2, QStringLiteral("Flat-top band with steep sides."), Qt::ToolTipRole);
    connect(front_edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        front_edge = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    EffectSliderRow* front_thickness_row = EffectUiRows::AppendSliderRow(
        fronts_layout,
        QStringLiteral("Front thickness:"),
        5,
        100,
        front_thickness,
        QStringLiteral("Traveling band thickness (higher = wider band)."));
    front_thickness_row->setObjectName(QStringLiteral("frontThicknessRow"));
    front_thickness_slider = front_thickness_row->slider();
    front_thickness_row->bindValueChanged(
        this, [this](int v) { front_thickness = v; }, [](int v) { return QString::number(v); }, on_changed);

    UpdateWaveFrontsControlsVisible();

    EffectLabeledComboRow* wipe_edge_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Wipe edge:"));
    wipe_edge_row->setObjectName(QStringLiteral("wipeEdgeRow"));
    QComboBox* wipe_edge_combo = wipe_edge_row->combo();
    wipe_edge_combo->addItem(QStringLiteral("Round"));
    wipe_edge_combo->addItem(QStringLiteral("Sharp"));
    wipe_edge_combo->addItem(QStringLiteral("Square"));
    wipe_edge_combo->setCurrentIndex(std::clamp(wipe_edge_shape, 0, 2));
    wipe_edge_combo->setToolTip(QStringLiteral("Cross-section of the wipe plane (Wipe style only)."));
    wipe_edge_combo->setItemData(0, QStringLiteral("Soft falloff at the lit boundary."), Qt::ToolTipRole);
    wipe_edge_combo->setItemData(1, QStringLiteral("Hard on/off boundary."), Qt::ToolTipRole);
    wipe_edge_combo->setItemData(2, QStringLiteral("Flat lit region with steep sides."), Qt::ToolTipRole);
    connect(wipe_edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        wipe_edge_shape = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    EffectSliderRow* panes_divisions_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Panes divisions:"),
        2,
        16,
        num_divisions,
        QStringLiteral("Number of bands or cells for Moving Panes and similar styles."));
    panes_divisions_row->setObjectName(QStringLiteral("panesDivisionsRow"));
    panes_divisions_row->bindValueChanged(
        this, [this](int v) { num_divisions = v; }, [](int v) { return QString::number(v); }, on_changed);

    EffectSliderRow* glow_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Glow (beams):"),
        10,
        100,
        (int)(glow * 100.0f),
        QStringLiteral("Beam softness for Crossing Beams and Rotating Beam (wider halo = higher)."));
    glow_row->setObjectName(QStringLiteral("glowRow"));
    glow_row->bindValueChanged(
        this, [this](int v) { glow = v / 100.0f; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

void TravelingLight::UpdateWaveFrontsControlsVisible()
{
    if(wave_fronts_panel)
        wave_fronts_panel->setVisible(mode == MODE_WAVE_FRONTS);
}

RGBColor TravelingLight::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated{x, y, z};
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float coord2 = SampleStratumYNorm01(rotated.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);


    float progress = CalculateProgress(time) * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;

    float color_cycle = progress * GetScaledFrequency() * 3.0f;

    const float size_m_tl = GetNormalizedSize();
    const float tl_phase01 = std::fmod(progress + color_cycle * (1.f / 360.f) + 1.f, 1.f);
    float tl_strip_p01 = 0.f;
    if(UseEffectStripColormap())
    {
        tl_strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                  GetEffectStripColormapRepeats(),
                                                  GetEffectStripColormapUnfold(),
                                                  GetEffectStripColormapDirectionDeg(),
                                                  tl_phase01,
                                                  time,
                                                  grid,
                                                  size_m_tl,
                                                  origin,
                                                  rotated);
    }
    auto tl_strip_rgb = [&](float p01_k) -> RGBColor {
        float pv = p01_k;
        return ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), pv, time);
    };
    auto tl_palette = [&](float p01) -> RGBColor {
        if(UseEffectStripColormap())
            return tl_strip_rgb(tl_strip_p01);
        return GetColorAtPosition(p01);
    };

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;
    auto tl_rainbow_hue = [&](float hue_deg, float driver01) -> RGBColor {
        if(UseEffectStripColormap())
            return tl_strip_rgb(tl_strip_p01);
        hue_deg = ApplySpatialRainbowHue(hue_deg, driver01, basis, sp, map, time, &grid);
        return GetRainbowColor(hue_deg);
    };

    if(volume_assist_.isAvailable())
    {
        float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
        SampleGpuVolumeOriginLocal01(rotated.x, rotated.y, rotated.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        const int m = std::max(0, std::min(this->mode, MODE_COUNT - 1));

        if(m == MODE_MOVING_PANES)
        {
            const float s = samp.y();
            const bool zone_id = samp.z() > 0.5f;
            RGBColor c0, c1;
            if(UseEffectStripColormap())
            {
                c0 = tl_strip_rgb(tl_strip_p01);
                c1 = tl_strip_rgb(std::fmod(tl_strip_p01 + 0.5f, 1.0f));
            }
            else if(GetRainbowMode())
            {
                c0 = tl_rainbow_hue(progress * 60.0f + color_cycle, progress);
                c1 = tl_rainbow_hue(progress * 60.0f + 180.0f + color_cycle, std::fmod(progress + 0.5f, 1.0f));
            }
            else
            {
                const std::vector<RGBColor>& cols = GetColors();
                c0 = (cols.size() > 0) ? cols[0] : 0x000000FF;
                c1 = (cols.size() > 1) ? cols[1] : 0x00FF0000;
            }
            return lerp_color(zone_id ? c1 : c0, zone_id ? c0 : c1, s);
        }
        if(m == MODE_CROSSING)
        {
            const float v1 = samp.x();
            const float v2 = samp.y();
            RGBColor c1, c2;
            if(UseEffectStripColormap())
            {
                c1 = tl_strip_rgb(tl_strip_p01);
                c2 = tl_strip_rgb(std::fmod(tl_strip_p01 + 0.5f, 1.0f));
            }
            else if(GetRainbowMode())
            {
                c1 = tl_rainbow_hue(progress * 120.0f + color_cycle, progress);
                c2 = tl_rainbow_hue(progress * 120.0f + 180.0f + color_cycle, std::fmod(progress + 0.5f, 1.0f));
            }
            else
            {
                const std::vector<RGBColor>& cols = GetColors();
                c1 = (cols.size() > 0) ? cols[0] : 0x000000FF;
                c2 = (cols.size() > 1) ? cols[1] : 0x0000FF00;
            }
            unsigned char r1 = (unsigned char)((c1 & 0xFF) * v1);
            unsigned char g1 = (unsigned char)(((c1 >> 8) & 0xFF) * v1);
            unsigned char b1 = (unsigned char)(((c1 >> 16) & 0xFF) * v1);
            unsigned char r2 = (unsigned char)((c2 & 0xFF) * v2);
            unsigned char g2 = (unsigned char)(((c2 >> 8) & 0xFF) * v2);
            unsigned char b2 = (unsigned char)(((c2 >> 16) & 0xFF) * v2);
            return (RGBColor)((screen_blend(b1, b2) << 16) | (screen_blend(g1, g2) << 8) | screen_blend(r1, r2));
        }

        float intensity = samp.x();
        if(intensity < 0.01f)
            return 0x00000000;
        const float driver = samp.y();
        RGBColor c = GetRainbowMode() ? tl_rainbow_hue(driver * 360.0f + color_cycle, driver)
                                      : tl_palette(driver);
        unsigned char r = (unsigned char)((c & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    return 0x00000000;
}

nlohmann::json TravelingLight::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = this->mode;
    j["glow"] = glow;
    j["wipe_edge_shape"] = wipe_edge_shape;
    j["num_divisions"] = num_divisions;
    j["front_shape"] = front_shape;
    j["front_edge"] = front_edge;
    j["front_thickness"] = front_thickness;
    return j;
}

void TravelingLight::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        this->mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("glow") && settings["glow"].is_number())
        glow = std::max(0.1f, std::min(1.0f, settings["glow"].get<float>()));
    if(settings.contains("wipe_edge_shape") && settings["wipe_edge_shape"].is_number_integer())
        wipe_edge_shape = std::clamp(settings["wipe_edge_shape"].get<int>(), 0, 2);
    if(settings.contains("num_divisions") && settings["num_divisions"].is_number_integer())
        num_divisions = std::max(2, std::min(16, settings["num_divisions"].get<int>()));
    if(settings.contains("front_shape") && settings["front_shape"].is_number_integer())
        front_shape = std::clamp(settings["front_shape"].get<int>(), 0, 3);
    if(settings.contains("front_edge") && settings["front_edge"].is_number_integer())
        front_edge = std::clamp(settings["front_edge"].get<int>(), 0, 2);
    if(settings.contains("front_thickness") && settings["front_thickness"].is_number_integer())
        front_thickness = std::clamp(settings["front_thickness"].get<int>(), 5, 100);

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "TravelingLightEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "styleRow", mode);
            EffectUiSync::setComboIndex(fx, "wipeEdgeRow", wipe_edge_shape);
            EffectUiSync::setSliderValue(fx, "panesDivisionsRow", num_divisions, [](int v) { return QString::number(v); });
            EffectUiSync::setSliderValue(fx, "glowRow", (int)(glow * 100.0f),
                                          [](int v) { return QString::number(v) + QStringLiteral("%"); });
            EffectUiSync::setComboIndex(fx, "frontShapeRow", front_shape);
            EffectUiSync::setComboIndex(fx, "frontEdgeRow", front_edge);
            EffectUiSync::setSliderValue(fx, "frontThicknessRow", front_thickness, [](int v) { return QString::number(v); });
        }
    }
    if(front_shape_combo)
        front_shape_combo->setCurrentIndex(front_shape);
    if(front_edge_combo)
        front_edge_combo->setCurrentIndex(front_edge);
    if(front_thickness_slider)
        front_thickness_slider->setValue(front_thickness);
    UpdateWaveFrontsControlsVisible();
}

REGISTER_EFFECT_3D(TravelingLight);
