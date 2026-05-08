// SPDX-License-Identifier: GPL-2.0-only
//
// Algorithm: time-varying axis-angle rotation in normalized centered cube, then
// dist = |z| - sqrt((x*x + y*y) / scale) (double cone), HSV from dist.
// Suitable for volumetric LED layouts; coordinates match per-axis 0..1 room mapping.

#include "RotatingConeSpotlights3D.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include <QColor>
#include <QLabel>
#include <QGridLayout>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

REGISTER_EFFECT_3D(RotatingConeSpotlights3D);

namespace
{
float Triangle01(float phase01)
{
    float p = phase01 - std::floor(phase01);
    return 1.0f - std::fabs(2.0f * p - 1.0f);
}
} // namespace

RGBColor RotatingConeSpotlights3D::Hsv01ToBgr(float h, float s, float v)
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
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break;
    }
    int ri = (int)std::lround(r * 255.0f);
    int gi = (int)std::lround(g * 255.0f);
    int bi = (int)std::lround(b * 255.0f);
    ri = std::clamp(ri, 0, 255);
    gi = std::clamp(gi, 0, 255);
    bi = std::clamp(bi, 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

void RotatingConeSpotlights3D::SetupRotationMatrix(float ux, float uy, float uz, float angle_rad, float R[3][3])
{
    float len = std::sqrt(ux * ux + uy * uy + uz * uz);
    if(len < 1e-5f)
    {
        ux = 0.0f;
        uy = 0.0f;
        uz = 1.0f;
        len = 1.0f;
    }
    ux /= len;
    uy /= len;
    uz /= len;
    float cosa = std::cos(angle_rad);
    float sina = std::sin(angle_rad);
    float ccosa = 1.0f - cosa;
    float xyccosa = ux * uy * ccosa;
    float xzccosa = ux * uz * ccosa;
    float yzccosa = uy * uz * ccosa;
    float xsina = ux * sina;
    float ysina = uy * sina;
    float zsina = uz * sina;
    R[0][0] = cosa + ux * ux * ccosa;
    R[0][1] = xyccosa - zsina;
    R[0][2] = xzccosa + ysina;
    R[1][0] = xyccosa + zsina;
    R[1][1] = cosa + uy * uy * ccosa;
    R[1][2] = yzccosa - xsina;
    R[2][0] = xzccosa - ysina;
    R[2][1] = yzccosa + xsina;
    R[2][2] = cosa + uz * uz * ccosa;
}

void RotatingConeSpotlights3D::Rotate3D(float x, float y, float z, const float R[3][3], float* rx, float* ry, float* rz)
{
    *rx = R[0][0] * x + R[0][1] * y + R[0][2] * z;
    *ry = R[1][0] * x + R[1][1] * y + R[1][2] * z;
    *rz = R[2][0] * x + R[2][1] * y + R[2][2] * z;
}

RotatingConeSpotlights3D::RotatingConeSpotlights3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(50);
    SetRainbowMode(false);
}

RotatingConeSpotlights3D::~RotatingConeSpotlights3D() = default;

