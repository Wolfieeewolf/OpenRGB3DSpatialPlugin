// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGame.h"
#include "MinecraftGameSettings.h"
#include "SpatialEffect3D.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace MinecraftGame
{

namespace
{
thread_local int tls_led_index = -1;
thread_local int tls_led_count = 0;
}

void SetRenderSampleIndexContext(int led_index, int led_count)
{
    tls_led_index = led_index;
    tls_led_count = led_count;
}

void ClearRenderSampleIndexContext()
{
    tls_led_index = -1;
    tls_led_count = 0;
}

void WireChildWidgetsToParametersChanged(QWidget* root, const std::function<void()>& on_changed)
{
    if(!root || !on_changed)
    {
        return;
    }
    for(QCheckBox* cb : root->findChildren<QCheckBox*>())
    {
        QObject::connect(cb, &QCheckBox::toggled, root, [on_changed](bool) { on_changed(); });
    }
    for(QSlider* sl : root->findChildren<QSlider*>())
    {
        QObject::connect(sl, &QSlider::valueChanged, root, [on_changed](int) { on_changed(); });
    }
    for(QSpinBox* sp : root->findChildren<QSpinBox*>())
    {
        QObject::connect(sp, QOverload<int>::of(&QSpinBox::valueChanged), root, [on_changed](int) { on_changed(); });
    }
    for(QComboBox* combo : root->findChildren<QComboBox*>())
    {
        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), root, [on_changed](int) { on_changed(); });
    }
}

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

static bool ch(std::uint32_t mask, std::uint32_t bit) { return (mask & bit) != 0u; }

static int ResolveHealthStripAxis(const GridContext3D& grid, int axis_in)
{
    if(axis_in >= 1 && axis_in <= 3)
    {
        return axis_in;
    }
    if(grid.width >= grid.height && grid.width >= grid.depth)
    {
        return 1;
    }
    if(grid.height >= grid.depth)
    {
        return 2;
    }
    return 3;
}

static bool HealthStripMappingUsable(const GridContext3D& grid, int axis_in)
{
    const int axis = ResolveHealthStripAxis(grid, axis_in);
    const float span = (axis == 1) ? grid.width : (axis == 2 ? grid.height : grid.depth);
    return span > 1e-4f;
}

static float HealthStripBrightnessAlongSlots(float fill_end, float total_slots, float u01)
{
    if(total_slots < 1e-4f)
    {
        return 0.0f;
    }
    const float u = std::clamp(u01, 0.0f, 1.0f);
    float center;
    if(total_slots <= 1.0f)
    {
        center = 0.5f * total_slots;
    }
    else
    {
        center = 0.5f + u * (total_slots - 1.0f);
    }
    const float lo = center - 0.5f;
    const float hi = center + 0.5f;
    const float overlap = std::max(0.0f, std::min(hi, fill_end) - std::max(lo, 0.0f));
    const float width = hi - lo;
    if(width < 1e-6f)
    {
        return 0.0f;
    }
    return std::clamp(overlap / width, 0.0f, 1.0f);
}

static float HealthStripBrightnessIndexedLed(float fill_end, float total_slots, int led_index, int led_count, bool invert)
{
    if(led_count <= 0 || total_slots < 1e-4f)
    {
        return 0.0f;
    }
    const float n = (float)led_count;
    const float hw_lo = (float)led_index / n;
    const float hw_hi = (float)(led_index + 1) / n;
    float su_lo = invert ? (1.0f - hw_hi) : hw_lo;
    float su_hi = invert ? (1.0f - hw_lo) : hw_hi;
    if(su_lo > su_hi)
    {
        std::swap(su_lo, su_hi);
    }
    float lo = su_lo * total_slots;
    float hi = su_hi * total_slots;
    if(lo > hi)
    {
        std::swap(lo, hi);
    }
    const float overlap = std::min(hi, fill_end) - lo;
    const float width = hi - lo;
    if(width < 1e-6f)
    {
        return 0.0f;
    }
    return std::clamp(overlap / width, 0.0f, 1.0f);
}

