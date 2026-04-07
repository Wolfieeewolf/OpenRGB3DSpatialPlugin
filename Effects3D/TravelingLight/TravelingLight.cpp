// SPDX-License-Identifier: GPL-2.0-only

#include "TravelingLight.h"
#include "../EffectHelpers.h"
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QGridLayout>
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

REGISTER_EFFECT_3D(TravelingLight);

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
}

EffectInfo3D TravelingLight::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Traveling Light";
    info.effect_description = "Comet, Chase, Marquee, ZigZag, KITT, Wipe, Moving Panes, Crossing Beams, or Rotating Beam";
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
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void TravelingLight::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;

    layout->addWidget(new QLabel("Style:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(this->mode, MODE_COUNT - 1)));
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        this->mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Wipe edge:"), row, 0);
    QComboBox* wipe_edge_combo = new QComboBox();
    wipe_edge_combo->addItem("Round");
    wipe_edge_combo->addItem("Sharp");
    wipe_edge_combo->addItem("Square");
    wipe_edge_combo->setCurrentIndex(std::clamp(wipe_edge_shape, 0, 2));
    layout->addWidget(wipe_edge_combo, row, 1, 1, 2);
    connect(wipe_edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        wipe_edge_shape = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Panes divisions:"), row, 0);
    QSlider* div_slider = new QSlider(Qt::Horizontal);
    div_slider->setRange(2, 16);
    div_slider->setValue(num_divisions);
    QLabel* div_label = new QLabel(QString::number(num_divisions));
    div_label->setMinimumWidth(36);
    layout->addWidget(div_slider, row, 1);
    layout->addWidget(div_label, row, 2);
    connect(div_slider, &QSlider::valueChanged, this, [this, div_label](int v){
        num_divisions = v;
        if(div_label) div_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Glow (beams):"), row, 0);
    QSlider* glow_slider = new QSlider(Qt::Horizontal);
    glow_slider->setRange(10, 100);
    glow_slider->setValue((int)(glow * 100.0f));
    QLabel* glow_label = new QLabel(QString::number((int)(glow * 100)) + "%");
    glow_label->setMinimumWidth(36);
    layout->addWidget(glow_slider, row, 1);
    layout->addWidget(glow_label, row, 2);
    connect(glow_slider, &QSlider::valueChanged, this, [this, glow_label](int v){
        glow = v / 100.0f;
        if(glow_label) glow_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void TravelingLight::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_COMET;
}


RGBColor TravelingLight::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated = TransformPointByRotation(x, y, z, origin);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;

    float size_scale = GetNormalizedSize() / 1.5f;
    float detail = std::max(0.05f, GetScaledDetail());
    float color_cycle = progress * GetScaledFrequency() * 3.0f;

    int m = std::max(0, std::min(this->mode, MODE_COUNT - 1));
    int ax = GetPathAxis();

    if(m == MODE_KITT)
    {
        float axis_val = (ax == 0) ? rotated.x : (ax == 1) ? rotated.y : rotated.z;
        float axis_min = (ax == 0) ? grid.min_x : (ax == 1) ? grid.min_y : grid.min_z;
        float axis_max = (ax == 0) ? grid.max_x : (ax == 1) ? grid.max_y : grid.max_z;
        float span = std::max(axis_max - axis_min, 1e-5f);
        bool step = (progress < 0.5f);
        float p_step = step ? (2.0f * progress) : (2.0f * (1.0f - progress));
        float beam_center = axis_min + p_step * span;
        float w = std::max(0.05f, std::min(0.5f, 0.15f * size_scale)) * span;
        float hw = w * 0.5f;
        float dist = beam_center - axis_val;
        float intensity = 0.0f;
        RGBColor c;
        if(GetRainbowMode())
        {
            float hue = fmodf(progress * 360.0f * (0.6f + 0.4f * detail) + color_cycle, 360.0f);
            if(hue < 0.0f) hue += 360.0f;
            c = GetRainbowColor(hue);
        }
        else if(dist < -hw)
            c = GetColorAtPosition(step ? 1.0f : 0.0f);
        else if(dist > hw)
            c = GetColorAtPosition(step ? 0.0f : 1.0f);
        else
        {
            float interp = std::max(0.0f, std::min(1.0f, (hw - dist) / w));
            c = GetColorAtPosition(step ? interp : (1.0f - interp));
        }
        if(dist < -hw)
            intensity = std::max(0.0f, std::min(1.0f, (w + dist) / w));
        else if(dist > hw)
            intensity = std::max(0.0f, std::min(1.0f, 1.0f - (dist - hw) / w));
        else
            intensity = 1.0f;
        if(intensity < 0.01f) return 0x00000000;
        unsigned char r = (unsigned char)((c & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    if(m == MODE_WIPE)
    {
        float prog = std::fmod(progress, 2.0f);
        if(prog > 1.0f) prog = 2.0f - prog;
        float rot_rel_x = rotated.x - origin.x;
        float rot_rel_y = rotated.y - origin.y;
        float rot_rel_z = rotated.z - origin.z;
        float position = NormalizeGridAxis01(rotated.x, grid.min_x, grid.max_x);
        float edge_distance = fabsf(position - prog);
        float thickness_factor = 0.2f * size_scale;
        float intensity;
        switch(wipe_edge_shape) {
        case 0: {
            float core = 1.0f - smoothstep(0.0f, thickness_factor * 0.6f, edge_distance);
            float glow_ = 0.4f * (1.0f - smoothstep(thickness_factor * 0.6f, thickness_factor * 1.2f, edge_distance));
            intensity = std::min(1.0f, core + glow_);
            break; }
        case 1: intensity = edge_distance < thickness_factor * 0.5f ? 1.0f : 0.0f; break;
        default: intensity = edge_distance < thickness_factor ? 1.0f : 0.0f; break;
        }
        float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
        float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
        float depth_factor = (max_radius > 0.001f) ? (0.5f + 0.5f * (1.0f - std::min(1.0f, radial_distance / max_radius) * 0.5f)) : 1.0f;
        float pos_color = std::fmod(prog + progress * GetScaledFrequency() * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        RGBColor final_color = GetRainbowMode() ? GetRainbowColor(prog * 360.0f + color_cycle) : GetColorAtPosition(pos_color);
        unsigned char r = (unsigned char)((final_color & 0xFF) * intensity * depth_factor);
        unsigned char g = (unsigned char)(((final_color >> 8) & 0xFF) * intensity * depth_factor);
        unsigned char b = (unsigned char)(((final_color >> 16) & 0xFF) * intensity * depth_factor);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    if(m == MODE_MOVING_PANES)
    {
        float lx = (rotated.x - grid.min_x) / std::max(0.001f, grid.width);
        float ly = (rotated.y - grid.min_y) / std::max(0.001f, grid.height);
        float lz = (rotated.z - grid.min_z) / std::max(0.001f, grid.depth);
        lx = std::max(0.0f, std::min(1.0f, lx));
        ly = std::max(0.0f, std::min(1.0f, ly));
        lz = std::max(0.0f, std::min(1.0f, lz));
        int ax_ = GetPathAxis();
        float prim = (ax_ == 0) ? lx : (ax_ == 1) ? ly : lz;
        float sec = (ax_ == 0) ? ly : (ax_ == 1) ? lz : lx;
        int ndiv = std::max(2, std::min(16, num_divisions));
        float zone_size = 1.0f / (float)ndiv;
        int zone = std::max(0, std::min(ndiv - 1, (int)(prim / zone_size)));
        int zone_id = zone % 2;
        float t = progress;
        float s = 0.5f * (1.0f + sinf(sec * (float)M_PI * 4.0f + (zone_id ? 1.0f : -1.0f) * t * (float)(2.0 * M_PI) + (float)M_PI * 0.25f));
        RGBColor c0, c1;
        if(GetRainbowMode()) {
            c0 = GetRainbowColor(progress * 60.0f + zone * 30.0f + color_cycle);
            c1 = GetRainbowColor(progress * 60.0f + zone * 30.0f + 180.0f + color_cycle);
        } else {
            const std::vector<RGBColor>& cols = GetColors();
            c0 = (cols.size() > 0) ? cols[0] : 0x000000FF;
            c1 = (cols.size() > 1) ? cols[1] : 0x00FF0000;
        }
        return lerp_color(zone_id ? c1 : c0, zone_id ? c0 : c1, s);
    }

    if(m == MODE_CROSSING)
    {
        float sine_x = sinf(progress * (float)M_PI);
        float sine_y = sinf(progress * (float)M_PI * 1.3f);
        float half_w = grid.width * 0.5f;
        float half_h = grid.height * 0.5f;
        float x_progress = origin.x + sine_x * half_w;
        float y_progress = origin.y + sine_y * half_h;
        float dist_x = fabsf(x - x_progress);
        float dist_y = fabsf(y - y_progress);
        float thick = std::max(0.02f, std::min(0.2f, 0.08f * size_scale)) * std::max(grid.width, grid.height);
        float glow_val = std::max(0.1f, std::min(1.0f, glow));
        float dx_pct = (dist_x > thick) ? powf(dist_x / std::max(0.001f, grid.width), 0.01f * glow_val * 100.0f) : 0.0f;
        float dy_pct = (dist_y > thick) ? powf(dist_y / std::max(0.001f, grid.height), 0.01f * glow_val * 100.0f) : 0.0f;
        dx_pct = std::min(1.0f, dx_pct);
        dy_pct = std::min(1.0f, dy_pct);
        RGBColor c1, c2;
        if(GetRainbowMode()) {
            c1 = GetRainbowColor(progress * 120.0f + color_cycle);
            c2 = GetRainbowColor(progress * 120.0f + 180.0f + color_cycle);
        } else {
            const std::vector<RGBColor>& cols = GetColors();
            c1 = (cols.size() > 0) ? cols[0] : 0x000000FF;
            c2 = (cols.size() > 1) ? cols[1] : 0x0000FF00;
        }
        float v1 = 1.0f - dx_pct, v2 = 1.0f - dy_pct;
        unsigned char r1 = (unsigned char)((c1 & 0xFF) * v1);
        unsigned char g1 = (unsigned char)(((c1 >> 8) & 0xFF) * v1);
        unsigned char b1 = (unsigned char)(((c1 >> 16) & 0xFF) * v1);
        unsigned char r2 = (unsigned char)((c2 & 0xFF) * v2);
        unsigned char g2 = (unsigned char)(((c2 >> 8) & 0xFF) * v2);
        unsigned char b2 = (unsigned char)(((c2 >> 16) & 0xFF) * v2);
        unsigned char r = screen_blend(r1, r2);
        unsigned char g = screen_blend(g1, g2);
        unsigned char b = screen_blend(b1, b2);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    if(m == MODE_ROTATING)
    {
        float beam_angle = progress * (float)(2.0 * M_PI);
        Vector3D rot = TransformPointByRotation(x, y, z, origin);
        float lx = rot.x - origin.x, ly = rot.y - origin.y, lz = rot.z - origin.z;
        int pl = GetPlane();
        float point_angle = (pl == 0) ? atan2f(lz, lx) : (pl == 1) ? atan2f(lx, ly) : atan2f(lz, ly);
        float diff = fmodf(point_angle - beam_angle + (float)M_PI, (float)(2.0 * M_PI));
        if(diff < 0.0f) diff += (float)(2.0 * M_PI);
        diff -= (float)M_PI;
        float abs_diff = fabsf(diff);
        float width = std::max(0.05f, std::min(0.5f, 0.15f * size_scale)) * (float)M_PI;
        float glow_val = std::max(0.1f, std::min(1.0f, glow));
        float intensity = (abs_diff <= width * 0.5f) ? 1.0f :
            (abs_diff <= width) ? (1.0f - (abs_diff - width * 0.5f) / (width * 0.5f)) :
            powf(1.0f - fminf(1.0f, (abs_diff - width) / ((float)M_PI * glow_val)), 2.0f);
        if(intensity < 0.01f) return 0x00000000;
        float hue = fmodf(progress * 60.0f + color_cycle, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        float pos_color = std::fmod(progress + progress * GetScaledFrequency() * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(pos_color);
        unsigned char r = (unsigned char)((c & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    float axis_val = (ax == 0) ? rotated.x : (ax == 1) ? rotated.y : rotated.z;
    float axis_min = (ax == 0) ? grid.min_x : (ax == 1) ? grid.min_y : grid.min_z;
    float axis_max = (ax == 0) ? grid.max_x : (ax == 1) ? grid.max_y : grid.max_z;
    float span = std::max(axis_max - axis_min, 1e-5f);
    float tail_len = 0.25f * span * size_scale;

    if(m == MODE_CHASE)
    {
        const int n_comets = 4;
        float intensity = 0.0f;
        float hue_offset = 0.0f;
        for(int c = 0; c < n_comets; c++)
        {
            float p = std::fmod(progress + (float)c / (float)n_comets, 1.0f);
            if(p < 0.0f) p += 1.0f;
            float head = axis_min + p * span;
            float distance = head - axis_val;
            if(distance >= 0.0f && distance <= tail_len)
            {
                float i = 1.0f - (distance / tail_len);
                i = i * i * 0.7f;
                if(i > intensity) { intensity = i; hue_offset = (1.0f - distance / tail_len) * 60.0f; }
            }
            else if(distance < 0.0f && distance > -tail_len * 0.2f)
            {
                if(1.0f > intensity) { intensity = 1.0f; hue_offset = 0.0f; }
            }
        }
        if(intensity <= 0.0f) return 0x00000000;
        float pos_color = std::fmod(progress + progress * GetScaledFrequency() * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        RGBColor color = GetRainbowMode()
            ? GetRainbowColor(progress * 360.0f + hue_offset + color_cycle)
            : GetColorAtPosition(pos_color);
        unsigned char r = (unsigned char)((color & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((color >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((color >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    if(m == MODE_MARQUEE)
    {
        float head = axis_min + progress * span;
        float distance = head - axis_val;
        float band = tail_len * 0.5f;
        float intensity = 0.0f;
        if(distance >= 0.0f && distance <= band)
            intensity = 1.0f;
        else if(distance < 0.0f && distance > -band * 0.3f)
            intensity = 0.6f;
        if(intensity <= 0.0f) return 0x00000000;
        float pos_color = std::fmod(progress + progress * GetScaledFrequency() * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        RGBColor color = GetRainbowMode()
            ? GetRainbowColor(progress * 360.0f + color_cycle)
            : GetColorAtPosition(pos_color);
        unsigned char r = (unsigned char)((color & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((color >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((color >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    if(m == MODE_ZIGZAG)
    {
        float lx = (rotated.x - origin.x) / std::max(0.001f, grid.width);
        float ly = (rotated.y - origin.y) / std::max(0.001f, grid.height);
        float lz = (rotated.z - origin.z) / std::max(0.001f, grid.depth);
        lx = std::max(0.0f, std::min(1.0f, (lx + 1.0f) * 0.5f));
        ly = std::max(0.0f, std::min(1.0f, (ly + 1.0f) * 0.5f));
        lz = std::max(0.0f, std::min(1.0f, (lz + 1.0f) * 0.5f));
        float prim = 0.0f, sec = 0.0f;
        if(ax == 0) { prim = lx; sec = ly; }
        else if(ax == 1) { prim = ly; sec = lz; }
        else { prim = lz; sec = lx; }
        const int n_cols = 24, n_rows = 24;
        float col_cont = prim * (float)n_cols;
        float row_cont = sec * (float)n_rows;
        int seg = std::max(0, std::min(n_cols - 1, (int)col_cont));
        float local = (seg % 2 == 0) ? row_cont : ((float)n_rows - row_cont);
        float path_pos = ((float)(seg * n_rows) + local) / (float)(n_cols * n_rows);
        path_pos = std::max(0.0f, std::min(1.0f, path_pos));
        float tail = std::max(0.1f, std::min(0.8f, 0.25f * size_scale));
        float distance = 1.0f;
        if(path_pos > progress) return 0x00000000;
        float dist_in_tail = progress - path_pos;
        if(dist_in_tail > tail) return 0x00000000;
        distance = 1.0f - (dist_in_tail / tail);
        distance = distance * distance * distance;
        float hue = fmodf(path_pos * 360.0f + color_cycle, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        float pos_color = std::fmod(path_pos + progress * GetScaledFrequency() * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(pos_color);
        int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * distance)));
        int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * distance)));
        int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * distance)));
        return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
    }

    /* MODE_COMET: single comet with tail */
    float head = axis_min + progress * span;
    float dist = head - axis_val;
    float intensity = 0.0f;
    float hue_offset = 0.0f;
    if(dist >= 0.0f && dist <= tail_len)
    {
        intensity = 1.0f - (dist / tail_len);
        intensity = intensity * intensity;
        hue_offset = (1.0f - (dist / tail_len)) * 60.0f;
    }
    else if(dist < 0.0f && dist > -tail_len * 0.2f)
        intensity = 1.0f;
    if(intensity <= 0.0f) return 0x00000000;
    float pos_color = std::fmod(progress + progress * GetScaledFrequency() * 0.02f, 1.0f);
    if(pos_color < 0.0f) pos_color += 1.0f;
    RGBColor color = GetRainbowMode()
        ? GetRainbowColor(progress * 360.0f + hue_offset + color_cycle)
        : GetColorAtPosition(pos_color);
    unsigned char r = (unsigned char)((color & 0xFF) * intensity);
    unsigned char g = (unsigned char)(((color >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)(((color >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json TravelingLight::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = this->mode;
    j["glow"] = glow;
    j["wipe_edge_shape"] = wipe_edge_shape;
    j["num_divisions"] = num_divisions;
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
    if(settings.contains("edge_shape") && settings["edge_shape"].is_number_integer())
        wipe_edge_shape = std::clamp(settings["edge_shape"].get<int>(), 0, 2);
    if(settings.contains("num_divisions") && settings["num_divisions"].is_number_integer())
        num_divisions = std::max(2, std::min(16, settings["num_divisions"].get<int>()));
}
