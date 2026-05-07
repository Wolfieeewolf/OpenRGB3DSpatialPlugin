// SPDX-License-Identifier: GPL-2.0-only

#include "Fireworks.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
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

static float hash_f(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

Fireworks::Fireworks(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D Fireworks::GetEffectInfo()
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
    layout->addWidget(new QLabel("Simultaneous:"), row, 0);
    QSlider* sim_slider = new QSlider(Qt::Horizontal);
    sim_slider->setRange(1, 5);
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
    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(fireworks_strip_cmap_on,
                                            fireworks_strip_cmap_kernel,
                                            fireworks_strip_cmap_rep,
                                            fireworks_strip_cmap_unfold,
                                            fireworks_strip_cmap_dir,
                                            fireworks_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &Fireworks::SyncStripColormapFromPanel);
    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Fireworks::OnStratumBandChanged);
    OnStratumBandChanged();
    AddWidgetToParent(w, parent);
}

void Fireworks::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    fireworks_strip_cmap_on = strip_cmap_panel->useStripColormap();
    fireworks_strip_cmap_kernel = strip_cmap_panel->kernelId();
    fireworks_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    fireworks_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    fireworks_strip_cmap_dir = strip_cmap_panel->directionDeg();
    fireworks_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void Fireworks::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
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
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float hw = e.hw, hh = e.hh, hd = e.hd;
    float h_scale = std::max({hw, hh, hd});
    constexpr float kFireworkGridFill = 3.0f;
    float speed_scale = GetScaledSpeed() * 0.015f * kFireworkGridFill;
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
    int n_sim = std::max(1, std::min(5, num_simultaneous));
    int type = std::max(0, std::min(firework_type, TYPE_COUNT - 1));

    if(particle_cache.empty() || fabsf(time - particle_cache_time) > 0.001f)
    {
        particle_cache.clear();
        particle_cache_time = time;
        float cycle = CYCLE_DURATION;
        if(type == TYPE_ROMAN_CANDLE) cycle = 4.0f;
        if(type == TYPE_FOUNTAIN) cycle = 3.0f;
        const float gravity_base = -0.95f * speed_scale * hh * grav_mult;
        const float decay_coeff = 6.0f * decay_mult;

        for(int launch = 0; launch < n_sim; launch++)
        {
            float phase = fmodf(time + (float)launch * (cycle / (float)n_sim), cycle);
            int use_type = type;
            if(use_type == TYPE_RANDOM)
            {
                float h = hash_f((unsigned int)launch, (unsigned int)(time / cycle * 1000.0f) + 1u);
                use_type = (int)((h + 1.0f) * 0.5f * (float)(TYPE_COUNT - 1)) % (TYPE_COUNT - 1);
                if(use_type < 0) use_type = 0;
            }

            if(use_type == TYPE_FOUNTAIN)
            {
                float spray_duration = 2.0f;
                float gravity = gravity_base * 0.6f;
                int n_pt = std::max(15, std::min(80, num_debris));
                for(int i = 0; i < n_pt; i++)
                {
                    float emit_t = (float)i / (float)n_pt * spray_duration;
                    if(phase < emit_t) continue;
                    float t = phase - emit_t;
                    float vx = hash_f((unsigned int)(launch * 1000 + i), 10u) * speed_scale * hw * 0.4f;
                    float vy = (0.5f + 0.4f * (hash_f((unsigned int)(launch * 1000 + i), 20u) + 1.0f) * 0.5f) * speed_scale * hh;
                    float vz = hash_f((unsigned int)(launch * 1000 + i), 30u) * speed_scale * hd * 0.4f;
                    float px = origin.x + vx * t;
                    float py = origin.y - hh * 0.5f + vy * t + 0.5f * gravity * t * t;
                    float pz = origin.z + vz * t;
                    float decay = 1.0f / (1.0f + t * decay_coeff * 0.4f);
                    float hue = fmodf((float)i * 3.0f + color_cycle, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({px, py, pz, decay, hue});
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
                    float by = origin.y - hh * 0.6f + (pop_time / rise) * hh;
                    float bx = origin.x; float bz = origin.z;
                    int n_pt = std::max(8, num_debris / 4);
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
                        particle_cache.push_back({px, py, pz, decay, hue});
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
                    float mx = origin.x + 0.3f * hw * cosf(time * 8.0f + (float)launch);
                    float my = origin.y - hh * 0.8f + t * (hh * 1.1f);
                    float mz = origin.z + 0.3f * hd * sinf(time * 8.0f + (float)launch);
                    float hue = fmodf(time * 60.0f, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({mx, my, mz, 1.0f, hue});
                    int trail = 12;
                    for(int i = 0; i < trail; i++)
                    {
                        float ti = (float)i / (float)trail * t;
                        float tx = origin.x + 0.35f * hw * cosf(time * 8.0f + (float)launch + ti * 6.0f);
                        float ty = origin.y - hh * 0.8f + ti * (hh * 1.1f);
                        float tz = origin.z + 0.35f * hd * sinf(time * 8.0f + (float)launch + ti * 6.0f);
                        float decay = 1.0f - ti * 0.7f;
                        float h = fmodf((float)i * 30.0f, 360.0f);
                        if(h < 0.0f) h += 360.0f;
                        particle_cache.push_back({tx, ty, tz, decay, h});
                    }
                }
                else
                {
                    float burst_t = phase - rise_duration;
                    float decay = 1.0f / (1.0f + burst_t * decay_coeff * 0.5f);
                    float ex = origin.x, ey = origin.y + hh * 0.3f, ez = origin.z;
                    int n_pt = std::max(10, num_debris / 2);
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
                        particle_cache.push_back({px, py, pz, decay, hue});
                    }
                }
                continue;
            }

            float missile_dur = (use_type == TYPE_BIG_EXPLOSION) ? 0.9f : MISSILE_DURATION;
            if(phase < missile_dur)
            {
                float t = phase / missile_dur;
                float mx = origin.x;
                float my = origin.y - hh * 0.8f + t * (hh * 1.2f);
                float mz = origin.z;
                float hue = fmodf(time * 50.0f + (float)launch * 70.0f, 360.0f);
                if(hue < 0.0f) hue += 360.0f;
                particle_cache.push_back({mx, my, mz, 1.0f, hue});
            }
            else
            {
                float explode_t = phase - missile_dur;
                float decay = 1.0f / (1.0f + explode_t * decay_coeff);
                float ex = origin.x, ey = origin.y + hh * 0.4f, ez = origin.z;
                int n_debris_use = std::max(10, std::min(100, (use_type == TYPE_BIG_EXPLOSION) ? (num_debris * 3 / 2) : num_debris));
                float vel_scale = (use_type == TYPE_BIG_EXPLOSION) ? 1.4f : 1.0f;

                for(int i = 0; i < n_debris_use; i++)
                {
                    unsigned int seed = (unsigned int)(launch * 1000 + i);
                    float vx = hash_f(seed, 10u) * speed_scale * hw * 0.8f * vel_scale;
                    float vy = (0.3f + 0.5f * ((hash_f(seed, 20u) + 1.0f) * 0.5f)) * speed_scale * hh * vel_scale;
                    float vz = hash_f(seed, 30u) * speed_scale * hd * 0.8f * vel_scale;
                    float px = ex + vx * explode_t;
                    float py = ey + vy * explode_t + 0.5f * gravity_base * explode_t * explode_t;
                    float pz = ez + vz * explode_t;
                    float hue = fmodf((float)i * 4.0f + time * 20.0f + (float)launch * 50.0f, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({px, py, pz, decay, hue});
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
        if(d2 > d2_cutoff) continue;
        float intensity = expf(-d2 / sigma_sq_use) * p.decay;
        if(intensity < 0.01f) continue;
        float hue_use = fmodf(p.hue + bb.phase_deg + time * GetScaledFrequency() * 6.0f * (bb.speed_mul - 1.0f), 360.0f);
        if(hue_use < 0.0f) hue_use += 360.0f;
        float pal01 = hue_use / 360.0f;
        RGBColor c;
        if(fireworks_strip_cmap_on)
        {
            const float ph01 = std::fmod(color_cycle * (1.f / 360.f) + p.hue * (1.f / 360.f) +
                                             time * GetScaledFrequency() * 0.04f * bb.speed_mul + 1.f,
                                         1.f);
            pal01 = SampleStripKernelPalette01(fireworks_strip_cmap_kernel,
                                                 fireworks_strip_cmap_rep,
                                                 fireworks_strip_cmap_unfold,
                                                 fireworks_strip_cmap_dir,
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
            pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
            c     = ResolveStripKernelFinalColor(*this,
                                                  fireworks_strip_cmap_kernel,
                                                  std::clamp(pal01, 0.0f, 1.0f),
                                                  fireworks_strip_cmap_color_style,
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
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "fireworks_stratum_layout_mode",
                                           sm,
                                           st,
                                           "fireworks_stratum_band_speed_pct",
                                           "fireworks_stratum_band_tight_pct",
                                           "fireworks_stratum_band_phase_deg");
    j["particle_size"] = particle_size;
    j["num_debris"] = num_debris;
    j["firework_type"] = firework_type;
    j["num_simultaneous"] = num_simultaneous;
    j["gravity_strength"] = gravity_strength;
    j["decay_speed"] = decay_speed;
    StripColormapSaveJson(j,
                          "fireworks",
                          fireworks_strip_cmap_on,
                          fireworks_strip_cmap_kernel,
                          fireworks_strip_cmap_rep,
                          fireworks_strip_cmap_unfold,
                          fireworks_strip_cmap_dir,
                          fireworks_strip_cmap_color_style);
    return j;
}

void Fireworks::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "fireworks_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "fireworks_stratum_band_speed_pct",
                                            "fireworks_stratum_band_tight_pct",
                                            "fireworks_stratum_band_phase_deg");
    if(settings.contains("particle_size") && settings["particle_size"].is_number())
        particle_size = std::max(0.02f, std::min(1.0f, settings["particle_size"].get<float>()));
    if(settings.contains("num_debris") && settings["num_debris"].is_number())
        num_debris = std::max(10, std::min(100, settings["num_debris"].get<int>()));
    if(settings.contains("firework_type") && settings["firework_type"].is_number())
        firework_type = std::max(0, std::min(settings["firework_type"].get<int>(), TYPE_COUNT - 1));
    if(settings.contains("num_simultaneous") && settings["num_simultaneous"].is_number())
        num_simultaneous = std::max(1, std::min(5, settings["num_simultaneous"].get<int>()));
    if(settings.contains("gravity_strength") && settings["gravity_strength"].is_number())
        gravity_strength = std::max(0.0f, std::min(2.0f, settings["gravity_strength"].get<float>()));
    if(settings.contains("decay_speed") && settings["decay_speed"].is_number())
        decay_speed = std::max(0.5f, std::min(6.0f, settings["decay_speed"].get<float>()));
    StripColormapLoadJson(settings,
                          "fireworks",
                          fireworks_strip_cmap_on,
                          fireworks_strip_cmap_kernel,
                          fireworks_strip_cmap_rep,
                          fireworks_strip_cmap_unfold,
                          fireworks_strip_cmap_dir,
                          fireworks_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(fireworks_strip_cmap_on,
                                                fireworks_strip_cmap_kernel,
                                                fireworks_strip_cmap_rep,
                                                fireworks_strip_cmap_unfold,
                                                fireworks_strip_cmap_dir,
                                                fireworks_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