static float HealthStripCoord01(float gx, float gy, float gz, const GridContext3D& grid, int axis_in, bool invert)
{
    const int axis = ResolveHealthStripAxis(grid, axis_in);
    float span = 1.0f;
    float pos = 0.0f;
    if(axis == 1)
    {
        span = grid.width;
        pos = gx - grid.min_x;
    }
    else if(axis == 2)
    {
        span = grid.height;
        pos = gy - grid.min_y;
    }
    else
    {
        span = grid.depth;
        pos = gz - grid.min_z;
    }
    if(span < 1e-6f)
    {
        return 0.5f;
    }
    float u = std::clamp(pos / span, 0.0f, 1.0f);
    if(invert)
    {
        u = 1.0f - u;
    }
    return u;
}

QWidget* CreateSettingsWidget(QWidget* parent, Settings& s, std::uint32_t channels)
{
    QWidget* panel = new QWidget(parent);
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    int row = 0;
    const bool all = (channels == ChAll);

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

    if(all || ch(channels, ChHealth) || ch(channels, ChHunger) || ch(channels, ChAir) || ch(channels, ChDurability))
    {
        layout->addWidget(new QLabel(QStringLiteral("Vitals")), row++, 0, 1, 2);
    }
    if(all || ch(channels, ChHealth))
    {
        QCheckBox* health_toggle = new QCheckBox("Enable health gradient");
        health_toggle->setChecked(s.enable_health_gradient);
        layout->addWidget(health_toggle, row++, 0, 1, 2);
        QObject::connect(health_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_health_gradient = v; });

        QCheckBox* strip_toggle = new QCheckBox("Per-heart strip (each heart uses LEDs along layout axis)");
        strip_toggle->setChecked(s.health_per_heart_strip);
        layout->addWidget(strip_toggle, row++, 0, 1, 2);
        QObject::connect(strip_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.health_per_heart_strip = v; });

        QCheckBox* index_toggle = new QCheckBox("Index strip mode (works on any controller; uses LED order)");
        index_toggle->setChecked(s.health_per_heart_indexed);
        layout->addWidget(index_toggle, row++, 0, 1, 2);
        QObject::connect(index_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.health_per_heart_indexed = v; });

        auto* lph_lab = new QLabel(QStringLiteral("LEDs per heart"));
        auto* lph_spin = new QSpinBox();
        lph_spin->setRange(1, 32);
        lph_spin->setValue(std::clamp(s.health_leds_per_heart, 1, 32));
        layout->addWidget(lph_lab, row, 0);
        layout->addWidget(lph_spin, row, 1);
        QObject::connect(lph_spin, QOverload<int>::of(&QSpinBox::valueChanged), panel, [&s](int v) { s.health_leds_per_heart = std::clamp(v, 1, 32); });
        row++;

        auto* axis_lab = new QLabel(QStringLiteral("Heart strip axis"));
        auto* axis_combo = new QComboBox();
        axis_combo->addItem(QStringLiteral("Auto (longest span)"));
        axis_combo->addItem(QStringLiteral("Along X"));
        axis_combo->addItem(QStringLiteral("Along Y"));
        axis_combo->addItem(QStringLiteral("Along Z"));
        axis_combo->setCurrentIndex(std::clamp(s.health_strip_axis, 0, 3));
        axis_combo->setToolTip(
            "Which room axis maps one heart's LED strip when Per-heart strip is on. "
            "Auto picks the longest layout span.");
        axis_combo->setItemData(0, QStringLiteral("Pick X, Y, or Z by largest LED span in the layout."), Qt::ToolTipRole);
        axis_combo->setItemData(1, QStringLiteral("Strip runs with increasing X along the strip."), Qt::ToolTipRole);
        axis_combo->setItemData(2, QStringLiteral("Strip runs with increasing Y along the strip."), Qt::ToolTipRole);
        axis_combo->setItemData(3, QStringLiteral("Strip runs with increasing Z along the strip."), Qt::ToolTipRole);
        layout->addWidget(axis_lab, row, 0);
        layout->addWidget(axis_combo, row, 1);
        QObject::connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), panel, [&s](int idx) { s.health_strip_axis = std::clamp(idx, 0, 3); });
        row++;

        QCheckBox* inv_toggle = new QCheckBox("Invert strip direction");
        inv_toggle->setChecked(s.health_strip_invert);
        layout->addWidget(inv_toggle, row++, 0, 1, 2);
        QObject::connect(inv_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.health_strip_invert = v; });

    }
    if(all || ch(channels, ChHunger))
    {
        QCheckBox* hunger_toggle = new QCheckBox("Enable hunger gradient");
        hunger_toggle->setChecked(s.enable_hunger_gradient);
        layout->addWidget(hunger_toggle, row++, 0, 1, 2);
        QObject::connect(hunger_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_hunger_gradient = v; });
        QCheckBox* hunger_strip_toggle = new QCheckBox("Per-strip hunger (uses strip/index settings above)");
        hunger_strip_toggle->setChecked(s.hunger_per_strip);
        layout->addWidget(hunger_strip_toggle, row++, 0, 1, 2);
        QObject::connect(hunger_strip_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.hunger_per_strip = v; });
        addPctSlider(QStringLiteral("Hunger gradient strength"), &s.hunger_mix);
    }
    if(all || ch(channels, ChAir))
    {
        QCheckBox* air_toggle = new QCheckBox("Enable air gradient");
        air_toggle->setChecked(s.enable_air_gradient);
        layout->addWidget(air_toggle, row++, 0, 1, 2);
        QObject::connect(air_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_air_gradient = v; });
        QCheckBox* air_strip_toggle = new QCheckBox("Per-strip air (uses strip/index settings above)");
        air_strip_toggle->setChecked(s.air_per_strip);
        layout->addWidget(air_strip_toggle, row++, 0, 1, 2);
        QObject::connect(air_strip_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.air_per_strip = v; });
        addPctSlider(QStringLiteral("Air gradient strength"), &s.air_mix);
    }
    if(all || ch(channels, ChDurability))
    {
        QCheckBox* dura_toggle = new QCheckBox("Enable item durability gradient");
        dura_toggle->setChecked(s.enable_durability_gradient);
        layout->addWidget(dura_toggle, row++, 0, 1, 2);
        QObject::connect(dura_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_durability_gradient = v; });
        QCheckBox* dura_strip_toggle = new QCheckBox("Per-strip durability (uses strip/index settings above)");
        dura_strip_toggle->setChecked(s.durability_per_strip);
        layout->addWidget(dura_strip_toggle, row++, 0, 1, 2);
        QObject::connect(dura_strip_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.durability_per_strip = v; });
        addPctSlider(QStringLiteral("Item durability gradient strength"), &s.durability_mix);
    }

    if(all || ch(channels, ChDamage))
    {
        layout->addWidget(new QLabel(QStringLiteral("Damage")), row++, 0, 1, 2);
        QCheckBox* damage_toggle = new QCheckBox("Enable damage flash");
        damage_toggle->setChecked(s.enable_damage_flash);
        layout->addWidget(damage_toggle, row++, 0, 1, 2);
        QObject::connect(damage_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_damage_flash = v; });
        addPctSlider(QStringLiteral("Directional hit (vs uniform)"), &s.damage_directional_mix);
        auto* dSharpLab = new QLabel(QStringLiteral("Damage direction sharpness"));
        auto* dSharpSl = new QSlider(Qt::Horizontal);
        dSharpSl->setRange(50, 400);
        dSharpSl->setValue((int)std::lround(s.damage_dir_sharpness * 100.0f));
        layout->addWidget(dSharpLab, row, 0);
        layout->addWidget(dSharpSl, row, 1);
        QObject::connect(dSharpSl, &QSlider::valueChanged, panel, [&s](int x) { s.damage_dir_sharpness = std::clamp(x / 100.0f, 0.5f, 5.0f); });
        row++;
        auto* dDecLab = new QLabel(QStringLiteral("Damage flash decay (ms)"));
        auto* dDecSl = new QSlider(Qt::Horizontal);
        dDecSl->setRange(100, 900);
        dDecSl->setValue((int)std::lround(std::clamp(s.damage_flash_decay_s, 0.10f, 0.90f) * 1000.0f));
        layout->addWidget(dDecLab, row, 0);
        layout->addWidget(dDecSl, row, 1);
        QObject::connect(dDecSl, &QSlider::valueChanged, panel, [&s](int x) { s.damage_flash_decay_s = std::clamp(x / 1000.0f, 0.10f, 0.90f); });
        row++;
        addPctSlider(QStringLiteral("Damage flash strength"), &s.damage_flash_strength);
    }

    if(all || ch(channels, ChWorldTint))
    {
        layout->addWidget(new QLabel(QStringLiteral("World tint")), row++, 0, 1, 2);
        QCheckBox* world_tint_toggle = new QCheckBox("Enable ambient world tint");
        world_tint_toggle->setChecked(s.enable_ambient_world_tint);
        layout->addWidget(world_tint_toggle, row++, 0, 1, 2);
        QObject::connect(world_tint_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_ambient_world_tint = v; });

        auto* mixLab = new QLabel(QStringLiteral("World tint strength"));
        auto* mixSl = new QSlider(Qt::Horizontal);
        mixSl->setRange(0, 100);
        mixSl->setValue((int)std::lround(std::clamp(s.world_light_mix, 0.0f, 1.0f) * 100.0f));
        layout->addWidget(mixLab, row, 0);
        layout->addWidget(mixSl, row, 1);
        QObject::connect(mixSl, &QSlider::valueChanged, panel, [&s](int x) { s.world_light_mix = std::clamp(x / 100.0f, 0.0f, 1.0f); });
        row++;

        auto* smoothLab = new QLabel(QStringLiteral("World tint smoothing"));
        auto* smoothSl = new QSlider(Qt::Horizontal);
        smoothSl->setRange(0, 95);
        smoothSl->setValue((int)std::lround(std::clamp(s.world_tint_smoothing, 0.0f, 0.95f) * 100.0f));
        layout->addWidget(smoothLab, row, 0);
        layout->addWidget(smoothSl, row, 1);
        QObject::connect(smoothSl, &QSlider::valueChanged, panel, [&s](int x) { s.world_tint_smoothing = std::clamp(x / 100.0f, 0.0f, 0.95f); });
        row++;

        auto* dirLab = new QLabel(QStringLiteral("World tint directional response"));
        auto* dirSl = new QSlider(Qt::Horizontal);
        dirSl->setRange(0, 100);
        dirSl->setValue((int)std::lround(std::clamp(s.world_tint_directional, 0.0f, 1.0f) * 100.0f));
        layout->addWidget(dirLab, row, 0);
        layout->addWidget(dirSl, row, 1);
        QObject::connect(dirSl, &QSlider::valueChanged, panel, [&s](int x) { s.world_tint_directional = std::clamp(x / 100.0f, 0.0f, 1.0f); });
        row++;

        auto* dirSharpLab = new QLabel(QStringLiteral("World tint directional sharpness"));
        auto* dirSharpSl = new QSlider(Qt::Horizontal);
        dirSharpSl->setRange(80, 320);
        dirSharpSl->setValue((int)std::lround(std::clamp(s.world_tint_dir_sharpness, 0.8f, 3.2f) * 100.0f));
        layout->addWidget(dirSharpLab, row, 0);
        layout->addWidget(dirSharpSl, row, 1);
        QObject::connect(dirSharpSl, &QSlider::valueChanged, panel, [&s](int x) { s.world_tint_dir_sharpness = std::clamp(x / 100.0f, 0.8f, 3.2f); });
        row++;

        auto* gEndLab = new QLabel(QStringLiteral("Ground-to-mid blend ends (grid Y %)"));
        auto* gEndSl = new QSlider(Qt::Horizontal);
        gEndSl->setRange(10, 55);
        gEndSl->setValue((int)std::lround(std::clamp(s.tint_layer_ground_end, 0.10f, 0.55f) * 100.0f));
        layout->addWidget(gEndLab, row, 0);
        layout->addWidget(gEndSl, row, 1);
        QObject::connect(gEndSl, &QSlider::valueChanged, panel, [&s](int x) { s.tint_layer_ground_end = std::clamp(x / 100.0f, 0.08f, 0.55f); });
        row++;

        auto* sStartLab = new QLabel(QStringLiteral("Mid-to-sky blend starts (grid Y %)"));
        auto* sStartSl = new QSlider(Qt::Horizontal);
        sStartSl->setRange(40, 85);
        sStartSl->setValue((int)std::lround(std::clamp(s.tint_layer_sky_start, 0.40f, 0.85f) * 100.0f));
        layout->addWidget(sStartLab, row, 0);
        layout->addWidget(sStartSl, row, 1);
        QObject::connect(sStartSl, &QSlider::valueChanged, panel, [&s](int x) { s.tint_layer_sky_start = std::clamp(x / 100.0f, 0.40f, 0.92f); });
        row++;

        addPctSlider(QStringLiteral("Biome sky overlay (BiomeEffects sky)"), &s.biome_sky_overlay);
        addPctSlider(QStringLiteral("Rain darkens sky layer"), &s.env_rain_darken_sky);
        addPctSlider(QStringLiteral("Thunder darkens sky layer"), &s.env_thunder_darken_sky);
    }

    if(all || ch(channels, ChLightning))
    {
        if(!all && !ch(channels, ChWorldTint))
        {
            layout->addWidget(new QLabel(QStringLiteral("Lightning")), row++, 0, 1, 2);
        }
        QCheckBox* lightning_toggle = new QCheckBox("Enable lightning flash");
        lightning_toggle->setChecked(s.enable_lightning_flash);
        layout->addWidget(lightning_toggle, row++, 0, 1, 2);
        QObject::connect(lightning_toggle, &QCheckBox::toggled, panel, [&s](bool v) { s.enable_lightning_flash = v; });

        auto* lStrLab = new QLabel(QStringLiteral("Lightning flash strength"));
        auto* lStrSl = new QSlider(Qt::Horizontal);
        lStrSl->setRange(0, 150);
        lStrSl->setValue((int)std::lround(std::clamp(s.lightning_flash_strength, 0.0f, 1.5f) * 100.0f));
        layout->addWidget(lStrLab, row, 0);
        layout->addWidget(lStrSl, row, 1);
        QObject::connect(lStrSl, &QSlider::valueChanged, panel, [&s](int x) { s.lightning_flash_strength = std::clamp(x / 100.0f, 0.0f, 1.5f); });
        row++;

        auto* lDecLab = new QLabel(QStringLiteral("Lightning decay (ms)"));
        auto* lDecSl = new QSlider(Qt::Horizontal);
        lDecSl->setRange(80, 900);
        lDecSl->setValue((int)std::lround(std::clamp(s.lightning_flash_decay_s, 0.08f, 0.90f) * 1000.0f));
        layout->addWidget(lDecLab, row, 0);
        layout->addWidget(lDecSl, row, 1);
        QObject::connect(lDecSl, &QSlider::valueChanged, panel, [&s](int x) { s.lightning_flash_decay_s = std::clamp(x / 1000.0f, 0.08f, 0.90f); });
        row++;
    }

    if(all)
    {
        layout->addWidget(new QLabel(QStringLiteral("Output")), row++, 0, 1, 2);
    }
    {
        auto* bLab = new QLabel(QStringLiteral("Base brightness"));
        auto* bSl = new QSlider(Qt::Horizontal);
        bSl->setRange(80, 150);
        bSl->setValue((int)std::lround(std::clamp(s.base_brightness, 0.8f, 1.5f) * 100.0f));
        layout->addWidget(bLab, row, 0);
        layout->addWidget(bSl, row, 1);
        QObject::connect(bSl, &QSlider::valueChanged, panel, [&s](int x) { s.base_brightness = std::clamp(x / 100.0f, 0.8f, 1.5f); });
        row++;
    }

    return panel;
}

