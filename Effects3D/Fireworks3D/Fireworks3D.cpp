// SPDX-License-Identifier: GPL-2.0-only

#include "Fireworks3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(Fireworks3D);

const char* Fireworks3D::TypeName(int t)
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

Fireworks3D::Fireworks3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D Fireworks3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Fireworks";
    info.effect_description = "Missile launches and explodes into debris (Mega-Cube style); gravity and decay";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
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

void Fireworks3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Type:"), row, 0);
    QComboBox* type_combo = new QComboBox();
    for(int t = 0; t < TYPE_COUNT; t++) type_combo->addItem(TypeName(t));
    type_combo->setCurrentIndex(std::max(0, std::min(firework_type, TYPE_COUNT - 1)));
    layout->addWidget(type_combo, row, 1, 1, 2);
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        firework_type = std::max(0, std::min(idx, TYPE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Simultaneous:"), row, 0);
    QSlider* sim_slider = new QSlider(Qt::Horizontal);
    sim_slider->setRange(1, 5);
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
    AddWidgetToParent(w, parent);
}

void Fireworks3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Fireworks3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor Fireworks3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float half = 0.5f * std::max(grid.width, std::max(grid.height, grid.depth)) * GetNormalizedScale();
    if(half < 1e-5f) half = 1.0f;
    float speed_scale = GetScaledSpeed() * 0.015f;
    // Cap sigma so huge particle size doesn't make every LED sample every particle (avoids slowdown)
    float sigma = std::max(particle_size * half, 5.0f);
    const float sigma_cap = half * 0.4f;
    if(sigma > sigma_cap) sigma = sigma_cap;
    float sigma_sq = sigma * sigma;
    const float d2_cutoff = 9.0f * sigma_sq;
    float grav_mult = std::max(0.0f, std::min(2.0f, gravity_strength));
    float decay_mult = std::max(0.5f, std::min(6.0f, decay_speed));
    int n_sim = std::max(1, std::min(5, num_simultaneous));
    int type = std::max(0, std::min(firework_type, TYPE_COUNT - 1));

    // Per-frame particle cache for FPS (compute positions once per frame)
    if(particle_cache.empty() || fabsf(time - particle_cache_time) > 0.001f)
    {
        particle_cache.clear();
        particle_cache_time = time;
        float cycle = CYCLE_DURATION;
        if(type == TYPE_ROMAN_CANDLE) cycle = 4.0f;
        if(type == TYPE_FOUNTAIN) cycle = 3.0f;
        const float gravity_base = -0.95f * speed_scale * half * grav_mult;
        const float decay_coeff = 6.0f * decay_mult;  // Strong decay so burst clears before next

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
                    float vx = hash_f((unsigned int)(launch * 1000 + i), 10u) * speed_scale * half * 0.4f;
                    float vy = (0.5f + 0.4f * (hash_f((unsigned int)(launch * 1000 + i), 20u) + 1.0f) * 0.5f) * speed_scale * half;
                    float vz = hash_f((unsigned int)(launch * 1000 + i), 30u) * speed_scale * half * 0.4f;
                    float px = origin.x + vx * t;
                    float py = origin.y - half * 0.5f + vy * t + 0.5f * gravity * t * t;
                    float pz = origin.z + vz * t;
                    float decay = 1.0f / (1.0f + t * decay_coeff * 0.4f);
                    float hue = fmodf((float)i * 3.0f + time * 15.0f, 360.0f);
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
                    float by = origin.y - half * 0.6f + (pop_time / rise) * (half * 1.0f);
                    float bx = origin.x; float bz = origin.z;
                    int n_pt = std::max(8, num_debris / 4);
                    for(int i = 0; i < n_pt; i++)
                    {
                        unsigned int seed = (unsigned int)(launch * 500 + p * 100 + i);
                        float vx = hash_f(seed, 10u) * speed_scale * half * 0.6f;
                        float vy = (0.2f + 0.4f * (hash_f(seed, 20u) + 1.0f) * 0.5f) * speed_scale * half;
                        float vz = hash_f(seed, 30u) * speed_scale * half * 0.6f;
                        float px = bx + vx * burst_t;
                        float py = by + vy * burst_t + 0.5f * gravity_base * burst_t * burst_t;
                        float pz = bz + vz * burst_t;
                        float hue = fmodf((float)(p * n_pt + i) + time * 25.0f, 360.0f);
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
                    float mx = origin.x + 0.3f * half * cosf(time * 8.0f + (float)launch);
                    float my = origin.y - half * 0.8f + t * (half * 1.1f);
                    float mz = origin.z + 0.3f * half * sinf(time * 8.0f + (float)launch);
                    float hue = fmodf(time * 60.0f, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({mx, my, mz, 1.0f, hue});
                    int trail = 12;
                    for(int i = 0; i < trail; i++)
                    {
                        float ti = (float)i / (float)trail * t;
                        float tx = origin.x + 0.35f * half * cosf(time * 8.0f + (float)launch + ti * 6.0f);
                        float ty = origin.y - half * 0.8f + ti * (half * 1.1f);
                        float tz = origin.z + 0.35f * half * sinf(time * 8.0f + (float)launch + ti * 6.0f);
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
                    float ex = origin.x, ey = origin.y + half * 0.3f, ez = origin.z;
                    int n_pt = std::max(10, num_debris / 2);
                    for(int i = 0; i < n_pt; i++)
                    {
                        float vx = hash_f((unsigned int)(launch * 200 + i), 10u) * speed_scale * half * 0.5f;
                        float vy = (0.2f + 0.3f * (hash_f((unsigned int)(launch * 200 + i), 20u) + 1.0f) * 0.5f) * speed_scale * half;
                        float vz = hash_f((unsigned int)(launch * 200 + i), 30u) * speed_scale * half * 0.5f;
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
                float my = origin.y - half * 0.8f + t * (half * 1.2f);
                float mz = origin.z;
                float hue = fmodf(time * 50.0f + (float)launch * 70.0f, 360.0f);
                if(hue < 0.0f) hue += 360.0f;
                particle_cache.push_back({mx, my, mz, 1.0f, hue});
            }
            else
            {
                float explode_t = phase - missile_dur;
                float decay = 1.0f / (1.0f + explode_t * decay_coeff);
                float ex = origin.x, ey = origin.y + half * 0.4f, ez = origin.z;
                int n_debris_use = std::max(10, std::min(100, (use_type == TYPE_BIG_EXPLOSION) ? (num_debris * 3 / 2) : num_debris));
                float vel_scale = (use_type == TYPE_BIG_EXPLOSION) ? 1.4f : 1.0f;

                for(int i = 0; i < n_debris_use; i++)
                {
                    unsigned int seed = (unsigned int)(launch * 1000 + i);
                    float vx = hash_f(seed, 10u) * speed_scale * half * 0.8f * vel_scale;
                    float vy = (0.3f + 0.5f * ((hash_f(seed, 20u) + 1.0f) * 0.5f)) * speed_scale * half * vel_scale;
                    float vz = hash_f(seed, 30u) * speed_scale * half * 0.8f * vel_scale;
                    float px = ex + vx * explode_t;
                    float py = ey + vy * explode_t + 0.5f * gravity_base * explode_t * explode_t;
                    float pz = ez + vz * explode_t;
                    float hue = fmodf((float)i * 4.0f + time * 20.0f + (float)launch * 50.0f, 360.0f);
                    if(hue < 0.0f) hue += 360.0f;
                    particle_cache.push_back({px, py, pz, decay, hue});
                }
            }
        }
    }

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    for(const CachedParticle& p : particle_cache)
    {
        float dx = x - p.px, dy = y - p.py, dz = z - p.pz;
        float d2 = dx*dx + dy*dy + dz*dz;
        if(d2 > d2_cutoff) continue;
        float intensity = expf(-d2 / sigma_sq) * p.decay;
        if(intensity < 0.01f) continue;
        RGBColor c = GetRainbowMode() ? GetRainbowColor(p.hue) : GetColorAtPosition(p.hue / 360.0f);
        sum_r += ((c & 0xFF) / 255.0f) * intensity;
        sum_g += (((c >> 8) & 0xFF) / 255.0f) * intensity;
        sum_b += (((c >> 16) & 0xFF) / 255.0f) * intensity;
    }
    // Clamp sums to 1 so additive blend keeps burst colors (rainbow/picker) instead of blowing to white
    sum_r = std::min(1.0f, sum_r);
    sum_g = std::min(1.0f, sum_g);
    sum_b = std::min(1.0f, sum_b);
    int r_ = std::min(255, std::max(0, (int)(sum_r * 255.0f)));
    int g_ = std::min(255, std::max(0, (int)(sum_g * 255.0f)));
    int b_ = std::min(255, std::max(0, (int)(sum_b * 255.0f)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json Fireworks3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["particle_size"] = particle_size;
    j["num_debris"] = num_debris;
    j["firework_type"] = firework_type;
    j["num_simultaneous"] = num_simultaneous;
    j["gravity_strength"] = gravity_strength;
    j["decay_speed"] = decay_speed;
    return j;
}

void Fireworks3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
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
}
