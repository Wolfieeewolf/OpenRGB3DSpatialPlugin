// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGame.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace MinecraftGame
{
float g_damage_flash_decay_s = 0.35f;
float g_world_light_mix = 0.85f;
float g_world_tint_smoothing = 0.72f;
float g_world_tint_directional = 0.46f;
float g_world_tint_dir_sharpness = 1.8f;
float g_lightning_flash_strength = 0.90f;
float g_lightning_flash_decay_s = 0.28f;
float g_damage_flash_strength = 1.0f;
float g_base_brightness = 1.12f;
bool g_enable_health_gradient = true;
bool g_enable_hunger_gradient = true;
bool g_enable_air_gradient = true;
bool g_enable_durability_gradient = true;
float g_hunger_mix = 0.45f;
float g_air_mix = 0.55f;
float g_durability_mix = 0.50f;
bool g_enable_damage_flash = true;
bool g_enable_ambient_world_tint = true;
bool g_enable_lightning_flash = true;
/** Vertical blend: low grid Y → ground, high Y → sky (fractions of grid height). */
float g_tint_layer_ground_end = 0.36f;
float g_tint_layer_sky_start = 0.54f;
/** Extra BiomeEffects-driven tint on top of mod-smoothed layers (0 = ignore vanilla palette fields). */
float g_biome_sky_overlay = 0.28f;
float g_env_rain_darken_sky = 0.45f;
float g_env_thunder_darken_sky = 0.35f;
/** 0 = uniform damage flash; 1 = strongest toward incoming hit in player-local space vs LED offset. */
float g_damage_directional_mix = 0.80f;
float g_damage_dir_sharpness = 1.35f;

bool g_has_smoothed_world_tint = false;
unsigned long long g_last_world_smooth_sample_ms = 0;
RGBColor g_smooth_world_sky = (RGBColor)0x00FFBEAA;
RGBColor g_smooth_world_mid = (RGBColor)0x0078B48C;
RGBColor g_smooth_world_ground = (RGBColor)0x00507864;

unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

RGBColor LerpColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const int ar = (int)(a & 0xFF);
    const int ag = (int)((a >> 8) & 0xFF);
    const int ab = (int)((a >> 16) & 0xFF);
    const int br = (int)(b & 0xFF);
    const int bg = (int)((b >> 8) & 0xFF);
    const int bb = (int)((b >> 16) & 0xFF);
    const int rr = (int)(ar + (br - ar) * t);
    const int rg = (int)(ag + (bg - ag) * t);
    const int rb = (int)(ab + (bb - ab) * t);
    return (RGBColor)((rb << 16) | (rg << 8) | rr);
}

RGBColor MakeRgb(unsigned char r, unsigned char g, unsigned char b)
{
    return (RGBColor)(((int)b << 16) | ((int)g << 8) | (int)r);
}