RGBColor RenderColor(const GameTelemetryBridge::TelemetrySnapshot& t,
                     float,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& s,
                     std::uint32_t channels,
                     WorldTintSmoothState* world_smooth)
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
    const int lph = std::clamp(s.health_leds_per_heart, 1, 32);
    const bool have_indexed =
        s.health_per_heart_indexed && tls_led_count > 0 && tls_led_index >= 0 && tls_led_index < tls_led_count;
    const bool have_spatial =
        !s.health_per_heart_indexed && HealthStripMappingUsable(grid, s.health_strip_axis);
    const auto strip_brightness_for = [&](float fill_end, float max_units) -> float {
        const float total_slots = max_units * (float)lph;
        if(total_slots < 0.01f || (!have_indexed && !have_spatial))
        {
            return -1.0f;
        }
        if(have_indexed)
        {
            return HealthStripBrightnessIndexedLed(fill_end, total_slots, tls_led_index, tls_led_count, s.health_strip_invert);
        }
        const float u = HealthStripCoord01(grid_x, grid_y, grid_z, grid, s.health_strip_axis, s.health_strip_invert);
        return HealthStripBrightnessAlongSlots(fill_end, total_slots, u);
    };

    if(ch(channels, ChHealth) && s.enable_health_gradient && t.has_health_state && t.hearts_max > 1e-4f)
    {
        const float max_h = std::max(t.hearts_max, 1e-4f);
        const float cur_h = std::clamp(t.hearts, 0.0f, max_h);
        const float filled_norm = std::clamp(cur_h / max_h, 0.0f, 1.0f);
        const RGBColor health_bar_color = LerpColor(low_health, high_health, filled_norm);

        if(s.health_per_heart_strip)
        {
            const float br = strip_brightness_for(cur_h * (float)lph, max_h);
            if(br < 0.0f)
            {
                const float health_norm = (t.health_max > 0.01f) ? std::clamp(t.health / t.health_max, 0.0f, 1.0f) : filled_norm;
                out = LerpColor(low_health, high_health, health_norm);
            }
            else
            {
                if(br > 1e-3f)
                {
                    out = LerpColor((RGBColor)0x00000000, health_bar_color, br);
                }
                else
                {
                    out = (RGBColor)0x00000000;
                }
            }
        }
        else if(t.health_max > 0.01f)
        {
            const float health_norm = std::clamp(t.health / t.health_max, 0.0f, 1.0f);
            out = LerpColor(low_health, high_health, health_norm);
        }
    }
    if(ch(channels, ChHunger) && s.enable_hunger_gradient && t.has_health_state && t.hunger_max > 0.01f)
    {
        const float hunger_norm = std::clamp(t.hunger / t.hunger_max, 0.0f, 1.0f);
        const RGBColor hunger_color = LerpColor(low_hunger, high_hunger, hunger_norm);
        if(s.hunger_per_strip)
        {
            const float br = strip_brightness_for(std::clamp(t.hunger, 0.0f, t.hunger_max) * (float)lph, t.hunger_max);
            const float mix = (br < 0.0f)
                ? std::clamp(s.hunger_mix, 0.0f, 1.0f)
                : std::clamp(br * s.hunger_mix, 0.0f, 1.0f);
            out = LerpColor(out, hunger_color, mix);
        }
        else
        {
            out = LerpColor(out, hunger_color, std::clamp(s.hunger_mix, 0.0f, 1.0f));
        }
    }
    if(ch(channels, ChAir) && s.enable_air_gradient && t.has_health_state && t.air_max > 0.01f)
    {
        const float air_norm = std::clamp(t.air / t.air_max, 0.0f, 1.0f);
        const RGBColor air_color = LerpColor(low_air, high_air, air_norm);
        if(s.air_per_strip)
        {
            const float br = strip_brightness_for(std::clamp(t.air, 0.0f, t.air_max) * (float)lph, t.air_max);
            const float mix = (br < 0.0f)
                ? std::clamp(s.air_mix, 0.0f, 1.0f)
                : std::clamp(br * s.air_mix, 0.0f, 1.0f);
            out = LerpColor(out, air_color, mix);
        }
        else
        {
            out = LerpColor(out, air_color, std::clamp(s.air_mix, 0.0f, 1.0f));
        }
    }
    if(ch(channels, ChDurability) && s.enable_durability_gradient && t.has_health_state && t.has_item_durability && t.item_durability_max > 0.01f)
    {
        const float dura_norm = std::clamp(t.item_durability / t.item_durability_max, 0.0f, 1.0f);
        const RGBColor dura_color = LerpColor(low_durability, high_durability, dura_norm);
        if(s.durability_per_strip)
        {
            const float br = strip_brightness_for(std::clamp(t.item_durability, 0.0f, t.item_durability_max) * (float)lph, t.item_durability_max);
            const float mix = (br < 0.0f)
                ? std::clamp(s.durability_mix, 0.0f, 1.0f)
                : std::clamp(br * s.durability_mix, 0.0f, 1.0f);
            out = LerpColor(out, dura_color, mix);
        }
        else
        {
            out = LerpColor(out, dura_color, std::clamp(s.durability_mix, 0.0f, 1.0f));
        }
    }

    if(ch(channels, ChWorldTint) && s.enable_ambient_world_tint && t.has_world_light && world_smooth != nullptr)
    {
        RGBColor wl = SuppressWhites(MakeRgb(t.world_light_r, t.world_light_g, t.world_light_b));
        if(t.has_world_layers)
        {
            const float y_norm = NormalizeGridAxis01(grid_y, grid.min_y, grid.max_y);
            RGBColor sky = SuppressWhites(MakeRgb(t.world_sky_r, t.world_sky_g, t.world_sky_b));
            RGBColor mid = SuppressWhites(MakeRgb(t.world_mid_r, t.world_mid_g, t.world_mid_b));
            RGBColor ground = SuppressWhites(MakeRgb(t.world_ground_r, t.world_ground_g, t.world_ground_b));
            if(t.has_vanilla_biome_colors)
            {
                if(s.biome_sky_overlay > 1e-4f)
                {
                    RGBColor bioSky = SuppressWhites(MakeRgb(t.biome_sky_r, t.biome_sky_g, t.biome_sky_b));
                    sky = LerpColor(sky, bioSky, std::clamp(s.biome_sky_overlay, 0.0f, 1.0f));
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
            const float weatherK = std::clamp(rainK * s.env_rain_darken_sky + thK * s.env_thunder_darken_sky, 0.0f, 1.0f);
            if(weatherK > 1e-4f)
            {
                sky = LerpColor(sky, (RGBColor)0x00002218, weatherK);
            }
            if(t.world_light_received_ms != 0 && t.world_light_received_ms != world_smooth->last_sample_ms)
            {
                world_smooth->last_sample_ms = t.world_light_received_ms;
                if(!world_smooth->has_smoothed)
                {
                    world_smooth->smooth_sky = sky;
                    world_smooth->smooth_mid = mid;
                    world_smooth->smooth_ground = ground;
                    world_smooth->has_smoothed = true;
                }
                else
                {
                    const float alpha = std::clamp(1.0f - s.world_tint_smoothing, 0.02f, 1.0f);
                    world_smooth->smooth_sky = LerpColor(world_smooth->smooth_sky, sky, alpha);
                    world_smooth->smooth_mid = LerpColor(world_smooth->smooth_mid, mid, alpha);
                    world_smooth->smooth_ground = LerpColor(world_smooth->smooth_ground, ground, alpha);
                }
            }
            if(world_smooth->has_smoothed)
            {
                sky = world_smooth->smooth_sky;
                mid = world_smooth->smooth_mid;
                ground = world_smooth->smooth_ground;
            }
            RGBColor projected = mid;
            const float gEnd = std::clamp(s.tint_layer_ground_end, 0.08f, 0.49f);
            const float sStart = std::clamp(s.tint_layer_sky_start, gEnd + 0.04f, 0.92f);
            if(y_norm < gEnd)
            {
                projected = LerpColor(ground, mid, y_norm / gEnd);
            }
            else if(y_norm > sStart)
            {
                const float denom = std::max(1e-3f, 1.0f - sStart);
                projected = LerpColor(mid, sky, (y_norm - sStart) / denom);
            }
            const float wi_disp = std::max(t.world_light_intensity, 0.22f);
            const float layer_mix = std::clamp((0.42f + 0.58f * s.world_light_mix) * wi_disp, 0.0f, 1.0f);
            float dirMul = 1.0f;
            if(t.has_player_pose && t.world_light_focus > 1e-4f && s.world_tint_directional > 1e-4f)
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
                        const float shaped = std::pow(std::clamp(hemi, 0.0f, 1.0f), std::max(0.8f, s.world_tint_dir_sharpness));
                        const float focus = std::clamp(t.world_light_focus, 0.0f, 1.0f);
                        const float dm = std::clamp(s.world_tint_directional * (0.25f + 0.75f * focus), 0.0f, 1.0f);
                        const float floor = 0.30f;
                        dirMul = (1.0f - dm) + dm * (floor + (1.0f - floor) * shaped);
                    }
                }
            }
            out = LerpColor(out, projected, std::clamp(layer_mix * dirMul, 0.0f, 1.0f));
            wl = LerpColor(wl, projected, 0.6f);
        }
        const float wi_disp2 = std::max(t.world_light_intensity, 0.22f);
        const float wl_mix = std::clamp(s.world_light_mix * wi_disp2, 0.0f, 1.0f);
        out = LerpColor(out, wl, wl_mix * 0.72f);
    }

    if(ch(channels, ChDamage) && s.enable_damage_flash && t.has_damage_event && t.damage_received_ms > 0)
    {
        const unsigned long long now = NowMs();
        const unsigned long long elapsed_ms = (now > t.damage_received_ms) ? (now - t.damage_received_ms) : 0;
        const float decay_ms = std::max(100.0f, s.damage_flash_decay_s * 1000.0f);
        const float damage_t = std::clamp(1.0f - (elapsed_ms / decay_ms), 0.0f, 1.0f);
        if(damage_t > 0.0f)
        {
            const float damage_strength = std::clamp(t.damage_amount / 20.0f, 0.0f, 1.0f);
            float flash_mix = std::clamp(s.damage_flash_strength * damage_t * (0.2f + 0.8f * damage_strength), 0.0f, 1.0f);
            if(s.damage_directional_mix > 1e-4f && t.has_player_pose)
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
                    const float shaped = std::pow(std::clamp(hemi, 0.0f, 1.0f), std::max(0.5f, s.damage_dir_sharpness));
                    const float dirMix = std::clamp(s.damage_directional_mix, 0.0f, 1.0f);
                    const float minFactor = 0.10f;
                    const float dirFactor = minFactor + (1.0f - minFactor) * shaped;
                    flash_mix *= (1.0f - dirMix) + dirMix * dirFactor;
                    flash_mix = std::clamp(flash_mix, 0.0f, 1.0f);
                }
            }
            out = LerpColor(out, (RGBColor)0x000000FF, flash_mix);
        }
    }

    if(ch(channels, ChLightning) && s.enable_lightning_flash && t.has_lightning_event && t.lightning_received_ms > 0)
    {
        const unsigned long long now = NowMs();
        const unsigned long long elapsed_ms = (now > t.lightning_received_ms) ? (now - t.lightning_received_ms) : 0;
        const float decay_ms = std::max(80.0f, s.lightning_flash_decay_s * 1000.0f);
        const float lt = std::clamp(1.0f - (elapsed_ms / decay_ms), 0.0f, 1.0f);
        if(lt > 0.0f)
        {
            const float y_norm = NormalizeGridAxis01(grid_y, grid.min_y, grid.max_y);
            const float skyBias = std::pow(y_norm, 1.9f);
            const float layer = 0.20f + 0.80f * skyBias;
            const float st = std::clamp(s.lightning_flash_strength * t.lightning_strength * lt * layer, 0.0f, 1.0f);
            out = LerpColor(out, (RGBColor)0x00FFF0DE, st);
        }
    }

    const int r = std::clamp((int)((out & 0xFF) * s.base_brightness), 0, 255);
    const int g = std::clamp((int)(((out >> 8) & 0xFF) * s.base_brightness), 0, 255);
    const int b = std::clamp((int)(((out >> 16) & 0xFF) * s.base_brightness), 0, 255);
    out = (RGBColor)((b << 16) | (g << 8) | r);

    return out;
}

