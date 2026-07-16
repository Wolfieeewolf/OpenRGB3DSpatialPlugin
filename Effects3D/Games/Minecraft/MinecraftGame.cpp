// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGame.h"
#include "MinecraftGameSettings.h"
#include "GameTelemetryStatusPanel.h"
#include "SpatialBasisUtils.h"
#include "SpatialEffect3D.h"
#include "RoomSampleMapping.h"
#include "Game/RoomSampleConfigPublisher.h"
#include "RoomSampleFrameShmReader.h"
#include "Game/GameTelemetryBridge.h"
#include "GridSpaceUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include "EffectUiRows.h"
#include "ui_MinecraftGameSettingsScroll.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>

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

static void AddSliderRow(QVBoxLayout* layout,
                         QWidget* panel,
                         const QString& caption,
                         int min,
                         int max,
                         int value,
                         const std::function<void(int)>& apply,
                         const std::function<QString(int)>& format,
                         const QString& tooltip = QString())
{
    EffectSliderRow* row = EffectUiRows::AppendSliderRow(layout, caption, min, max, value, tooltip);
    if(row)
    {
        row->bindValueChanged(panel, apply, format);
    }
}

static void AddPctSlider(QVBoxLayout* layout, QWidget* panel, const QString& caption, float* v)
{
    AddSliderRow(layout,
                 panel,
                 caption,
                 0,
                 100,
                 (int)std::lround(std::clamp(*v, 0.0f, 1.0f) * 100.0f),
                 [v](int x) { *v = std::clamp(x / 100.0f, 0.0f, 1.0f); },
                 [](int x) { return QString::number(x) + QStringLiteral("%"); });
}

static QComboBox* AddComboRow(QVBoxLayout* layout, const QString& caption)
{
    EffectLabeledComboRow* row = EffectUiRows::AppendComboRow(layout, caption);
    return row ? row->combo() : nullptr;
}

static void AddSpinRow(QVBoxLayout* layout,
                       QWidget* panel,
                       const QString& caption,
                       int min,
                       int max,
                       int value,
                       const std::function<void(int)>& apply,
                       const QString& tooltip = QString())
{
    EffectLabeledSpinRow* row = EffectUiRows::AppendSpinRow(layout, caption, min, max, value, tooltip);
    if(row)
    {
        row->bindValueChanged(panel, apply);
    }
}

static QString McLabel(const char* utf8)
{
    return QString::fromUtf8(utf8);
}

static void AddCheckRow(QVBoxLayout* layout,
                        QWidget* panel,
                        const QString& text,
                        bool checked,
                        const std::function<void(bool)>& apply,
                        const QString& tooltip = QString())
{
    EffectCheckRow* row = EffectUiRows::AppendCheckRow(layout, text, checked, tooltip);
    if(row)
    {
        row->bindToggled(panel, apply);
    }
}

