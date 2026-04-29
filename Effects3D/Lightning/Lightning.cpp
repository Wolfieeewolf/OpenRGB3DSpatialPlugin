// SPDX-License-Identifier: GPL-2.0-only

#include "Lightning.h"
#include "../EffectHelpers.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <algorithm>
#include <cmath>

const char* Lightning::ModeName(int m)
{
    switch(m) {
    case MODE_PLASMA_BALL: return "Plasma Ball";
    case MODE_SKY_FLASH: return "Sky Flash";
    case MODE_SKY_BOLT: return "Sky Bolt";
    case MODE_SKY_FORKED: return "Sky Forked";
    case MODE_SKY_STORM: return "Sky Storm";
    default: return "Plasma Ball";
    }
}

float Lightning::hash11(float t)
{
    float s = sinf(t * 12.9898f) * 43758.5453f;
    return s - floorf(s);
}

Lightning::Lightning(QWidget* parent) : SpatialEffect3D(parent)
{
    strike_rate_slider = nullptr;
    strike_rate_label = nullptr;
    branch_slider = nullptr;
    branch_label = nullptr;
    strike_rate = 5;
    branches = 6;
    cache_time = -1e9f;
    cache_grid_hash = 0.0f;
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x00FF64B4);
    SetColors(cols);
}

Lightning::~Lightning() = default;

