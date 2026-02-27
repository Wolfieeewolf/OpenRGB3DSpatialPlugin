// SPDX-License-Identifier: GPL-2.0-only

#include "WireframeCube3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

REGISTER_EFFECT_3D(WireframeCube3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void rotate_axis_angle(float& x, float& y, float& z, float ax, float ay, float az, float angle_rad)
{
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    float dot = ax*x + ay*y + az*z;
    float nx = x*c + (ay*z - az*y)*s + ax*dot*(1.0f - c);
    float ny = y*c + (az*x - ax*z)*s + ay*dot*(1.0f - c);
    float nz = z*c + (ax*y - ay*x)*s + az*dot*(1.0f - c);
    x = nx; y = ny; z = nz;
}

float WireframeCube3D::PointToSegmentDistance(float px, float py, float pz,
                                               float ax, float ay, float az,
                                               float bx, float by, float bz)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len2 = dx*dx + dy*dy + dz*dz;
    if(len2 < 1e-10f) return sqrtf((px-ax)*(px-ax) + (py-ay)*(py-ay) + (pz-az)*(pz-az));
    float t = ((px-ax)*dx + (py-ay)*dy + (pz-az)*dz) / len2;
    t = fmaxf(0.0f, fminf(1.0f, t));
    float qx = ax + t*dx, qy = ay + t*dy, qz = az + t*dz;
    return sqrtf((px-qx)*(px-qx) + (py-qy)*(py-qy) + (pz-qz)*(pz-qz));
}

WireframeCube3D::WireframeCube3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D WireframeCube3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Wireframe Cube";
    info.effect_description = "Rotating wireframe cube (Mega-Cube style); soft glow along edges";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 40.0f;
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

void WireframeCube3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Edge glow:"), 0, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setValue((int)(thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, 0, 1);
    layout->addWidget(thick_label, 0, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    layout->addWidget(new QLabel("Line brightness:"), 1, 0);
    QSlider* bright_slider = new QSlider(Qt::Horizontal);
    bright_slider->setRange(0, 100);
    bright_slider->setValue((int)(line_brightness * 100.0f));
    QLabel* bright_label = new QLabel(QString::number((int)(line_brightness * 100)) + "%");
    bright_label->setMinimumWidth(36);
    layout->addWidget(bright_slider, 1, 1);
    layout->addWidget(bright_label, 1, 2);
    connect(bright_slider, &QSlider::valueChanged, this, [this, bright_label](int v){
        line_brightness = v / 100.0f;
        if(bright_label) bright_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void WireframeCube3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

RGBColor WireframeCube3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor WireframeCube3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    // Cache rotated cube corners once per frame (major FPS win)
    if(fabsf(time - cube_cache_time) > 0.001f)
    {
        cube_cache_time = time;
        float progress_val = CalculateProgress(time);
        float angle_deg = fmodf(progress_val * 360.0f, 360.0f * 6.0f);
        if(angle_deg < 0.0f) angle_deg += 360.0f * 6.0f;
        cached_angle_deg = angle_deg;
        float angle_rad = angle_deg * (float)(M_PI / 180.0);

        float ax = 0.0f, ay = 0.0f, az = 1.0f;
        if(angle_deg > 4.0f * 360.0f)
        {
            ax = 0.0f; ay = 1.0f; az = 0.0f;
        }
        else if(angle_deg > 2.0f * 360.0f)
        {
            ax = ay = az = 1.0f / sqrtf(3.0f);
        }

        float corners[8][3] = {
            {-1,-1,-1}, {+1,-1,-1}, {-1,+1,-1}, {+1,+1,-1},
            {-1,-1,+1}, {+1,-1,+1}, {-1,+1,+1}, {+1,+1,+1}
        };
        for(int i = 0; i < 8; i++)
        {
            rotate_axis_angle(corners[i][0], corners[i][1], corners[i][2], ax, ay, az, angle_rad);
            cube_corners[i][0] = corners[i][0];
            cube_corners[i][1] = corners[i][1];
            cube_corners[i][2] = corners[i][2];
        }
    }

    float half = 0.5f * std::max(grid.width, std::max(grid.height, grid.depth)) * GetNormalizedScale();
    if(half < 1e-5f) half = 1.0f;
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = (rot.x - origin.x) / half;
    float ly = (rot.y - origin.y) / half;
    float lz = (rot.z - origin.z) / half;

    const int edges[12][2] = {
        {0,1},{2,3},{0,2},{1,3},{4,5},{6,7},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}
    };
    float sigma = std::max(thickness, 0.02f);
    float sigma_sq = sigma * sigma;
    const float d2_cutoff = 9.0f * sigma_sq;
    float total = 0.0f;
    for(int e = 0; e < 12; e++)
    {
        int i = edges[e][0], j = edges[e][1];
        float d = PointToSegmentDistance(lx, ly, lz,
            cube_corners[i][0], cube_corners[i][1], cube_corners[i][2],
            cube_corners[j][0], cube_corners[j][1], cube_corners[j][2]);
        float d2 = d * d;
        if(d2 > d2_cutoff) continue;
        total += expf(-d2 / sigma_sq);
    }
    total = fminf(1.0f, total * 0.35f);
    total *= std::max(0.0f, std::min(1.0f, line_brightness));

    float hue = fmodf(cached_angle_deg * 0.1f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(0.5f);
    int r = (int)((c & 0xFF) * total);
    int g = (int)(((c >> 8) & 0xFF) * total);
    int b = (int)(((c >> 16) & 0xFF) * total);
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json WireframeCube3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["thickness"] = thickness;
    j["line_brightness"] = line_brightness;
    return j;
}

void WireframeCube3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("thickness") && settings["thickness"].is_number())
    {
        float v = settings["thickness"].get<float>();
        thickness = std::max(0.02f, std::min(1.0f, v));
    }
    if(settings.contains("line_brightness") && settings["line_brightness"].is_number())
    {
        float v = settings["line_brightness"].get<float>();
        line_brightness = std::max(0.0f, std::min(1.0f, v));
    }
}