EffectInfo3D RotatingConeSpotlights3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Rotating Cone Spotlights 3D";
    info.effect_description =
        "Rotating cone spotlights: time-varying axis-angle rotation in a centered unit volume, "
        "then a double cone dist = |z'| − sqrt((x'² + y'²) / scale); hue follows room X like the reference "
        "`hsv(x, 1−dist, (1+dist)⁴)`. Best on full 3D grids.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_ROTATING_CONE_SPOTLIGHTS;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 400.0f;
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void RotatingConeSpotlights3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(w);
    main_layout->setContentsMargins(0, 0, 0, 0);
    QGridLayout* g = new QGridLayout();
    int row = 0;

    g->addWidget(new QLabel("Cone scale:"), row, 0);
    cone_slider = new QSlider(Qt::Horizontal);
    cone_slider->setRange(5, 500);
    cone_slider->setValue((int)std::lround(cone_scale * 1000.0f));
    cone_label = new QLabel(QString::number(cone_scale, 'g', 4));
    cone_slider->setToolTip("1/(π²) is default; lower = tighter cones, higher = wider wash.");
    g->addWidget(cone_slider, row, 1);
    g->addWidget(cone_label, row, 2);
    row++;

    g->addWidget(new QLabel("Hue shift:"), row, 0);
    hue_slider = new QSlider(Qt::Horizontal);
    hue_slider->setRange(0, 1000);
    hue_slider->setValue((int)std::lround(hue01 * 1000.0f));
    hue_label = new QLabel(QString::number(hue01, 'f', 3));
    hue_slider->setToolTip("Added to hue from position along room X (reference pattern uses x as hue).");
    g->addWidget(hue_slider, row, 1);
    g->addWidget(hue_label, row, 2);
    row++;

    g->addWidget(new QLabel("Motion:"), row, 0);
    motion_slider = new QSlider(Qt::Horizontal);
    motion_slider->setRange(20, 300);
    motion_slider->setValue((int)std::lround(motion_rate * 100.0f));
    motion_label = new QLabel(QString::number(motion_rate, 'f', 2));
    motion_slider->setToolTip("How fast the rotation axis and angle evolve.");
    g->addWidget(motion_slider, row, 1);
    g->addWidget(motion_label, row, 2);

    main_layout->addLayout(g);

    connect(cone_slider, &QSlider::valueChanged, this, [this](int v) {
        cone_scale = std::max(0.02f, v / 1000.0f);
        if(cone_label)
            cone_label->setText(QString::number(cone_scale, 'g', 4));
        emit ParametersChanged();
    });
    connect(hue_slider, &QSlider::valueChanged, this, [this](int v) {
        hue01 = std::clamp(v / 1000.0f, 0.0f, 1.0f);
        if(hue_label)
            hue_label->setText(QString::number(hue01, 'f', 3));
        emit ParametersChanged();
    });
    connect(motion_slider, &QSlider::valueChanged, this, [this](int v) {
        motion_rate = v / 100.0f;
        if(motion_label)
            motion_label->setText(QString::number(motion_rate, 'f', 2));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(cones_spot_strip_cmap_on,
                                            cones_spot_strip_cmap_kernel,
                                            cones_spot_strip_cmap_rep,
                                            cones_spot_strip_cmap_unfold,
                                            cones_spot_strip_cmap_dir,
                                            cones_spot_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &RotatingConeSpotlights3D::SyncStripColormapFromPanel);

    AddWidgetToParent(w, parent);
}

void RotatingConeSpotlights3D::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    cones_spot_strip_cmap_on = strip_cmap_panel->useStripColormap();
    cones_spot_strip_cmap_kernel = strip_cmap_panel->kernelId();
    cones_spot_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    cones_spot_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    cones_spot_strip_cmap_dir = strip_cmap_panel->directionDeg();
    cones_spot_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void RotatingConeSpotlights3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_ROTATING_CONE_SPOTLIGHTS;
}