RGBColor SuppressWhites(RGBColor c)
{
    float r = (float)(c & 0xFF) / 255.0f;
    float g = (float)((c >> 8) & 0xFF) / 255.0f;
    float b = (float)((c >> 16) & 0xFF) / 255.0f;
    float maxc = std::max(r, std::max(g, b));
    float minc = std::min(r, std::min(g, b));
    float chroma = maxc - minc;
    /* Only strong grays near white — leave tinted / saturated colors alone (more pop, less muddy). */
    if(maxc > 0.68f && chroma < 0.14f)
    {
        float k = std::clamp((0.14f - chroma) / 0.14f, 0.0f, 1.0f) * std::clamp((maxc - 0.68f) / 0.32f, 0.0f, 1.0f);
        float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        float target = luma * (0.88f - 0.22f * k);
        r = std::clamp(r + (target - r) * 0.50f * k, 0.0f, 1.0f);
        g = std::clamp(g + (target - g) * 0.50f * k, 0.0f, 1.0f);
        b = std::clamp(b + (target - b) * 0.50f * k, 0.0f, 1.0f);
    }
    const int ri = std::clamp((int)std::round(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::round(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::round(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

QWidget* CreateMinecraftSettingsWidget(QWidget* parent)
{
    QWidget* panel = new QWidget(parent);
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    int row = 0;
    layout->addWidget(new QLabel(QStringLiteral("Health")), row++, 0, 1, 2);
    QCheckBox* health_toggle = new QCheckBox("Enable health gradient");
    health_toggle->setChecked(g_enable_health_gradient);
    layout->addWidget(health_toggle, row++, 0, 1, 2);
    QCheckBox* hunger_toggle = new QCheckBox("Enable hunger gradient");
    hunger_toggle->setChecked(g_enable_hunger_gradient);
    layout->addWidget(hunger_toggle, row++, 0, 1, 2);
    QCheckBox* air_toggle = new QCheckBox("Enable air gradient");
    air_toggle->setChecked(g_enable_air_gradient);
    layout->addWidget(air_toggle, row++, 0, 1, 2);
    QCheckBox* dura_toggle = new QCheckBox("Enable item durability gradient");
    dura_toggle->setChecked(g_enable_durability_gradient);
    layout->addWidget(dura_toggle, row++, 0, 1, 2);

    layout->addWidget(new QLabel(QStringLiteral("Damage")), row++, 0, 1, 2);
    QCheckBox* damage_toggle = new QCheckBox("Enable damage flash");
    damage_toggle->setChecked(g_enable_damage_flash);
    layout->addWidget(damage_toggle, row++, 0, 1, 2);

    QObject::connect(health_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_health_gradient = v; });
    QObject::connect(hunger_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_hunger_gradient = v; });
    QObject::connect(air_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_air_gradient = v; });
    QObject::connect(dura_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_durability_gradient = v; });
    QObject::connect(damage_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_damage_flash = v; });
    auto addPctSlider = [&](const QString& name, float* v) {
        auto* lab = new QLabel(name);
        auto* sl = new QSlider(Qt::Horizontal);
        sl->setRange(0, 100);
        sl->setValue((int)std::lround(std::clamp(*v, 0.0f, 1.0f) * 100.0f));
        layout->addWidget(lab, row, 0);
        layout->addWidget(sl, row, 1);
        QObject::connect(sl, &QSlider::valueChanged, panel, [v](int x) { *v = std::clamp(x / 100.0f, 0.0f, 1.0f); });
        row++;
    };

    addPctSlider(QStringLiteral("Directional hit (vs uniform)"), &g_damage_directional_mix);
    addPctSlider(QStringLiteral("Hunger gradient strength"), &g_hunger_mix);
    addPctSlider(QStringLiteral("Air gradient strength"), &g_air_mix);
    addPctSlider(QStringLiteral("Item durability gradient strength"), &g_durability_mix);

    auto* dSharpLab = new QLabel(QStringLiteral("Damage direction sharpness"));
    auto* dSharpSl = new QSlider(Qt::Horizontal);
    dSharpSl->setRange(50, 400);
    dSharpSl->setValue((int)std::lround(g_damage_dir_sharpness * 100.0f));
    layout->addWidget(dSharpLab, row, 0);
    layout->addWidget(dSharpSl, row, 1);
    QObject::connect(dSharpSl, &QSlider::valueChanged, panel, [](int x) { g_damage_dir_sharpness = std::clamp(x / 100.0f, 0.5f, 5.0f); });
    row++;

    layout->addWidget(new QLabel(QStringLiteral("World tint")), row++, 0, 1, 2);
    QCheckBox* world_tint_toggle = new QCheckBox("Enable ambient world tint");
    world_tint_toggle->setChecked(g_enable_ambient_world_tint);
    layout->addWidget(world_tint_toggle, row++, 0, 1, 2);
    QObject::connect(world_tint_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_ambient_world_tint = v; });

    QCheckBox* lightning_toggle = new QCheckBox("Enable lightning flash");
    lightning_toggle->setChecked(g_enable_lightning_flash);
    layout->addWidget(lightning_toggle, row++, 0, 1, 2);
    QObject::connect(lightning_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_lightning_flash = v; });

    auto* mixLab = new QLabel(QStringLiteral("World tint strength"));
    auto* mixSl = new QSlider(Qt::Horizontal);
    mixSl->setRange(0, 100);
    mixSl->setValue((int)std::lround(std::clamp(g_world_light_mix, 0.0f, 1.0f) * 100.0f));
    layout->addWidget(mixLab, row, 0);
    layout->addWidget(mixSl, row, 1);
    QObject::connect(mixSl, &QSlider::valueChanged, panel, [](int x) { g_world_light_mix = std::clamp(x / 100.0f, 0.0f, 1.0f); });
    row++;

    auto* smoothLab = new QLabel(QStringLiteral("World tint smoothing"));
    auto* smoothSl = new QSlider(Qt::Horizontal);
    smoothSl->setRange(0, 95);
    smoothSl->setValue((int)std::lround(std::clamp(g_world_tint_smoothing, 0.0f, 0.95f) * 100.0f));
    layout->addWidget(smoothLab, row, 0);
    layout->addWidget(smoothSl, row, 1);
    QObject::connect(smoothSl, &QSlider::valueChanged, panel, [](int x) { g_world_tint_smoothing = std::clamp(x / 100.0f, 0.0f, 0.95f); });
    row++;

    auto* dirLab = new QLabel(QStringLiteral("World tint directional response"));
    auto* dirSl = new QSlider(Qt::Horizontal);
    dirSl->setRange(0, 100);
    dirSl->setValue((int)std::lround(std::clamp(g_world_tint_directional, 0.0f, 1.0f) * 100.0f));
    layout->addWidget(dirLab, row, 0);
    layout->addWidget(dirSl, row, 1);
    QObject::connect(dirSl, &QSlider::valueChanged, panel, [](int x) { g_world_tint_directional = std::clamp(x / 100.0f, 0.0f, 1.0f); });
    row++;

    auto* dirSharpLab = new QLabel(QStringLiteral("World tint directional sharpness"));
    auto* dirSharpSl = new QSlider(Qt::Horizontal);
    dirSharpSl->setRange(80, 320);
    dirSharpSl->setValue((int)std::lround(std::clamp(g_world_tint_dir_sharpness, 0.8f, 3.2f) * 100.0f));
    layout->addWidget(dirSharpLab, row, 0);
    layout->addWidget(dirSharpSl, row, 1);
    QObject::connect(dirSharpSl, &QSlider::valueChanged, panel, [](int x) { g_world_tint_dir_sharpness = std::clamp(x / 100.0f, 0.8f, 3.2f); });
    row++;

    auto* lStrLab = new QLabel(QStringLiteral("Lightning flash strength"));
    auto* lStrSl = new QSlider(Qt::Horizontal);
    lStrSl->setRange(0, 150);
    lStrSl->setValue((int)std::lround(std::clamp(g_lightning_flash_strength, 0.0f, 1.5f) * 100.0f));
    layout->addWidget(lStrLab, row, 0);
    layout->addWidget(lStrSl, row, 1);
    QObject::connect(lStrSl, &QSlider::valueChanged, panel, [](int x) { g_lightning_flash_strength = std::clamp(x / 100.0f, 0.0f, 1.5f); });
    row++;

    auto* lDecLab = new QLabel(QStringLiteral("Lightning decay (ms)"));
    auto* lDecSl = new QSlider(Qt::Horizontal);
    lDecSl->setRange(80, 900);
    lDecSl->setValue((int)std::lround(std::clamp(g_lightning_flash_decay_s, 0.08f, 0.90f) * 1000.0f));
    layout->addWidget(lDecLab, row, 0);
    layout->addWidget(lDecSl, row, 1);
    QObject::connect(lDecSl, &QSlider::valueChanged, panel, [](int x) { g_lightning_flash_decay_s = std::clamp(x / 1000.0f, 0.08f, 0.90f); });
    row++;

    auto* gEndLab = new QLabel(QStringLiteral("Ground-to-mid blend ends (grid Y %)"));
    auto* gEndSl = new QSlider(Qt::Horizontal);
    gEndSl->setRange(10, 55);
    gEndSl->setValue((int)std::lround(std::clamp(g_tint_layer_ground_end, 0.10f, 0.55f) * 100.0f));
    layout->addWidget(gEndLab, row, 0);
    layout->addWidget(gEndSl, row, 1);
    QObject::connect(gEndSl, &QSlider::valueChanged, panel, [](int x) { g_tint_layer_ground_end = std::clamp(x / 100.0f, 0.08f, 0.55f); });
    row++;

    auto* sStartLab = new QLabel(QStringLiteral("Mid-to-sky blend starts (grid Y %)"));
    auto* sStartSl = new QSlider(Qt::Horizontal);
    sStartSl->setRange(40, 85);
    sStartSl->setValue((int)std::lround(std::clamp(g_tint_layer_sky_start, 0.40f, 0.85f) * 100.0f));
    layout->addWidget(sStartLab, row, 0);
    layout->addWidget(sStartSl, row, 1);
    QObject::connect(sStartSl, &QSlider::valueChanged, panel, [](int x) { g_tint_layer_sky_start = std::clamp(x / 100.0f, 0.40f, 0.92f); });
    row++;

    addPctSlider(QStringLiteral("Biome sky overlay (BiomeEffects sky)"), &g_biome_sky_overlay);
    addPctSlider(QStringLiteral("Rain darkens sky layer"), &g_env_rain_darken_sky);
    addPctSlider(QStringLiteral("Thunder darkens sky layer"), &g_env_thunder_darken_sky);

    return panel;
}

