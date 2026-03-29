// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGame.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace MinecraftGame
{
float g_damage_flash_decay_s = 0.35f;
float g_world_light_mix = 0.85f;
float g_damage_flash_strength = 1.0f;
float g_base_brightness = 1.12f;
float g_probe_mix = 0.92f;
bool g_enable_health_gradient = true;
bool g_enable_damage_flash = true;
bool g_enable_ambient_world_tint = true;
bool g_enable_spatial_camera = true;
/** 0 = legacy probe hemispheres, 1 = six-face cube, 2 = amBX-style compass (8 horizontal + center) with soft blending. */
int g_spatial_map_mode = 2;

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

/** Match GameTelemetryBridge compass layout: N,NE,E,SE,S,SW,W,NW from telemetry forward/up (horizontal). */
bool BuildPlayerCompassDirs(const GameTelemetryBridge::TelemetrySnapshot& t,
                            float sx[8],
                            float sy[8],
                            float sz[8],
                            float* out_ux,
                            float* out_uy,
                            float* out_uz)
{
    float ux = t.up_x;
    float uy = t.up_y;
    float uz = t.up_z;
    float ulen = std::sqrt(ux * ux + uy * uy + uz * uz);
    if(ulen < 1e-5f)
    {
        ux = 0.0f;
        uy = 1.0f;
        uz = 0.0f;
        ulen = 1.0f;
    }
    else
    {
        ux /= ulen;
        uy /= ulen;
        uz /= ulen;
    }
    float fx = t.forward_x;
    float fy = t.forward_y;
    float fz = t.forward_z;
    float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
    if(flen < 1e-5f)
    {
        fx = 0.0f;
        fy = 0.0f;
        fz = 1.0f;
        flen = 1.0f;
    }
    else
    {
        fx /= flen;
        fy /= flen;
        fz /= flen;
    }
    float fhx = fx - ux * (fx * ux + fy * uy + fz * uz);
    float fhy = fy - uy * (fx * ux + fy * uy + fz * uz);
    float fhz = fz - uz * (fx * ux + fy * uy + fz * uz);
    float fhlen = std::sqrt(fhx * fhx + fhy * fhy + fhz * fhz);
    if(fhlen < 1e-5f)
    {
        fhx = 1.0f;
        fhy = 0.0f;
        fhz = 0.0f;
        fhlen = 1.0f;
    }
    else
    {
        fhx /= fhlen;
        fhy /= fhlen;
        fhz /= fhlen;
    }
    float rhx = uy * fhz - uz * fhy;
    float rhy = uz * fhx - ux * fhz;
    float rhz = ux * fhy - uy * fhx;
    float rhlen = std::sqrt(rhx * rhx + rhy * rhy + rhz * rhz);
    if(rhlen > 1e-5f)
    {
        rhx /= rhlen;
        rhy /= rhlen;
        rhz /= rhlen;
    }
    auto add_norm = [](float ax, float ay, float az, float bx, float by, float bz, float& ox, float& oy, float& oz) {
        ox = ax + bx;
        oy = ay + by;
        oz = az + bz;
        const float l = std::sqrt(ox * ox + oy * oy + oz * oz);
        if(l > 1e-5f)
        {
            ox /= l;
            oy /= l;
            oz /= l;
        }
    };
    sx[0] = fhx;
    sy[0] = fhy;
    sz[0] = fhz;
    sx[4] = -fhx;
    sy[4] = -fhy;
    sz[4] = -fhz;
    sx[2] = rhx;
    sy[2] = rhy;
    sz[2] = rhz;
    sx[6] = -rhx;
    sy[6] = -rhy;
    sz[6] = -rhz;
    add_norm(fhx, fhy, fhz, rhx, rhy, rhz, sx[1], sy[1], sz[1]);
    add_norm(-fhx, -fhy, -fhz, rhx, rhy, rhz, sx[3], sy[3], sz[3]);
    add_norm(-fhx, -fhy, -fhz, -rhx, -rhy, -rhz, sx[5], sy[5], sz[5]);
    add_norm(fhx, fhy, fhz, -rhx, -rhy, -rhz, sx[7], sy[7], sz[7]);
    *out_ux = ux;
    *out_uy = uy;
    *out_uz = uz;
    return true;
}

QWidget* CreateMinecraftSettingsWidget(QWidget* parent)
{
    QWidget* panel = new QWidget(parent);
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    QCheckBox* health_toggle = new QCheckBox("Health gradient");
    health_toggle->setChecked(g_enable_health_gradient);
    layout->addWidget(health_toggle, 0, 0, 1, 2);

    QCheckBox* damage_toggle = new QCheckBox("Damage flash");
    damage_toggle->setChecked(g_enable_damage_flash);
    layout->addWidget(damage_toggle, 1, 0, 1, 2);

    QCheckBox* world_tint_toggle = new QCheckBox("Ambient world tint");
    world_tint_toggle->setChecked(g_enable_ambient_world_tint);
    layout->addWidget(world_tint_toggle, 2, 0, 1, 2);

    QCheckBox* spatial_toggle = new QCheckBox("Spatial camera (probes)");
    spatial_toggle->setChecked(g_enable_spatial_camera);
    layout->addWidget(spatial_toggle, 3, 0, 1, 2);

    QComboBox* spatial_map_combo = new QComboBox();
    spatial_map_combo->addItem(QStringLiteral("Probes: Legacy hemispheres"));
    spatial_map_combo->addItem(QStringLiteral("Probes: Six-face cube"));
    spatial_map_combo->addItem(QStringLiteral("Probes: Compass (8 + center)"));
    spatial_map_combo->setCurrentIndex(std::clamp(g_spatial_map_mode, 0, 2));
    layout->addWidget(spatial_map_combo, 4, 0, 1, 2);

    QObject::connect(health_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_health_gradient = v; });
    QObject::connect(damage_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_damage_flash = v; });
    QObject::connect(world_tint_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_ambient_world_tint = v; });
    QObject::connect(spatial_toggle, &QCheckBox::toggled, panel, [](bool v) { g_enable_spatial_camera = v; });
    QObject::connect(spatial_map_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), panel, [](int idx) {
        g_spatial_map_mode = std::clamp(idx, 0, 2);
    });

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
    float health_norm = 1.0f;
    if(t.has_health_state && t.health_max > 0.01f)
    {
        health_norm = std::clamp(t.health / t.health_max, 0.0f, 1.0f);
    }

    const RGBColor low_health = (RGBColor)0x000022FF;
    const RGBColor high_health = (RGBColor)0x0000FF22;
    RGBColor out = g_enable_health_gradient ? LerpColor(low_health, high_health, health_norm) : (RGBColor)0x00000000;

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
            RGBColor projected = mid;
            /* World tint: low grid Y = ground, high Y = sky (matches typical floor/ceiling rigs). */
            if(y_norm < 0.36f)
            {
                projected = LerpColor(ground, mid, y_norm / 0.36f);
            }
            else if(y_norm > 0.54f)
            {
                projected = LerpColor(mid, sky, (y_norm - 0.54f) / 0.46f);
            }
            const float layer_mix = std::clamp((0.30f + 0.70f * g_world_light_mix) * std::max(0.0f, t.world_light_intensity), 0.0f, 1.0f);
            out = LerpColor(out, projected, layer_mix);
            wl = LerpColor(wl, projected, 0.6f);
        }
        const float wl_mix = std::clamp(g_world_light_mix * std::max(0.0f, t.world_light_intensity), 0.0f, 1.0f);
        out = LerpColor(out, wl, wl_mix * 0.55f);
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
            const float flash_mix = std::clamp(g_damage_flash_strength * damage_t * (0.2f + 0.8f * damage_strength), 0.0f, 1.0f);
            out = LerpColor(out, (RGBColor)0x000000FF, flash_mix);
        }
    }

    if(g_enable_spatial_camera && t.has_world_probes && t.world_probe_count > 0)
    {
        float ox = grid_x - origin_x;
        float oy = grid_y - origin_y;
        float oz = grid_z - origin_z;
        float len = std::sqrt(ox * ox + oy * oy + oz * oz);
        if(len > 1e-4f)
        {
            ox /= len;
            oy /= len;
            oz /= len;
            float max_probe_i = 0.0f;
            float sum_probe_i = 0.0f;
            const int count = std::min(64, t.world_probe_count);
            for(int i = 0; i < count; i++)
            {
                const float pi = std::clamp(t.world_probe_intensity[i], 0.0f, 1.2f);
                max_probe_i = std::max(max_probe_i, pi);
                sum_probe_i += pi;
            }
            const float mean_probe_i = (count > 0) ? (sum_probe_i / (float)count) : 0.0f;

            const auto finish_spatial = [&](float prf, float pgf, float pbf)
            {
                if(sum_probe_i <= 1e-6f)
                {
                    return;
                }
                const float chroma = std::max(prf, std::max(pgf, pbf)) - std::min(prf, std::min(pgf, pbf));
                const float luma = 0.2126f * prf + 0.7152f * pgf + 0.0722f * pbf;
                const float dim = std::clamp(0.32f + 0.68f * std::pow(std::clamp(mean_probe_i, 0.0f, 1.0f), 1.12f), 0.0f, 1.0f);
                if(chroma < 24.0f && luma > 42.0f)
                {
                    const float crush = std::clamp((luma - 42.0f) / 140.0f, 0.0f, 1.0f) * (1.0f - dim) * 0.75f;
                    const float k = std::clamp(0.62f + 0.38f * dim, 0.0f, 1.0f);
                    prf = std::clamp(prf * k - crush * 18.0f, 0.0f, 255.0f);
                    pgf = std::clamp(pgf * k - crush * 18.0f, 0.0f, 255.0f);
                    pbf = std::clamp(pbf * k - crush * 18.0f, 0.0f, 255.0f);
                }
                else
                {
                    const float sat_lift = 1.0f + 0.40f * std::clamp(chroma / 70.0f, 0.0f, 1.0f);
                    prf = std::clamp(prf * dim * sat_lift, 0.0f, 255.0f);
                    pgf = std::clamp(pgf * dim * sat_lift, 0.0f, 255.0f);
                    pbf = std::clamp(pbf * dim * sat_lift, 0.0f, 255.0f);
                }
                const int pr = std::clamp((int)std::lround(prf), 0, 255);
                const int pg = std::clamp((int)std::lround(pgf), 0, 255);
                const int pb = std::clamp((int)std::lround(pbf), 0, 255);
                const RGBColor raw_probe = (RGBColor)((pb << 16) | (pg << 8) | pr);
                const RGBColor probe_color = (chroma >= 32.0f) ? raw_probe : SuppressWhites(raw_probe);
                const float peak = std::pow(std::clamp(max_probe_i, 0.0f, 1.0f), 1.32f);
                const float fill = std::pow(std::clamp(mean_probe_i, 0.0f, 1.0f), 1.06f);
                const float spatial_t = std::clamp(g_probe_mix * std::min(1.0f, peak * 1.22f) * fill, 0.0f, 1.0f);
                out = LerpColor(out, probe_color, spatial_t);
            };

            bool used_spatial_path = false;
            if(g_spatial_map_mode == 2 && t.has_world_probe_compass)
            {
                float sx[8], sy[8], sz[8];
                float ux = 0.0f;
                float uy = 1.0f;
                float uz = 0.0f;
                if(BuildPlayerCompassDirs(t, sx, sy, sz, &ux, &uy, &uz))
                {
                    constexpr float fe_led = 2.2f;
                    constexpr float led_iso = 0.11f;
                    float lw[8];
                    float lsum = 0.0f;
                    for(int k = 0; k < 8; ++k)
                    {
                        const float dp = std::max(0.0f, ox * sx[k] + oy * sy[k] + oz * sz[k]);
                        lw[k] = led_iso + (1.0f - led_iso) * std::pow(dp, fe_led);
                        lsum += lw[k];
                    }
                    if(lsum > 1e-8f)
                    {
                        float hr = 0.0f;
                        float hg = 0.0f;
                        float hb = 0.0f;
                        for(int k = 0; k < 8; ++k)
                        {
                            hr += t.world_probe_compass_r[k] * lw[k];
                            hg += t.world_probe_compass_g[k] * lw[k];
                            hb += t.world_probe_compass_b[k] * lw[k];
                        }
                        hr /= lsum;
                        hg /= lsum;
                        hb /= lsum;
                        const float mx = *std::max_element(lw, lw + 8);
                        const float spread = std::clamp(1.0f - mx / lsum, 0.0f, 1.0f);
                        const float dup = ox * ux + oy * uy + oz * uz;
                        const float elev = std::abs(dup);
                        constexpr float k_vert = 0.48f;
                        const float vert_mix = std::clamp(k_vert * elev * elev, 0.0f, 1.0f);
                        float vr = hr;
                        float vg = hg;
                        float vb = hb;
                        if(t.has_world_probe_cube)
                        {
                            const float w_up = std::pow(std::max(0.0f, dup), fe_led);
                            const float w_dn = std::pow(std::max(0.0f, -dup), fe_led);
                            const float ws = w_up + w_dn + 1e-6f;
                            vr = (w_up * t.world_probe_cube_r[2] + w_dn * t.world_probe_cube_r[3]) / ws;
                            vg = (w_up * t.world_probe_cube_g[2] + w_dn * t.world_probe_cube_g[3]) / ws;
                            vb = (w_up * t.world_probe_cube_b[2] + w_dn * t.world_probe_cube_b[3]) / ws;
                        }
                        float prf = hr * (1.0f - vert_mix) + vr * vert_mix;
                        float pgf = hg * (1.0f - vert_mix) + vg * vert_mix;
                        float pbf = hb * (1.0f - vert_mix) + vb * vert_mix;
                        const float cr = t.world_probe_compass_r[8];
                        const float cg = t.world_probe_compass_g[8];
                        const float cb = t.world_probe_compass_b[8];
                        const float ca = std::clamp(0.08f + 0.38f * elev * elev + 0.22f * spread * spread, 0.0f, 0.72f);
                        prf = prf * (1.0f - ca) + cr * ca;
                        pgf = pgf * (1.0f - ca) + cg * ca;
                        pbf = pbf * (1.0f - ca) + cb * ca;
                        finish_spatial(prf, pgf, pbf);
                        used_spatial_path = true;
                    }
                }
            }
            if(!used_spatial_path && (g_spatial_map_mode == 1 || g_spatial_map_mode == 2) && t.has_world_probe_cube)
            {
                /* Same axis weighting as bridge: squared positive part along each axis, exponent for sharpness. */
                constexpr float fe = 2.2f;
                const float lw0 = std::pow(std::max(0.0f, ox), fe);
                const float lw1 = std::pow(std::max(0.0f, -ox), fe);
                const float lw2 = std::pow(std::max(0.0f, oy), fe);
                const float lw3 = std::pow(std::max(0.0f, -oy), fe);
                const float lw4 = std::pow(std::max(0.0f, oz), fe);
                const float lw5 = std::pow(std::max(0.0f, -oz), fe);
                const float wl = lw0 + lw1 + lw2 + lw3 + lw4 + lw5;
                if(wl > 1e-8f)
                {
                    const float lws[6] = {lw0, lw1, lw2, lw3, lw4, lw5};
                    float prf = 0.0f;
                    float pgf = 0.0f;
                    float pbf = 0.0f;
                    for(int f = 0; f < 6; f++)
                    {
                        prf += (float)t.world_probe_cube_r[f] * lws[f];
                        pgf += (float)t.world_probe_cube_g[f] * lws[f];
                        pbf += (float)t.world_probe_cube_b[f] * lws[f];
                    }
                    prf /= wl;
                    pgf /= wl;
                    pbf /= wl;
                    finish_spatial(prf, pgf, pbf);
                    used_spatial_path = true;
                }
            }
            if(!used_spatial_path)
            {
                float sum_w = 0.0f;
                float acc_r = 0.0f;
                float acc_g = 0.0f;
                float acc_b = 0.0f;
                /* Directional lobe + small isotropic floor: if grid Z/X vs game space is flipped, pure hemispherical
                   weights can all be ~0 so spatial shows nothing while world tint still works. */
                constexpr float k_dot_power = 7.0f;
                constexpr float k_isotropic_mix = 0.14f;
                for(int i = 0; i < count; i++)
                {
                    const float pi = std::clamp(t.world_probe_intensity[i], 0.0f, 1.2f);
                    float px = t.world_probe_dir_x[i];
                    float py = t.world_probe_dir_y[i];
                    float pz = t.world_probe_dir_z[i];
                    float plen = std::sqrt(px * px + py * py + pz * pz);
                    if(plen < 1e-5f)
                    {
                        continue;
                    }
                    px /= plen;
                    py /= plen;
                    pz /= plen;
                    float dot = ox * px + oy * py + oz * pz;
                    const float hem = std::pow(std::clamp(dot, 0.0f, 1.0f), k_dot_power);
                    const float w = pi * (k_isotropic_mix + (1.0f - k_isotropic_mix) * hem);
                    if(w <= 1e-8f)
                    {
                        continue;
                    }
                    sum_w += w;
                    acc_r += (float)t.world_probe_r[i] * w;
                    acc_g += (float)t.world_probe_g[i] * w;
                    acc_b += (float)t.world_probe_b[i] * w;
                }
                if(sum_w > 1e-8f)
                {
                    finish_spatial(acc_r / sum_w, acc_g / sum_w, acc_b / sum_w);
                }
            }
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
