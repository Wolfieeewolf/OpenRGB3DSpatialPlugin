// SPDX-License-Identifier: GPL-2.0-only

#include "Fireworks.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(Fireworks);

const char* Fireworks::TypeName(int t)
{
    switch(t)
    {
    case TYPE_SINGLE: return "Single burst";
    case TYPE_BIG_EXPLOSION: return "Big explosion";
    case TYPE_ROMAN_CANDLE: return "Roman candle";
    case TYPE_SPINNER: return "Spinner";
    case TYPE_FOUNTAIN: return "Fountain";
    case TYPE_RANDOM: return "Random (mixed styles)";
    default: return "Single burst";
    }
}

const char* Fireworks::BurstStyleName(int s)
{
    switch(s)
    {
    case BURST_AUTO: return "Auto mix";
    case BURST_PEONY: return "Peony";
    case BURST_CHRYSANTHEMUM: return "Chrysanthemum";
    case BURST_WILLOW: return "Willow";
    case BURST_PALM: return "Palm";
    case BURST_CROSSETTE: return "Crossette";
    default: return "Auto mix";
    }
}

static float hash_f(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

static float dist_point_segment_sq(float px, float py, float pz,
                                   float ax, float ay, float az,
                                   float bx, float by, float bz)
{
    const float abx = bx - ax;
    const float aby = by - ay;
    const float abz = bz - az;
    const float apx = px - ax;
    const float apy = py - ay;
    const float apz = pz - az;
    const float ab2 = abx * abx + aby * aby + abz * abz;
    if(ab2 <= 1e-8f)
    {
        const float dx = px - ax;
        const float dy = py - ay;
        const float dz = pz - az;
        return dx * dx + dy * dy + dz * dz;
    }
    float t = (apx * abx + apy * aby + apz * abz) / ab2;
    t = std::clamp(t, 0.0f, 1.0f);
    const float qx = ax + abx * t;
    const float qy = ay + aby * t;
    const float qz = az + abz * t;
    const float dx = px - qx;
    const float dy = py - qy;
    const float dz = pz - qz;
    return dx * dx + dy * dy + dz * dz;
}

Fireworks::Fireworks(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D Fireworks::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Fireworks";
    info.effect_description =
        "Missile launches and explodes into debris (Mega-Cube style); gravity, decay, and optional floor/mid/ceiling band tuning";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
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

void Fireworks::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);
    int row = 0;
    layout->addWidget(new QLabel("Type:"), row, 0);
    QComboBox* type_combo = new QComboBox();
    for(int t = 0; t < TYPE_COUNT; t++) type_combo->addItem(TypeName(t));
    type_combo->setCurrentIndex(std::max(0, std::min(firework_type, TYPE_COUNT - 1)));
    type_combo->setToolTip("Launch and burst personality. Random cycles mixed styles each cycle.");
    type_combo->setItemData(0, "One classic mortar burst.", Qt::ToolTipRole);
    type_combo->setItemData(1, "Large aerial burst.", Qt::ToolTipRole);
    type_combo->setItemData(2, "Repeated upward shots from low height.", Qt::ToolTipRole);
    type_combo->setItemData(3, "Spinning fountain-style emission.", Qt::ToolTipRole);
    type_combo->setItemData(4, "Upward spray that falls with gravity.", Qt::ToolTipRole);
    type_combo->setItemData(5, "Pick a different style each cycle.", Qt::ToolTipRole);
    layout->addWidget(type_combo, row, 1, 1, 2);
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        firework_type = std::max(0, std::min(idx, TYPE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Burst style:"), row, 0);
    QComboBox* burst_combo = new QComboBox();
    for(int s = 0; s < BURST_COUNT; s++) burst_combo->addItem(BurstStyleName(s));
    burst_combo->setCurrentIndex(std::max(0, std::min(burst_style, BURST_COUNT - 1)));
    burst_combo->setToolTip("Shape profile for aerial shell particles.");
    burst_combo->setItemData(0, "Blend styles automatically per launch.", Qt::ToolTipRole);
    burst_combo->setItemData(1, "Classic spherical shell.", Qt::ToolTipRole);
    burst_combo->setItemData(2, "Round shell with dense outer ring.", Qt::ToolTipRole);
    burst_combo->setItemData(3, "Slow, drooping trails.", Qt::ToolTipRole);
    burst_combo->setItemData(4, "Upward-heavy palm fronds.", Qt::ToolTipRole);
    burst_combo->setItemData(5, "Small branch splits from each star.", Qt::ToolTipRole);
    layout->addWidget(burst_combo, row, 1, 1, 2);
    connect(burst_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        burst_style = std::max(0, std::min(idx, BURST_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Simultaneous:"), row, 0);
    QSlider* sim_slider = new QSlider(Qt::Horizontal);
    sim_slider->setRange(1, 10);
    sim_slider->setToolTip("How many bursts can be active at once (more = busier, heavier).");
    sim_slider->setValue(num_simultaneous);
    QLabel* sim_label = new QLabel(QString::number(num_simultaneous));
    sim_label->setMinimumWidth(36);
    layout->addWidget(sim_slider, row, 1);
    layout->addWidget(sim_label, row, 2);
    connect(sim_slider, &QSlider::valueChanged, this, [this, sim_label](int v){
        num_simultaneous = v;
        if(sim_label) sim_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Show density:"), row, 0);
    QSlider* density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(50, 200);
    density_slider->setToolTip("How packed the overall show is (active launches and particle volume).");
    density_slider->setValue((int)(show_density * 100.0f));
    QLabel* density_label = new QLabel(QString::number((int)(show_density * 100.0f)) + "%");
    density_label->setMinimumWidth(36);
    layout->addWidget(density_slider, row, 1);
    layout->addWidget(density_label, row, 2);
    connect(density_slider, &QSlider::valueChanged, this, [this, density_label](int v){
        show_density = v / 100.0f;
        if(density_label) density_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Show variety:"), row, 0);
    QSlider* variety_slider = new QSlider(Qt::Horizontal);
    variety_slider->setRange(0, 100);
    variety_slider->setToolTip("How much shell size/height/position variation is added across launches.");
    variety_slider->setValue((int)(show_variety * 100.0f));
    QLabel* variety_label = new QLabel(QString::number((int)(show_variety * 100.0f)) + "%");
    variety_label->setMinimumWidth(36);
    layout->addWidget(variety_slider, row, 1);
    layout->addWidget(variety_label, row, 2);
    connect(variety_slider, &QSlider::valueChanged, this, [this, variety_label](int v){
        show_variety = v / 100.0f;
        if(variety_label) variety_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Particle count:"), row, 0);
    QSlider* count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(15, 100);
    count_slider->setToolTip("Debris particle count for styles that spawn sparks (not all types use it equally).");
    count_slider->setValue(num_debris);
    QLabel* count_label = new QLabel(QString::number(num_debris));
    count_label->setMinimumWidth(36);
    layout->addWidget(count_slider, row, 1);
    layout->addWidget(count_label, row, 2);
    connect(count_slider, &QSlider::valueChanged, this, [this, count_label](int v){
        num_debris = v;
        if(count_label) count_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Particle size:"), row, 0);
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(2, 100);
    size_slider->setToolTip("Visual size of debris streaks and sparks.");
    size_slider->setValue((int)(particle_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(particle_size * 100)) + "%");
    size_label->setMinimumWidth(36);
    layout->addWidget(size_slider, row, 1);
    layout->addWidget(size_label, row, 2);
    connect(size_slider, &QSlider::valueChanged, this, [this, size_label](int v){
        particle_size = v / 100.0f;
        if(size_label) size_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Gravity:"), row, 0);
    QSlider* grav_slider = new QSlider(Qt::Horizontal);
    grav_slider->setRange(0, 200);
    grav_slider->setToolTip("How strongly particles fall after burst (0 = floaty).");
    grav_slider->setValue((int)(gravity_strength * 100.0f));
    QLabel* grav_label = new QLabel(QString::number((int)(gravity_strength * 100)) + "%");
    grav_label->setMinimumWidth(36);
    layout->addWidget(grav_slider, row, 1);
    layout->addWidget(grav_label, row, 2);
    connect(grav_slider, &QSlider::valueChanged, this, [this, grav_label](int v){
        gravity_strength = v / 100.0f;
        if(grav_label) grav_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Decay speed:"), row, 0);
    QSlider* decay_slider = new QSlider(Qt::Horizontal);
    decay_slider->setRange(50, 600);
    decay_slider->setToolTip("How quickly sparks and trails fade out.");
    decay_slider->setValue((int)(decay_speed * 100.0f));
    QLabel* decay_label = new QLabel(QString::number(decay_speed, 'f', 1));
    decay_label->setMinimumWidth(36);
    layout->addWidget(decay_slider, row, 1);
    layout->addWidget(decay_label, row, 2);
    connect(decay_slider, &QSlider::valueChanged, this, [this, decay_label](int v){
        decay_speed = v / 100.0f;
        if(decay_label) decay_label->setText(QString::number(decay_speed, 'f', 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Launch trail:"), row, 0);
    QSlider* trail_slider = new QSlider(Qt::Horizontal);
    trail_slider->setRange(0, 200);
    trail_slider->setToolTip("Comet tail strength during launch.");
    trail_slider->setValue((int)std::lround(launch_trail_amount * 100.0f));
    QLabel* trail_label = new QLabel(QString::number((int)std::lround(launch_trail_amount * 100.0f)) + "%");
    trail_label->setMinimumWidth(36);
    layout->addWidget(trail_slider, row, 1);
    layout->addWidget(trail_label, row, 2);
    connect(trail_slider, &QSlider::valueChanged, this, [this, trail_label](int v){
        launch_trail_amount = v / 100.0f;
        if(trail_label) trail_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Burst flash:"), row, 0);
    QSlider* flash_slider = new QSlider(Qt::Horizontal);
    flash_slider->setRange(0, 200);
    flash_slider->setToolTip("Brightness and duration of the burst ignition flash.");
    flash_slider->setValue((int)std::lround(burst_flash_amount * 100.0f));
    QLabel* flash_label = new QLabel(QString::number((int)std::lround(burst_flash_amount * 100.0f)) + "%");
    flash_label->setMinimumWidth(36);
    layout->addWidget(flash_slider, row, 1);
    layout->addWidget(flash_label, row, 2);
    connect(flash_slider, &QSlider::valueChanged, this, [this, flash_label](int v){
        burst_flash_amount = v / 100.0f;
        if(flash_label) flash_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Ember amount:"), row, 0);
    QSlider* ember_slider = new QSlider(Qt::Horizontal);
    ember_slider->setRange(0, 200);
    ember_slider->setToolTip("How strong the post-burst ember layer is.");
    ember_slider->setValue((int)std::lround(ember_amount * 100.0f));
    QLabel* ember_label = new QLabel(QString::number((int)std::lround(ember_amount * 100.0f)) + "%");
    ember_label->setMinimumWidth(36);
    layout->addWidget(ember_slider, row, 1);
    layout->addWidget(ember_label, row, 2);
    connect(ember_slider, &QSlider::valueChanged, this, [this, ember_label](int v){
        ember_amount = v / 100.0f;
        if(ember_label) ember_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Ember hang time:"), row, 0);
    QSlider* hang_slider = new QSlider(Qt::Horizontal);
    hang_slider->setRange(40, 220);
    hang_slider->setToolTip("How long embers linger before fading.");
    hang_slider->setValue((int)std::lround(ember_hang_time * 100.0f));
    QLabel* hang_label = new QLabel(QString::number((int)std::lround(ember_hang_time * 100.0f)) + "%");
    hang_label->setMinimumWidth(36);
    layout->addWidget(hang_slider, row, 1);
    layout->addWidget(hang_label, row, 2);
    connect(hang_slider, &QSlider::valueChanged, this, [this, hang_label](int v){
        ember_hang_time = v / 100.0f;
        if(hang_label) hang_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
AddWidgetToParent(w, parent);
}

void Fireworks::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Fireworks::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);


    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float hw = e.hw, hh = e.hh, hd = e.hd;
    float h_scale = std::max({hw, hh, hd});
    constexpr float kFireworkGridFill = 3.0f;
    float speed_scale = GetScaledSpeed() * 0.015f * kFireworkGridFill;

    
    const float launch_inset_x = std::min(grid.width * 0.05f, hw);
    const float launch_inset_z = std::min(grid.depth * 0.05f, hd);
    const float launch_x = std::clamp(origin.x,
                                      grid.min_x + launch_inset_x,
                                      grid.max_x - launch_inset_x);
    const float launch_z = std::clamp(origin.z,
                                      grid.min_z + launch_inset_z,
                                      grid.max_z - launch_inset_z);
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float color_cycle = time * GetScaledFrequency() * 12.0f * bb.speed_mul;
    float sigma = std::max(particle_size * h_scale * size_m, 5.0f) / std::max(0.35f, detail);
    const float sigma_cap = h_scale * 1.0f;
    if(sigma > sigma_cap) sigma = sigma_cap;
    float sigma_sq = sigma * sigma;
    const float sigma_sq_use = sigma_sq / std::max(0.0625f, bb.tight_mul * bb.tight_mul);
    const float d2_cutoff = 9.0f * sigma_sq_use;
    float grav_mult = std::max(0.0f, std::min(2.0f, gravity_strength));
    float decay_mult = std::max(0.5f, std::min(6.0f, decay_speed));
    int n_sim = std::max(1, std::min(10, num_simultaneous));
    int type = std::max(0, std::min(firework_type, TYPE_COUNT - 1));
    const float show_density_m = std::clamp(show_density, 0.5f, 2.0f);
    const float show_variety_m = std::clamp(show_variety, 0.0f, 1.0f);
    const float trail_m = std::clamp(launch_trail_amount, 0.0f, 2.0f);
    const float flash_m = std::clamp(burst_flash_amount, 0.0f, 2.0f);
    const float ember_m = std::clamp(ember_amount, 0.0f, 2.0f);
    const float ember_hang_m = std::clamp(ember_hang_time, 0.4f, 2.2f);
    const int n_sim_eff = std::max(1, std::min(12, (int)std::lround((float)n_sim * (0.65f + 0.85f * show_density_m))));

    if(particle_cache.empty() || fabsf(time - particle_cache_time) > 0.001f)
    {
        particle_cache.clear();
        particle_cache_time = time;
        float cycle = CYCLE_DURATION;
        if(type == TYPE_ROMAN_CANDLE) cycle = 4.0f;
        if(type == TYPE_FOUNTAIN) cycle = 3.0f;
        const float gravity_base = -0.95f * speed_scale * hh * grav_mult;
        const float decay_coeff = 6.0f * decay_mult;

        for(int launch = 0; launch < n_sim_eff; launch++)
        {
            float phase = fmodf(time + (float)launch * (cycle / (float)n_sim_eff), cycle);
            int use_type = type;
            const unsigned int lseed = (unsigned int)(launch * 1543 + (int)std::floor(time / std::max(0.25f, cycle)) * 7919);
            if(use_type == TYPE_RANDOM)
            {
                float h = hash_f((unsigned int)launch, (unsigned int)(time / cycle * 1000.0f) + 1u);
                use_type = (int)((h + 1.0f) * 0.5f * (float)(TYPE_COUNT - 1)) % (TYPE_COUNT - 1);
                if(use_type < 0) use_type = 0;
            }
            float shell_pick = (hash_f(lseed, 41u) + 1.0f) * 0.5f;
            float shell_scale = (shell_pick < 0.33f) ? 0.72f : (shell_pick < 0.78f ? 1.00f : 1.35f);
            shell_scale *= (0.85f + 0.45f * show_variety_m);
            float launch_spread_x = hw * (0.20f + 0.65f * show_variety_m);
            float launch_spread_z = hd * (0.20f + 0.65f * show_variety_m);
            float launch_x_i = std::clamp(launch_x + hash_f(lseed, 101u) * launch_spread_x, grid.min_x, grid.max_x);
            float launch_z_i = std::clamp(launch_z + hash_f(lseed, 102u) * launch_spread_z, grid.min_z, grid.max_z);
            float launch_floor = grid.min_y + grid.height * (0.08f + 0.20f * ((hash_f(lseed, 103u) + 1.0f) * 0.5f));
            float burst_y = grid.min_y + grid.height * (0.45f + 0.45f * ((hash_f(lseed, 104u) + 1.0f) * 0.5f));

            if(use_type == TYPE_FOUNTAIN)
            {
                float spray_duration = 2.0f;
                float gravity = gravity_base * 0.6f;
                int n_pt = std::max(18, std::min(120, (int)std::lround((float)num_debris * (0.6f + 0.8f * show_density_m))));
                for(int i = 0; i < n_pt; i++)
                {
                    float emit_t = (float)i / (float)n_pt * spray_duration;
                    if(phase < emit_t) continue;
                    float t = phase - emit_t;
                    float vx = hash_f((unsigned int)(launch * 1000 + i), 10u) * speed_scale * hw * (0.3f + 0.25f * shell_scale);
                    float vy = (0.45f + 0.55f * (hash_f((unsigned int)(launch * 1000 + i), 20u) + 1.0f) * 0.5f) * speed_scale * hh * shell_scale;
                    float vz = hash_f((unsigned int)(launch * 1000 + i), 30u) * speed_scale * hd * (0.3f + 0.25f * shell_scale);
                    float px = launch_x_i + vx * t;
                    float py = launch_floor + vy * t + 0.5f * gravity * t * t;
                    float pz = launch_z_i + vz * t;
                    float decay = 1.0f / (1.0f + t * decay_coeff * 0.4f);
                    float hue = fmodf((float)i * 3.0f + color_cycle, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({px, py, pz, vx, vy, vz, decay, hue, hash_f((unsigned int)(launch * 1000 + i), 91u)});
                }
                continue;
            }

            if(use_type == TYPE_ROMAN_CANDLE)
            {
                float rise = 1.0f;
                float pop_interval = 0.35f;
                int num_pops = 5;
                for(int p = 0; p < num_pops; p++)
                {
                    float pop_time = rise + p * pop_interval;
                    if(phase < pop_time) continue;
                    float burst_t = phase - pop_time;
                    float decay = 1.0f / (1.0f + burst_t * decay_coeff * 0.6f);
                    float by = launch_floor + (pop_time / rise) * grid.height * (0.35f + 0.35f * shell_scale);
                    float bx = launch_x_i; float bz = launch_z_i;
                    int n_pt = std::max(10, (int)std::lround((float)num_debris * (0.18f + 0.18f * show_density_m)));
                    for(int i = 0; i < n_pt; i++)
                    {
                        unsigned int seed = (unsigned int)(launch * 500 + p * 100 + i);
                        float vx = hash_f(seed, 10u) * speed_scale * hw * 0.6f;
                        float vy = (0.2f + 0.4f * (hash_f(seed, 20u) + 1.0f) * 0.5f) * speed_scale * hh;
                        float vz = hash_f(seed, 30u) * speed_scale * hd * 0.6f;
                        float px = bx + vx * burst_t;
                        float py = by + vy * burst_t + 0.5f * gravity_base * burst_t * burst_t;
                        float pz = bz + vz * burst_t;
                        float hue = fmodf((float)(p * n_pt + i) + color_cycle, 360.0f);
                        if(hue < 0.0f) hue += 360.0f;
                        particle_cache.push_back({px, py, pz, vx, vy, vz, decay, hue, hash_f(seed, 92u)});
                    }
                }
                continue;
            }

            if(use_type == TYPE_SPINNER)
            {
                float rise_duration = 0.9f;
                if(phase < rise_duration)
                {
                    float t = phase / rise_duration;
                    float mx = launch_x_i + (0.18f + 0.20f * shell_scale) * hw * cosf(time * 8.0f + (float)launch);
                    float my = launch_floor + t * grid.height * (0.45f + 0.25f * shell_scale);
                    float mz = launch_z_i + (0.18f + 0.20f * shell_scale) * hd * sinf(time * 8.0f + (float)launch);
                    float hue = fmodf(time * 60.0f, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({mx, my, mz, 0.0f, speed_scale * hh * 0.9f, 0.0f, 1.0f, hue, hash_f(lseed, 93u)});
                    int trail = 12;
                    for(int i = 0; i < trail; i++)
                    {
                        float ti = (float)i / (float)trail * t;
                        float tx = launch_x_i + (0.22f + 0.22f * shell_scale) * hw * cosf(time * 8.0f + (float)launch + ti * 6.0f);
                        float ty = launch_floor + ti * grid.height * (0.45f + 0.25f * shell_scale);
                        float tz = launch_z_i + (0.22f + 0.22f * shell_scale) * hd * sinf(time * 8.0f + (float)launch + ti * 6.0f);
                        float decay = 1.0f - ti * 0.7f;
                        float h = fmodf((float)i * 30.0f, 360.0f);
                        if(h < 0.0f) h += 360.0f;
                        particle_cache.push_back({tx, ty, tz, 0.0f, speed_scale * hh * 0.5f, 0.0f, decay, h, hash_f((unsigned int)(launch * 300 + i), 94u)});
                    }
                }
                else
                {
                    float burst_t = phase - rise_duration;
                    float decay = 1.0f / (1.0f + burst_t * decay_coeff * 0.5f);
                    float ex = launch_x_i, ey = burst_y, ez = launch_z_i;
                    int n_pt = std::max(12, (int)std::lround((float)num_debris * (0.35f + 0.35f * show_density_m)));
                    for(int i = 0; i < n_pt; i++)
                    {
                        float vx = hash_f((unsigned int)(launch * 200 + i), 10u) * speed_scale * hw * 0.5f;
                        float vy = (0.2f + 0.3f * (hash_f((unsigned int)(launch * 200 + i), 20u) + 1.0f) * 0.5f) * speed_scale * hh;
                        float vz = hash_f((unsigned int)(launch * 200 + i), 30u) * speed_scale * hd * 0.5f;
                        float px = ex + vx * burst_t;
                        float py = ey + vy * burst_t + 0.5f * gravity_base * 0.4f * burst_t * burst_t;
                        float pz = ez + vz * burst_t;
                        float hue = fmodf((float)i * 5.0f + time * 20.0f, 360.0f);
                        if(hue < 0.0f) hue += 360.0f;
                        particle_cache.push_back({px, py, pz, vx, vy, vz, decay, hue, hash_f((unsigned int)(launch * 200 + i), 95u)});
                    }
                }
                continue;
            }

            float missile_dur = (use_type == TYPE_BIG_EXPLOSION) ? 0.9f : MISSILE_DURATION;
            if(phase < missile_dur)
            {
                float t = phase / missile_dur;
                float mx = launch_x_i;
                float my = launch_floor + t * (burst_y - launch_floor);
                float mz = launch_z_i;
                float hue = fmodf(time * 50.0f + (float)launch * 70.0f, 360.0f);
                if(hue < 0.0f) hue += 360.0f;
                particle_cache.push_back({mx, my, mz, 0.0f, speed_scale * hh * 1.1f, 0.0f, 1.0f, hue, hash_f(lseed, 96u)});
                const int trail_pts = std::max(2, (int)std::lround(14.0f * trail_m));
                for(int ti = 1; ti <= trail_pts; ++ti)
                {
                    const float tt = std::max(0.0f, t - (float)ti / (float)trail_pts * 0.32f);
                    const float tx = launch_x_i;
                    const float ty = launch_floor + tt * (burst_y - launch_floor);
                    const float tz = launch_z_i;
                    const float tail_decay = std::max(0.04f, (1.0f - (float)ti / (float)trail_pts) * (0.35f + 0.65f * trail_m));
                    const float tail_hue = std::fmod(hue * 0.3f + 18.0f + (float)ti * 2.5f, 360.0f);
                    particle_cache.push_back({tx,
                                              ty,
                                              tz,
                                              0.0f,
                                              speed_scale * hh * 0.65f,
                                              0.0f,
                                              tail_decay,
                                              tail_hue,
                                              hash_f((unsigned int)(lseed + (unsigned int)ti * 17u), 960u)});
                }
            }
            else
            {
                float explode_t = phase - missile_dur;
                float decay = 1.0f / (1.0f + explode_t * decay_coeff);
                float ex = launch_x_i, ey = burst_y, ez = launch_z_i;
                const float flash_window = 0.03f + 0.08f * flash_m;
                if(explode_t < flash_window && flash_m > 0.001f)
                {
                    const float flash_decay = 1.0f - explode_t / std::max(1e-3f, flash_window);
                    particle_cache.push_back(
                        {ex, ey, ez, 0.0f, 0.0f, 0.0f, std::max(0.2f, flash_decay) * flash_m, 48.0f, 0.99f});
                }
                int n_debris_use = (use_type == TYPE_BIG_EXPLOSION)
                    ? (int)std::lround((float)num_debris * (1.4f + 0.9f * show_density_m))
                    : (int)std::lround((float)num_debris * (0.7f + 0.8f * show_density_m));
                n_debris_use = std::max(16, std::min(160, n_debris_use));
                float vel_scale = ((use_type == TYPE_BIG_EXPLOSION) ? 1.35f : 0.95f) * shell_scale;
                int style_use = std::max(0, std::min(burst_style, BURST_COUNT - 1));
                if(style_use == BURST_AUTO)
                {
                    style_use = 1 + ((int)std::fabs(hash_f(lseed, 401u) * 1000.0f) % (BURST_COUNT - 1));
                }

                for(int i = 0; i < n_debris_use; i++)
                {
                    unsigned int seed = (unsigned int)(launch * 1000 + i);
                    float u = (hash_f(seed, 10u) + 1.0f) * 0.5f;
                    float v = (hash_f(seed, 11u) + 1.0f) * 0.5f;
                    float az = u * 2.0f * (float)M_PI;
                    float cz = 2.0f * v - 1.0f;
                    float sz = std::sqrt(std::max(0.0f, 1.0f - cz * cz));
                    float dir_x = cosf(az) * sz;
                    float dir_y = cz;
                    float dir_z = sinf(az) * sz;
                    float base_v = speed_scale * std::max(hw, hd) * (0.45f + 0.35f * vel_scale);
                    float vy_scale = speed_scale * hh * (0.65f + 0.45f * vel_scale);
                    float decay_local = decay;
                    float sparkle_local = hash_f(seed, 97u);
                    if(style_use == BURST_PEONY)
                    {
                        dir_y *= 0.9f;
                    }
                    else if(style_use == BURST_CHRYSANTHEMUM)
                    {
                        const float ring = 0.85f + 0.30f * ((hash_f(seed, 402u) + 1.0f) * 0.5f);
                        base_v *= ring;
                        dir_y *= 0.75f;
                    }
                    else if(style_use == BURST_WILLOW)
                    {
                        base_v *= 0.65f;
                        vy_scale *= 0.55f;
                        dir_y = std::max(0.0f, dir_y) * 0.35f;
                        decay_local *= 1.45f;
                    }
                    else if(style_use == BURST_PALM)
                    {
                        const float frond = (hash_f(seed, 403u) + 1.0f) * 0.5f;
                        dir_y = 0.35f + 0.65f * std::max(0.0f, dir_y) + frond * 0.2f;
                        base_v *= 0.80f;
                        vy_scale *= 1.25f;
                    }
                    else if(style_use == BURST_CROSSETTE)
                    {
                        base_v *= 0.78f;
                        sparkle_local = 0.95f;
                    }
                    float vx = dir_x * base_v;
                    float vy = dir_y * vy_scale;
                    float vz = dir_z * base_v;
                    float px = ex + vx * explode_t;
                    float py = ey + vy * explode_t + 0.5f * gravity_base * explode_t * explode_t;
                    float pz = ez + vz * explode_t;
                    float hue = fmodf((float)i * 4.0f + time * 20.0f + (float)launch * 50.0f, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({px, py, pz, vx, vy, vz, decay_local, hue, sparkle_local});
                    const float ember_vx = vx * (0.18f + 0.16f * ember_m);
                    const float ember_vy = vy * (0.10f + 0.15f * ember_m);
                    const float ember_vz = vz * (0.18f + 0.16f * ember_m);
                    const float ember_px = ex + ember_vx * explode_t;
                    const float ember_py = ey + ember_vy * explode_t + 0.5f * gravity_base * (0.65f / ember_hang_m) * explode_t * explode_t;
                    const float ember_pz = ez + ember_vz * explode_t;
                    const float ember_decay = std::min(1.0f, decay_local * (0.7f + 0.95f * ember_m) * ember_hang_m);
                    const float ember_hue = std::fmod(hue * 0.45f + 22.0f, 360.0f);
                    if(ember_m > 0.001f)
                    {
                        particle_cache.push_back(
                            {ember_px, ember_py, ember_pz, ember_vx, ember_vy, ember_vz, ember_decay, ember_hue, 0.92f});
                    }
                    if(style_use == BURST_CROSSETTE)
                    {
                        for(int c = 0; c < 3; ++c)
                        {
                            const float branch = (2.0f * (float)M_PI * (float)c) / 3.0f;
                            const float bx = vx * 0.35f + cosf(branch) * base_v * 0.25f;
                            const float by = vy * 0.35f + (0.10f + 0.15f * c) * vy_scale;
                            const float bz = vz * 0.35f + sinf(branch) * base_v * 0.25f;
                            particle_cache.push_back({px, py, pz, bx, by, bz, decay_local * 0.85f, hue + 8.0f * c, 0.98f});
                        }
                    }
                }
            }
        }
        if(!particle_cache.empty())
        {
            float margin = 3.0f * sigma;
            float min_x = particle_cache[0].px, min_y = particle_cache[0].py, min_z = particle_cache[0].pz;
            float max_x = min_x, max_y = min_y, max_z = min_z;
            for(const CachedParticle& p : particle_cache)
            {
                if(p.px < min_x) min_x = p.px; if(p.px > max_x) max_x = p.px;
                if(p.py < min_y) min_y = p.py; if(p.py > max_y) max_y = p.py;
                if(p.pz < min_z) min_z = p.pz; if(p.pz > max_z) max_z = p.pz;
            }
            particle_aabb_min_x = min_x - margin; particle_aabb_max_x = max_x + margin;
            particle_aabb_min_y = min_y - margin; particle_aabb_max_y = max_y + margin;
            particle_aabb_min_z = min_z - margin; particle_aabb_max_z = max_z + margin;
        }
    }

    if(particle_cache.empty())
        return (RGBColor)((0 << 16) | (0 << 8) | 0);
    if(x < particle_aabb_min_x || x > particle_aabb_max_x ||
       y < particle_aabb_min_y || y > particle_aabb_max_y ||
       z < particle_aabb_min_z || z > particle_aabb_max_z)
        return (RGBColor)((0 << 16) | (0 << 8) | 0);

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    for(const CachedParticle& p : particle_cache)
    {
        float dx = x - p.px, dy = y - p.py, dz = z - p.pz;
        float d2 = dx*dx + dy*dy + dz*dz;
        if(d2 > d2_cutoff * 1.35f) continue;
        const float vmag = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
        const float vnorm = (vmag > 1e-6f) ? 1.0f / vmag : 0.0f;
        const float trail_len = sigma * (0.8f + 3.0f * p.decay) * (1.0f + std::min(1.5f, vmag / std::max(1e-3f, speed_scale * h_scale)));
        const float ax = p.px;
        const float ay = p.py;
        const float az = p.pz;
        const float bx = p.px - p.vx * vnorm * trail_len;
        const float by = p.py - p.vy * vnorm * trail_len;
        const float bz = p.pz - p.vz * vnorm * trail_len;
        const float d2_trail = dist_point_segment_sq(x, y, z, ax, ay, az, bx, by, bz);
        const float head = expf(-d2 / sigma_sq_use);
        const float trail = expf(-d2_trail / (sigma_sq_use * 0.35f));
        const float twinkle = 0.72f + 0.28f * (0.5f + 0.5f * sinf(time * 35.0f + p.sparkle * 12.0f));
        float intensity = (0.35f * head + 0.90f * trail) * p.decay * twinkle;
        if(intensity < 0.01f) continue;
        float hue_use = fmodf(p.hue  + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f + time * GetScaledFrequency() * 6.0f * (bb.speed_mul - 1.0f), 360.0f);
        if(hue_use < 0.0f) hue_use += 360.0f;
        float pal01 = hue_use / 360.0f;
        RGBColor c;
        if(UseEffectStripColormap())
        {
            const float ph01 = std::fmod(color_cycle * (1.f / 360.f) + p.hue * (1.f / 360.f) +
                                             time * GetScaledFrequency() * 0.04f * bb.speed_mul + 1.f,
                                         1.f);
            pal01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
            pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
            c     = ResolveStripKernelFinalColor(*this,
                                                  GetEffectStripColormapKernel(),
                                                  std::clamp(pal01, 0.0f, 1.0f),
                                                  GetEffectStripColormapColorStyle(),
                                                  time,
                                                  GetScaledFrequency() * 12.0f * bb.speed_mul);
        }
        else
        {
            c = GetRainbowMode() ? GetRainbowColor(hue_use) : GetColorAtPosition(pal01);
        }
        sum_r += ((c & 0xFF) / 255.0f) * intensity;
        sum_g += (((c >> 8) & 0xFF) / 255.0f) * intensity;
        sum_b += (((c >> 16) & 0xFF) / 255.0f) * intensity;
    }
    sum_r = std::min(1.0f, sum_r);
    sum_g = std::min(1.0f, sum_g);
    sum_b = std::min(1.0f, sum_b);
    int r_ = std::min(255, std::max(0, (int)(sum_r * 255.0f)));
    int g_ = std::min(255, std::max(0, (int)(sum_g * 255.0f)));
    int b_ = std::min(255, std::max(0, (int)(sum_b * 255.0f)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json Fireworks::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["particle_size"] = particle_size;
    j["num_debris"] = num_debris;
    j["firework_type"] = firework_type;
    j["burst_style"] = burst_style;
    j["num_simultaneous"] = num_simultaneous;
    j["show_density"] = show_density;
    j["show_variety"] = show_variety;
    j["launch_trail_amount"] = launch_trail_amount;
    j["burst_flash_amount"] = burst_flash_amount;
    j["ember_amount"] = ember_amount;
    j["ember_hang_time"] = ember_hang_time;
    j["gravity_strength"] = gravity_strength;
    j["decay_speed"] = decay_speed;
return j;
}

void Fireworks::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("particle_size") && settings["particle_size"].is_number())
        particle_size = std::max(0.02f, std::min(1.0f, settings["particle_size"].get<float>()));
    if(settings.contains("num_debris") && settings["num_debris"].is_number())
        num_debris = std::max(10, std::min(100, settings["num_debris"].get<int>()));
    if(settings.contains("firework_type") && settings["firework_type"].is_number())
        firework_type = std::max(0, std::min(settings["firework_type"].get<int>(), TYPE_COUNT - 1));
    if(settings.contains("burst_style") && settings["burst_style"].is_number())
        burst_style = std::max(0, std::min(settings["burst_style"].get<int>(), BURST_COUNT - 1));
    if(settings.contains("num_simultaneous") && settings["num_simultaneous"].is_number())
        num_simultaneous = std::max(1, std::min(10, settings["num_simultaneous"].get<int>()));
    if(settings.contains("show_density") && settings["show_density"].is_number())
        show_density = std::max(0.5f, std::min(2.0f, settings["show_density"].get<float>()));
    if(settings.contains("show_variety") && settings["show_variety"].is_number())
        show_variety = std::max(0.0f, std::min(1.0f, settings["show_variety"].get<float>()));
    if(settings.contains("launch_trail_amount") && settings["launch_trail_amount"].is_number())
        launch_trail_amount = std::max(0.0f, std::min(2.0f, settings["launch_trail_amount"].get<float>()));
    if(settings.contains("burst_flash_amount") && settings["burst_flash_amount"].is_number())
        burst_flash_amount = std::max(0.0f, std::min(2.0f, settings["burst_flash_amount"].get<float>()));
    if(settings.contains("ember_amount") && settings["ember_amount"].is_number())
        ember_amount = std::max(0.0f, std::min(2.0f, settings["ember_amount"].get<float>()));
    if(settings.contains("ember_hang_time") && settings["ember_hang_time"].is_number())
        ember_hang_time = std::max(0.4f, std::min(2.2f, settings["ember_hang_time"].get<float>()));
    if(settings.contains("gravity_strength") && settings["gravity_strength"].is_number())
        gravity_strength = std::max(0.0f, std::min(2.0f, settings["gravity_strength"].get<float>()));
    if(settings.contains("decay_speed") && settings["decay_speed"].is_number())
        decay_speed = std::max(0.5f, std::min(6.0f, settings["decay_speed"].get<float>()));
}