RGBColor RenderMinecraftColor(const GameTelemetryBridge::TelemetrySnapshot& t,
                              float,
                              float grid_x,
                              float grid_y,
                              float grid_z,
                              float origin_x,
                              float origin_y,
                              float origin_z,
                              const GridContext3D& grid)
{
    const RGBColor low_health = (RGBColor)0x000022FF;
    const RGBColor high_health = (RGBColor)0x0000FF22;
    const RGBColor low_hunger = (RGBColor)0x000020FF;
    const RGBColor high_hunger = (RGBColor)0x0000E0FF;
    const RGBColor low_air = (RGBColor)0x0000A0FF;
    const RGBColor high_air = (RGBColor)0x00FFC040;
    const RGBColor low_durability = (RGBColor)0x000000FF;
    const RGBColor high_durability = (RGBColor)0x0000FF60;
    RGBColor out = (RGBColor)0x00000000;
    if(g_enable_health_gradient && t.has_health_state && t.health_max > 0.01f)
    {
        const float health_norm = std::clamp(t.health / t.health_max, 0.0f, 1.0f);
        out = LerpColor(low_health, high_health, health_norm);
    }
    if(g_enable_hunger_gradient && t.has_health_state && t.hunger_max > 0.01f)
    {
        const float hunger_norm = std::clamp(t.hunger / t.hunger_max, 0.0f, 1.0f);
        const RGBColor hunger_color = LerpColor(low_hunger, high_hunger, hunger_norm);
        out = LerpColor(out, hunger_color, std::clamp(g_hunger_mix, 0.0f, 1.0f));
    }
    if(g_enable_air_gradient && t.has_health_state && t.air_max > 0.01f)
    {
        const float air_norm = std::clamp(t.air / t.air_max, 0.0f, 1.0f);
        const RGBColor air_color = LerpColor(low_air, high_air, air_norm);
        out = LerpColor(out, air_color, std::clamp(g_air_mix, 0.0f, 1.0f));
    }
    if(g_enable_durability_gradient && t.has_health_state && t.has_item_durability && t.item_durability_max > 0.01f)
    {
        const float dura_norm = std::clamp(t.item_durability / t.item_durability_max, 0.0f, 1.0f);
        const RGBColor dura_color = LerpColor(low_durability, high_durability, dura_norm);
        out = LerpColor(out, dura_color, std::clamp(g_durability_mix, 0.0f, 1.0f));
    }

    if(g_enable_ambient_world_tint && t.has_world_light)
    {
        RGBColor wl = SuppressWhites(MakeRgb(t.world_light_r, t.world_light_g, t.world_light_b));
        if(t.has_world_layers)
        {
            const float height = (grid.height > 1e-3f) ? grid.height : 1.0f;
            const float y_norm = std::clamp((grid_y - grid.min_y) / height, 0.0f, 1.0f);
            RGBColor sky = SuppressWhites(MakeRgb(t.world_sky_r, t.world_sky_g, t.world_sky_b));
            RGBColor mid = SuppressWhites(MakeRgb(t.world_mid_r, t.world_mid_g, t.world_mid_b));
            RGBColor ground = SuppressWhites(MakeRgb(t.world_ground_r, t.world_ground_g, t.world_ground_b));
            if(t.has_vanilla_biome_colors)
            {
                if(g_biome_sky_overlay > 1e-4f)
                {
                    RGBColor bioSky = SuppressWhites(MakeRgb(t.biome_sky_r, t.biome_sky_g, t.biome_sky_b));
                    sky = LerpColor(sky, bioSky, std::clamp(g_biome_sky_overlay, 0.0f, 1.0f));
                }
            }
            const float waterK = std::clamp(t.water_submerge, 0.0f, 1.0f);
            if(waterK > 1e-4f)
            {
                RGBColor wFog = SuppressWhites(MakeRgb(t.water_fog_r, t.water_fog_g, t.water_fog_b));
                const float gk = std::clamp(0.30f + 0.70f * waterK, 0.0f, 1.0f);
                const float mk = std::clamp(0.20f + 0.68f * waterK, 0.0f, 1.0f);
                const float sk = std::clamp(0.10f + 0.52f * waterK, 0.0f, 1.0f);
                ground = LerpColor(ground, wFog, gk);
                mid = LerpColor(mid, wFog, mk);
                sky = LerpColor(sky, wFog, sk);
            }
            const float rainK = std::clamp(t.env_rain, 0.0f, 1.0f);
            const float thK = std::clamp(t.env_thunder, 0.0f, 1.0f);
            const float weatherK = std::clamp(rainK * g_env_rain_darken_sky + thK * g_env_thunder_darken_sky, 0.0f, 1.0f);
            if(weatherK > 1e-4f)
            {
                sky = LerpColor(sky, (RGBColor)0x00002218, weatherK);
            }
            if(t.world_light_received_ms != 0 && t.world_light_received_ms != g_last_world_smooth_sample_ms)
            {
                g_last_world_smooth_sample_ms = t.world_light_received_ms;
                if(!g_has_smoothed_world_tint)
                {
                    g_smooth_world_sky = sky;
                    g_smooth_world_mid = mid;
                    g_smooth_world_ground = ground;
                    g_has_smoothed_world_tint = true;
                }
                else
                {
                    const float alpha = std::clamp(1.0f - g_world_tint_smoothing, 0.02f, 1.0f);
                    g_smooth_world_sky = LerpColor(g_smooth_world_sky, sky, alpha);
                    g_smooth_world_mid = LerpColor(g_smooth_world_mid, mid, alpha);
                    g_smooth_world_ground = LerpColor(g_smooth_world_ground, ground, alpha);
                }
            }
            if(g_has_smoothed_world_tint)
            {
                sky = g_smooth_world_sky;
                mid = g_smooth_world_mid;
                ground = g_smooth_world_ground;
            }
            RGBColor projected = mid;
            const float gEnd = std::clamp(g_tint_layer_ground_end, 0.08f, 0.49f);
            const float sStart = std::clamp(g_tint_layer_sky_start, gEnd + 0.04f, 0.92f);
            /* World tint: low grid Y = ground, high Y = sky (ceiling / floor rigs). */
            if(y_norm < gEnd)
            {
                projected = LerpColor(ground, mid, y_norm / gEnd);
            }
            else if(y_norm > sStart)
            {
                const float denom = std::max(1e-3f, 1.0f - sStart);
                projected = LerpColor(mid, sky, (y_norm - sStart) / denom);
            }
            /* Keep room-wide tint present even in darker scenes so sky/mid/ground layering remains visible. */
            const float wi_disp = std::max(t.world_light_intensity, 0.22f);
            const float layer_mix = std::clamp((0.42f + 0.58f * g_world_light_mix) * wi_disp, 0.0f, 1.0f);
            float dirMul = 1.0f;
            if(t.has_player_pose && t.world_light_focus > 1e-4f && g_world_tint_directional > 1e-4f)
            {
                float fx = t.forward_x, fy = t.forward_y, fz = t.forward_z;
                float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
                if(fl > 1e-5f)
                {
                    fx /= fl; fy /= fl; fz /= fl;
                }
                else
                {
                    fx = 0.0f; fy = 0.0f; fz = 1.0f;
                }
                float ux = t.up_x, uy = t.up_y, uz = t.up_z;
                float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
                if(ul > 1e-5f)
                {
                    ux /= ul; uy /= ul; uz /= ul;
                }
                else
                {
                    ux = 0.0f; uy = 1.0f; uz = 0.0f;
                }
                float rx = fy * uz - fz * uy;
                float ry = fz * ux - fx * uz;
                float rz = fx * uy - fy * ux;
                float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
                if(rl > 1e-5f)
                {
                    rx /= rl; ry /= rl; rz /= rl;
                }
                float ldx = t.world_light_dir_x, ldy = t.world_light_dir_y, ldz = t.world_light_dir_z;
                float ll = std::sqrt(ldx * ldx + ldy * ldy + ldz * ldz);
                if(ll > 1e-5f)
                {
                    ldx /= ll; ldy /= ll; ldz /= ll;
                    const float lx = ldx * rx + ldy * ry + ldz * rz;
                    const float ly = ldx * ux + ldy * uy + ldz * uz;
                    const float lz = ldx * fx + ldy * fy + ldz * fz;
                    float ox = grid_x - origin_x, oy = grid_y - origin_y, oz = grid_z - origin_z;
                    float ol = std::sqrt(ox * ox + oy * oy + oz * oz);
                    if(ol > 1e-5f)
                    {
                        ox /= ol; oy /= ol; oz /= ol;
                        const float sa = std::clamp(ox * lx + oy * ly + oz * lz, -1.0f, 1.0f);
                        const float hemi = 0.5f * (sa + 1.0f);
                        const float shaped = std::pow(std::clamp(hemi, 0.0f, 1.0f), std::max(0.8f, g_world_tint_dir_sharpness));
                        const float focus = std::clamp(t.world_light_focus, 0.0f, 1.0f);
                        const float dm = std::clamp(g_world_tint_directional * (0.25f + 0.75f * focus), 0.0f, 1.0f);
                        const float floor = 0.30f;
                        dirMul = (1.0f - dm) + dm * (floor + (1.0f - floor) * shaped);
                    }
                }
            }
            out = LerpColor(out, projected, std::clamp(layer_mix * dirMul, 0.0f, 1.0f));
            wl = LerpColor(wl, projected, 0.6f);
        }
        const float wi_disp2 = std::max(t.world_light_intensity, 0.22f);
        const float wl_mix = std::clamp(g_world_light_mix * wi_disp2, 0.0f, 1.0f);
        out = LerpColor(out, wl, wl_mix * 0.72f);
    }

    if(g_enable_damage_flash && t.has_damage_event && t.damage_received_ms > 0)
    {
        const unsigned long long now = NowMs();
        const unsigned long long elapsed_ms = (now > t.damage_received_ms) ? (now - t.damage_received_ms) : 0;
        const float decay_ms = std::max(100.0f, g_damage_flash_decay_s * 1000.0f);
        const float damage_t = std::clamp(1.0f - (elapsed_ms / decay_ms), 0.0f, 1.0f);
        if(damage_t > 0.0f)
        {
            const float damage_strength = std::clamp(t.damage_amount / 20.0f, 0.0f, 1.0f);
            float flash_mix = std::clamp(g_damage_flash_strength * damage_t * (0.2f + 0.8f * damage_strength), 0.0f, 1.0f);
            if(g_damage_directional_mix > 1e-4f && t.has_player_pose)
            {
                float dwx = t.damage_dir_x;
                float dwy = t.damage_dir_y;
                float dwz = t.damage_dir_z;
                const float dl = std::sqrt(dwx * dwx + dwy * dwy + dwz * dwz);
                if(dl > 1e-4f)
                {
                    dwx /= dl;
                    dwy /= dl;
                    dwz /= dl;
                    float fx = t.forward_x;
                    float fy = t.forward_y;
                    float fz = t.forward_z;
                    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
                    if(fl > 1e-4f)
                    {
                        fx /= fl;
                        fy /= fl;
                        fz /= fl;
                    }
                    else
                    {
                        fx = 0.0f;
                        fy = 0.0f;
                        fz = 1.0f;
                    }
                    float ux = t.up_x;
                    float uy = t.up_y;
                    float uz = t.up_z;
                    float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
                    if(ul > 1e-4f)
                    {
                        ux /= ul;
                        uy /= ul;
                        uz /= ul;
                    }
                    else
                    {
                        ux = 0.0f;
                        uy = 1.0f;
                        uz = 0.0f;
                    }
                    float rx = fy * uz - fz * uy;
                    float ry = fz * ux - fx * uz;
                    float rz = fx * uy - fy * ux;
                    const float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
                    if(rl > 1e-4f)
                    {
                        rx /= rl;
                        ry /= rl;
                        rz /= rl;
                    }
                    const float lx = dwx * rx + dwy * ry + dwz * rz;
                    const float ly = dwx * ux + dwy * uy + dwz * uz;
                    const float lz = dwx * fx + dwy * fy + dwz * fz;
                    float ox = grid_x - origin_x;
                    float oy = grid_y - origin_y;
                    float oz = grid_z - origin_z;
                    const float olen = std::sqrt(ox * ox + oy * oy + oz * oz);
                    if(olen > 1e-4f)
                    {
                        ox /= olen;
                        oy /= olen;
                        oz /= olen;
                    }
                    const float signed_align = std::clamp(ox * lx + oy * ly + oz * lz, -1.0f, 1.0f);
                    const float hemi = 0.5f * (signed_align + 1.0f);
                    const float shaped = std::pow(std::clamp(hemi, 0.0f, 1.0f), std::max(0.5f, g_damage_dir_sharpness));
                    const float dirMix = std::clamp(g_damage_directional_mix, 0.0f, 1.0f);
                    const float minFactor = 0.10f;
                    const float dirFactor = minFactor + (1.0f - minFactor) * shaped;
                    flash_mix *= (1.0f - dirMix) + dirMix * dirFactor;
                    flash_mix = std::clamp(flash_mix, 0.0f, 1.0f);
                }
            }
            out = LerpColor(out, (RGBColor)0x000000FF, flash_mix);
        }
    }

    if(g_enable_lightning_flash && t.has_lightning_event && t.lightning_received_ms > 0)
    {
        const unsigned long long now = NowMs();
        const unsigned long long elapsed_ms = (now > t.lightning_received_ms) ? (now - t.lightning_received_ms) : 0;
        const float decay_ms = std::max(80.0f, g_lightning_flash_decay_s * 1000.0f);
        const float lt = std::clamp(1.0f - (elapsed_ms / decay_ms), 0.0f, 1.0f);
        if(lt > 0.0f)
        {
            const float height = (grid.height > 1e-3f) ? grid.height : 1.0f;
            const float y_norm = std::clamp((grid_y - grid.min_y) / height, 0.0f, 1.0f);
            const float skyBias = std::pow(y_norm, 1.9f);
            const float layer = 0.20f + 0.80f * skyBias;
            const float st = std::clamp(g_lightning_flash_strength * t.lightning_strength * lt * layer, 0.0f, 1.0f);
            out = LerpColor(out, (RGBColor)0x00FFF0DE, st);
        }
    }

    const int r = std::clamp((int)((out & 0xFF) * g_base_brightness), 0, 255);
    const int g = std::clamp((int)(((out >> 8) & 0xFF) * g_base_brightness), 0, 255);
    const int b = std::clamp((int)(((out >> 16) & 0xFF) * g_base_brightness), 0, 255);
    out = (RGBColor)((b << 16) | (g << 8) | r);

    return out;
}

QWidget* CreateSettingsWidget(QWidget* parent)
{
    return CreateMinecraftSettingsWidget(parent);
}

RGBColor RenderColor(const GameTelemetryBridge::TelemetrySnapshot& t,
                     float time,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid)
{
    return RenderMinecraftColor(t, time, grid_x, grid_y, grid_z, origin_x, origin_y, origin_z, grid);
}
}