EffectInfo3D Lightning::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Lightning";
    info.effect_description = "Plasma ball arcs plus sky lightning styles (flash, bolt, forked, storm)";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_LIGHTNING;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 1.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void Lightning::SetupCustomUI(QWidget* parent)
{
    QWidget* outer_w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(outer_w);
    vbox->setContentsMargins(0, 0, 0, 0);
    QWidget* w = new QWidget();
    vbox->addWidget(w);
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    int row = 0;
    layout->addWidget(new QLabel("Style:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(this->mode, MODE_COUNT - 1)));
    mode_combo->setToolTip(
        "Plasma Ball uses arcs/sec and max arcs. Sky modes use ceiling/height cues—try zone bounds on strips.");
    mode_combo->setItemData(0, "Grounded energy sphere with branching arcs.", Qt::ToolTipRole);
    mode_combo->setItemData(1, "Brief ceiling-wide flash.", Qt::ToolTipRole);
    mode_combo->setItemData(2, "Single bolt from cloud band to floor.", Qt::ToolTipRole);
    mode_combo->setItemData(3, "Forked discharge from cloud height.", Qt::ToolTipRole);
    mode_combo->setItemData(4, "Multiple strikes and ambient flicker.", Qt::ToolTipRole);
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        this->mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Arches/sec (Plasma):"), row, 0);
    strike_rate_slider = new QSlider(Qt::Horizontal);
    strike_rate_slider->setRange(1, 20);
    strike_rate_slider->setValue(strike_rate);
    strike_rate_slider->setToolTip("Number of new lightning arcs per second");
    layout->addWidget(strike_rate_slider, row, 1);
    strike_rate_label = new QLabel(QString::number(strike_rate));
    strike_rate_label->setMinimumWidth(30);
    layout->addWidget(strike_rate_label, row, 2);
    row++;

    layout->addWidget(new QLabel("Max arcs (Plasma):"), row, 0);
    branch_slider = new QSlider(Qt::Horizontal);
    branch_slider->setRange(2, 12);
    branch_slider->setValue(branches);
    branch_slider->setToolTip("Maximum simultaneous arcs (lower = better FPS)");
    layout->addWidget(branch_slider, row, 1);
    branch_label = new QLabel(QString::number(branches));
    branch_label->setMinimumWidth(30);
    layout->addWidget(branch_label, row, 2);
    row++;

    layout->addWidget(new QLabel("Sky event rate:"), row, 0);
    QSlider* rate_slider = new QSlider(Qt::Horizontal);
    rate_slider->setRange(5, 50);
    rate_slider->setValue((int)(flash_rate * 100.0f));
    QLabel* rate_label = new QLabel(QString::number(flash_rate, 'f', 2));
    rate_label->setMinimumWidth(36);
    layout->addWidget(rate_slider, row, 1);
    layout->addWidget(rate_label, row, 2);
    connect(rate_slider, &QSlider::valueChanged, this, [this, rate_label](int v){
        flash_rate = v / 100.0f;
        if(rate_label) rate_label->setText(QString::number(flash_rate, 'f', 2));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Sky event duration:"), row, 0);
    QSlider* dur_slider = new QSlider(Qt::Horizontal);
    dur_slider->setRange(2, 25);
    dur_slider->setValue((int)(flash_duration * 100.0f));
    QLabel* dur_label = new QLabel(QString::number(flash_duration * 1000.0f, 'f', 0) + " ms");
    dur_label->setMinimumWidth(50);
    layout->addWidget(dur_slider, row, 1);
    layout->addWidget(dur_label, row, 2);
    connect(dur_slider, &QSlider::valueChanged, this, [this, dur_label](int v){
        flash_duration = v / 100.0f;
        if(dur_label) dur_label->setText(QString::number(flash_duration * 1000.0f, 'f', 0) + " ms");
        emit ParametersChanged();
    });

    stratum_panel = new StratumBandPanel(outer_w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Lightning::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(outer_w, parent);

    connect(strike_rate_slider, &QSlider::valueChanged, this, &Lightning::OnLightningParameterChanged);
    connect(branch_slider, &QSlider::valueChanged, this, &Lightning::OnLightningParameterChanged);
}

void Lightning::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_LIGHTNING;
}

void Lightning::OnLightningParameterChanged()
{
    if(strike_rate_slider)
    {
        strike_rate = strike_rate_slider->value();
        if(strike_rate_label) strike_rate_label->setText(QString::number(strike_rate));
    }
    if(branch_slider)
    {
        branches = branch_slider->value();
        if(branch_label) branch_label->setText(QString::number(branches));
    }
    cache_time = -1e9f;
    emit ParametersChanged();
}

void Lightning::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    cache_time = -1e9f;
    emit ParametersChanged();
}

float Lightning::HashF(unsigned int seed)
{
    seed = (seed * 1103515245u + 12345u) & 0x7FFFFFFFu;
    return (float)seed / 2147483648.0f;
}

Vector3D Lightning::RandomPointOnGlass(const GridContext3D& grid, unsigned int seed)
{
    float cx = (grid.min_x + grid.max_x) * 0.5f;
    float cy = (grid.min_y + grid.max_y) * 0.5f;
    float cz = (grid.min_z + grid.max_z) * 0.5f;

    int face = (int)(HashF(seed) * 6.0f) % 6;
    float u = HashF(seed + 1);
    float v = HashF(seed + 2);

    Vector3D p;
    switch(face)
    {
    case 0: p.x = grid.min_x; p.y = cy + (u - 0.5f) * grid.height; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 1: p.x = grid.max_x; p.y = cy + (u - 0.5f) * grid.height; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 2: p.x = cx + (u - 0.5f) * grid.width; p.y = grid.min_y; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 3: p.x = cx + (u - 0.5f) * grid.width; p.y = grid.max_y; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 4: p.x = cx + (u - 0.5f) * grid.width; p.y = cy + (v - 0.5f) * grid.height; p.z = grid.min_z; break;
    default: p.x = cx + (u - 0.5f) * grid.width; p.y = cy + (v - 0.5f) * grid.height; p.z = grid.max_z; break;
    }
    return p;
}

float Lightning::DistToSegment(float px, float py, float pz,
                                 float ax, float ay, float az,
                                 float bx, float by, float bz)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len_sq = dx*dx + dy*dy + dz*dz;
    if(len_sq < 1e-12f) return sqrtf((px-ax)*(px-ax) + (py-ay)*(py-ay) + (pz-az)*(pz-az));

    float t = ((px-ax)*dx + (py-ay)*dy + (pz-az)*dz) / len_sq;
    t = std::max(0.0f, std::min(1.0f, t));
    float qx = ax + t * dx, qy = ay + t * dy, qz = az + t * dz;
    float ddx = px - qx, ddy = py - qy, ddz = pz - qz;
    return sqrtf(ddx*ddx + ddy*ddy + ddz*ddz);
}

void Lightning::UpdateArchCache(float time, const GridContext3D& grid)
{
    float grid_hash = grid.min_x
        + grid.max_x * 31.0f
        + grid.min_y * 31.0f * 31.0f
        + grid.max_y * 31.0f * 31.0f * 31.0f
        + grid.min_z * 31.0f * 31.0f * 31.0f * 31.0f
        + grid.max_z * 31.0f * 31.0f * 31.0f * 31.0f * 31.0f;
    if(fabsf(time - cache_time) < 0.008f && fabsf(grid_hash - cache_grid_hash) < 0.01f)
        return;

    cache_time = time;
    cache_grid_hash = grid_hash;
    cached_arches.clear();

    Vector3D origin = GetEffectOriginGrid(grid);
    float speed_factor = 0.5f + 0.5f * GetScaledSpeed();
    float arch_duration = (0.35f + 0.25f / (float)std::max(1u, strike_rate)) / std::max(0.3f, speed_factor);
    int max_arches = (int)branches;
    float spawn_rate = std::max(1.0f, (float)strike_rate);
    float spawn_interval = 1.0f / spawn_rate;

    float room_avg = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float margin = room_avg * 0.15f;
    float start_jitter = room_avg * (0.01f + 0.03f * GetNormalizedSize());

    for(int i = 0; i < max_arches; i++)
    {
        float shifted_time = time - ((float)i * (spawn_interval / (float)std::max(1, max_arches)));
        float event_idx_f = floorf(shifted_time / spawn_interval);
        float birth = event_idx_f * spawn_interval;
        unsigned int event_idx = (unsigned int)std::max(0.0f, event_idx_f);
        unsigned int seed = event_idx * 2654435761u ^ (unsigned int)(i * 7919u + 17u);

        PlasmaArc3D arc;
        float ux = HashF(seed + 11u) * 2.0f - 1.0f;
        float uy = HashF(seed + 13u) * 2.0f - 1.0f;
        float uz = HashF(seed + 15u) * 2.0f - 1.0f;
        float ulen = sqrtf(ux * ux + uy * uy + uz * uz);
        if(ulen < 1e-4f) { ux = 1.0f; uy = 0.0f; uz = 0.0f; ulen = 1.0f; }
        ux /= ulen; uy /= ulen; uz /= ulen;
        float jitter_mag = start_jitter * (0.2f + 0.8f * HashF(seed + 19u));
        arc.start = {origin.x + ux * jitter_mag, origin.y + uy * jitter_mag, origin.z + uz * jitter_mag};
        arc.end = RandomPointOnGlass(grid, seed + 23u);
        arc.birth_time = birth;
        arc.duration = arch_duration;
        arc.seed = seed;
        cached_arches.push_back(arc);
    }

    if(!cached_arches.empty())
    {
        float min_x = origin.x, min_y = origin.y, min_z = origin.z;
        float max_x = origin.x, max_y = origin.y, max_z = origin.z;
        for(const PlasmaArc3D& arc : cached_arches)
        {
            if(arc.start.x < min_x) min_x = arc.start.x; if(arc.start.x > max_x) max_x = arc.start.x;
            if(arc.start.y < min_y) min_y = arc.start.y; if(arc.start.y > max_y) max_y = arc.start.y;
            if(arc.start.z < min_z) min_z = arc.start.z; if(arc.start.z > max_z) max_z = arc.start.z;
            if(arc.end.x < min_x) min_x = arc.end.x; if(arc.end.x > max_x) max_x = arc.end.x;
            if(arc.end.y < min_y) min_y = arc.end.y; if(arc.end.y > max_y) max_y = arc.end.y;
            if(arc.end.z < min_z) min_z = arc.end.z; if(arc.end.z > max_z) max_z = arc.end.z;
        }
        arc_aabb_min_x = min_x - margin; arc_aabb_max_x = max_x + margin;
        arc_aabb_min_y = min_y - margin; arc_aabb_max_y = max_y + margin;
        arc_aabb_min_z = min_z - margin; arc_aabb_max_z = max_z + margin;
    }
}


RGBColor Lightning::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);
    const float tm = std::max(0.25f, bb.tight_mul);
    float color_cycle = time * GetScaledFrequency() * 12.0f * bb.speed_mul;
    float detail = std::max(0.05f, GetScaledDetail());
    if(mode != MODE_PLASMA_BALL)
    {
        float time_m = time * bb.speed_mul;
        float rate = std::max(0.05f, std::min(0.5f, flash_rate));
        float interval = 1.0f / rate;
        float dur = std::max(0.02f, std::min(0.25f, flash_duration));

        float cycle = floorf(time_m / interval);
        float flash_offset = hash11(cycle) * interval * 0.6f;
        float phase_in_cycle = time_m - cycle * interval;
        float flash_phase = phase_in_cycle - flash_offset;

        float flash_env = 0.0f;
        if(flash_phase >= 0.0f && flash_phase < dur)
        {
            float rise = (flash_phase < dur * 0.15f) ? (flash_phase / (dur * 0.15f)) : 1.0f;
            float fall = (flash_phase > dur * 0.6f) ? (1.0f - (flash_phase - dur * 0.6f) / (dur * 0.4f)) : 1.0f;
            flash_env = rise * fall;
        }

        if(flash_env <= 0.001f)
            return 0x00000000;

        float norm_y = coord2;
        float sky_factor = 0.5f + 0.5f * norm_y;

        RGBColor base;
        if(GetRainbowMode())
            base = GetRainbowColor(color_cycle + norm_y * 100.0f * (0.6f + 0.4f * detail) + bb.phase_deg);
        else
        {
            const std::vector<RGBColor>& cols = GetColors();
            base = (cols.size() > 0) ? cols[0] : 0x00FFFFFF;
        }

        if(mode == MODE_SKY_FLASH)
        {
            int r = base & 0xFF, g = (base >> 8) & 0xFF, b = (base >> 16) & 0xFF;
            r = (int)(r * flash_env * sky_factor);
            g = (int)(g * flash_env * sky_factor);
            b = (int)(b * flash_env * sky_factor);
            r = std::min(255, r); g = std::min(255, g); b = std::min(255, b);
            return (RGBColor)((b << 16) | (g << 8) | r);
        }

        float room_avg = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
        float bolt_core = room_avg * (0.010f + 0.008f * GetNormalizedSize()) / tm;
        float bolt_glow = room_avg * (0.035f + 0.020f * GetNormalizedSize()) / tm;
        int main_bolts = (mode == MODE_SKY_BOLT) ? 1 : (mode == MODE_SKY_FORKED ? 2 : 3);
        int branch_count = (mode == MODE_SKY_BOLT) ? 0 : (mode == MODE_SKY_FORKED ? 2 : 4);
        float bolt_intensity = 0.0f;

        for(int bi = 0; bi < main_bolts; bi++)
        {
            unsigned int seed = (unsigned int)cycle * 9781u ^ (unsigned int)(bi * 6271u + mode * 911u);
            float sx = grid.min_x + HashF(seed + 1u) * grid.width;
            float sy = grid.max_y;
            float sz = grid.min_z + HashF(seed + 2u) * grid.depth;
            float ex = sx + (HashF(seed + 3u) - 0.5f) * grid.width * (mode == MODE_SKY_STORM ? 0.55f : 0.35f);
            float ey = grid.min_y + (0.10f + 0.45f * HashF(seed + 4u)) * grid.height;
            float ez = sz + (HashF(seed + 5u) - 0.5f) * grid.depth * (mode == MODE_SKY_STORM ? 0.55f : 0.35f);

            float d_main = DistToSegment(x, y, z, sx, sy, sz, ex, ey, ez);
            float main_core = std::max(0.0f, 1.0f - d_main / (bolt_core + 1e-6f));
            float main_glow = std::max(0.0f, 1.0f - d_main / (bolt_glow + 1e-6f)) * 0.65f;
            bolt_intensity = std::max(bolt_intensity, main_core + main_glow);

            for(int br = 0; br < branch_count; br++)
            {
                unsigned int bseed = seed ^ (unsigned int)(br * 3571u + 17u);
                float t = 0.20f + 0.70f * HashF(bseed + 1u);
                float bx = sx + (ex - sx) * t;
                float by = sy + (ey - sy) * t;
                float bz = sz + (ez - sz) * t;
                float bdx = (HashF(bseed + 2u) - 0.5f) * grid.width * 0.25f;
                float bdy = -(0.08f + 0.20f * HashF(bseed + 3u)) * grid.height;
                float bdz = (HashF(bseed + 4u) - 0.5f) * grid.depth * 0.25f;
                float tx = bx + bdx;
                float ty = by + bdy;
                float tz = bz + bdz;

                float d_branch = DistToSegment(x, y, z, bx, by, bz, tx, ty, tz);
                float branch_core = std::max(0.0f, 1.0f - d_branch / (bolt_core * 0.75f + 1e-6f));
                float branch_glow = std::max(0.0f, 1.0f - d_branch / (bolt_glow * 0.85f + 1e-6f)) * 0.45f;
                bolt_intensity = std::max(bolt_intensity, branch_core + branch_glow);
            }
        }

        float ambient = (mode == MODE_SKY_STORM) ? 0.22f : 0.12f;
        float intensity = std::min(1.0f, flash_env * (ambient * sky_factor + bolt_intensity));
        int r = base & 0xFF, g = (base >> 8) & 0xFF, b = (base >> 16) & 0xFF;
        r = (int)(r * intensity * sky_factor);
        g = (int)(g * intensity * sky_factor);
        b = (int)(b * intensity * sky_factor);
        r = std::min(255, r); g = std::min(255, g); b = std::min(255, b);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    float rx = x - origin.x, ry = y - origin.y, rz = z - origin.z;
    if(!IsWithinEffectBoundary(rx, ry, rz, grid))
        return 0x00000000;

    float dist_from_center = sqrtf(rx*rx + ry*ry + rz*rz);

    float size_m = GetNormalizedSize();
    float room_avg = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float core_radius = room_avg * (0.04f + 0.06f * size_m) / tm;
    float core_glow = room_avg * (0.08f + 0.10f * size_m) / tm;
    float arc_core = room_avg * (0.015f + 0.02f * size_m) / tm;
    float arc_glow = room_avg * (0.06f + 0.08f * size_m) / tm;

    float center_intensity = 0.0f;
    if(dist_from_center < core_glow * 2.0f)
    {
        float t = dist_from_center / (core_radius + 1e-6f);
        center_intensity = (t < 1.0f) ? (1.0f - t * 0.5f) : 0.0f;
        float glow_t = dist_from_center / (core_glow + 1e-6f);
        center_intensity += 0.6f * std::max(0.0f, 1.0f - glow_t);
        center_intensity = std::min(1.0f, center_intensity);
    }

    UpdateArchCache(time, grid);

    float arc_intensity = 0.0f;
    RGBColor arc_color = GetRainbowMode() ? GetRainbowColor(220.0f + color_cycle + bb.phase_deg) : GetColorAtPosition(0.5f);

    bool in_arc_aabb = (cached_arches.empty()) ||
        (x >= arc_aabb_min_x && x <= arc_aabb_max_x &&
         y >= arc_aabb_min_y && y <= arc_aabb_max_y &&
         z >= arc_aabb_min_z && z <= arc_aabb_max_z);

    if(in_arc_aabb)
    for(size_t i = 0; i < cached_arches.size(); i++)
    {
        const PlasmaArc3D& arc = cached_arches[i];
        float age = time - arc.birth_time;
        float decay = std::max(0.0f, 1.0f - age / arc.duration);
        float flicker = 0.7f + 0.3f * sinf(age * 80.0f * bb.speed_mul + bb.phase_deg * (float)(M_PI / 180.0f) + (float)arc.seed);
        float a_int = decay * flicker;
        if(a_int < 0.02f) continue;

        float d = DistToSegment(rx + origin.x, ry + origin.y, rz + origin.z,
                               arc.start.x, arc.start.y, arc.start.z,
                               arc.end.x, arc.end.y, arc.end.z);

        float core = std::max(0.0f, 1.0f - d / (arc_core + 1e-6f));
        float glow = std::max(0.0f, 1.0f - d / (arc_glow + 1e-6f)) * 0.7f;
        float contrib = (core + glow) * a_int;
        if(contrib > arc_intensity)
        {
            arc_intensity = contrib;
            if(GetRainbowMode())
                arc_color = GetRainbowColor(220.0f + age * 100.0f * bb.speed_mul + (float)arc.seed * 0.1f + color_cycle + bb.phase_deg);
        }
    }

    float total = std::min(1.0f, center_intensity + arc_intensity);
    total *= GetNormalizedScale();

    int r = (int)((arc_color & 0xFF) * total);
    int g = (int)(((arc_color >> 8) & 0xFF) * total);
    int b = (int)(((arc_color >> 16) & 0xFF) * total);
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Lightning::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "lightning_stratum_layout_mode",
                                           sm,
                                           st,
                                           "lightning_stratum_band_speed_pct",
                                           "lightning_stratum_band_tight_pct",
                                           "lightning_stratum_band_phase_deg");
    j["mode"] = mode;
    j["strike_rate"] = strike_rate;
    j["branches"] = branches;
    j["flash_rate"] = flash_rate;
    j["flash_duration"] = flash_duration;
    return j;
}

void Lightning::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "lightning_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "lightning_stratum_band_speed_pct",
                                            "lightning_stratum_band_tight_pct",
                                            "lightning_stratum_band_phase_deg");
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("strike_rate") && settings["strike_rate"].is_number_integer())
        strike_rate = std::max(1u, std::min(20u, (unsigned int)settings["strike_rate"].get<int>()));
    if(settings.contains("branches") && settings["branches"].is_number_integer())
        branches = std::max(2u, std::min(12u, (unsigned int)settings["branches"].get<int>()));
    if(settings.contains("flash_rate") && settings["flash_rate"].is_number())
        flash_rate = std::max(0.05f, std::min(0.5f, settings["flash_rate"].get<float>()));
    if(settings.contains("flash_duration") && settings["flash_duration"].is_number())
        flash_duration = std::max(0.02f, std::min(0.25f, settings["flash_duration"].get<float>()));

    if(strike_rate_slider) { strike_rate_slider->setValue((int)strike_rate); }
    if(branch_slider) { branch_slider->setValue((int)branches); }
    cache_time = -1e9f;
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(Lightning);