RGBColor RotatingConeSpotlights3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    float px = nx - 0.5f;
    float py = ny - 0.5f;
    float pz = nz - 0.5f;

    float spd = std::max(0.05f, GetScaledSpeed());
    float rate = std::max(0.05f, GetScaledFrequency());
    float tm = time * (0.12f * motion_rate) * (0.5f + 0.5f * spd) * (0.4f + 0.6f * rate / 50.0f);

    float t1 = 2.0f * Triangle01(tm * 0.85f) - 1.0f;
    float t2 = 2.0f * Triangle01(tm * 1.12f) - 1.0f;
    float t3 = 2.0f * Triangle01(tm * 1.35f) - 1.0f;
    float t4 = std::fmod(tm * 0.55f + 1000.0f, 1.0f);

    float R[3][3];
    SetupRotationMatrix(t1, t2, t3, t4 * TWO_PI, R);
    float rx, ry, rz;
    Rotate3D(px, py, pz, R, &rx, &ry, &rz);

    float scale = std::max(1e-5f, cone_scale * (0.5f + 0.5f * GetNormalizedSize()));
    float dist = std::fabs(rz) - std::sqrt((rx * rx + ry * ry) / scale);
    dist = std::clamp(dist, -1.0f, 1.0f);

    float sat = std::clamp(1.0f - dist, 0.0f, 1.0f);
    float val = std::pow(1.0f + dist, 4.0f);
    val = std::clamp(val, 0.0f, 1.0f);

    float h = std::fmod(px + 0.5f + hue01 + 1.0f, 1.0f);
    if(cones_spot_strip_cmap_on)
    {
        const float size_m = GetNormalizedSize();
        const float ph01 = std::fmod(CalculateProgress(time) * 0.4f + time * rate * 0.01f + dist * 0.08f + 1.f, 1.f);
        float pal01 = SampleStripKernelPalette01(cones_spot_strip_cmap_kernel,
                                                 cones_spot_strip_cmap_rep,
                                                 cones_spot_strip_cmap_unfold,
                                                 cones_spot_strip_cmap_dir,
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rot);
        pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
        const int kid = SpatialPatternKernelClamp(cones_spot_strip_cmap_kernel);
        RGBColor c = ResolveStripKernelFinalColor(*this,
                                                  kid,
                                                  std::clamp(pal01, 0.0f, 1.0f),
                                                  cones_spot_strip_cmap_color_style,
                                                  time,
                                                  rate * 0.24f);
        const int cr = (int)(c & 0xFF);
        const int cg = (int)((c >> 8) & 0xFF);
        const int cb = (int)((c >> 16) & 0xFF);
        QColor qc = QColor::fromRgb(cr, cg, cb);
        const QColor hsv = qc.toHsv();
        const float ch = static_cast<float>(hsv.hueF());
        const float cv = static_cast<float>(hsv.valueF());
        h = (ch >= 0.0f) ? std::fmod(ch + hue01 + 1.0f, 1.0f) : std::fmod(hue01 + 1.0f, 1.0f);
        return Hsv01ToBgr(h, sat, std::clamp(val * cv, 0.0f, 1.0f));
    }
    if(GetRainbowMode())
    {
        float p = std::fmod(px + 0.5f + hue01 + dist * 0.35f + CalculateProgress(time) * 0.4f + time * rate * 0.01f, 1.0f);
        if(p < 0.0f)
            p += 1.0f;
        h = p;
    }

    return Hsv01ToBgr(h, sat, val);
}

nlohmann::json RotatingConeSpotlights3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["cone_spot_scale"] = cone_scale;
    j["cone_spot_hue01"] = hue01;
    j["cone_spot_motion"] = motion_rate;
    StripColormapSaveJson(j,
                          "cones_spot",
                          cones_spot_strip_cmap_on,
                          cones_spot_strip_cmap_kernel,
                          cones_spot_strip_cmap_rep,
                          cones_spot_strip_cmap_unfold,
                          cones_spot_strip_cmap_dir,
                          cones_spot_strip_cmap_color_style);
    return j;
}

void RotatingConeSpotlights3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("cone_spot_scale") && settings["cone_spot_scale"].is_number())
        cone_scale = std::max(0.02f, std::min(0.5f, settings["cone_spot_scale"].get<float>()));
    if(settings.contains("cone_spot_hue01") && settings["cone_spot_hue01"].is_number())
        hue01 = std::clamp(settings["cone_spot_hue01"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("cone_spot_motion") && settings["cone_spot_motion"].is_number())
        motion_rate = std::clamp(settings["cone_spot_motion"].get<float>(), 0.2f, 4.0f);

    StripColormapLoadJson(settings,
                          "cones_spot",
                          cones_spot_strip_cmap_on,
                          cones_spot_strip_cmap_kernel,
                          cones_spot_strip_cmap_rep,
                          cones_spot_strip_cmap_unfold,
                          cones_spot_strip_cmap_dir,
                          cones_spot_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(cones_spot_strip_cmap_on,
                                                cones_spot_strip_cmap_kernel,
                                                cones_spot_strip_cmap_rep,
                                                cones_spot_strip_cmap_unfold,
                                                cones_spot_strip_cmap_dir,
                                                cones_spot_strip_cmap_color_style);
    }

    if(cone_slider)
    {
        cone_slider->setValue((int)std::lround(cone_scale * 1000.0f));
        if(cone_label)
            cone_label->setText(QString::number(cone_scale, 'g', 4));
    }
    if(hue_slider)
    {
        hue_slider->setValue((int)std::lround(hue01 * 1000.0f));
        if(hue_label)
            hue_label->setText(QString::number(hue01, 'f', 3));
    }
    if(motion_slider)
    {
        motion_slider->setValue((int)std::lround(motion_rate * 100.0f));
        if(motion_label)
            motion_label->setText(QString::number(motion_rate, 'f', 2));
    }
}