void ApplyFabricGameEffectChrome(SpatialEffect3D* effect)
{
    if(!effect)
    {
        return;
    }
    effect->SetControlGroupVisibility(effect->speed_slider, effect->speed_label, QStringLiteral("Speed:"), false);
    effect->SetControlGroupVisibility(effect->frequency_slider, effect->frequency_label, QStringLiteral("Frequency:"), false);
    effect->SetControlGroupVisibility(effect->detail_slider, effect->detail_label, QStringLiteral("Detail:"), false);
    effect->SetControlGroupVisibility(effect->size_slider, effect->size_label, QStringLiteral("Size:"), false);
    effect->SetControlGroupVisibility(effect->scale_slider, effect->scale_label, QStringLiteral("Scale:"), false);

    effect->SetControlGroupVisibility(effect->brightness_slider, effect->brightness_label, QStringLiteral("Brightness:"), true);
    effect->SetControlGroupVisibility(effect->intensity_slider, effect->intensity_label, QStringLiteral("Intensity:"), true);
    effect->SetControlGroupVisibility(effect->sharpness_slider, effect->sharpness_label, QStringLiteral("Sharpness:"), true);

    if(effect->color_controls_group)
    {
        effect->color_controls_group->setVisible(false);
    }
    if(effect->surfaces_group)
    {
        effect->surfaces_group->setVisible(false);
    }
    if(effect->position_offset_group)
    {
        effect->position_offset_group->setVisible(false);
    }
    if(effect->edge_shape_group)
    {
        effect->edge_shape_group->setVisible(false);
    }
    if(effect->path_plane_group)
    {
        effect->path_plane_group->setVisible(false);
    }

    if(effect->effect_controls_group)
    {
        const QList<QGroupBox*> groups =
            effect->effect_controls_group->findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
        for(QGroupBox* gb : groups)
        {
            const QString t = gb->title();
            if(t == QStringLiteral("Effect geometry"))
            {
                gb->setVisible(false);
            }
        }
    }
}

}
