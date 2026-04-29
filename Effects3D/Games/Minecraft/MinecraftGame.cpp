// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGame.h"
#include "MinecraftGameSettings.h"
#include "Game/GameTelemetryStatusPanel.h"
#include "SpatialBasisUtils.h"
#include "SpatialLayerCore.h"
#include "VoxelRoomCore.h"
#include "SpatialEffect3D.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>

namespace MinecraftGame
{

namespace
{
thread_local int tls_led_index = -1;
thread_local int tls_led_count = 0;

struct AdaptiveLayerState
{
    std::array<unsigned int, 48> y_hist{};
    unsigned int y_samples = 0;
    bool has_gamma = false;
    float gamma = 1.0f;
    bool has_auto_bounds = false;
    float auto_floor_end = 0.30f;
    float auto_desk_end = 0.55f;
    float auto_upper_end = 0.78f;
};

thread_local AdaptiveLayerState tls_layer_state;

// Published from the render path when spatial debug sweep is active; read by the settings UI timer.
std::atomic<int> g_spatial_sweep_live{0};
std::atomic<int> g_spatial_sweep_step{0};
std::atomic<int> g_spatial_sweep_step_count{0};
std::atomic<int> g_spatial_sweep_layer_count{0};
std::atomic<int> g_spatial_sweep_top_band{0};
std::atomic<int> g_spatial_sweep_sector{0};

static float QuantileFromHist(const std::array<unsigned int, 48>& hist, unsigned int total, float q)
{
    if(total == 0)
    {
        return 0.5f;
    }
    const float target = std::clamp(q, 0.0f, 1.0f) * (float)total;
    unsigned int run = 0;
    for(size_t i = 0; i < hist.size(); i++)
    {
        run += hist[i];
        if((float)run >= target)
        {
            const float b0 = (float)i / (float)hist.size();
            const float b1 = (float)(i + 1) / (float)hist.size();
            return 0.5f * (b0 + b1);
        }
    }
    return 1.0f;
}

static void FinalizeAdaptiveLayerState()
{
    if(tls_layer_state.y_samples < 8)
    {
        tls_layer_state.y_hist.fill(0u);
        tls_layer_state.y_samples = 0;
        return;
    }

    const float q10 = QuantileFromHist(tls_layer_state.y_hist, tls_layer_state.y_samples, 0.10f);
    const float q25 = QuantileFromHist(tls_layer_state.y_hist, tls_layer_state.y_samples, 0.25f);
    const float q50 = QuantileFromHist(tls_layer_state.y_hist, tls_layer_state.y_samples, 0.50f);
    const float q75 = QuantileFromHist(tls_layer_state.y_hist, tls_layer_state.y_samples, 0.75f);
    const float q90 = QuantileFromHist(tls_layer_state.y_hist, tls_layer_state.y_samples, 0.90f);
    const float spread = q90 - q10;

    float target_gamma = 1.0f;
    if(spread > 0.10f)
    {
        // Typical room layouts have dense "desk/mid" LEDs sitting lower than half-height
        // once floor + ceiling strips are included. Lift that middle mass adaptively.
        const float target_mid = 0.46f;
        const float m = std::clamp(q50, 0.05f, 0.95f);
        target_gamma = std::log(target_mid) / std::log(m);
        target_gamma = std::clamp(target_gamma, 0.65f, 1.35f);
    }

    if(!tls_layer_state.has_gamma)
    {
        tls_layer_state.gamma = target_gamma;
        tls_layer_state.has_gamma = true;
    }
    else
    {
        tls_layer_state.gamma = tls_layer_state.gamma + (target_gamma - tls_layer_state.gamma) * 0.25f;
    }

    const float floor_target = std::clamp(q25 + 0.02f, 0.10f, 0.48f);
    const float desk_target = std::clamp(q50 + 0.04f, floor_target + 0.08f, 0.80f);
    const float upper_target = std::clamp(q75 + 0.03f, desk_target + 0.08f, 0.94f);
    if(!tls_layer_state.has_auto_bounds)
    {
        tls_layer_state.auto_floor_end = floor_target;
        tls_layer_state.auto_desk_end = desk_target;
        tls_layer_state.auto_upper_end = upper_target;
        tls_layer_state.has_auto_bounds = true;
    }
    else
    {
        tls_layer_state.auto_floor_end += (floor_target - tls_layer_state.auto_floor_end) * 0.25f;
        tls_layer_state.auto_desk_end += (desk_target - tls_layer_state.auto_desk_end) * 0.25f;
        tls_layer_state.auto_upper_end += (upper_target - tls_layer_state.auto_upper_end) * 0.25f;
    }

    tls_layer_state.y_hist.fill(0u);
    tls_layer_state.y_samples = 0;
}
}

void SetRenderSampleIndexContext(int led_index, int led_count)
{
    tls_led_index = led_index;
    tls_led_count = led_count;
}

void ClearRenderSampleIndexContext()
{
    FinalizeAdaptiveLayerState();
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

RGBColor ApplyVividness(RGBColor c, float vividness)
{
    const float v = std::clamp(vividness, 0.60f, 2.00f);
    if(std::fabs(v - 1.0f) < 1e-4f)
    {
        return c;
    }
    float r = (float)(c & 0xFF) / 255.0f;
    float g = (float)((c >> 8) & 0xFF) / 255.0f;
    float b = (float)((c >> 16) & 0xFF) / 255.0f;
    const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    r = luma + (r - luma) * v;
    g = luma + (g - luma) * v;
    b = luma + (b - luma) * v;
    const float value_gain = 1.0f + 0.10f * (v - 1.0f);
    r = std::clamp(r * value_gain, 0.0f, 1.0f);
    g = std::clamp(g * value_gain, 0.0f, 1.0f);
    b = std::clamp(b * value_gain, 0.0f, 1.0f);
    const int ri = std::clamp((int)std::round(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::round(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::round(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

static bool ch(std::uint32_t mask, std::uint32_t bit) { return (mask & bit) != 0u; }

static SpatialLayerCore::LayerProfileMode ResolveLayerProfileMode(const Settings& s)
{
    if(s.spatial_layer_profile_mode == 3)
    {
        return SpatialLayerCore::LayerProfileMode::ThreeLayer;
    }
    if(s.spatial_layer_profile_mode == 4)
    {
        return SpatialLayerCore::LayerProfileMode::FourLayer;
    }
    return SpatialLayerCore::LayerProfileMode::Auto;
}

static int ResolveLayerCountForDebug(const Settings& s)
{
    if(s.spatial_layer_profile_mode == 3)
    {
        return 3;
    }
    return 4;
}

static SpatialLayerCore::ProbeInput BuildProbeInput(const GameTelemetryBridge::TelemetrySnapshot& t)
{
    SpatialLayerCore::ProbeInput input;
    if(t.has_layered_world_probes &&
       (t.layered_probe_layer_count == 3 || t.layered_probe_layer_count == 4) &&
       t.layered_probe_sector_count == 9)
    {
        input.layered.has_layered = true;
        input.layered.layer_count = t.layered_probe_layer_count;
        const int count = t.layered_probe_layer_count * t.layered_probe_sector_count;
        for(int i = 0; i < count; i++)
        {
            const int off = i * 3;
            const unsigned char r = t.layered_probe_rgb[(size_t)off + 0];
            const unsigned char g = t.layered_probe_rgb[(size_t)off + 1];
            const unsigned char b = t.layered_probe_rgb[(size_t)off + 2];
            input.layered.colors[(size_t)i] = MakeRgb(r, g, b);
        }
    }
    return input;
}

/** Minecraft / layout: Y is up (sky). Compass and horizontal cues follow yaw only, not look pitch. */
static void MinecraftRoomHorizontalBasis(float look_x,
                                         float look_y,
                                         float look_z,
                                         float heading_offset_deg,
                                         float& out_ux,
                                         float& out_uy,
                                         float& out_uz,
                                         float& out_fx,
                                         float& out_fy,
                                         float& out_fz,
                                         float& out_rx,
                                         float& out_ry,
                                         float& out_rz)
{
    out_ux = 0.0f;
    out_uy = 1.0f;
    out_uz = 0.0f;
    float lx = look_x;
    float ly = look_y;
    float lz = look_z;
    float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    if(ll <= 1e-5f)
    {
        lx = 0.0f;
        ly = 0.0f;
        lz = 1.0f;
    }
    else
    {
        lx /= ll;
        ly /= ll;
        lz /= ll;
    }
    const float horiz = lx * out_ux + ly * out_uy + lz * out_uz;
    out_fx = lx - horiz * out_ux;
    out_fy = ly - horiz * out_uy;
    out_fz = lz - horiz * out_uz;
    float fl = std::sqrt(out_fx * out_fx + out_fy * out_fy + out_fz * out_fz);
    if(fl <= 1e-5f)
    {
        out_fx = 0.0f;
        out_fy = 0.0f;
        out_fz = 1.0f;
    }
    else
    {
        out_fx /= fl;
        out_fy /= fl;
        out_fz /= fl;
    }
    out_rx = out_fy * out_uz - out_fz * out_uy;
    out_ry = out_fz * out_ux - out_fx * out_uz;
    out_rz = out_fx * out_uy - out_fy * out_ux;
    float rl = std::sqrt(out_rx * out_rx + out_ry * out_ry + out_rz * out_rz);
    if(rl <= 1e-5f)
    {
        out_rx = 1.0f;
        out_ry = 0.0f;
        out_rz = 0.0f;
    }
    else
    {
        out_rx /= rl;
        out_ry /= rl;
        out_rz /= rl;
    }

    const float yaw = heading_offset_deg * 0.01745329251f;
    if(std::fabs(yaw) > 1e-5f)
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        const float fx2 = out_fx * c + out_rx * s;
        const float fy2 = out_fy * c + out_ry * s;
        const float fz2 = out_fz * c + out_rz * s;
        const float rx2 = out_rx * c - out_fx * s;
        const float ry2 = out_ry * c - out_fy * s;
        const float rz2 = out_rz * c - out_fz * s;
        out_fx = fx2; out_fy = fy2; out_fz = fz2;
        out_rx = rx2; out_ry = ry2; out_rz = rz2;
    }
}

/**
 * Scene / LED layout (Spatial tab, front view): X left–right, Y up–down (floor→ceiling), Z depth
 * (toward/away). Matches SpatialLayerCore’s Y-up sample space — pass positions through unchanged.
 * Minecraft camera vectors from telemetry stay in their native Y-up world frame.
 */
static inline void RoomLedToSpatialCore(float rx,
                                        float ry,
                                        float rz,
                                        float rox,
                                        float roy,
                                        float roz,
                                        float& sx,
                                        float& sy,
                                        float& sz,
                                        float& sox,
                                        float& soy,
                                        float& soz)
{
    sx = rx;
    sy = ry;
    sz = rz;
    sox = rox;
    soy = roy;
    soz = roz;
}

// Y is vertical; bounds may increase either toward floor or ceiling. Infer floor vs ceiling from
// origin proximity (front-left-floor) so 0.0 = floor and 1.0 = ceiling.
static inline float NormalizeRoomVertical01(float y, const GridContext3D& grid, float origin_y)
{
    const float d_min = std::fabs(origin_y - grid.min_y);
    const float d_max = std::fabs(origin_y - grid.max_y);
    const float floor_y = (d_min <= d_max) ? grid.min_y : grid.max_y;
    const float ceil_y = (d_min <= d_max) ? grid.max_y : grid.min_y;
    if(std::fabs(ceil_y - floor_y) <= 1e-6f)
    {
        return 0.5f;
    }
    return std::clamp((y - floor_y) / (ceil_y - floor_y), 0.0f, 1.0f);
}

enum class SpatialMappingMode : int
{
    Compass = 0,
    Voxel = 1,
    Classic = 2
};

static SpatialMappingMode ResolveSpatialMappingMode(const Settings& s)
{
    if(s.spatial_mapping_mode == 1)
    {
        return SpatialMappingMode::Voxel;
    }
    if(s.spatial_mapping_mode == 2)
    {
        return SpatialMappingMode::Classic;
    }
    return SpatialMappingMode::Compass;
}

static bool ComputeNormalizedSampleVector(float grid_x,
                                          float grid_y,
                                          float grid_z,
                                          float origin_x,
                                          float origin_y,
                                          float origin_z,
                                          float& out_x,
                                          float& out_y,
                                          float& out_z)
{
    float sx, sy, sz, sox, soy, soz;
    RoomLedToSpatialCore(grid_x,
                         grid_y,
                         grid_z,
                         origin_x,
                         origin_y,
                         origin_z,
                         sx,
                         sy,
                         sz,
                         sox,
                         soy,
                         soz);
    return SpatialBasisUtils::NormalizeDirection(sx - sox, sy - soy, sz - soz, out_x, out_y, out_z);
}

static float ComputeDirectionalFactorHorizontalPose(const GameTelemetryBridge::TelemetrySnapshot& t,
                                                    float dir_x,
                                                    float dir_y,
                                                    float dir_z,
                                                    float grid_x,
                                                    float grid_y,
                                                    float grid_z,
                                                    float origin_x,
                                                    float origin_y,
                                                    float origin_z,
                                                    float heading_offset_deg,
                                                    float mix,
                                                    float sharpness,
                                                    float min_factor)
{
    if(!t.has_player_pose || mix <= 1e-4f)
    {
        return 1.0f;
    }

    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    float fx = 0.0f, fy = 0.0f, fz = 1.0f;
    float rx = 1.0f, ry = 0.0f, rz = 0.0f;
    MinecraftRoomHorizontalBasis(t.forward_x,
                                 t.forward_y,
                                 t.forward_z,
                                 heading_offset_deg,
                                 ux,
                                 uy,
                                 uz,
                                 fx,
                                 fy,
                                 fz,
                                 rx,
                                 ry,
                                 rz);

    float ldx, ldy, ldz;
    if(!SpatialBasisUtils::NormalizeDirection(dir_x, dir_y, dir_z, ldx, ldy, ldz))
    {
        return 1.0f;
    }

    const float lx = ldx * rx + ldy * ry + ldz * rz;
    const float ly = ldx * ux + ldy * uy + ldz * uz;
    const float lz = ldx * fx + ldy * fy + ldz * fz;

    float ox, oy, oz;
    if(!ComputeNormalizedSampleVector(grid_x, grid_y, grid_z, origin_x, origin_y, origin_z, ox, oy, oz))
    {
        return 1.0f;
    }

    const float sa = std::clamp(ox * lx + oy * ly + oz * lz, -1.0f, 1.0f);
    const float hemi = 0.5f * (sa + 1.0f);
    const float shaped = std::pow(std::clamp(hemi, 0.0f, 1.0f), std::max(0.8f, sharpness));
    return (1.0f - mix) + mix * (min_factor + (1.0f - min_factor) * shaped);
}

static float ComputeDirectionalFactorFullPose(const GameTelemetryBridge::TelemetrySnapshot& t,
                                              float dir_x,
                                              float dir_y,
                                              float dir_z,
                                              float grid_x,
                                              float grid_y,
                                              float grid_z,
                                              float origin_x,
                                              float origin_y,
                                              float origin_z,
                                              float mix,
                                              float sharpness,
                                              float min_factor)
{
    if(!t.has_player_pose || mix <= 1e-4f)
    {
        return 1.0f;
    }

    SpatialBasisUtils::BasisVectors basis = SpatialBasisUtils::BuildOrthonormalBasis(t.forward_x,
                                                                                      t.forward_y,
                                                                                      t.forward_z,
                                                                                      t.up_x,
                                                                                      t.up_y,
                                                                                      t.up_z);

    float ldx, ldy, ldz;
    if(!SpatialBasisUtils::NormalizeDirection(dir_x, dir_y, dir_z, ldx, ldy, ldz))
    {
        return 1.0f;
    }
    float lx, ly, lz;
    SpatialBasisUtils::ToLocal(basis, ldx, ldy, ldz, lx, ly, lz);

    float ox, oy, oz;
    if(!ComputeNormalizedSampleVector(grid_x, grid_y, grid_z, origin_x, origin_y, origin_z, ox, oy, oz))
    {
        return 1.0f;
    }
    const float signed_align = std::clamp(ox * lx + oy * ly + oz * lz, -1.0f, 1.0f);
    const float hemi = 0.5f * (signed_align + 1.0f);
    const float shaped = std::pow(std::clamp(hemi, 0.0f, 1.0f), std::max(0.5f, sharpness));
    return (1.0f - mix) + mix * (min_factor + (1.0f - min_factor) * shaped);
}

static void StampPreviewVoxelBox(VoxelRoomCore::VoxelGrid& grid,
                                 float cx,
                                 float cy,
                                 float cz,
                                 float hx,
                                 float hy,
                                 float hz,
                                 RGBColor c,
                                 float alpha)
{
    const unsigned char rr = (unsigned char)(c & 0xFF);
    const unsigned char gg = (unsigned char)((c >> 8) & 0xFF);
    const unsigned char bb = (unsigned char)((c >> 16) & 0xFF);
    const unsigned char aa = (unsigned char)std::clamp((int)std::lround(alpha * 255.0f), 0, 255);
    for(int ix = 0; ix < grid.size_x; ix++)
    {
        const float x = grid.min_x + ((float)ix + 0.5f) * grid.voxel_size;
        if(std::fabs(x - cx) > hx)
        {
            continue;
        }
        for(int iy = 0; iy < grid.size_y; iy++)
        {
            const float y = grid.min_y + ((float)iy + 0.5f) * grid.voxel_size;
            if(std::fabs(y - cy) > hy)
            {
                continue;
            }
            for(int iz = 0; iz < grid.size_z; iz++)
            {
                const float z = grid.min_z + ((float)iz + 0.5f) * grid.voxel_size;
                if(std::fabs(z - cz) > hz)
                {
                    continue;
                }
                const int idx = ((ix * grid.size_y + iy) * grid.size_z + iz) * 4;
                if(idx < 0 || idx + 3 >= (int)grid.rgba.size())
                {
                    continue;
                }
                grid.rgba[(size_t)(idx + 0)] = rr;
                grid.rgba[(size_t)(idx + 1)] = gg;
                grid.rgba[(size_t)(idx + 2)] = bb;
                grid.rgba[(size_t)(idx + 3)] = aa;
            }
        }
    }
}

static const VoxelRoomCore::VoxelGrid& GetPreviewVoxelGrid()
{
    static VoxelRoomCore::VoxelGrid grid;
    static bool initialized = false;
    if(!initialized)
    {
        initialized = true;
        grid.valid = true;
        grid.size_x = 28;
        grid.size_y = 18;
        grid.size_z = 28;
        grid.voxel_size = 0.25f;
        grid.min_x = -3.5f;
        grid.min_y = -0.5f;
        grid.min_z = -3.5f;
        grid.rgba.resize((size_t)grid.size_x * (size_t)grid.size_y * (size_t)grid.size_z * 4u, 0u);

        // Left-side "bed" block (purple), right-side warm source, plus a soft ceiling lamp.
        StampPreviewVoxelBox(grid, -1.35f, 0.30f, -0.15f, 0.55f, 0.20f, 0.90f, (RGBColor)0x00B45CE0, 0.88f);
        StampPreviewVoxelBox(grid, 1.25f, 1.05f, 0.35f, 0.30f, 0.30f, 0.30f, (RGBColor)0x0040B0FF, 0.85f);
        StampPreviewVoxelBox(grid, 0.00f, 1.90f, 0.00f, 0.95f, 0.18f, 0.95f, (RGBColor)0x00FFE8CC, 0.38f);
        StampPreviewVoxelBox(grid, 0.00f, -0.15f, 0.00f, 3.20f, 0.10f, 3.20f, (RGBColor)0x00162A32, 0.22f);
    }
    return grid;
}

static RGBColor RenderVoxelRoomPreviewColor(const GameTelemetryBridge::TelemetrySnapshot& t,
                                            float grid_x,
                                            float grid_y,
                                            float grid_z,
                                            float origin_x,
                                            float origin_y,
                                            float origin_z,
                                            const Settings& s)
{
    VoxelRoomCore::VoxelGrid vg = GetPreviewVoxelGrid();
    if(t.voxel_frame.has_voxel_frame &&
       t.voxel_frame.size_x > 0 &&
       t.voxel_frame.size_y > 0 &&
       t.voxel_frame.size_z > 0)
    {
        vg.valid = true;
        vg.size_x = t.voxel_frame.size_x;
        vg.size_y = t.voxel_frame.size_y;
        vg.size_z = t.voxel_frame.size_z;
        vg.min_x = t.voxel_frame.origin_x;
        vg.min_y = t.voxel_frame.origin_y;
        vg.min_z = t.voxel_frame.origin_z;
        vg.voxel_size = std::max(1e-4f, t.voxel_frame.voxel_size);
        vg.rgba = t.voxel_frame.rgba;
    }
    if(!vg.valid)
    {
        return (RGBColor)0x00000000;
    }

    VoxelRoomCore::Basis basis;
    basis.valid = true;
    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    float fx = 0.0f, fy = 0.0f, fz = 1.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    if(t.has_player_pose)
    {
        MinecraftRoomHorizontalBasis(t.forward_x,
                                     t.forward_y,
                                     t.forward_z,
                                     s.spatial_heading_offset_deg,
                                     ux,
                                     uy,
                                     uz,
                                     fx,
                                     fy,
                                     fz,
                                     rx,
                                     ry,
                                     rz);
    }
    else
    {
        MinecraftRoomHorizontalBasis(0.0f,
                                     0.0f,
                                     1.0f,
                                     s.spatial_heading_offset_deg,
                                     ux,
                                     uy,
                                     uz,
                                     fx,
                                     fy,
                                     fz,
                                     rx,
                                     ry,
                                     rz);
    }
    basis.forward_x = fx;
    basis.forward_y = fy;
    basis.forward_z = fz;
    basis.up_x = ux;
    basis.up_y = uy;
    basis.up_z = uz;

    VoxelRoomCore::RoomSamplePoint sp;
    RoomLedToSpatialCore(grid_x,
                         grid_y,
                         grid_z,
                         origin_x,
                         origin_y,
                         origin_z,
                         sp.room_x,
                         sp.room_y,
                         sp.room_z,
                         sp.origin_x,
                         sp.origin_y,
                         sp.origin_z);

    VoxelRoomCore::MapperSettings ms;
    ms.room_to_world_scale = std::clamp(s.spatial_voxel_room_scale, 0.02f, 0.80f);
    ms.alpha_cutoff = 0.01f;

    bool used = false;
    const float ax = t.has_player_pose ? t.player_x : 0.0f;
    const float ay = t.has_player_pose ? t.player_y : 0.0f;
    const float az = t.has_player_pose ? t.player_z : 0.0f;
    RGBColor out = VoxelRoomCore::ComputeRoomMappedVoxelColor(vg, basis, sp, ax, ay, az, ms, &used);
    if(!used)
    {
        return (RGBColor)0x00000000;
    }
    return out;
}

static RGBColor LayerDebugColorByTopIndex(int top_layer_idx, int layer_count)
{
    if(layer_count <= 3)
    {
        if(top_layer_idx <= 0) return (RGBColor)0x00E0A060; // top
        if(top_layer_idx == 1) return (RGBColor)0x0060E060; // mid
        return (RGBColor)0x0060A0E0; // bottom
    }
    if(top_layer_idx <= 0) return (RGBColor)0x00E0A060; // ceiling
    if(top_layer_idx == 1) return (RGBColor)0x0090D060; // upper wall
    if(top_layer_idx == 2) return (RGBColor)0x0060D0A0; // desk/monitor
    return (RGBColor)0x0060A0E0; // floor
}

static RGBColor RenderSpatialLayerSweepDebug(float time,
                                             const GameTelemetryBridge::TelemetrySnapshot& t,
                                             float grid_x,
                                             float grid_y,
                                             float grid_z,
                                             float origin_x,
                                             float origin_y,
                                             float origin_z,
                                             float y_norm,
                                             const Settings& s,
                                             float floor_end,
                                             float desk_end,
                                             float upper_end)
{
    const int layer_count = ResolveLayerCountForDebug(s);
    const int step_count = layer_count * 9;
    const float hz = std::clamp(s.spatial_debug_sweep_hz, 0.2f, 12.0f);
    const int step = (int)std::floor(std::max(0.0f, time) * hz);
    const int active = (step_count > 0) ? (step % step_count) : 0;
    const int active_top_layer = active / 9;
    const int active_sector = active % 9;
    const int active_layer = (layer_count - 1) - active_top_layer;

    g_spatial_sweep_live.store(1, std::memory_order_relaxed);
    g_spatial_sweep_step.store(active, std::memory_order_relaxed);
    g_spatial_sweep_step_count.store(step_count, std::memory_order_relaxed);
    g_spatial_sweep_layer_count.store(layer_count, std::memory_order_relaxed);
    g_spatial_sweep_top_band.store(active_top_layer, std::memory_order_relaxed);
    g_spatial_sweep_sector.store(active_sector, std::memory_order_relaxed);

    SpatialLayerCore::ProbeInput probe_input;
    probe_input.layered.has_layered = true;
    probe_input.layered.layer_count = layer_count;
    for(int l = 0; l < layer_count; l++)
    {
        for(int sec = 0; sec < 9; sec++)
        {
            probe_input.layered.colors[(size_t)(l * 9 + sec)] = (RGBColor)0x00000000;
        }
    }
    probe_input.layered.colors[(size_t)(active_layer * 9 + active_sector)] = LayerDebugColorByTopIndex(active_top_layer, layer_count);

    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    float fx = 0.0f, fy = 0.0f, fz = 1.0f;
    float rx = 1.0f, ry = 0.0f, rz = 0.0f;
    if(t.has_player_pose)
    {
        MinecraftRoomHorizontalBasis(t.forward_x,
                                     t.forward_y,
                                     t.forward_z,
                                     s.spatial_heading_offset_deg,
                                     ux,
                                     uy,
                                     uz,
                                     fx,
                                     fy,
                                     fz,
                                     rx,
                                     ry,
                                     rz);
    }

    SpatialLayerCore::Basis basis;
    basis.forward_x = fx;
    basis.forward_y = fy;
    basis.forward_z = fz;
    basis.up_x = ux;
    basis.up_y = uy;
    basis.up_z = uz;
    basis.valid = true;

    SpatialLayerCore::MapperSettings map_settings;
    map_settings.profile_mode = (layer_count == 3)
        ? SpatialLayerCore::LayerProfileMode::ThreeLayer
        : SpatialLayerCore::LayerProfileMode::FourLayer;
    map_settings.directional_response = 1.0f;
    map_settings.directional_sharpness = std::max(1.2f, std::clamp(s.world_tint_dir_sharpness, 0.8f, 3.2f));
    map_settings.center_size = std::clamp(s.spatial_center_size, 0.02f, 0.65f);
    map_settings.blend_softness = std::clamp(s.spatial_blend_softness, 0.02f, 0.35f);
    map_settings.floor_end = floor_end;
    map_settings.desk_end = desk_end;
    map_settings.upper_end = upper_end;
    map_settings.compass_azimuth_offset_rad = s.spatial_compass_offset_deg * 0.01745329252f;

    SpatialLayerCore::SamplePoint sample;
    RoomLedToSpatialCore(grid_x,
                         grid_y,
                         grid_z,
                         origin_x,
                         origin_y,
                         origin_z,
                         sample.grid_x,
                         sample.grid_y,
                         sample.grid_z,
                         sample.origin_x,
                         sample.origin_y,
                         sample.origin_z);
    sample.y_norm = y_norm;

    bool used_layered = false;
    RGBColor c = SpatialLayerCore::ComputeProjectedProbeColor(probe_input, basis, sample, map_settings, &used_layered);
    if(!used_layered)
    {
        return (RGBColor)0x00000000;
    }
    return c;
}

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

        auto* vividLab = new QLabel(QStringLiteral("World tint vividness"));
        auto* vividSl = new QSlider(Qt::Horizontal);
        vividSl->setRange(60, 200);
        vividSl->setValue((int)std::lround(std::clamp(s.world_tint_vividness, 0.60f, 2.00f) * 100.0f));
        layout->addWidget(vividLab, row, 0);
        layout->addWidget(vividSl, row, 1);
        QObject::connect(vividSl, &QSlider::valueChanged, panel, [&s](int x) { s.world_tint_vividness = std::clamp(x / 100.0f, 0.60f, 2.00f); });
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

        auto* profileLab = new QLabel(QStringLiteral("Spatial layer profile"));
        auto* profileCombo = new QComboBox();
        profileCombo->addItem(QStringLiteral("Auto"), 0);
        profileCombo->addItem(QStringLiteral("3-layer (floor/mid/ceiling)"), 3);
        profileCombo->addItem(QStringLiteral("4-layer (floor/desk/upper/ceiling)"), 4);
        const int profileIdx = std::max(0, profileCombo->findData(s.spatial_layer_profile_mode));
        profileCombo->setCurrentIndex(profileIdx);
        layout->addWidget(profileLab, row, 0);
        layout->addWidget(profileCombo, row, 1);
        QObject::connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), panel, [&s, profileCombo](int idx) {
            const QVariant v = profileCombo->itemData(idx);
            s.spatial_layer_profile_mode = v.isValid() ? v.toInt() : 0;
        });
        row++;

        auto* mapModeLab = new QLabel(QStringLiteral("Spatial mapping mode"));
        auto* mapModeCombo = new QComboBox();
        mapModeCombo->addItem(QStringLiteral("Classic world tint (MineLights style)"), 2);
        mapModeCombo->addItem(QStringLiteral("Compass directional probes"), 0);
        mapModeCombo->addItem(QStringLiteral("Voxel room mapping (core preview)"), 1);
        const QString mapModeTip = QStringLiteral(
            "Choose how ambient world tint is projected: Classic = stable sky/mid/ground layers, "
            "Compass = directional layered probes around horizontal player yaw (matches Room heading offset), "
            "Voxel = sample an RGBA volume: built-in preview boxes, or live frames from UDP "
            "(voxel_size_*, voxel_origin_* corner, voxel_cell_size, voxel_rgba x-major).");
        mapModeLab->setToolTip(mapModeTip);
        mapModeCombo->setToolTip(mapModeTip);
        const int mapModeIdx = std::max(0, mapModeCombo->findData(s.spatial_mapping_mode));
        mapModeCombo->setCurrentIndex(mapModeIdx);
        layout->addWidget(mapModeLab, row, 0);
        layout->addWidget(mapModeCombo, row, 1);
        QObject::connect(mapModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), panel, [&s, mapModeCombo](int idx) {
            const QVariant v = mapModeCombo->itemData(idx);
            s.spatial_mapping_mode = v.isValid() ? v.toInt() : 0;
        });
        row++;

        auto* centerLab = new QLabel(QStringLiteral("Center sector size"));
        auto* centerSl = new QSlider(Qt::Horizontal);
        centerSl->setRange(2, 65);
        centerSl->setValue((int)std::lround(std::clamp(s.spatial_center_size, 0.02f, 0.65f) * 100.0f));
        layout->addWidget(centerLab, row, 0);
        layout->addWidget(centerSl, row, 1);
        QObject::connect(centerSl, &QSlider::valueChanged, panel, [&s](int x) { s.spatial_center_size = std::clamp(x / 100.0f, 0.02f, 0.65f); });
        row++;

        auto* softLab = new QLabel(QStringLiteral("Layer blend softness"));
        auto* softSl = new QSlider(Qt::Horizontal);
        softSl->setRange(2, 35);
        softSl->setValue((int)std::lround(std::clamp(s.spatial_blend_softness, 0.02f, 0.35f) * 100.0f));
        layout->addWidget(softLab, row, 0);
        layout->addWidget(softSl, row, 1);
        QObject::connect(softSl, &QSlider::valueChanged, panel, [&s](int x) { s.spatial_blend_softness = std::clamp(x / 100.0f, 0.02f, 0.35f); });
        row++;

        auto* headingLab = new QLabel(QStringLiteral("Room heading offset (deg)"));
        auto* headingSl = new QSlider(Qt::Horizontal);
        headingSl->setRange(-180, 180);
        headingSl->setValue((int)std::lround(std::clamp(s.spatial_heading_offset_deg, -180.0f, 180.0f)));
        layout->addWidget(headingLab, row, 0);
        layout->addWidget(headingSl, row, 1);
        QObject::connect(headingSl, &QSlider::valueChanged, panel, [&s](int x) { s.spatial_heading_offset_deg = std::clamp((float)x, -180.0f, 180.0f); });
        row++;

        auto* compassLab = new QLabel(QStringLiteral("Compass sector offset (deg)"));
        auto* compassSl = new QSlider(Qt::Horizontal);
        compassSl->setRange(-180, 180);
        compassSl->setValue((int)std::lround(std::clamp(s.spatial_compass_offset_deg, -180.0f, 180.0f)));
        const QString compassTip = QStringLiteral(
            "Rotates which direction maps to layered probe N, NE, … (independent of room heading). "
            "Default −45° lines North up with the room front wall for the usual 8-sector order.");
        compassLab->setToolTip(compassTip);
        compassSl->setToolTip(compassTip);
        layout->addWidget(compassLab, row, 0);
        layout->addWidget(compassSl, row, 1);
        QObject::connect(compassSl, &QSlider::valueChanged, panel, [&s](int x) {
            s.spatial_compass_offset_deg = std::clamp((float)x, -180.0f, 180.0f);
        });
        row++;

        auto* voxScaleLab = new QLabel(QStringLiteral("Voxel room scale"));
        auto* voxScaleSl = new QSlider(Qt::Horizontal);
        voxScaleSl->setRange(2, 80); // 0.02 .. 0.80 world units per room unit
        voxScaleSl->setValue((int)std::lround(std::clamp(s.spatial_voxel_room_scale, 0.02f, 0.80f) * 100.0f));
        const QString voxScaleTip = QStringLiteral(
            "How large the room maps into voxel world space (LED offset → world ray into the volume). "
            "Tune with live frames if the mod sends voxel_origin_* as the grid corner in the same units as player position.");
        voxScaleLab->setToolTip(voxScaleTip);
        voxScaleSl->setToolTip(voxScaleTip);
        layout->addWidget(voxScaleLab, row, 0);
        layout->addWidget(voxScaleSl, row, 1);
        QObject::connect(voxScaleSl, &QSlider::valueChanged, panel, [&s](int x) {
            s.spatial_voxel_room_scale = std::clamp(x / 100.0f, 0.02f, 0.80f);
        });
        row++;

        auto* voxMixLab = new QLabel(QStringLiteral("Voxel room mix"));
        auto* voxMixSl = new QSlider(Qt::Horizontal);
        voxMixSl->setRange(0, 100);
        voxMixSl->setValue((int)std::lround(std::clamp(s.spatial_voxel_mix, 0.0f, 1.0f) * 100.0f));
        const QString voxMixTip = QStringLiteral(
            "Blend live voxel RGBA into ambient tint. Requires UDP events with voxel_size_x/y/z, voxel_origin_x/y/z, "
            "voxel_cell_size, and voxel_rgba byte array in x-major order ((x*sy+y)*sz+z)*4 — see GameTelemetryBridge.");
        voxMixLab->setToolTip(voxMixTip);
        voxMixSl->setToolTip(voxMixTip);
        layout->addWidget(voxMixLab, row, 0);
        layout->addWidget(voxMixSl, row, 1);
        QObject::connect(voxMixSl, &QSlider::valueChanged, panel, [&s](int x) {
            s.spatial_voxel_mix = std::clamp(x / 100.0f, 0.0f, 1.0f);
        });
        row++;

        QCheckBox* dbgSweepToggle = new QCheckBox("Debug layered sweep (step through compass cells)");
        dbgSweepToggle->setChecked(s.spatial_debug_sweep_enabled);
        layout->addWidget(dbgSweepToggle, row++, 0, 1, 2);
        QObject::connect(dbgSweepToggle, &QCheckBox::toggled, panel, [&s](bool v) { s.spatial_debug_sweep_enabled = v; });

        auto* dbgSweepStatus = new QLabel(QStringLiteral("Sweep status: —"));
        dbgSweepStatus->setWordWrap(true);
        layout->addWidget(dbgSweepStatus, row++, 0, 1, 2);

        auto* dbgSweepTimer = new QTimer(panel);
        dbgSweepTimer->setInterval(120);
        QObject::connect(dbgSweepTimer, &QTimer::timeout, panel, [dbgSweepStatus, &s]() {
            static const char* kSec[9] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW", "C"};
            if(!s.spatial_debug_sweep_enabled)
            {
                dbgSweepStatus->setText(QStringLiteral("Sweep status: off"));
                return;
            }
            if(!s.enable_ambient_world_tint)
            {
                dbgSweepStatus->setText(QStringLiteral("Sweep status: enable Ambient World Tint to run"));
                return;
            }
            if(g_spatial_sweep_live.load(std::memory_order_relaxed) == 0)
            {
                dbgSweepStatus->setText(QStringLiteral("Sweep status: waiting for render…"));
                return;
            }
            const int lc = g_spatial_sweep_layer_count.load(std::memory_order_relaxed);
            const int top = g_spatial_sweep_top_band.load(std::memory_order_relaxed);
            const int sec = g_spatial_sweep_sector.load(std::memory_order_relaxed);
            const int step = g_spatial_sweep_step.load(std::memory_order_relaxed);
            const int sc = g_spatial_sweep_step_count.load(std::memory_order_relaxed);
            QString band;
            if(lc <= 3)
            {
                if(top <= 0)
                {
                    band = QStringLiteral("Top");
                }
                else if(top == 1)
                {
                    band = QStringLiteral("Mid");
                }
                else
                {
                    band = QStringLiteral("Bottom");
                }
            }
            else
            {
                if(top <= 0)
                {
                    band = QStringLiteral("Ceiling");
                }
                else if(top == 1)
                {
                    band = QStringLiteral("Upper");
                }
                else if(top == 2)
                {
                    band = QStringLiteral("Desk");
                }
                else
                {
                    band = QStringLiteral("Floor");
                }
            }
            const char* sn = (sec >= 0 && sec < 9) ? kSec[sec] : "?";
            dbgSweepStatus->setText(QStringLiteral("Sweep: %1 / %2 — %3 — %4")
                                        .arg(step + 1)
                                        .arg(sc)
                                        .arg(band)
                                        .arg(QString::fromUtf8(sn)));
        });
        dbgSweepTimer->start();

        auto* dbgHzLab = new QLabel(QStringLiteral("Debug sweep speed (cells/sec)"));
        auto* dbgHzSl = new QSlider(Qt::Horizontal);
        dbgHzSl->setRange(2, 120); // 0.2 .. 12.0
        dbgHzSl->setValue((int)std::lround(std::clamp(s.spatial_debug_sweep_hz, 0.2f, 12.0f) * 10.0f));
        layout->addWidget(dbgHzLab, row, 0);
        layout->addWidget(dbgHzSl, row, 1);
        QObject::connect(dbgHzSl, &QSlider::valueChanged, panel, [&s](int x) {
            s.spatial_debug_sweep_hz = std::clamp(x / 10.0f, 0.2f, 12.0f);
        });
        row++;

        auto* floorOffLab = new QLabel(QStringLiteral("Auto floor boundary offset"));
        auto* floorOffSl = new QSlider(Qt::Horizontal);
        floorOffSl->setRange(-30, 30);
        floorOffSl->setValue((int)std::lround(std::clamp(s.spatial_floor_offset, -0.30f, 0.30f) * 100.0f));
        layout->addWidget(floorOffLab, row, 0);
        layout->addWidget(floorOffSl, row, 1);
        QObject::connect(floorOffSl, &QSlider::valueChanged, panel, [&s](int x) { s.spatial_floor_offset = std::clamp(x / 100.0f, -0.30f, 0.30f); });
        row++;

        auto* deskOffLab = new QLabel(QStringLiteral("Auto desk boundary offset"));
        auto* deskOffSl = new QSlider(Qt::Horizontal);
        deskOffSl->setRange(-30, 30);
        deskOffSl->setValue((int)std::lround(std::clamp(s.spatial_desk_offset, -0.30f, 0.30f) * 100.0f));
        layout->addWidget(deskOffLab, row, 0);
        layout->addWidget(deskOffSl, row, 1);
        QObject::connect(deskOffSl, &QSlider::valueChanged, panel, [&s](int x) { s.spatial_desk_offset = std::clamp(x / 100.0f, -0.30f, 0.30f); });
        row++;

        auto* upperOffLab = new QLabel(QStringLiteral("Auto upper boundary offset"));
        auto* upperOffSl = new QSlider(Qt::Horizontal);
        upperOffSl->setRange(-30, 30);
        upperOffSl->setValue((int)std::lround(std::clamp(s.spatial_upper_offset, -0.30f, 0.30f) * 100.0f));
        layout->addWidget(upperOffLab, row, 0);
        layout->addWidget(upperOffSl, row, 1);
        QObject::connect(upperOffSl, &QSlider::valueChanged, panel, [&s](int x) { s.spatial_upper_offset = std::clamp(x / 100.0f, -0.30f, 0.30f); });
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

        addPctSlider(QStringLiteral("Lightning directional response"), &s.lightning_directional_mix);
        auto* lDirSharpLab = new QLabel(QStringLiteral("Lightning directional sharpness"));
        auto* lDirSharpSl = new QSlider(Qt::Horizontal);
        lDirSharpSl->setRange(50, 500);
        lDirSharpSl->setValue((int)std::lround(std::clamp(s.lightning_dir_sharpness, 0.5f, 5.0f) * 100.0f));
        layout->addWidget(lDirSharpLab, row, 0);
        layout->addWidget(lDirSharpSl, row, 1);
        QObject::connect(lDirSharpSl, &QSlider::valueChanged, panel, [&s](int x) { s.lightning_dir_sharpness = std::clamp(x / 100.0f, 0.5f, 5.0f); });
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

QWidget* CreateEffectWidget(QWidget* parent,
                            const QString& title,
                            Settings& settings,
                            std::uint32_t channels,
                            QWidget* telemetry_owner,
                            const std::function<void()>& on_changed)
{
    QGroupBox* w = new QGroupBox(title, parent);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);

    QWidget* settings_widget = CreateSettingsWidget(w, settings, channels);
    if(settings_widget)
    {
        layout->addWidget(settings_widget);
        WireChildWidgetsToParametersChanged(settings_widget, on_changed);
    }

    layout->addWidget(new GameTelemetryStatusPanel(telemetry_owner));
    return w;
}

static RGBColor ApplyDamageFlashChannel(RGBColor in_color,
                                        const GameTelemetryBridge::TelemetrySnapshot& t,
                                        float grid_x,
                                        float grid_y,
                                        float grid_z,
                                        float origin_x,
                                        float origin_y,
                                        float origin_z,
                                        const Settings& s)
{
    if(!s.enable_damage_flash || !t.has_damage_event || t.damage_received_ms == 0)
    {
        return in_color;
    }
    const unsigned long long now = NowMs();
    const unsigned long long elapsed_ms = (now > t.damage_received_ms) ? (now - t.damage_received_ms) : 0;
    const float decay_ms = std::max(100.0f, s.damage_flash_decay_s * 1000.0f);
    const float damage_t = std::clamp(1.0f - (elapsed_ms / decay_ms), 0.0f, 1.0f);
    if(damage_t <= 0.0f)
    {
        return in_color;
    }
    const float damage_strength = std::clamp(t.damage_amount / 20.0f, 0.0f, 1.0f);
    float flash_mix = std::clamp(s.damage_flash_strength * damage_t * (0.2f + 0.8f * damage_strength), 0.0f, 1.0f);
    if(s.damage_directional_mix > 1e-4f && t.has_player_pose)
    {
        const float dir_factor = ComputeDirectionalFactorFullPose(t,
                                                                  t.damage_dir_x,
                                                                  t.damage_dir_y,
                                                                  t.damage_dir_z,
                                                                  grid_x,
                                                                  grid_y,
                                                                  grid_z,
                                                                  origin_x,
                                                                  origin_y,
                                                                  origin_z,
                                                                  std::clamp(s.damage_directional_mix, 0.0f, 1.0f),
                                                                  s.damage_dir_sharpness,
                                                                  0.10f);
        flash_mix = std::clamp(flash_mix * dir_factor, 0.0f, 1.0f);
    }
    return LerpColor(in_color, (RGBColor)0x000000FF, flash_mix);
}

static RGBColor ApplyLightningFlashChannel(RGBColor in_color,
                                           const GameTelemetryBridge::TelemetrySnapshot& t,
                                           float grid_x,
                                           float grid_y,
                                           float grid_z,
                                           float origin_x,
                                           float origin_y,
                                           float origin_z,
                                           const GridContext3D& grid,
                                           const Settings& s)
{
    if(!s.enable_lightning_flash || !t.has_lightning_event || t.lightning_received_ms == 0)
    {
        return in_color;
    }
    const unsigned long long now = NowMs();
    const unsigned long long elapsed_ms = (now > t.lightning_received_ms) ? (now - t.lightning_received_ms) : 0;
    const float decay_ms = std::max(80.0f, s.lightning_flash_decay_s * 1000.0f);
    const float lt = std::clamp(1.0f - (elapsed_ms / decay_ms), 0.0f, 1.0f);
    if(lt <= 0.0f)
    {
        return in_color;
    }

    const float y_norm = NormalizeRoomVertical01(grid_y, grid, origin_y);
    const float skyBias = std::pow(y_norm, 1.9f);
    const float layer = 0.20f + 0.80f * skyBias;
    float st = std::clamp(s.lightning_flash_strength * t.lightning_strength * lt * layer, 0.0f, 1.0f);
    if(s.lightning_directional_mix > 1e-4f && t.has_player_pose)
    {
        const float dir_mix = std::clamp(s.lightning_directional_mix, 0.0f, 1.0f) *
                              (0.25f + 0.75f * std::clamp(t.lightning_dir_focus, 0.0f, 1.0f));
        const float dir_factor = ComputeDirectionalFactorFullPose(t,
                                                                  t.lightning_dir_x,
                                                                  t.lightning_dir_y,
                                                                  t.lightning_dir_z,
                                                                  grid_x,
                                                                  grid_y,
                                                                  grid_z,
                                                                  origin_x,
                                                                  origin_y,
                                                                  origin_z,
                                                                  dir_mix,
                                                                  s.lightning_dir_sharpness,
                                                                  0.20f);
        st = std::clamp(st * dir_factor, 0.0f, 1.0f);
    }
    return LerpColor(in_color, (RGBColor)0x00FFF0DE, st);
}

RGBColor RenderColor(const GameTelemetryBridge::TelemetrySnapshot& t,
                     float time,
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

    if(ch(channels, ChWorldTint) && s.enable_ambient_world_tint && world_smooth != nullptr)
    {
        const SpatialMappingMode mapping_mode = ResolveSpatialMappingMode(s);
        const float y_norm_raw = NormalizeRoomVertical01(grid_y, grid, origin_y);
        const int bin_count = (int)tls_layer_state.y_hist.size();
        const int bin = std::clamp((int)(std::clamp(y_norm_raw, 0.0f, 1.0f) * (float)bin_count), 0, bin_count - 1);
        tls_layer_state.y_hist[(size_t)bin]++;
        tls_layer_state.y_samples++;
        const float gamma = tls_layer_state.has_gamma ? tls_layer_state.gamma : 1.0f;
        const float y_norm = std::pow(std::clamp(y_norm_raw, 0.0f, 1.0f), gamma);

        float floor_end_dbg = tls_layer_state.has_auto_bounds
            ? tls_layer_state.auto_floor_end
            : s.tint_layer_ground_end;
        float desk_end_dbg = tls_layer_state.has_auto_bounds
            ? tls_layer_state.auto_desk_end
            : (s.tint_layer_ground_end + s.tint_layer_sky_start) * 0.5f;
        float upper_end_dbg = tls_layer_state.has_auto_bounds
            ? tls_layer_state.auto_upper_end
            : s.tint_layer_sky_start;
        floor_end_dbg = std::clamp(floor_end_dbg + s.spatial_floor_offset, 0.08f, 0.52f);
        desk_end_dbg = std::clamp(desk_end_dbg + s.spatial_desk_offset, floor_end_dbg + 0.06f, 0.88f);
        upper_end_dbg = std::clamp(upper_end_dbg + s.spatial_upper_offset, desk_end_dbg + 0.06f, 0.95f);

        if(!s.spatial_debug_sweep_enabled)
        {
            g_spatial_sweep_live.store(0, std::memory_order_relaxed);
        }

        if(s.spatial_debug_sweep_enabled)
        {
            out = RenderSpatialLayerSweepDebug(time,
                                               t,
                                               grid_x,
                                               grid_y,
                                               grid_z,
                                               origin_x,
                                               origin_y,
                                               origin_z,
                                               y_norm,
                                               s,
                                               floor_end_dbg,
                                               desk_end_dbg,
                                               upper_end_dbg);
            const int dr = std::clamp((int)((out & 0xFF) * s.base_brightness), 0, 255);
            const int dg = std::clamp((int)(((out >> 8) & 0xFF) * s.base_brightness), 0, 255);
            const int db = std::clamp((int)(((out >> 16) & 0xFF) * s.base_brightness), 0, 255);
            return (RGBColor)((db << 16) | (dg << 8) | dr);
        }

        switch(mapping_mode)
        {
        case SpatialMappingMode::Voxel:
        {
            RGBColor voxel_c = RenderVoxelRoomPreviewColor(t,
                                                           grid_x,
                                                           grid_y,
                                                           grid_z,
                                                           origin_x,
                                                           origin_y,
                                                           origin_z,
                                                           s);
            if(voxel_c != (RGBColor)0x00000000)
            {
                out = LerpColor(out, voxel_c, std::clamp(s.spatial_voxel_mix, 0.0f, 1.0f));
            }
            break;
        }
        case SpatialMappingMode::Classic:
        case SpatialMappingMode::Compass:
            if(t.has_world_light)
            {
                RGBColor wl = SuppressWhites(MakeRgb(t.world_light_r, t.world_light_g, t.world_light_b));
                if(t.has_world_layers)
                {
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
            const float vivid = std::clamp(s.world_tint_vividness, 0.60f, 2.00f);
            sky = ApplyVividness(sky, vivid);
            mid = ApplyVividness(mid, vivid);
            ground = ApplyVividness(ground, vivid);
            wl = ApplyVividness(wl, vivid);
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
            // Keep ceiling LEDs reading as "sky" even when directional/world-light mixes push
            // the palette toward ground/mid hues.
            if(t.has_vanilla_biome_colors)
            {
                RGBColor bioSky = SuppressWhites(MakeRgb(t.biome_sky_r, t.biome_sky_g, t.biome_sky_b));
                const float sky_top = std::clamp((y_norm - (sStart - 0.03f)) / std::max(1e-3f, 1.0f - (sStart - 0.03f)), 0.0f, 1.0f);
                const float sky_focus = sky_top * sky_top;
                const float sky_keep = std::clamp((0.25f + 0.75f * s.biome_sky_overlay) * sky_focus, 0.0f, 0.85f);
                projected = LerpColor(projected, bioSky, sky_keep);
            }
            if(mapping_mode == SpatialMappingMode::Compass && t.has_layered_world_probes && t.has_player_pose)
            {
                SpatialLayerCore::ProbeInput probe_input = BuildProbeInput(t);
                float ux = 0.0f;
                float uy = 1.0f;
                float uz = 0.0f;
                float fx = 0.0f;
                float fy = 0.0f;
                float fz = 1.0f;
                float rx = 1.0f;
                float ry = 0.0f;
                float rz = 0.0f;
                MinecraftRoomHorizontalBasis(t.forward_x,
                                             t.forward_y,
                                             t.forward_z,
                                             s.spatial_heading_offset_deg,
                                             ux,
                                             uy,
                                             uz,
                                             fx,
                                             fy,
                                             fz,
                                             rx,
                                             ry,
                                             rz);
                SpatialLayerCore::Basis basis;
                basis.forward_x = fx;
                basis.forward_y = fy;
                basis.forward_z = fz;
                basis.up_x = ux;
                basis.up_y = uy;
                basis.up_z = uz;
                basis.valid = true;

                float floor_end = t.has_layered_world_probes && tls_layer_state.has_auto_bounds
                    ? tls_layer_state.auto_floor_end
                    : s.tint_layer_ground_end;
                float desk_end = tls_layer_state.has_auto_bounds
                    ? tls_layer_state.auto_desk_end
                    : (s.tint_layer_ground_end + s.tint_layer_sky_start) * 0.5f;
                float upper_end = t.has_layered_world_probes && tls_layer_state.has_auto_bounds
                    ? tls_layer_state.auto_upper_end
                    : s.tint_layer_sky_start;
                floor_end = std::clamp(floor_end + s.spatial_floor_offset, 0.08f, 0.52f);
                desk_end = std::clamp(desk_end + s.spatial_desk_offset, floor_end + 0.06f, 0.88f);
                upper_end = std::clamp(upper_end + s.spatial_upper_offset, desk_end + 0.06f, 0.95f);

                SpatialLayerCore::MapperSettings map_settings;
                map_settings.profile_mode = ResolveLayerProfileMode(s);
                map_settings.directional_response = std::clamp(s.world_tint_directional, 0.0f, 1.0f);
                map_settings.directional_sharpness = std::clamp(s.world_tint_dir_sharpness, 0.8f, 3.2f);
                map_settings.center_size = std::clamp(s.spatial_center_size, 0.02f, 0.65f);
                map_settings.blend_softness = std::clamp(s.spatial_blend_softness, 0.02f, 0.35f);
                map_settings.floor_end = floor_end;
                map_settings.desk_end = desk_end;
                map_settings.upper_end = upper_end;
                map_settings.compass_azimuth_offset_rad = s.spatial_compass_offset_deg * 0.01745329252f;

                SpatialLayerCore::SamplePoint sample;
                RoomLedToSpatialCore(grid_x,
                                     grid_y,
                                     grid_z,
                                     origin_x,
                                     origin_y,
                                     origin_z,
                                     sample.grid_x,
                                     sample.grid_y,
                                     sample.grid_z,
                                     sample.origin_x,
                                     sample.origin_y,
                                     sample.origin_z);
                sample.y_norm = y_norm;

                bool used_layered = false;
                RGBColor probe_projected = SpatialLayerCore::ComputeProjectedProbeColor(probe_input,
                                                                                         basis,
                                                                                         sample,
                                                                                         map_settings,
                                                                                         &used_layered);
                if(probe_input.layered.has_layered)
                {
                    probe_projected = ApplyVividness(SuppressWhites(probe_projected), vivid);
                    const float layered_bonus = used_layered ? 0.08f : 0.0f;
                    const float probe_mix = std::clamp(0.20f + 0.60f * s.world_tint_directional + layered_bonus, 0.0f, 0.88f);
                    projected = LerpColor(projected, probe_projected, probe_mix);
                }
            }
            const float wi_disp = std::clamp(t.world_light_intensity, 0.0f, 1.0f);
            const float dark_gate = std::clamp((wi_disp - 0.035f) / 0.215f, 0.0f, 1.0f);
            const float layer_mix = std::clamp((0.10f + 0.80f * s.world_light_mix) * wi_disp * dark_gate, 0.0f, 1.0f);
            float dirMul = 1.0f;
            if(mapping_mode == SpatialMappingMode::Compass &&
               t.world_light_focus > 1e-4f &&
               s.world_tint_directional > 1e-4f)
            {
                const float focus = std::clamp(t.world_light_focus, 0.0f, 1.0f);
                const float dm = std::clamp(s.world_tint_directional * (0.25f + 0.75f * focus), 0.0f, 1.0f);
                dirMul = ComputeDirectionalFactorHorizontalPose(t,
                                                                t.world_light_dir_x,
                                                                t.world_light_dir_y,
                                                                t.world_light_dir_z,
                                                                grid_x,
                                                                grid_y,
                                                                grid_z,
                                                                origin_x,
                                                                origin_y,
                                                                origin_z,
                                                                s.spatial_heading_offset_deg,
                                                                dm,
                                                                s.world_tint_dir_sharpness,
                                                                0.30f);
            }
            out = LerpColor(out, projected, std::clamp(layer_mix * dirMul, 0.0f, 1.0f));
            wl = LerpColor(wl, projected, 0.6f);
        }
            const float wi_disp2 = std::clamp(t.world_light_intensity, 0.0f, 1.0f);
            const float dark_gate2 = std::clamp((wi_disp2 - 0.035f) / 0.215f, 0.0f, 1.0f);
            const float wl_mix = std::clamp((0.05f + 0.35f * s.world_light_mix) * wi_disp2 * dark_gate2, 0.0f, 0.40f);
                out = LerpColor(out, wl, wl_mix);
            }
            break;
        default:
            break;
        }
    }

    if(ch(channels, ChDamage))
    {
        out = ApplyDamageFlashChannel(out, t, grid_x, grid_y, grid_z, origin_x, origin_y, origin_z, s);
    }

    if(ch(channels, ChLightning))
    {
        out = ApplyLightningFlashChannel(out, t, grid_x, grid_y, grid_z, origin_x, origin_y, origin_z, grid, s);
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

    if(effect->voxel_volume_group)
    {
        effect->voxel_volume_group->setVisible(false);
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