static void AddCheckRow(QVBoxLayout* layout,
                        QWidget* panel,
                        const char* utf8_text,
                        bool checked,
                        const std::function<void(bool)>& apply,
                        const QString& tooltip = QString())
{
    AddCheckRow(layout, panel, McLabel(utf8_text), checked, apply, tooltip);
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

static bool ch(std::uint32_t mask, std::uint32_t bit) { return (mask & bit) != 0u; }

static float ComputeRoomToWorldScale(const GridContext3D& grid, float blocks_per_m, float scale_tune)
{
    const float bpm = std::max(0.05f, blocks_per_m);
    const float mm_per_block = 1000.0f / bpm;
    const float grid_mm = SafeGridScaleMm(grid.grid_scale_mm);
    const float grid_units_per_block = mm_per_block / grid_mm;
    if(grid_units_per_block < 1e-3f)
    {
        return 0.18f;
    }
    return std::clamp(scale_tune, 0.1f, 6.0f) / grid_units_per_block;
}

const GameTelemetryBridge::TelemetrySnapshot& PrepareRenderFrame(const GridContext3D& grid,
                                                                 const Settings& settings,
                                                                 std::uint32_t channels,
                                                                 float origin_x,
                                                                 float origin_y,
                                                                 float origin_z)
{
    struct FrameState
    {
        std::uint64_t prepared_sequence = UINT64_MAX;
        bool room_config_published = false;
    };
    static thread_local FrameState state;
    static thread_local GameTelemetryBridge::TelemetrySnapshot snapshot;

    const bool preview_pass = (grid.render_sequence == 0);
    const bool new_frame = preview_pass || state.prepared_sequence != grid.render_sequence;

    if(new_frame)
    {
        RoomSampleFrameShmReader::TryApplyLatest();
        if(!preview_pass)
        {
            state.prepared_sequence = grid.render_sequence;
        }
        state.room_config_published = false;
    }

    snapshot = GameTelemetryBridge::GetTelemetrySnapshot();

    if(ch(channels, ChRoomVrTint) && !state.room_config_published)
    {
        const float blocks_per_m =
            snapshot.has_player_blocks_per_m ? snapshot.player_blocks_per_m : 1.0f;
        const float room_scale = ComputeRoomToWorldScale(grid, blocks_per_m, settings.room_vr_scale_tune);
        RoomSampleConfigPublisher::PublishIfNeeded(grid, settings, origin_x, origin_y, origin_z, room_scale);
        state.room_config_published = true;
    }

    return snapshot;
}

static RGBColor EnhanceRoomVrColor(RGBColor c, float saturation, float contrast)
{
    float r = (float)(c & 0xFF) / 255.0f;
    float g = (float)((c >> 8) & 0xFF) / 255.0f;
    float b = (float)((c >> 16) & 0xFF) / 255.0f;
    const float gray = (r + g + b) / 3.0f;
    r = gray + (r - gray) * saturation;
    g = gray + (g - gray) * saturation;
    b = gray + (b - gray) * saturation;
    r = (r - 0.5f) * contrast + 0.5f;
    g = (g - 0.5f) * contrast + 0.5f;
    b = (b - 0.5f) * contrast + 0.5f;
    return MakeRgb((unsigned char)std::clamp((int)std::lround(r * 255.0f), 0, 255),
                   (unsigned char)std::clamp((int)std::lround(g * 255.0f), 0, 255),
                   (unsigned char)std::clamp((int)std::lround(b * 255.0f), 0, 255));
}

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

/** When a controller has no room Y span (flat strip), drive sky/mid/ground along its longest horizontal axis. */
static inline float ResolveTintLayerNorm(float grid_x,
                                         float grid_z,
                                         const GridContext3D& grid,
                                         float y_norm)
{
    const float y_span = std::fabs(grid.max_y - grid.min_y);
    if(y_span > 1e-4f)
    {
        return y_norm;
    }
    const float z_span = std::fabs(grid.max_z - grid.min_z);
    const float x_span = std::fabs(grid.max_x - grid.min_x);
    if(z_span >= x_span && z_span > 1e-4f)
    {
        return std::clamp((grid_z - grid.min_z) / z_span, 0.0f, 1.0f);
    }
    if(x_span > 1e-4f)
    {
        return std::clamp((grid_x - grid.min_x) / x_span, 0.0f, 1.0f);
    }
    return y_norm;
}

static RGBColor SampleWorldProbeCell(const GameTelemetryBridge::TelemetrySnapshot& t, int layer, int sector)
{
    const int cell = layer * 9 + sector;
    const int idx = cell * 3;
    if(idx + 2 >= (int)t.layered_probe_rgb.size())
    {
        return (RGBColor)0;
    }
    return MakeRgb(t.layered_probe_rgb[(size_t)idx],
                   t.layered_probe_rgb[(size_t)idx + 1],
                   t.layered_probe_rgb[(size_t)idx + 2]);
}

static RGBColor SampleVerticalWorldLayers(const GameTelemetryBridge::TelemetrySnapshot& t, float layer_norm)
{
    if(!t.has_world_layers)
    {
        return MakeRgb(t.world_light_r, t.world_light_g, t.world_light_b);
    }
    constexpr float kGroundEnd = 0.30f;
    constexpr float kSkyStart = 0.68f;
    const RGBColor sky = MakeRgb(t.world_sky_r, t.world_sky_g, t.world_sky_b);
    const RGBColor mid = MakeRgb(t.world_mid_r, t.world_mid_g, t.world_mid_b);
    const RGBColor ground = MakeRgb(t.world_ground_r, t.world_ground_g, t.world_ground_b);
    if(layer_norm < kGroundEnd)
    {
        return LerpColor(ground, mid, layer_norm / kGroundEnd);
    }
    if(layer_norm > kSkyStart)
    {
        const float denom = std::max(1e-3f, 1.0f - kSkyStart);
        return LerpColor(mid, sky, (layer_norm - kSkyStart) / denom);
    }
    return mid;
}

/** MineLights-style 3D tint: bilinear sample of 4x9 directional world probes per room LED. */
static RGBColor SampleLayeredWorldProbe(const GameTelemetryBridge::TelemetrySnapshot& t,
                                        float grid_x,
                                        float grid_y,
                                        float grid_z,
                                        float origin_x,
                                        float origin_y,
                                        float origin_z,
                                        const GridContext3D& grid,
                                        float heading_offset_deg)
{
    if(!t.has_layered_world_probes)
    {
        return (RGBColor)0;
    }

    const float y_norm = ResolveTintLayerNorm(grid_x,
                                              grid_z,
                                              grid,
                                              NormalizeRoomVertical01(grid_y, grid, origin_y));
    const float layer_f = std::clamp(y_norm * 3.0f, 0.0f, 3.0f);
    const int layer0 = std::clamp((int)std::floor(layer_f), 0, 3);
    const int layer1 = std::min(layer0 + 1, 3);
    const float layer_t = layer_f - (float)layer0;

    auto sample_vertical_center = [&]() {
        return LerpColor(SampleWorldProbeCell(t, layer0, 8), SampleWorldProbeCell(t, layer1, 8), layer_t);
    };

    if(!t.has_player_pose)
    {
        return sample_vertical_center();
    }

    SpatialBasisUtils::BasisVectors basis = SpatialBasisUtils::BuildOrthonormalBasis(t.forward_x,
                                                                                      t.forward_y,
                                                                                      t.forward_z,
                                                                                      t.up_x,
                                                                                      t.up_y,
                                                                                      t.up_z);
    const float wx = grid_x - origin_x;
    const float wy = grid_y - origin_y;
    const float wz = grid_z - origin_z;
    float lx = 0.0f;
    float ly = 0.0f;
    float lz = 0.0f;
    SpatialBasisUtils::ToLocal(basis, wx, wy, wz, lx, ly, lz);

    const float yaw = heading_offset_deg * 0.01745329251f;
    if(std::fabs(yaw) > 1e-5f)
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        const float lx2 = lx * c + lz * s;
        const float lz2 = -lx * s + lz * c;
        lx = lx2;
        lz = lz2;
    }

    const float horiz_len = std::sqrt(lx * lx + lz * lz);
    if(horiz_len < 0.08f)
    {
        return sample_vertical_center();
    }

    float angle = std::atan2(lx, lz);
    float sector_f = angle * 1.27323954f;
    if(sector_f < 0.0f)
    {
        sector_f += 8.0f;
    }
    const int sector0 = ((int)std::floor(sector_f)) % 8;
    const int sector1 = (sector0 + 1) % 8;
    const float sector_t = sector_f - std::floor(sector_f);

    const RGBColor c00 = SampleWorldProbeCell(t, layer0, sector0);
    const RGBColor c01 = SampleWorldProbeCell(t, layer0, sector1);
    const RGBColor c10 = SampleWorldProbeCell(t, layer1, sector0);
    const RGBColor c11 = SampleWorldProbeCell(t, layer1, sector1);
    const RGBColor c0 = LerpColor(c00, c01, sector_t);
    const RGBColor c1 = LerpColor(c10, c11, sector_t);
    return LerpColor(c0, c1, layer_t);
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

QWidget* CreateSettingsWidget(QWidget* parent,
                              Settings& s,
                              std::uint32_t channels)
{
    QScrollArea* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    Ui::MinecraftGameSettingsScroll scroll_ui;
    scroll_ui.setupUi(scroll);
    QVBoxLayout* content_layout = scroll_ui.contentLayout;
    QWidget* panel = scroll_ui.scrollContents;

    QVBoxLayout* vitals_layout = content_layout;
    if(ch(channels, ChHealth) || ch(channels, ChHunger) || ch(channels, ChAir) || ch(channels, ChDurability))
    {
        if(QVBoxLayout* body = EffectUiRows::AppendCollapsibleSectionBody(content_layout, QStringLiteral("Vitals")))
        {
            vitals_layout = body;
        }
    }
    if(ch(channels, ChHealth))
    {
        AddCheckRow(vitals_layout, panel, "Per-heart strip (each heart uses LEDs along layout axis)", s.health_per_heart_strip,
                    [&s](bool v) { s.health_per_heart_strip = v; });

        AddCheckRow(vitals_layout, panel, "Index strip mode (works on any controller; uses LED order)", s.health_per_heart_indexed,
                    [&s](bool v) { s.health_per_heart_indexed = v; });

        AddSpinRow(vitals_layout,
                   panel,
                   QStringLiteral("LEDs per heart"),
                   1,
                   32,
                   std::clamp(s.health_leds_per_heart, 1, 32),
                   [&s](int v) { s.health_leds_per_heart = std::clamp(v, 1, 32); });
        QComboBox* axis_combo = AddComboRow(vitals_layout, QStringLiteral("Heart strip axis"));
        axis_combo->addItem(QStringLiteral("Auto (longest span)"));
        axis_combo->addItem(QStringLiteral("Along X"));
        axis_combo->addItem(QStringLiteral("Along Y"));
        axis_combo->addItem(QStringLiteral("Along Z"));
        axis_combo->setCurrentIndex(std::clamp(s.health_strip_axis, 0, 3));
        axis_combo->setToolTip(
            QStringLiteral(
                "Which room axis maps one heart's LED strip when Per-heart strip is on. "
                "Auto picks the longest layout span."));
        axis_combo->setItemData(0, QStringLiteral("Pick X, Y, or Z by largest LED span in the layout."), Qt::ToolTipRole);
        axis_combo->setItemData(1, QStringLiteral("Strip runs with increasing X along the strip."), Qt::ToolTipRole);
        axis_combo->setItemData(2, QStringLiteral("Strip runs with increasing Y along the strip."), Qt::ToolTipRole);
        axis_combo->setItemData(3, QStringLiteral("Strip runs with increasing Z along the strip."), Qt::ToolTipRole);
        QObject::connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), panel, [&s](int idx) { s.health_strip_axis = std::clamp(idx, 0, 3); });
        AddCheckRow(vitals_layout, panel, QStringLiteral("Invert strip direction"), s.health_strip_invert,
                    [&s](bool v) { s.health_strip_invert = v; });

    }
    if(ch(channels, ChHunger))
    {
        AddCheckRow(vitals_layout, panel, "Per-strip hunger (uses strip/index settings above)", s.hunger_per_strip,
                    [&s](bool v) { s.hunger_per_strip = v; });
        AddPctSlider(vitals_layout, panel, QStringLiteral("Hunger gradient strength"), &s.hunger_mix);
    }
    if(ch(channels, ChAir))
    {
        AddCheckRow(vitals_layout, panel, "Per-strip air (uses strip/index settings above)", s.air_per_strip,
                    [&s](bool v) { s.air_per_strip = v; });
        AddPctSlider(vitals_layout, panel, QStringLiteral("Air gradient strength"), &s.air_mix);
    }
    if(ch(channels, ChDurability))
    {
        AddCheckRow(vitals_layout, panel, "Per-strip durability (uses strip/index settings above)", s.durability_per_strip,
                    [&s](bool v) { s.durability_per_strip = v; });
        AddPctSlider(vitals_layout, panel, QStringLiteral("Item durability gradient strength"), &s.durability_mix);
    }

    if(ch(channels, ChDamage))
    {
        QVBoxLayout* damage_layout = content_layout;
        if(QVBoxLayout* body = EffectUiRows::AppendCollapsibleSectionBody(content_layout, QStringLiteral("Damage")))
        {
            damage_layout = body;
        }
        AddPctSlider(damage_layout, panel, QStringLiteral("Directional hit (vs uniform)"), &s.damage_directional_mix);
        AddSliderRow(damage_layout, panel, QStringLiteral("Damage direction sharpness"), 50, 400, (int)std::lround(s.damage_dir_sharpness * 100.0f),
                     [&s](int x) { s.damage_dir_sharpness = std::clamp(x / 100.0f, 0.5f, 5.0f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
        AddSliderRow(damage_layout, panel, QStringLiteral("Damage flash decay (ms)"), 100, 900, (int)std::lround(std::clamp(s.damage_flash_decay_s, 0.10f, 0.90f) * 1000.0f),
                     [&s](int x) { s.damage_flash_decay_s = std::clamp(x / 1000.0f, 0.10f, 0.90f); },
                     [](int x) { return QString::number(x); });
        AddPctSlider(damage_layout, panel, QStringLiteral("Damage flash strength"), &s.damage_flash_strength);
    }

    if(ch(channels, ChWorldTint))
    {
        QVBoxLayout* world_layout = content_layout;
        if(QVBoxLayout* body = EffectUiRows::AppendCollapsibleSectionBody(content_layout, QStringLiteral("World tint")))
        {
            world_layout = body;
        }
        QLabel* hint = new QLabel(
            QStringLiteral(
                "3D world color: each LED samples directional world_light probes "
                "from Fabric (4 height layers × 8 compass sectors). Place a reference point at "
                "eye height, set this effect's 3D origin there, then stand at that spot in-game. "
                "Room yaw aligns your room layout with in-game look (not player position)."),
            panel);
        hint->setWordWrap(true);
        world_layout->addWidget(hint);
        AddSliderRow(world_layout, panel, QStringLiteral("World tint strength"), 0, 100, (int)std::lround(std::clamp(s.world_light_mix, 0.0f, 1.0f) * 100.0f),
                     [&s](int x) { s.world_light_mix = std::clamp(x / 100.0f, 0.0f, 1.0f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
        AddSliderRow(world_layout, panel, QStringLiteral("Room yaw (deg)"), -180, 180, (int)std::lround(s.world_heading_offset_deg),
                     [&s](int x) { s.world_heading_offset_deg = (float)std::clamp(x, -180, 180); },
                     [](int x) { return QString::number(x) + QStringLiteral("°"); });
    }

    if(ch(channels, ChRoomVrTint))
    {
        QVBoxLayout* room_layout = content_layout;
        if(QVBoxLayout* body = EffectUiRows::AppendCollapsibleSectionBody(content_layout, QStringLiteral("Room tint (VR)")))
        {
            room_layout = body;
        }
        QLabel* hint = new QLabel(
            QStringLiteral(
                "Ambilight viewport: LEDs show what you can see from your eyes, not the whole world volume. "
                "Place a reference point at eye height, set this effect's 3D origin there, then stand "
                "in-game at that spot. Walls and caves show only line-of-sight surfaces (no x-ray). "
                "Room yaw fine-tunes heading; block offsets trim origin without turning."),
            panel);
        hint->setWordWrap(true);
        room_layout->addWidget(hint);
        AddSliderRow(room_layout, panel, QStringLiteral("Room VR strength"), 0, 100, (int)std::lround(std::clamp(s.room_vr_mix, 0.0f, 1.0f) * 100.0f),
                     [&s](int x) { s.room_vr_mix = std::clamp(x / 100.0f, 0.0f, 1.0f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
        AddSliderRow(room_layout, panel, QStringLiteral("Room yaw (deg)"), -180, 180, (int)std::lround(s.room_vr_heading_offset_deg),
                     [&s](int x) { s.room_vr_heading_offset_deg = (float)std::clamp(x, -180, 180); },
                     [](int x) { return QString::number(x) + QStringLiteral("°"); });
        auto pos_offset_label = [](int tenths) {
            return QString::number(tenths / 10.0, 'f', 1) + QStringLiteral(" blk");
        };
        AddSliderRow(room_layout,
                     panel,
                     QStringLiteral("Player offset forward"),
                     -80,
                     80,
                     (int)std::lround(std::clamp(s.room_vr_pos_offset_forward_blocks, -8.0f, 8.0f) * 10.0f),
                     [&s](int x) { s.room_vr_pos_offset_forward_blocks = std::clamp(x / 10.0f, -8.0f, 8.0f); },
                     pos_offset_label);
        AddSliderRow(room_layout,
                     panel,
                     QStringLiteral("Player offset right"),
                     -80,
                     80,
                     (int)std::lround(std::clamp(s.room_vr_pos_offset_right_blocks, -8.0f, 8.0f) * 10.0f),
                     [&s](int x) { s.room_vr_pos_offset_right_blocks = std::clamp(x / 10.0f, -8.0f, 8.0f); },
                     pos_offset_label);
        AddSliderRow(room_layout,
                     panel,
                     QStringLiteral("Player offset up"),
                     -80,
                     80,
                     (int)std::lround(std::clamp(s.room_vr_pos_offset_up_blocks, -8.0f, 8.0f) * 10.0f),
                     [&s](int x) { s.room_vr_pos_offset_up_blocks = std::clamp(x / 10.0f, -8.0f, 8.0f); },
                     pos_offset_label);
        AddSliderRow(room_layout, panel, QStringLiteral("MC world scale"), 10, 600, (int)std::lround(std::clamp(s.room_vr_scale_tune, 0.1f, 6.0f) * 100.0f),
                     [&s](int x) { s.room_vr_scale_tune = std::clamp(x / 100.0f, 0.1f, 6.0f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); },
                     QStringLiteral("How many Minecraft blocks map to 1 metre of physical room (relative to blocks-per-metre). "
                                    "Higher = more MC blocks visible across the room = finer colour detail per LED. "
                                    "100% = 1:1 with blocks-per-metre setting. Try 200-400% for good block-level variety."));
        AddSliderRow(room_layout, panel, QStringLiteral("Color saturation"), 80, 250, (int)std::lround(std::clamp(s.room_vr_saturation, 0.8f, 2.5f) * 100.0f),
                     [&s](int x) { s.room_vr_saturation = std::clamp(x / 100.0f, 0.8f, 2.5f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
        AddSliderRow(room_layout, panel, QStringLiteral("Color contrast"), 80, 200, (int)std::lround(std::clamp(s.room_vr_contrast, 0.8f, 2.0f) * 100.0f),
                     [&s](int x) { s.room_vr_contrast = std::clamp(x / 100.0f, 0.8f, 2.0f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
        AddCheckRow(room_layout, panel, QStringLiteral("Sharp block sampling (less blur)"), s.room_vr_sharp_sampling,
                    [&s](bool v) { s.room_vr_sharp_sampling = v; });
    }

    if(ch(channels, ChLightning))
    {
        QVBoxLayout* lightning_layout = content_layout;
        if(!ch(channels, ChWorldTint))
        {
            if(QVBoxLayout* body = EffectUiRows::AppendCollapsibleSectionBody(content_layout, QStringLiteral("Lightning")))
            {
                lightning_layout = body;
            }
        }
        AddSliderRow(lightning_layout, panel, QStringLiteral("Lightning flash strength"), 0, 150, (int)std::lround(std::clamp(s.lightning_flash_strength, 0.0f, 1.5f) * 100.0f),
                     [&s](int x) { s.lightning_flash_strength = std::clamp(x / 100.0f, 0.0f, 1.5f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
        AddSliderRow(lightning_layout, panel, QStringLiteral("Lightning decay (ms)"), 80, 900, (int)std::lround(std::clamp(s.lightning_flash_decay_s, 0.08f, 0.90f) * 1000.0f),
                     [&s](int x) { s.lightning_flash_decay_s = std::clamp(x / 1000.0f, 0.08f, 0.90f); },
                     [](int x) { return QString::number(x); });
        AddPctSlider(lightning_layout, panel, QStringLiteral("Lightning directional response"), &s.lightning_directional_mix);
        AddSliderRow(lightning_layout, panel, QStringLiteral("Lightning directional sharpness"), 50, 500, (int)std::lround(std::clamp(s.lightning_dir_sharpness, 0.5f, 5.0f) * 100.0f),
                     [&s](int x) { s.lightning_dir_sharpness = std::clamp(x / 100.0f, 0.5f, 5.0f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
    }

    QVBoxLayout* output_layout = content_layout;
    {
        AddSliderRow(output_layout, panel, QStringLiteral("Base brightness"), 80, 150, (int)std::lround(std::clamp(s.base_brightness, 0.8f, 1.5f) * 100.0f),
                     [&s](int x) { s.base_brightness = std::clamp(x / 100.0f, 0.8f, 1.5f); },
                     [](int x) { return QString::number(x) + QStringLiteral("%"); });
    }

    return scroll;
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
    if(!t.has_damage_event || t.damage_received_ms == 0)
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
    if(!t.has_lightning_event || t.lightning_received_ms == 0)
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
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& s,
                     std::uint32_t channels)
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
    const std::function<float(float, float)> strip_brightness_for = [&](float fill_end, float max_units) -> float {
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

    if(ch(channels, ChHealth) && t.has_health_state && t.hearts_max > 1e-4f)
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
    if(ch(channels, ChHunger) && t.has_health_state && t.hunger_max > 0.01f)
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
    if(ch(channels, ChAir) && t.has_health_state && t.air_max > 0.01f)
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
    if(ch(channels, ChDurability) && t.has_health_state && t.has_item_durability && t.item_durability_max > 0.01f)
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

    if(ch(channels, ChWorldTint) && t.has_world_light)
    {
        const float y_norm = NormalizeRoomVertical01(grid_y, grid, origin_y);
        const float layer_norm = ResolveTintLayerNorm(grid_x, grid_z, grid, y_norm);

        RGBColor projected;
        if(t.has_layered_world_probes)
        {
            projected = SampleLayeredWorldProbe(t,
                                                grid_x,
                                                grid_y,
                                                grid_z,
                                                origin_x,
                                                origin_y,
                                                origin_z,
                                                grid,
                                                s.world_heading_offset_deg);
        }
        else
        {
            projected = SampleVerticalWorldLayers(t, layer_norm);
        }

        const float wi = std::clamp(t.world_light_intensity, 0.0f, 1.0f);
        const float mix = std::clamp(s.world_light_mix * wi, 0.0f, 1.0f);
        out = LerpColor(out, projected, mix);
    }

    if(ch(channels, ChRoomVrTint))
    {
        bool got_room_sample = false;
        RGBColor room_rgb = (RGBColor)0;

        // Room sample grid — true 1:1 per-LED game-world block colours.
        if(t.room_sample.has_frame)
        {
            room_rgb = RoomSampleMapping::SampleAtRoomGrid(t, grid_x, grid_y, grid_z, &got_room_sample);
        }

        // No sky/ambient fill while room viewport is active — air and open LOS stay dark on LEDs.
        if(got_room_sample)
        {
            room_rgb = EnhanceRoomVrColor(room_rgb, s.room_vr_saturation, s.room_vr_contrast);
            const float mix = std::clamp(s.room_vr_mix, 0.0f, 1.0f);
            out = LerpColor(out, room_rgb, mix);
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

    if(effect->surfaces_section)
    {
        effect->surfaces_section->setVisible(false);
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
