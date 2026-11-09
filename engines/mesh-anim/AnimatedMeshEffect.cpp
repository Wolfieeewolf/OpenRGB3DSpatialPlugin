// SPDX-License-Identifier: GPL-2.0-only

#include "AnimatedMeshEffect.h"

#include "EffectListManager3D.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PlayerEngines.h"
#include "PluginSettingsPaths.h"
#include "SpatialEffectTypes.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "viewport/MeshAnim.h"
#include "viewport/MeshImport.h"
#include "viewport/ScenePropMeshPaths.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace
{
constexpr SpatialEffectType kAnimatedMeshEffectType = static_cast<SpatialEffectType>(37);
constexpr size_t kMaxMeshLightSamples = 4800;
constexpr int kMaxGpuGlowSamples = 512;

/* Soft-glow volumeMain: splat mesh samples from u_media (row0=pos01+w, row1=rgb+emis). */
const char* kMeshGlowVolumeBody = R"GLSL(
void volumeMain(out vec4 out_color, in vec3 p01)
{
    float hx = max(u_params[0], 1e-4);
    float hy = max(u_params[1], 1e-4);
    float hz = max(u_params[2], 1e-4);
    float radius = max(u_params[3], 1e-4);
    int n = int(u_params[4] + 0.5);
    n = clamp(n, 0, 512);
    float surface_mode = u_params[5];
    float detail_n = clamp(u_params[6], 0.0, 1.0);
    float bright = clamp(u_params[7], 0.0, 1.0);
    float emis = clamp(u_params[8], 0.0, 8.0);
    float effect_tint = u_params[9];
    float hue_dens = max(u_params[10], 0.25);
    float cycle_hz = u_params[11];

    vec3 local = (p01 * 2.0 - 1.0) * vec3(hx, hy, hz);
    float best_d2 = radius * radius;
    vec3 best_c = vec3(0.0);
    float best_emis = 1.0;
    bool hit = false;
    float inv_n = 1.0 / max(float(n), 1.0);

    for(int i = 0; i < 512; ++i)
    {
        if(i >= n) { break; }
        float u = (float(i) + 0.5) * inv_n;
        vec4 pos = texture(u_media, vec2(u, 0.25));
        vec4 col = texture(u_media, vec2(u, 0.75));
        if(pos.a < 0.02) { continue; }
        vec3 sp = (pos.xyz * 2.0 - 1.0) * vec3(hx, hy, hz);
        vec3 d = local - sp;
        float d2 = dot(d, d);
        if(d2 < best_d2)
        {
            best_d2 = d2;
            best_c = col.rgb;
            /* Media packs part_emis/4 into alpha — unpack to ~[0.05,4]. */
            best_emis = max(col.a * 4.0, 0.05);
            hit = true;
        }
    }
    if(!hit) { out_color = vec4(0.0); return; }

    float dist = sqrt(best_d2);
    float t = 1.0 - (dist / radius);
    if(surface_mode > 0.5)
    {
        t = (t > 0.35) ? 1.0 : (t / 0.35);
    }
    else
    {
        t = pow(max(t, 0.0), 1.0 + detail_n * 2.5);
    }

    if(effect_tint > 0.5)
    {
        float driver = fract(dot(p01.xz, vec2(0.5)) * hue_dens + u_time * cycle_hz);
        float h = driver * 6.0;
        float x = 1.0 - abs(mod(h, 2.0) - 1.0);
        vec3 tint = vec3(0.0);
        if(h < 1.0)      tint = vec3(1.0, x, 0.0);
        else if(h < 2.0) tint = vec3(x, 1.0, 0.0);
        else if(h < 3.0) tint = vec3(0.0, 1.0, x);
        else if(h < 4.0) tint = vec3(0.0, x, 1.0);
        else if(h < 5.0) tint = vec3(x, 0.0, 1.0);
        else             tint = vec3(1.0, 0.0, x);
        best_c *= max(tint, vec3(0.15));
    }

    /* Gain = falloff × Brightness × soft Emissive. Never multiply HDR again then
       clamp — that crushed Sketchfab neon to white soup. Preserve chromaticity. */
    float emis_boost = clamp(emis * best_emis, 0.0, 8.0);
    float gain = t * bright * (0.55 + 0.45 * min(emis_boost, 4.0) / 4.0);
    vec3 c = best_c * max(gain, 0.0);
    float peak = max(max(c.r, c.g), c.b);
    if(peak > 1.0) { c /= peak; }
    out_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
)GLSL";

RGBColor MulRgb(RGBColor c, float r_mul, float g_mul, float b_mul, float bright)
{
    const float br = (float)RGBGetRValue(c) * r_mul * bright;
    const float bg = (float)RGBGetGValue(c) * g_mul * bright;
    const float bb = (float)RGBGetBValue(c) * b_mul * bright;
    return ToRGBColor((int)std::lround(std::min(255.0f, br)),
                      (int)std::lround(std::min(255.0f, bg)),
                      (int)std::lround(std::min(255.0f, bb)));
}
} // namespace

void RegisterAnimatedMeshEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "AnimatedMesh",
        "Animated Mesh",
        "Media",
        "",
        "",
        []() { return new AnimatedMeshEffect; },
        SpatialEffectSettingsLayout::FullWithTransport);
}

AnimatedMeshEffect::AnimatedMeshEffect(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetSpeed(100);
}

EffectInfo3D AnimatedMeshEffect::GetEffectInfo() const
{
    EffectInfo3D info = {};
    info.effect_name = "Animated Mesh";
    info.effect_description =
        "Place a glTF/GLB in the room. Speed plays the clip (100 = 1x). "
        "Scale sizes the mesh; LED volume stays full so Soft glow can fill the grid. "
        "LED colour: Materials (file look) or Effect (Rainbow / strip).";
    info.category = "Media";
    info.effect_type = kAnimatedMeshEffectType;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_frequency = true;
    info.use_size_parameter = true;
    info.show_color_controls = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_detail_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_surface_control = false;
    info.show_position_offset_control = true;
    info.show_room_output_control = true;
    return info;
}

float AnimatedMeshEffect::PlaybackRate() const
{
    return (float)GetSpeed() / 100.0f;
}

void AnimatedMeshEffect::SetupCustomUI(QWidget* parent)
{
    if(!parent)
    {
        return;
    }

    QWidget* panel = EffectUiRows::NewEffectPanel("AnimatedMeshEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(panel);
    if(!layout)
    {
        return;
    }

    EffectUiRows::AppendInfoLabel(
        layout,
        tr("Import a .glb / .gltf. Speed plays the clip. "
           "Scale sizes the object (not the LED volume). "
           "LED lighting: Off / Mesh surface / Soft glow. "
           "LED colour: Materials keeps the file look; Effect enables Rainbow / strip."));

    path_label_ = new QLabel(tr("No mesh imported"), panel);
    path_label_->setWordWrap(true);
    layout->addWidget(path_label_);

    summary_label_ = new QLabel(panel);
    summary_label_->setWordWrap(true);
    summary_label_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(summary_label_);

    browse_button_ = new QPushButton(tr("Import mesh\u2026"), panel);
    clear_button_ = new QPushButton(tr("Clear"), panel);
    auto* path_row = new QHBoxLayout();
    path_row->addWidget(browse_button_, 1);
    path_row->addWidget(clear_button_);
    layout->addLayout(path_row);

    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto fmt_emissive = [](int v) { return QString::number(v / 100.0, 'f', 2); };
    const auto fmt_pct = [](int v) { return QStringLiteral("%1%").arg(v); };

    emissive_row_ = EffectUiRows::AppendSliderRow(
        layout, tr("Emissive:"), 0, 800, (int)std::lround(emissive_ * 100.0f),
        tr("Extra neon/Soft-glow boost on top of Brightness.\n"
           "0 = materials at Brightness only (viewport still visible).\n"
           "Above 1.0 also brightens the viewport mesh."));
    if(emissive_row_)
    {
        emissive_row_->bindValueChanged(
            this,
            [this](int v) {
                emissive_ = std::clamp((float)v / 100.0f, 0.0f, 8.0f);
            },
            fmt_emissive, on_changed);
    }

    auto* light_mode_row = new QHBoxLayout();
    light_mode_row->addWidget(new QLabel(tr("LED lighting:"), panel));
    light_mode_combo_ = new QComboBox(panel);
    light_mode_combo_->addItem(tr("Off"), (int)MeshLightMode::Off);
    light_mode_combo_->addItem(tr("Mesh surface"), (int)MeshLightMode::Surface);
    light_mode_combo_->addItem(tr("Soft glow"), (int)MeshLightMode::Glow);
    light_mode_combo_->setToolTip(
        tr("Off: viewport mesh only (check the sculpture look).\n"
           "Mesh surface: LEDs near the mesh (Size thickens the shell).\n"
           "Soft glow: LEDs use the same file materials/emissive look as the viewport mesh.\n"
           "Set LED colour to Materials (file). Scale sizes the mesh; Soft glow fills the grid."));
    light_mode_row->addWidget(light_mode_combo_, 1);
    layout->addLayout(light_mode_row);
    connect(light_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if(!light_mode_combo_)
                {
                    return;
                }
                light_mode_ = (MeshLightMode)light_mode_combo_->currentData().toInt();
                syncLightModeUi();
                samples_sequence_ = 0;
                emit ParametersChanged();
            });

    auto* led_color_row = new QHBoxLayout();
    led_color_row->addWidget(new QLabel(tr("LED colour:"), panel));
    led_color_combo_ = new QComboBox(panel);
    led_color_combo_->addItem(tr("Materials (file)"), (int)LedColorSource::Materials);
    led_color_combo_->addItem(tr("Effect colour"), (int)LedColorSource::Effect);
    led_color_combo_->setToolTip(
        tr("Materials (file): Soft glow / Mesh surface LEDs match the viewport sculpture colours.\n"
           "Effect colour: Rainbow / palette / strip *tint* on top of that look.\n"
           "Per-part Override colour always wins."));
    led_color_row->addWidget(led_color_combo_, 1);
    layout->addLayout(led_color_row);
    connect(led_color_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if(!led_color_combo_)
                {
                    return;
                }
                led_color_source_ = (LedColorSource)led_color_combo_->currentData().toInt();
                samples_sequence_ = 0;
                emit ParametersChanged();
            });

    light_radius_row_ = EffectUiRows::AppendSliderRow(
        layout, tr("Light radius:"), 5, 150, (int)std::lround(light_radius_ * 100.0f),
        tr("Soft-glow distance as % of the LED volume's smallest half-extent (Size scales it). "
           "Only used when LED lighting is Soft glow. Scale does not shrink this volume."));
    if(light_radius_row_)
    {
        light_radius_row_->bindValueChanged(
            this,
            [this](int v) {
                light_radius_ = std::clamp((float)v / 100.0f, 0.05f, 1.5f);
            },
            fmt_pct, on_changed);
    }

    auto* parts_header = new QHBoxLayout();
    parts_header->addWidget(new QLabel(tr("Parts:"), panel), 1);
    auto* show_all = new QPushButton(tr("Show all"), panel);
    auto* hide_all = new QPushButton(tr("Hide all"), panel);
    auto* leds_on = new QPushButton(tr("LEDs on"), panel);
    auto* leds_off = new QPushButton(tr("LEDs off"), panel);
    show_all->setToolTip(tr("Make every part visible"));
    hide_all->setToolTip(tr("Hide every part"));
    leds_on->setToolTip(tr("All parts contribute to LED / grid lighting"));
    leds_off->setToolTip(tr("No parts contribute to LED / grid lighting"));
    parts_header->addWidget(show_all);
    parts_header->addWidget(hide_all);
    parts_header->addWidget(leds_on);
    parts_header->addWidget(leds_off);
    layout->addLayout(parts_header);

    parts_list_ = new QListWidget(panel);
    parts_list_->setMinimumHeight(110);
    parts_list_->setMaximumHeight(180);
    parts_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(parts_list_);

    part_editor_ = new QWidget(panel);
    auto* editor_layout = new QVBoxLayout(part_editor_);
    editor_layout->setContentsMargins(0, 4, 0, 0);
    editor_layout->setSpacing(4);

    part_editor_title_ = new QLabel(tr("Select a part to edit"), part_editor_);
    part_editor_title_->setWordWrap(true);
    editor_layout->addWidget(part_editor_title_);

    part_id_label_ = new QLabel(part_editor_);
    part_id_label_->setWordWrap(true);
    part_id_label_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    editor_layout->addWidget(part_id_label_);

    auto* flags_row = new QHBoxLayout();
    part_visible_check_ = new QCheckBox(tr("Visible"), part_editor_);
    part_leds_check_ = new QCheckBox(tr("Light LEDs"), part_editor_);
    flags_row->addWidget(part_visible_check_);
    flags_row->addWidget(part_leds_check_);
    flags_row->addStretch(1);
    editor_layout->addLayout(flags_row);

    auto* tint_row = new QHBoxLayout();
    tint_row->addWidget(new QLabel(tr("Tint:"), part_editor_));
    part_tint_combo_ = new QComboBox(part_editor_);
    part_tint_combo_->addItem(tr("Effect colour"), (int)PartTintMode::Effect);
    part_tint_combo_->addItem(tr("Material colour"), (int)PartTintMode::Material);
    part_tint_combo_->addItem(tr("Override colour"), (int)PartTintMode::Override);
    part_tint_combo_->setItemData(
        part_tint_combo_->findData((int)PartTintMode::Material),
        tr("Use file material / texture colours"),
        Qt::ToolTipRole);
    tint_row->addWidget(part_tint_combo_, 1);
    editor_layout->addLayout(tint_row);

    part_texture_note_ = new QLabel(part_editor_);
    part_texture_note_->setWordWrap(true);
    part_texture_note_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    part_texture_note_->hide();
    editor_layout->addWidget(part_texture_note_);

    auto* color_row = new QHBoxLayout();
    color_row->addWidget(new QLabel(tr("Colour:"), part_editor_));
    part_color_button_ = new QPushButton(tr("Pick\u2026"), part_editor_);
    part_color_button_->setMinimumWidth(72);
    part_material_swatch_ = new QLabel(part_editor_);
    part_material_swatch_->setFixedSize(22, 22);
    part_material_swatch_->setToolTip(tr("Material base colour from the file"));
    part_use_material_button_ = new QPushButton(tr("Use material"), part_editor_);
    part_reset_button_ = new QPushButton(tr("Reset"), part_editor_);
    part_reset_button_->setToolTip(tr("Reset this part to defaults (visible, LEDs on, material tint)"));
    color_row->addWidget(part_color_button_);
    color_row->addWidget(part_material_swatch_);
    color_row->addWidget(part_use_material_button_);
    color_row->addWidget(part_reset_button_);
    color_row->addStretch(1);
    editor_layout->addLayout(color_row);

    part_emissive_row_ = EffectUiRows::AppendSliderRow(
        editor_layout,
        tr("Part emissive:"),
        0,
        400,
        100,
        tr("Per-part glow multiplier on top of the global Emissive slider (100% = 1x)."));
    if(part_emissive_row_)
    {
        part_emissive_row_->bindValueChanged(
            this,
            [this](int v) {
                if(PartOverride* p = selectedPart())
                {
                    p->emissive_scale = std::clamp((float)v / 100.0f, 0.0f, 4.0f);
                    refreshPartListItem(selected_part_index_);
                    emit ParametersChanged();
                }
            },
            [](int v) { return QStringLiteral("%1%").arg(v); },
            nullptr);
    }

    layout->addWidget(part_editor_);

    connect(browse_button_, &QPushButton::clicked, this, &AnimatedMeshEffect::onBrowseMesh);
    connect(clear_button_, &QPushButton::clicked, this, &AnimatedMeshEffect::onClearMesh);
    connect(parts_list_, &QListWidget::currentRowChanged, this, &AnimatedMeshEffect::onPartListSelectionChanged);
    connect(part_visible_check_, &QCheckBox::toggled, this, &AnimatedMeshEffect::onPartVisibleToggled);
    connect(part_leds_check_, &QCheckBox::toggled, this, &AnimatedMeshEffect::onPartLedsToggled);
    connect(part_tint_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnimatedMeshEffect::onPartTintModeChanged);
    connect(part_color_button_, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartPickColor);
    connect(part_use_material_button_, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartUseMaterialColor);
    connect(part_reset_button_, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartResetDefaults);
    connect(show_all, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartsShowAll);
    connect(hide_all, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartsHideAll);
    connect(leds_on, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartsLedsAllOn);
    connect(leds_off, &QPushButton::clicked, this, &AnimatedMeshEffect::onPartsLedsAllOff);

    AddWidgetToParent(panel, parent);
    refreshPathLabel();
    refreshSummaryLabel();
    syncCustomSlidersFromModel();
    syncLightModeUi();
    syncLedColorSourceUi();
    rebuildPartsUi();
}

void AnimatedMeshEffect::syncLightModeUi()
{
    if(light_mode_combo_)
    {
        QSignalBlocker b(light_mode_combo_);
        const int idx = light_mode_combo_->findData((int)light_mode_);
        light_mode_combo_->setCurrentIndex(idx >= 0 ? idx : (int)MeshLightMode::Glow);
    }
    if(light_radius_row_)
    {
        light_radius_row_->setEnabled(light_mode_ == MeshLightMode::Glow);
    }
}

void AnimatedMeshEffect::syncLedColorSourceUi()
{
    if(!led_color_combo_)
    {
        return;
    }
    QSignalBlocker b(led_color_combo_);
    const int idx = led_color_combo_->findData((int)led_color_source_);
    led_color_combo_->setCurrentIndex(idx >= 0 ? idx : (int)LedColorSource::Materials);
}

bool AnimatedMeshEffect::UsesSpatialEffectColour() const
{
    return led_color_source_ == LedColorSource::Effect
        && (GetRainbowMode() || UseEffectStripColormap());
}

void AnimatedMeshEffect::GetVolumeHalfExtents(const GridContext3D& grid, float& hx, float& hy, float& hz) const
{
    /* LED / gizmo volume always fills from the Spatial Anchor. Scale only sizes the mesh. */
    const Vector3D origin = GetEffectOriginGrid(grid);
    const float cover_x = std::max(1e-4f, std::max(origin.x - grid.min_x, grid.max_x - origin.x));
    const float cover_y = std::max(1e-4f, std::max(origin.y - grid.min_y, grid.max_y - origin.y));
    const float cover_z = std::max(1e-4f, std::max(origin.z - grid.min_z, grid.max_z - origin.z));
    hx = std::max(1e-4f, cover_x * ((float)effect_scale_x / 100.0f));
    hy = std::max(1e-4f, cover_y * ((float)effect_scale_y / 100.0f));
    hz = std::max(1e-4f, cover_z * ((float)effect_scale_z / 100.0f));
}

void AnimatedMeshEffect::GetMeshFitHalfExtents(const GridContext3D& grid, float& hx, float& hy, float& hz) const
{
    GetVolumeHalfExtents(grid, hx, hy, hz);
    const float mesh_s = std::max(0.05f, GetNormalizedScale());
    hx *= mesh_s;
    hy *= mesh_s;
    hz *= mesh_s;
}

float AnimatedMeshEffect::effectiveLightRadius(float smallest_half_extent) const
{
    const float size_m = std::max(0.05f, GetNormalizedSize());
    if(light_mode_ == MeshLightMode::Off)
    {
        return 0.0f;
    }
    if(light_mode_ == MeshLightMode::Surface)
    {
        /* Shell around the mesh (Scale), not the full LED volume. */
        float mesh_smallest = samples_fit_hx_;
        if(samples_fit_hy_ > 0.0f && samples_fit_hy_ < mesh_smallest) { mesh_smallest = samples_fit_hy_; }
        if(samples_fit_hz_ > 0.0f && samples_fit_hz_ < mesh_smallest) { mesh_smallest = samples_fit_hz_; }
        if(mesh_smallest < 1e-4f) { mesh_smallest = smallest_half_extent; }
        float r = mesh_smallest * (0.10f + 0.20f * size_m);
        if(r < 1e-3f)
        {
            r = 1e-3f;
        }
        return r;
    }
    /* Soft glow: radius vs LED volume so maxed Light radius can fill the grid. */
    float r = light_radius_ * smallest_half_extent * size_m;
    if(r < 1e-4f)
    {
        r = 1e-4f;
    }
    return r;
}

void AnimatedMeshEffect::refreshPathLabel()
{
    if(!path_label_)
    {
        return;
    }
    if(mesh_asset_.empty())
    {
        path_label_->setText(tr("No mesh imported"));
        return;
    }
    QString note;
    const std::string abs = ResolvedMeshPath();
    if(!abs.empty())
    {
        const MeshAnim::Scene* anim = MeshAnim::Cache::instance()->GetOrLoad(abs);
        if(anim && anim->hasAnimation())
        {
            note = tr(" — animation %1s").arg(anim->duration_sec, 0, 'f', 2);
        }
        else if(anim)
        {
            note = tr(" — static mesh");
        }
    }
    path_label_->setText(QString::fromStdString(mesh_asset_) + note);
}

void AnimatedMeshEffect::refreshSummaryLabel()
{
    if(!summary_label_)
    {
        return;
    }
    if(mesh_asset_.empty())
    {
        summary_label_->setText(QString());
        return;
    }
    const std::string abs = ResolvedMeshPath();
    const MeshAnim::Scene* anim = abs.empty() ? nullptr : MeshAnim::Cache::instance()->GetOrLoad(abs);
    if(!anim)
    {
        summary_label_->setText(tr("Asset summary unavailable"));
        return;
    }
    const MeshAnim::AssetSummary& s = anim->summary;
    QString skin = s.has_skins ? tr("ignored") : tr("no");
    QString morph = s.has_morphs ? tr("ignored") : tr("no");
    summary_label_->setText(
        tr("Parts %1 · Tris %2 · Verts %3 · Mats %4 · Tex %5 · Clips %6 · UV %7 · Skin %8 · Morph %9")
            .arg(s.mesh_count)
            .arg(s.triangle_count)
            .arg(s.vertex_count)
            .arg(s.material_count)
            .arg(s.texture_count)
            .arg(s.clip_count)
            .arg(s.has_uvs ? tr("yes") : tr("no"))
            .arg(skin)
            .arg(morph));
}

std::string AnimatedMeshEffect::ResolvedMeshPath() const
{
    if(mesh_asset_.empty() || !OpenRGB3DSpatialPlugin::APIPointer)
    {
        return {};
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    ScenePropMeshPaths::SetMeshesRoot(PluginSettingsPaths::MeshesDir(OpenRGB3DSpatialPlugin::APIPointer));
    const filesystem::path abs = ScenePropMeshPaths::Resolve(mesh_asset_);
    return abs.empty() ? std::string{} : abs.string();
}

void AnimatedMeshEffect::syncPartsFromScene()
{
    const std::string abs = ResolvedMeshPath();
    const MeshAnim::Scene* anim = abs.empty() ? nullptr : MeshAnim::Cache::instance()->GetOrLoad(abs);
    if(!anim)
    {
        part_overrides_.clear();
        return;
    }

    std::vector<PartOverride> next;
    next.reserve(anim->meshes.size());
    for(const MeshAnim::NodeMesh& nm : anim->meshes)
    {
        PartOverride po;
        po.part_id = nm.part_id;
        po.display_name = nm.display_name.empty() ? nm.part_id : nm.display_name;
        po.base_r = nm.base_r;
        po.base_g = nm.base_g;
        po.base_b = nm.base_b;
        /* Any assigned glTF material (incl. white+texture) uses Material tint. */
        const bool has_mat = nm.material_index >= 0
            && nm.material_index < (int)anim->materials.size();
        po.tint_mode = has_mat ? PartTintMode::Material : PartTintMode::Effect;
        if(nm.base_a < 0.01f
           || (has_mat && anim->materials[(size_t)nm.material_index].isFullyTransparent()))
        {
            po.visible = false;
            po.light_leds = false;
        }

        for(const PartOverride& old : part_overrides_)
        {
            if(old.part_id == nm.part_id)
            {
                po.visible = old.visible;
                po.light_leds = old.light_leds;
                po.tint_mode = old.tint_mode;
                po.override_r = old.override_r;
                po.override_g = old.override_g;
                po.override_b = old.override_b;
                po.emissive_scale = old.emissive_scale;
                break;
            }
        }
        next.push_back(std::move(po));
    }
    part_overrides_ = std::move(next);
}

void AnimatedMeshEffect::setPartSwatchColor(QPushButton* button, float r, float g, float b)
{
    if(!button)
    {
        return;
    }
    const QColor c(qRound(std::clamp(r, 0.0f, 1.0f) * 255.0f),
                   qRound(std::clamp(g, 0.0f, 1.0f) * 255.0f),
                   qRound(std::clamp(b, 0.0f, 1.0f) * 255.0f));
    button->setStyleSheet(QStringLiteral("background:%1;").arg(c.name()));
}

void AnimatedMeshEffect::markPartsDirty()
{
    samples_sequence_ = 0;
    local_samples_path_.clear(); /* rebuild local sample set (visibility / LEDs) */
    emit ParametersChanged();
}

AnimatedMeshEffect::PartOverride* AnimatedMeshEffect::selectedPart()
{
    if(selected_part_index_ < 0 || selected_part_index_ >= (int)part_overrides_.size())
    {
        return nullptr;
    }
    return &part_overrides_[(size_t)selected_part_index_];
}

const AnimatedMeshEffect::PartOverride* AnimatedMeshEffect::selectedPart() const
{
    if(selected_part_index_ < 0 || selected_part_index_ >= (int)part_overrides_.size())
    {
        return nullptr;
    }
    return &part_overrides_[(size_t)selected_part_index_];
}

void AnimatedMeshEffect::refreshPartListItem(int index)
{
    if(!parts_list_ || index < 0 || index >= parts_list_->count()
       || index >= (int)part_overrides_.size())
    {
        return;
    }
    const PartOverride& p = part_overrides_[(size_t)index];
    QListWidgetItem* item = parts_list_->item(index);
    if(!item)
    {
        return;
    }

    QString tint_tag = tr("Effect");
    if(p.tint_mode == PartTintMode::Material)
    {
        tint_tag = tr("Material");
    }
    else if(p.tint_mode == PartTintMode::Override)
    {
        tint_tag = tr("Override");
    }

    QString flags;
    if(!p.visible)
    {
        flags += tr(" · hidden");
    }
    if(!p.light_leds)
    {
        flags += tr(" · no LEDs");
    }

    item->setText(QStringLiteral("%1  [%2]%3")
                      .arg(QString::fromStdString(p.display_name), tint_tag, flags));
    item->setToolTip(QString::fromStdString(p.part_id));
}

void AnimatedMeshEffect::rebuildPartsUi()
{
    if(!parts_list_)
    {
        return;
    }

    const QString previous_id =
        (selected_part_index_ >= 0 && selected_part_index_ < (int)part_overrides_.size())
            ? QString::fromStdString(part_overrides_[(size_t)selected_part_index_].part_id)
            : QString();

    {
        QSignalBlocker block(parts_list_);
        parts_list_->clear();
        for(size_t i = 0; i < part_overrides_.size(); ++i)
        {
            parts_list_->addItem(QString());
            refreshPartListItem((int)i);
        }
    }

    int select = -1;
    if(!previous_id.isEmpty())
    {
        for(size_t i = 0; i < part_overrides_.size(); ++i)
        {
            if(part_overrides_[i].part_id == previous_id.toStdString())
            {
                select = (int)i;
                break;
            }
        }
    }
    if(select < 0 && !part_overrides_.empty())
    {
        select = 0;
    }
    selected_part_index_ = select;
    if(select >= 0)
    {
        parts_list_->setCurrentRow(select);
    }
    syncPartEditorFromSelection();
}

void AnimatedMeshEffect::syncPartEditorFromSelection()
{
    const PartOverride* part = selectedPart();
    const bool has = part != nullptr;
    if(part_editor_)
    {
        part_editor_->setEnabled(has);
    }
    if(!has)
    {
        if(part_editor_title_)
        {
            part_editor_title_->setText(
                part_overrides_.empty() ? tr("No parts — import a mesh.")
                                        : tr("Select a part to edit"));
        }
        if(part_id_label_)
        {
            part_id_label_->clear();
        }
        if(part_texture_note_)
        {
            part_texture_note_->hide();
            part_texture_note_->clear();
        }
        return;
    }

    if(part_editor_title_)
    {
        part_editor_title_->setText(
            tr("Editing: %1").arg(QString::fromStdString(part->display_name)));
    }
    if(part_id_label_)
    {
        part_id_label_->setText(QString::fromStdString(part->part_id));
    }

    if(part_visible_check_)
    {
        QSignalBlocker b(part_visible_check_);
        part_visible_check_->setChecked(part->visible);
    }
    if(part_leds_check_)
    {
        QSignalBlocker b(part_leds_check_);
        part_leds_check_->setChecked(part->light_leds);
    }
    if(part_tint_combo_)
    {
        QSignalBlocker b(part_tint_combo_);
        const int idx = part_tint_combo_->findData((int)part->tint_mode);
        part_tint_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    setPartSwatchColor(part_color_button_, part->override_r, part->override_g, part->override_b);
    if(part_material_swatch_)
    {
        const QColor mat(qRound(part->base_r * 255), qRound(part->base_g * 255),
                         qRound(part->base_b * 255));
        part_material_swatch_->setStyleSheet(
            QStringLiteral("background:%1; border:1px solid palette(mid);").arg(mat.name()));
    }
    if(part_texture_note_)
    {
        bool has_tex = false;
        const std::string path = ResolvedMeshPath();
        if(!path.empty() && selected_part_index_ >= 0)
        {
            const MeshAnim::Scene* anim = MeshAnim::Cache::instance()->GetOrLoad(path);
            if(anim && selected_part_index_ < (int)anim->meshes.size())
            {
                const MeshAnim::NodeMesh& nm = anim->meshes[(size_t)selected_part_index_];
                if(nm.material_index >= 0 && nm.material_index < (int)anim->materials.size())
                {
                    const int bt = anim->materials[(size_t)nm.material_index].base_tex;
                    has_tex = bt >= 0 && bt < (int)anim->images.size();
                }
            }
        }
        if(has_tex)
        {
            part_texture_note_->setText(
                tr("Has texture — Tint \"Material colour\" uses the file material/texture."));
            part_texture_note_->show();
        }
        else
        {
            part_texture_note_->hide();
            part_texture_note_->clear();
        }
    }
    if(part_emissive_row_)
    {
        part_emissive_row_->syncSliderValue(
            (int)std::lround(std::clamp(part->emissive_scale, 0.0f, 4.0f) * 100.0f),
            [](int v) { return QStringLiteral("%1%").arg(v); });
    }
}

void AnimatedMeshEffect::onPartListSelectionChanged()
{
    selected_part_index_ = parts_list_ ? parts_list_->currentRow() : -1;
    syncPartEditorFromSelection();
}

void AnimatedMeshEffect::onPartVisibleToggled(bool on)
{
    if(PartOverride* p = selectedPart())
    {
        p->visible = on;
        refreshPartListItem(selected_part_index_);
        markPartsDirty();
    }
}

void AnimatedMeshEffect::onPartLedsToggled(bool on)
{
    if(PartOverride* p = selectedPart())
    {
        p->light_leds = on;
        refreshPartListItem(selected_part_index_);
        markPartsDirty();
    }
}

void AnimatedMeshEffect::onPartTintModeChanged(int /*index*/)
{
    if(!part_tint_combo_)
    {
        return;
    }
    if(PartOverride* p = selectedPart())
    {
        p->tint_mode = (PartTintMode)part_tint_combo_->currentData().toInt();
        refreshPartListItem(selected_part_index_);
        markPartsDirty();
    }
}

void AnimatedMeshEffect::onPartPickColor()
{
    PartOverride* p = selectedPart();
    if(!p)
    {
        return;
    }
    const QColor cur(qRound(p->override_r * 255), qRound(p->override_g * 255),
                     qRound(p->override_b * 255));
    const QColor picked = QColorDialog::getColor(cur, this, tr("Part override colour"));
    if(!picked.isValid())
    {
        return;
    }
    p->override_r = (float)picked.redF();
    p->override_g = (float)picked.greenF();
    p->override_b = (float)picked.blueF();
    p->tint_mode = PartTintMode::Override;
    setPartSwatchColor(part_color_button_, p->override_r, p->override_g, p->override_b);
    if(part_tint_combo_)
    {
        QSignalBlocker b(part_tint_combo_);
        const int idx = part_tint_combo_->findData((int)PartTintMode::Override);
        if(idx >= 0)
        {
            part_tint_combo_->setCurrentIndex(idx);
        }
    }
    refreshPartListItem(selected_part_index_);
    markPartsDirty();
}

void AnimatedMeshEffect::onPartUseMaterialColor()
{
    PartOverride* p = selectedPart();
    if(!p)
    {
        return;
    }
    p->override_r = p->base_r;
    p->override_g = p->base_g;
    p->override_b = p->base_b;
    p->tint_mode = PartTintMode::Material;
    setPartSwatchColor(part_color_button_, p->override_r, p->override_g, p->override_b);
    if(part_tint_combo_)
    {
        QSignalBlocker b(part_tint_combo_);
        const int idx = part_tint_combo_->findData((int)PartTintMode::Material);
        if(idx >= 0)
        {
            part_tint_combo_->setCurrentIndex(idx);
        }
    }
    refreshPartListItem(selected_part_index_);
    markPartsDirty();
}

void AnimatedMeshEffect::onPartResetDefaults()
{
    PartOverride* p = selectedPart();
    if(!p)
    {
        return;
    }
    p->emissive_scale = 1.0f;
    p->override_r = p->base_r;
    p->override_g = p->base_g;
    p->override_b = p->base_b;
    bool has_mat = false;
    bool transparent = false;
    const std::string path = ResolvedMeshPath();
    const MeshAnim::Scene* anim = path.empty() ? nullptr : MeshAnim::Cache::instance()->GetOrLoad(path);
    if(anim && selected_part_index_ >= 0 && selected_part_index_ < (int)anim->meshes.size())
    {
        const MeshAnim::NodeMesh& nm = anim->meshes[(size_t)selected_part_index_];
        has_mat = nm.material_index >= 0 && nm.material_index < (int)anim->materials.size();
        transparent = nm.base_a < 0.01f
            || (has_mat && anim->materials[(size_t)nm.material_index].isFullyTransparent());
    }
    p->visible = !transparent;
    p->light_leds = !transparent;
    p->tint_mode = has_mat ? PartTintMode::Material : PartTintMode::Effect;
    syncPartEditorFromSelection();
    refreshPartListItem(selected_part_index_);
    markPartsDirty();
}

void AnimatedMeshEffect::onPartsShowAll()
{
    for(PartOverride& p : part_overrides_)
    {
        p.visible = true;
    }
    for(int i = 0; i < (int)part_overrides_.size(); ++i)
    {
        refreshPartListItem(i);
    }
    syncPartEditorFromSelection();
    markPartsDirty();
}

void AnimatedMeshEffect::onPartsHideAll()
{
    for(PartOverride& p : part_overrides_)
    {
        p.visible = false;
    }
    for(int i = 0; i < (int)part_overrides_.size(); ++i)
    {
        refreshPartListItem(i);
    }
    syncPartEditorFromSelection();
    markPartsDirty();
}

void AnimatedMeshEffect::onPartsLedsAllOn()
{
    for(PartOverride& p : part_overrides_)
    {
        p.light_leds = true;
    }
    for(int i = 0; i < (int)part_overrides_.size(); ++i)
    {
        refreshPartListItem(i);
    }
    syncPartEditorFromSelection();
    markPartsDirty();
}

void AnimatedMeshEffect::onPartsLedsAllOff()
{
    for(PartOverride& p : part_overrides_)
    {
        p.light_leds = false;
    }
    for(int i = 0; i < (int)part_overrides_.size(); ++i)
    {
        refreshPartListItem(i);
    }
    syncPartEditorFromSelection();
    markPartsDirty();
}

void AnimatedMeshEffect::importMeshFile(const QString& source_path)
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
    {
        return;
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    const filesystem::path meshes_dir = PluginSettingsPaths::MeshesDir(OpenRGB3DSpatialPlugin::APIPointer);
    ScenePropMeshPaths::SetMeshesRoot(meshes_dir);

    std::string install_err;
    const std::string dest_rel =
        MeshImport::InstallToMeshesDir(meshes_dir.string(), source_path.toStdString(), &install_err);
    if(dest_rel.empty())
    {
        if(path_label_)
        {
            path_label_->setText(tr("Import failed: %1").arg(QString::fromStdString(install_err)));
        }
        return;
    }

    mesh_asset_ = dest_rel;
    samples_sequence_ = 0;
    mesh_samples_.clear();
    const filesystem::path dest_path = meshes_dir / dest_rel;
    MeshAnim::Cache::instance()->Invalidate(dest_path.string());
    MeshAnim::Cache::instance()->GetOrLoad(dest_path.string());
    syncPartsFromScene();
    refreshPathLabel();
    refreshSummaryLabel();
    rebuildPartsUi();
    emit ParametersChanged();
}

void AnimatedMeshEffect::onBrowseMesh()
{
    QString start_dir;
    if(OpenRGB3DSpatialPlugin::APIPointer)
    {
        PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
        const filesystem::path meshes_dir =
            PluginSettingsPaths::MeshesDir(OpenRGB3DSpatialPlugin::APIPointer);
        std::error_code ec;
        filesystem::create_directories(meshes_dir, ec);
        start_dir = QString::fromStdString(meshes_dir.string());
    }
    const QString filter = QString::fromStdString(MeshImport::OpenFileFilter());
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Animated Mesh"), start_dir, filter);
    if(path.isEmpty())
    {
        return;
    }
    if(MeshImport::IsBlockedExtension(path.toStdString()))
    {
        if(path_label_)
        {
            path_label_->setText(tr("Unsupported format — export as .glb from Blender."));
        }
        return;
    }

    std::string err;
    MeshImport::TriangleMesh probe;
    if(!MeshImport::LoadTriangleMesh(path.toStdString(), &probe, &err) || probe.empty())
    {
        if(path_label_)
        {
            path_label_->setText(tr("Could not load mesh: %1").arg(QString::fromStdString(err)));
        }
        return;
    }
    importMeshFile(path);
}

void AnimatedMeshEffect::onClearMesh()
{
    mesh_asset_.clear();
    mesh_samples_.clear();
    sample_grid_heads_.clear();
    sample_grid_next_.clear();
    local_light_samples_.clear();
    local_samples_path_.clear();
    samples_static_pose_ = false;
    part_overrides_.clear();
    samples_sequence_ = 0;
    refreshPathLabel();
    refreshSummaryLabel();
    rebuildPartsUi();
    emit ParametersChanged();
}

void AnimatedMeshEffect::syncCustomSlidersFromModel()
{
    if(emissive_row_)
    {
        emissive_row_->syncSliderValue((int)std::lround(emissive_ * 100.0f), [](int v) {
            return QString::number(v / 100.0, 'f', 2);
        });
    }
    if(light_radius_row_)
    {
        light_radius_row_->syncSliderValue((int)std::lround(light_radius_ * 100.0f), [](int v) {
            return QStringLiteral("%1%").arg(v);
        });
    }
    syncLightModeUi();
    syncLedColorSourceUi();
}

void AnimatedMeshEffect::resolvePartRgb(const PartOverride& part, float time_sec,
                                        float* r, float* g, float* b) const
{
    float pr = 1.0f, pg = 1.0f, pb = 1.0f;

    /* Override always wins. Effect colour tints materials (never replaces on GPU). */
    if(part.tint_mode == PartTintMode::Override)
    {
        pr = part.override_r;
        pg = part.override_g;
        pb = part.override_b;
    }
    else if(led_color_source_ == LedColorSource::Effect)
    {
        if(GetRainbowMode())
        {
            const float hue = std::fmod(time_sec * GetColorCycleHz() * 360.0f + 720.0f, 360.0f);
            const RGBColor c = GetRainbowColor(hue);
            pr = RGBGetRValue(c) / 255.0f;
            pg = RGBGetGValue(c) / 255.0f;
            pb = RGBGetBValue(c) / 255.0f;
        }
        else if(UseEffectStripColormap())
        {
            /* Strip drives LED samples; keep mesh tint neutral so textures stay. */
            pr = 1.0f;
            pg = 1.0f;
            pb = 1.0f;
        }
        else
        {
            const RGBColor c = GetColorAtPosition(0.0f);
            pr = RGBGetRValue(c) / 255.0f;
            pg = RGBGetGValue(c) / 255.0f;
            pb = RGBGetBValue(c) / 255.0f;
        }
    }
    else if(part.tint_mode == PartTintMode::Material)
    {
        pr = part.base_r;
        pg = part.base_g;
        pb = part.base_b;
    }
    else
    {
        const RGBColor c = GetColorAtPosition(0.0f);
        pr = RGBGetRValue(c) / 255.0f;
        pg = RGBGetGValue(c) / 255.0f;
        pb = RGBGetBValue(c) / 255.0f;
    }
    *r = std::clamp(pr, 0.0f, 1.0f);
    *g = std::clamp(pg, 0.0f, 1.0f);
    *b = std::clamp(pb, 0.0f, 1.0f);
}

std::vector<AnimatedMeshEffect::PartRenderTint>
AnimatedMeshEffect::BuildPartRenderTints(float time_sec) const
{
    std::vector<PartRenderTint> out;
    out.reserve(part_overrides_.size());
    const float bright = std::clamp(effect_brightness / 100.0f, 0.0f, 1.0f);
    float emis = emissive_;
    if(emis < 0.0f) { emis = 0.0f; }
    if(emis > 8.0f) { emis = 8.0f; }

    for(size_t i = 0; i < part_overrides_.size(); ++i)
    {
        const PartOverride& part = part_overrides_[i];
        PartRenderTint pt;
        pt.part_index = (int)i;
        pt.visible = part.visible;
        pt.light_leds = part.light_leds;
        pt.emis_scale = std::max(0.05f, part.emissive_scale);
        float r, g, b;
        resolvePartRgb(part, time_sec, &r, &g, &b);
        /* Material: leave factors alone — mesh shader applies emissive + part emis_scale.
           Effect/Override: bake brightness into tint RGB. */
        if(part.tint_mode == PartTintMode::Material)
        {
            pt.r = r;
            pt.g = g;
            pt.b = b;
        }
        else
        {
            const float scale = emis * bright * pt.emis_scale;
            pt.r = std::min(1.0f, r * scale);
            pt.g = std::min(1.0f, g * scale);
            pt.b = std::min(1.0f, b * scale);
        }
        out.push_back(pt);
    }
    return out;
}

RGBColor AnimatedMeshEffect::sampleEffectColor(float x, float y, float z, float time,
                                               const GridContext3D& grid,
                                               float gradient_pos01) const
{
    const Vector3D origin = GetEffectOriginGrid(grid);
    const Vector3D sample_pos{x, y, z};
    const float y_norm = SampleStratumYNorm01(y, grid, origin);

    SpatialLayerCore::Basis basis{};
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(
        GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map{};
    SpatialLayerCore::InitAudioEffectMapperSettings(
        map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = y_norm;

    const float density = GetHueBandDensity();
    const float driver = std::fmod(gradient_pos01 * density
                                       + time * GetColorCycleHz()
                                       + CalculateProgress(time) * 0.25f
                                       + 2.0f,
                                   1.0f);

    if(UseEffectStripColormap())
    {
        const float strip_p01 = SampleEffectStripColormap01(
            GetEffectStripColormapRepeats(),
            GetEffectStripColormapUnfold(),
            GetEffectStripColormapDirectionDeg(),
            driver,
            time,
            grid,
            GetNormalizedSize(),
            origin,
            sample_pos);
        return ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
    }
    if(GetRainbowMode())
    {
        float hue = driver * 360.0f;
        hue = ApplySpatialRainbowHue(hue, gradient_pos01, basis, sp, map, time, &grid);
        return GetRainbowColor(std::fmod(hue + 720.0f, 360.0f));
    }
    const float pal = ApplySpatialPalette01(driver, basis, sp, map, time, &grid);
    return GetColorAtPosition(std::clamp(pal, 0.0f, 1.0f));
}

void AnimatedMeshEffect::ensureLocalLightSamples(const MeshAnim::Scene& anim)
{
    const std::string path = ResolvedMeshPath();
    if(path == local_samples_path_ && !local_light_samples_.empty())
    {
        return;
    }
    local_light_samples_.clear();
    local_samples_path_ = path;

    for(size_t pi = 0; pi < anim.meshes.size(); ++pi)
    {
        if(pi < part_overrides_.size()
           && (!part_overrides_[pi].visible || !part_overrides_[pi].light_leds))
        {
            continue;
        }
        const MeshAnim::NodeMesh& nm = anim.meshes[pi];
        const size_t tri_count = nm.local_positions.size() / 9;
        if(tri_count == 0)
        {
            continue;
        }
        const size_t remaining = (kMaxMeshLightSamples > local_light_samples_.size())
                                     ? (kMaxMeshLightSamples - local_light_samples_.size())
                                     : 0;
        if(remaining == 0)
        {
            break;
        }
        const bool has_uvs = nm.hasUvs() && (nm.local_uvs.size() / 6) >= tri_count;
        const size_t step = (tri_count > remaining)
                                ? std::max<size_t>(1, tri_count / remaining)
                                : 1;
        for(size_t t = 0; t < tri_count && local_light_samples_.size() < kMaxMeshLightSamples; t += step)
        {
            const size_t i = t * 9;
            if(i + 8 >= nm.local_positions.size())
            {
                break;
            }
            LocalSample s;
            s.lx = (nm.local_positions[i] + nm.local_positions[i + 3] + nm.local_positions[i + 6]) / 3.0f;
            s.ly = (nm.local_positions[i + 1] + nm.local_positions[i + 4] + nm.local_positions[i + 7]) / 3.0f;
            s.lz = (nm.local_positions[i + 2] + nm.local_positions[i + 5] + nm.local_positions[i + 8]) / 3.0f;
            s.part_index = (int)pi;
            s.node_index = nm.node_index;
            if(has_uvs)
            {
                const size_t ui = t * 6;
                s.u = (nm.local_uvs[ui] + nm.local_uvs[ui + 2] + nm.local_uvs[ui + 4]) / 3.0f;
                s.v = (nm.local_uvs[ui + 1] + nm.local_uvs[ui + 3] + nm.local_uvs[ui + 5]) / 3.0f;
            }
            local_light_samples_.push_back(s);
        }
    }
}

void AnimatedMeshEffect::rebuildMeshSamples(float time_sec, const GridContext3D& grid)
{
    mesh_samples_.clear();
    const std::string path = ResolvedMeshPath();
    if(path.empty())
    {
        return;
    }

    float hx = 1.0f, hy = 1.0f, hz = 1.0f;
    GetVolumeHalfExtents(grid, hx, hy, hz);
    samples_hx_ = hx;
    samples_hy_ = hy;
    samples_hz_ = hz;

    float fit_hx = hx, fit_hy = hy, fit_hz = hz;
    GetMeshFitHalfExtents(grid, fit_hx, fit_hy, fit_hz);
    samples_fit_hx_ = fit_hx;
    samples_fit_hy_ = fit_hy;
    samples_fit_hz_ = fit_hz;

    const MeshAnim::Scene* anim = MeshAnim::Cache::instance()->GetOrLoad(path);
    if(!anim)
    {
        return;
    }
    if(part_overrides_.empty())
    {
        syncPartsFromScene();
    }

    ensureLocalLightSamples(*anim);
    if(local_light_samples_.empty())
    {
        return;
    }

    /* Bind-pose AABB fit (matches GPU viewport path). Scale sizes the mesh only. */
    const MeshImport::TriangleMesh& bp = anim->bind_pose;
    float min_x = bp.aabb_min[0], min_y = bp.aabb_min[1], min_z = bp.aabb_min[2];
    float max_x = bp.aabb_max[0], max_y = bp.aabb_max[1], max_z = bp.aabb_max[2];
    if(bp.empty())
    {
        min_x = min_y = min_z = 0.0f;
        max_x = max_y = max_z = 1.0f;
    }
    fit_cx_ = 0.5f * (min_x + max_x);
    fit_cy_ = 0.5f * (min_y + max_y);
    fit_cz_ = 0.5f * (min_z + max_z);
    const float sx = std::max(max_x - min_x, 1e-6f);
    const float sy = std::max(max_y - min_y, 1e-6f);
    const float sz = std::max(max_z - min_z, 1e-6f);
    /* Match viewport: uniform scale so LED samples stay aligned with a round mesh. */
    const float s = std::min((2.0f * fit_hx) / sx, std::min((2.0f * fit_hy) / sy, (2.0f * fit_hz) / sz));
    fit_scale_x_ = s;
    fit_scale_y_ = s;
    fit_scale_z_ = s;

    samples_static_pose_ = !anim->hasAnimation();

    std::vector<MeshAnim::Mat4> worlds;
    MeshAnim::EvaluateNodeWorlds(*anim, time_sec * PlaybackRate(), &worlds);

    mesh_samples_.reserve(local_light_samples_.size());
    for(const LocalSample& ls : local_light_samples_)
    {
        if(ls.part_index >= 0 && ls.part_index < (int)part_overrides_.size()
           && (!part_overrides_[(size_t)ls.part_index].visible
               || !part_overrides_[(size_t)ls.part_index].light_leds))
        {
            continue;
        }
        float wx = ls.lx, wy = ls.ly, wz = ls.lz;
        if(ls.node_index >= 0 && ls.node_index < (int)worlds.size())
        {
            MeshAnim::TransformPoint(worlds[(size_t)ls.node_index], ls.lx, ls.ly, ls.lz, &wx, &wy, &wz);
        }
        SamplePoint p;
        p.x = (wx - fit_cx_) * fit_scale_x_;
        p.y = (wy - fit_cy_) * fit_scale_y_;
        p.z = (wz - fit_cz_) * fit_scale_z_;
        p.u = ls.u;
        p.v = ls.v;
        p.part_index = ls.part_index;
        p.r = 1.0f;
        p.g = 1.0f;
        p.b = 1.0f;
        p.part_emis = 1.0f;
        p.baked_rgb = false;

        if(ls.part_index >= 0 && ls.part_index < (int)part_overrides_.size())
        {
            const PartOverride& part = part_overrides_[(size_t)ls.part_index];
            p.part_emis = std::max(0.05f, part.emissive_scale);
            if(part.tint_mode == PartTintMode::Override)
            {
                p.r = part.override_r;
                p.g = part.override_g;
                p.b = part.override_b;
                p.baked_rgb = true;
            }
            else if(part.tint_mode == PartTintMode::Material
                    || led_color_source_ == LedColorSource::Materials)
            {
                /* Same MeshUnlit-ish look as the viewport sculpture (LED Off).
                   Bake at neutral exposure — Soft glow applies Brightness/Emissive. */
                float pr = part.base_r;
                float pg = part.base_g;
                float pb = part.base_b;
                if(ls.part_index < (int)anim->meshes.size())
                {
                    const MeshAnim::NodeMesh& nm = anim->meshes[(size_t)ls.part_index];
                    MeshAnim::SampleMaterialViewportLook(
                        *anim, nm.material_index, ls.u, ls.v, 1.0f, &pr, &pg, &pb);
                }
                p.r = pr;
                p.g = pg;
                p.b = pb;
                p.baked_rgb = true;
            }
        }
        mesh_samples_.push_back(p);
    }

    float smallest = samples_hx_;
    if(samples_hy_ < smallest) { smallest = samples_hy_; }
    if(samples_hz_ < smallest) { smallest = samples_hz_; }
    if(smallest < 1e-4f) { smallest = 1e-4f; }
    rebuildSampleGrid(effectiveLightRadius(smallest));
}

void AnimatedMeshEffect::rebuildSampleGrid(float search_radius)
{
    sample_grid_heads_.clear();
    sample_grid_next_.clear();
    sample_grid_nx_ = sample_grid_ny_ = sample_grid_nz_ = 0;
    sample_grid_cell_ = 0.0f;
    sample_grid_radius_ = search_radius;
    if(mesh_samples_.empty() || search_radius <= 1e-6f)
    {
        return;
    }

    float min_x = FLT_MAX, min_y = FLT_MAX, min_z = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX, max_z = -FLT_MAX;
    for(const SamplePoint& p : mesh_samples_)
    {
        min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
        min_z = std::min(min_z, p.z); max_z = std::max(max_z, p.z);
    }
    const float pad = search_radius;
    min_x -= pad; min_y -= pad; min_z -= pad;
    max_x += pad; max_y += pad; max_z += pad;

    /* Cell ≈ search radius so a query checks a small neighbourhood. */
    float cell = std::max(search_radius, 1e-3f);
    int nx = std::max(1, (int)std::ceil((max_x - min_x) / cell));
    int ny = std::max(1, (int)std::ceil((max_y - min_y) / cell));
    int nz = std::max(1, (int)std::ceil((max_z - min_z) / cell));
    constexpr int kMaxAxis = 48;
    if(nx > kMaxAxis || ny > kMaxAxis || nz > kMaxAxis)
    {
        const float sx = (max_x - min_x) / (float)kMaxAxis;
        const float sy = (max_y - min_y) / (float)kMaxAxis;
        const float sz = (max_z - min_z) / (float)kMaxAxis;
        cell = std::max(cell, std::max(sx, std::max(sy, sz)));
        nx = std::max(1, (int)std::ceil((max_x - min_x) / cell));
        ny = std::max(1, (int)std::ceil((max_y - min_y) / cell));
        nz = std::max(1, (int)std::ceil((max_z - min_z) / cell));
        nx = std::min(nx, kMaxAxis);
        ny = std::min(ny, kMaxAxis);
        nz = std::min(nz, kMaxAxis);
    }

    sample_grid_ox_ = min_x;
    sample_grid_oy_ = min_y;
    sample_grid_oz_ = min_z;
    sample_grid_cell_ = cell;
    sample_grid_nx_ = nx;
    sample_grid_ny_ = ny;
    sample_grid_nz_ = nz;
    sample_grid_heads_.assign((size_t)nx * (size_t)ny * (size_t)nz, -1);
    sample_grid_next_.assign(mesh_samples_.size(), -1);

    for(int i = 0; i < (int)mesh_samples_.size(); ++i)
    {
        const SamplePoint& p = mesh_samples_[(size_t)i];
        int ix = (int)std::floor((p.x - sample_grid_ox_) / cell);
        int iy = (int)std::floor((p.y - sample_grid_oy_) / cell);
        int iz = (int)std::floor((p.z - sample_grid_oz_) / cell);
        ix = std::clamp(ix, 0, nx - 1);
        iy = std::clamp(iy, 0, ny - 1);
        iz = std::clamp(iz, 0, nz - 1);
        const int cell_i = (iz * ny + iy) * nx + ix;
        sample_grid_next_[(size_t)i] = sample_grid_heads_[(size_t)cell_i];
        sample_grid_heads_[(size_t)cell_i] = i;
    }
}

bool AnimatedMeshEffect::findNearestSample(float lx, float ly, float lz, float search_radius,
                                           int* out_index, float* out_d2) const
{
    if(out_index) { *out_index = -1; }
    if(out_d2) { *out_d2 = 1.0e30f; }
    if(mesh_samples_.empty())
    {
        return false;
    }

    float min_d2 = 1.0e30f;
    int best = -1;
    const float r2 = search_radius * search_radius;

    if(sample_grid_heads_.empty() || sample_grid_cell_ <= 0.0f)
    {
        for(int i = 0; i < (int)mesh_samples_.size(); ++i)
        {
            const SamplePoint& p = mesh_samples_[(size_t)i];
            const float dx = lx - p.x;
            const float dy = ly - p.y;
            const float dz = lz - p.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if(d2 < min_d2)
            {
                min_d2 = d2;
                best = i;
            }
        }
    }
    else
    {
        const float cell = sample_grid_cell_;
        const int nx = sample_grid_nx_;
        const int ny = sample_grid_ny_;
        const int nz = sample_grid_nz_;
        const int reach = std::max(1, (int)std::ceil(search_radius / cell));
        int cx = (int)std::floor((lx - sample_grid_ox_) / cell);
        int cy = (int)std::floor((ly - sample_grid_oy_) / cell);
        int cz = (int)std::floor((lz - sample_grid_oz_) / cell);
        const int ix0 = std::max(0, cx - reach);
        const int iy0 = std::max(0, cy - reach);
        const int iz0 = std::max(0, cz - reach);
        const int ix1 = std::min(nx - 1, cx + reach);
        const int iy1 = std::min(ny - 1, cy + reach);
        const int iz1 = std::min(nz - 1, cz + reach);

        for(int iz = iz0; iz <= iz1; ++iz)
        {
            for(int iy = iy0; iy <= iy1; ++iy)
            {
                for(int ix = ix0; ix <= ix1; ++ix)
                {
                    int idx = sample_grid_heads_[(size_t)((iz * ny + iy) * nx + ix)];
                    while(idx >= 0)
                    {
                        const SamplePoint& p = mesh_samples_[(size_t)idx];
                        const float dx = lx - p.x;
                        const float dy = ly - p.y;
                        const float dz = lz - p.z;
                        const float d2 = dx * dx + dy * dy + dz * dz;
                        if(d2 < min_d2)
                        {
                            min_d2 = d2;
                            best = idx;
                        }
                        idx = sample_grid_next_[(size_t)idx];
                    }
                }
            }
        }
    }

    if(best < 0 || min_d2 > r2)
    {
        return false;
    }
    if(out_index) { *out_index = best; }
    if(out_d2) { *out_d2 = min_d2; }
    return true;
}

bool AnimatedMeshEffect::PrefersTrueGpuVolumeSamples() const
{
    return !mesh_asset_.empty() && light_mode_ != MeshLightMode::Off;
}

std::uint32_t AnimatedMeshEffect::PackNorm01Key(float n1, float n2, float n3)
{
    const auto q = [](float v) -> std::uint32_t {
        return (std::uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 1023.0f) & 1023u;
    };
    return (q(n1) << 20) | (q(n2) << 10) | q(n3);
}

void AnimatedMeshEffect::ensureGlowVolumeBody()
{
    if(glow_body_set_)
    {
        return;
    }
    glow_volume_.setFragmentBody(QString::fromUtf8(kMeshGlowVolumeBody));
    glow_volume_.setResolution(18);
    glow_body_set_ = true;
}

QImage AnimatedMeshEffect::packGlowSampleMedia(float hx, float hy, float hz, int* out_count) const
{
    const int n_src = (int)mesh_samples_.size();
    const int n = std::min(n_src, kMaxGpuGlowSamples);
    if(out_count)
    {
        *out_count = n;
    }
    if(n <= 0)
    {
        return {};
    }
    const int step = (n_src > n) ? std::max(1, n_src / n) : 1;
    QImage img(n, 2, QImage::Format_RGBA8888);
    img.fill(0);
    const float inv_hx = 1.0f / std::max(hx, 1e-4f);
    const float inv_hy = 1.0f / std::max(hy, 1e-4f);
    const float inv_hz = 1.0f / std::max(hz, 1e-4f);
    int written = 0;
    for(int si = 0; si < n_src && written < n; si += step, ++written)
    {
        const SamplePoint& p = mesh_samples_[(size_t)si];
        const float nx = std::clamp(0.5f + 0.5f * p.x * inv_hx, 0.0f, 1.0f);
        const float ny = std::clamp(0.5f + 0.5f * p.y * inv_hy, 0.0f, 1.0f);
        const float nz = std::clamp(0.5f + 0.5f * p.z * inv_hz, 0.0f, 1.0f);
        const auto q8 = [](float v) -> int {
            return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
        };
        img.setPixel(written, 0, qRgba(q8(nx), q8(ny), q8(nz), 255));
        img.setPixel(written, 1, qRgba(q8(p.r), q8(p.g), q8(p.b), q8(std::clamp(p.part_emis / 4.0f, 0.0f, 1.0f))));
    }
    return img;
}

bool AnimatedMeshEffect::prepareGlowVolume(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    ensureGlowVolumeBody();
    if(mesh_samples_.empty())
    {
        return false;
    }
    float hx = samples_hx_, hy = samples_hy_, hz = samples_hz_;
    if(hx < 1e-4f || hy < 1e-4f || hz < 1e-4f)
    {
        GetVolumeHalfExtents(grid, hx, hy, hz);
    }
    int n = 0;
    const QImage media = packGlowSampleMedia(hx, hy, hz, &n);
    if(media.isNull() || n <= 0)
    {
        glow_gpu_sample_count_ = 0;
        return false;
    }
    glow_gpu_sample_count_ = n;
    glow_volume_.setMediaTexture(media, false);

    float smallest = hx;
    if(hy < smallest) { smallest = hy; }
    if(hz < smallest) { smallest = hz; }
    const float radius = effectiveLightRadius(std::max(smallest, 1e-4f));

    float params[SpatialVolumeFieldEngine::kMaxParams] = {};
    params[0] = hx;
    params[1] = hy;
    params[2] = hz;
    params[3] = radius;
    params[4] = (float)n;
    params[5] = (light_mode_ == MeshLightMode::Surface) ? 1.0f : 0.0f;
    params[6] = std::clamp(GetNormalizedDetail(), 0.0f, 1.0f);
    params[7] = std::clamp(GetBrightness() / 100.0f, 0.0f, 1.0f);
    params[8] = std::clamp(emissive_, 0.0f, 8.0f);
    params[9] = UsesSpatialEffectColour() ? 1.0f : 0.0f;
    params[10] = std::max(0.25f, GetHueBandDensity());
    params[11] = GetColorCycleHz();
    return glow_volume_.prepareProgram(render_sequence, time_sec, params, 12);
}

bool AnimatedMeshEffect::bakeGlowVolumeGpu(const float* sample_xyz, int count, float time,
                                           const GridContext3D& grid)
{
    if(!sample_xyz || count <= 0 || glow_gpu_sample_count_ <= 0 || !glow_volume_.isAvailable())
    {
        return false;
    }

    std::unordered_map<std::uint32_t, int> unique_index;
    unique_index.reserve((size_t)std::max(16, count / 2));
    std::vector<float> n01;
    n01.reserve((size_t)count * 3u);
    std::vector<std::uint32_t> keys;
    keys.reserve((size_t)count);

    for(int i = 0; i < count; ++i)
    {
        const float x = sample_xyz[(size_t)i * 3u + 0u];
        const float y = sample_xyz[(size_t)i * 3u + 1u];
        const float z = sample_xyz[(size_t)i * 3u + 2u];
        float u = 0.5f, v = 0.5f, w = 0.5f;
        if(!SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
        {
            continue;
        }
        const std::uint32_t key = PackNorm01Key(u, v, w);
        if(unique_index.find(key) != unique_index.end())
        {
            continue;
        }
        const int idx = (int)unique_index.size();
        unique_index.emplace(key, idx);
        keys.push_back(key);
        n01.push_back(u);
        n01.push_back(v);
        n01.push_back(w);
    }
    const int unique = (int)unique_index.size();
    if(unique <= 0)
    {
        return true;
    }

    std::vector<float> rgb((size_t)unique * 3u, 0.0f);
    if(!glow_volume_.evaluateAtPoints(n01.data(), unique, rgb.data()))
    {
        return false;
    }

    true_sample_map_.reserve((size_t)unique);
    for(size_t i = 0; i < keys.size(); ++i)
    {
        const int r = (int)std::lround(std::clamp(rgb[i * 3u + 0u], 0.0f, 1.0f) * 255.0f);
        const int g = (int)std::lround(std::clamp(rgb[i * 3u + 1u], 0.0f, 1.0f) * 255.0f);
        const int b = (int)std::lround(std::clamp(rgb[i * 3u + 2u], 0.0f, 1.0f) * 255.0f);
        true_sample_map_[keys[i]] = ToRGBColor(r, g, b);
    }
    (void)time;
    (void)grid;
    return true;
}

void AnimatedMeshEffect::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    true_sample_map_.clear();
    if(mesh_asset_.empty() || light_mode_ == MeshLightMode::Off)
    {
        mesh_samples_.clear();
        sample_grid_heads_.clear();
        sample_grid_next_.clear();
        glow_gpu_sample_count_ = 0;
        samples_sequence_ = render_sequence;
        samples_time_ = time_sec;
        return;
    }

    bool need_rebuild = mesh_samples_.empty();
    if(!need_rebuild && samples_sequence_ != render_sequence)
    {
        if(!samples_static_pose_)
        {
            need_rebuild = true;
        }
        else
        {
            float hx = 1.0f, hy = 1.0f, hz = 1.0f;
            GetVolumeHalfExtents(grid, hx, hy, hz);
            float fit_hx = hx, fit_hy = hy, fit_hz = hz;
            GetMeshFitHalfExtents(grid, fit_hx, fit_hy, fit_hz);
            if(std::fabs(hx - samples_hx_) > 1e-4f
               || std::fabs(hy - samples_hy_) > 1e-4f
               || std::fabs(hz - samples_hz_) > 1e-4f
               || std::fabs(fit_hx - samples_fit_hx_) > 1e-4f
               || std::fabs(fit_hy - samples_fit_hy_) > 1e-4f
               || std::fabs(fit_hz - samples_fit_hz_) > 1e-4f)
            {
                need_rebuild = true;
            }
            else
            {
                float smallest = samples_hx_;
                if(samples_hy_ < smallest) { smallest = samples_hy_; }
                if(samples_hz_ < smallest) { smallest = samples_hz_; }
                const float radius = effectiveLightRadius(std::max(smallest, 1e-4f));
                if(std::fabs(radius - sample_grid_radius_) > 1e-3f)
                {
                    rebuildSampleGrid(radius);
                }
            }
        }
    }
    if(need_rebuild)
    {
        rebuildMeshSamples(time_sec, grid);
    }
    samples_sequence_ = render_sequence;
    samples_time_ = time_sec;
    prepareGlowVolume(render_sequence, time_sec, grid);
}

RGBColor AnimatedMeshEffect::evalMeshLightAt(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(mesh_asset_.empty() || light_mode_ == MeshLightMode::Off)
    {
        return ToRGBColor(0, 0, 0);
    }
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return ToRGBColor(0, 0, 0);
    }
    if(mesh_samples_.empty())
    {
        rebuildMeshSamples(samples_time_ < 0.0f ? time : samples_time_, grid);
        if(mesh_samples_.empty())
        {
            return ToRGBColor(0, 0, 0);
        }
    }

    float smallest = samples_hx_;
    if(samples_hy_ < smallest) { smallest = samples_hy_; }
    if(samples_hz_ < smallest) { smallest = samples_hz_; }
    if(smallest < 1e-4f) { smallest = 1e-4f; }
    const float radius = effectiveLightRadius(smallest);
    if(radius <= 0.0f)
    {
        return ToRGBColor(0, 0, 0);
    }

    const Vector3D origin = GetEffectOriginGrid(grid);
    const Vector3D local = VolumeWorldDeltaToLocal(x - origin.x, y - origin.y, z - origin.z);

    int nearest_i = -1;
    float min_d2 = 1.0e30f;
    if(!findNearestSample(local.x, local.y, local.z, radius, &nearest_i, &min_d2))
    {
        return ToRGBColor(0, 0, 0);
    }

    const float dist = std::sqrt(min_d2);
    float t = 1.0f - (dist / radius);
    if(light_mode_ == MeshLightMode::Surface)
    {
        t = (t > 0.35f) ? 1.0f : (t / 0.35f);
    }
    else
    {
        const float detail_n = std::clamp(GetNormalizedDetail(), 0.0f, 1.0f);
        const float exponent = 1.0f + detail_n * 2.5f;
        t = std::pow(std::max(0.0f, t), exponent);
    }

    const SamplePoint& nearest = mesh_samples_[(size_t)nearest_i];
    float part_emis = nearest.part_emis;
    const bool spatial_look = UsesSpatialEffectColour();

    RGBColor effect_c;
    if(nearest.baked_rgb)
    {
        effect_c = ToRGBColor((int)std::lround(nearest.r * 255.0f),
                              (int)std::lround(nearest.g * 255.0f),
                              (int)std::lround(nearest.b * 255.0f));
        if(spatial_look)
        {
            float u = 0.5f, v = 0.5f, w = 0.5f;
            float gradient = 0.5f;
            if(SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
            {
                gradient = std::clamp((u + w) * 0.5f, 0.0f, 1.0f);
            }
            const RGBColor tint = sampleEffectColor(x, y, z, time, grid, gradient);
            const float tr = std::max(0.15f, RGBGetRValue(tint) / 255.0f);
            const float tg = std::max(0.15f, RGBGetGValue(tint) / 255.0f);
            const float tb = std::max(0.15f, RGBGetBValue(tint) / 255.0f);
            effect_c = MulRgb(effect_c, tr, tg, tb, 1.0f);
        }
    }
    else
    {
        float u = 0.5f, v = 0.5f, w = 0.5f;
        float gradient = 0.5f;
        if(SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
        {
            gradient = std::clamp((u + w) * 0.5f, 0.0f, 1.0f);
        }
        effect_c = sampleEffectColor(x, y, z, time, grid, gradient);
        if(spatial_look && nearest.part_index >= 0
           && nearest.part_index < (int)part_overrides_.size()
           && part_overrides_[(size_t)nearest.part_index].tint_mode == PartTintMode::Override)
        {
            float pr, pg, pb;
            resolvePartRgb(part_overrides_[(size_t)nearest.part_index], time, &pr, &pg, &pb);
            effect_c = MulRgb(effect_c, pr, pg, pb, 1.0f);
        }
    }

    /* Match GPU Soft glow: falloff × Brightness × soft Emissive, hue-preserving. */
    float emissive_boost = std::clamp(emissive_ * part_emis, 0.0f, 8.0f);
    float gain = t * (GetBrightness() / 100.0f)
                 * (0.55f + 0.45f * std::min(emissive_boost, 4.0f) / 4.0f);
    float rr = (RGBGetRValue(effect_c) / 255.0f) * gain;
    float rg = (RGBGetGValue(effect_c) / 255.0f) * gain;
    float rb = (RGBGetBValue(effect_c) / 255.0f) * gain;
    const float peak = std::max(rr, std::max(rg, rb));
    if(peak > 1.0f)
    {
        rr /= peak;
        rg /= peak;
        rb /= peak;
    }
    return ToRGBColor((int)std::lround(std::clamp(rr, 0.0f, 1.0f) * 255.0f),
                      (int)std::lround(std::clamp(rg, 0.0f, 1.0f) * 255.0f),
                      (int)std::lround(std::clamp(rb, 0.0f, 1.0f) * 255.0f));
}

void AnimatedMeshEffect::BakeTrueGpuVolumeSamples(const float* sample_xyz, int count, float time,
                                                  const GridContext3D& grid)
{
    true_sample_map_.clear();
    if(!sample_xyz || count <= 0 || !PrefersTrueGpuVolumeSamples())
    {
        return;
    }
    if(mesh_samples_.empty())
    {
        rebuildMeshSamples(time, grid);
        if(mesh_samples_.empty())
        {
            return;
        }
        prepareGlowVolume(samples_sequence_, time, grid);
    }

    /* Prefer GPU splat (volumeMain over packed samples); CPU nearest only if GL fails. */
    if(bakeGlowVolumeGpu(sample_xyz, count, time, grid))
    {
        return;
    }

    true_sample_map_.reserve((size_t)std::max(16, count / 2));
    for(int i = 0; i < count; ++i)
    {
        const float x = sample_xyz[(size_t)i * 3u + 0u];
        const float y = sample_xyz[(size_t)i * 3u + 1u];
        const float z = sample_xyz[(size_t)i * 3u + 2u];
        float u = 0.5f, v = 0.5f, w = 0.5f;
        if(!SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
        {
            continue;
        }
        const std::uint32_t key = PackNorm01Key(u, v, w);
        if(true_sample_map_.find(key) != true_sample_map_.end())
        {
            continue;
        }
        true_sample_map_.emplace(key, evalMeshLightAt(x, y, z, time, grid));
    }
}

RGBColor AnimatedMeshEffect::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(!true_sample_map_.empty())
    {
        float u = 0.5f, v = 0.5f, w = 0.5f;
        if(SampleVolumeLocalUv01(x, y, z, grid, &u, &v, &w))
        {
            const auto it = true_sample_map_.find(PackNorm01Key(u, v, w));
            if(it != true_sample_map_.end())
            {
                return it->second;
            }
        }
    }
    return evalMeshLightAt(x, y, z, time, grid);
}

nlohmann::json AnimatedMeshEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mesh_asset"] = mesh_asset_;
    j["emissive"] = emissive_;
    j["light_radius"] = light_radius_;
    j["light_mode"] = (int)light_mode_;
    j["led_color_source"] = (int)led_color_source_;
    nlohmann::json parts = nlohmann::json::array();
    for(const PartOverride& p : part_overrides_)
    {
        parts.push_back({
            {"id", p.part_id},
            {"visible", p.visible},
            {"light_leds", p.light_leds},
            {"tint_mode", (int)p.tint_mode},
            {"override_r", p.override_r},
            {"override_g", p.override_g},
            {"override_b", p.override_b},
            {"emissive_scale", p.emissive_scale}
        });
    }
    j["parts"] = parts;
    return j;
}

void AnimatedMeshEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("mesh_asset") && settings["mesh_asset"].is_string())
    {
        mesh_asset_ = settings["mesh_asset"].get<std::string>();
    }
    emissive_ = settings.value("emissive", 2.0f);
    light_radius_ = settings.value("light_radius", 0.35f);
    {
        const int mode = settings.value("light_mode", (int)MeshLightMode::Glow);
        light_mode_ = (mode >= (int)MeshLightMode::Off && mode <= (int)MeshLightMode::Glow)
                          ? (MeshLightMode)mode
                          : MeshLightMode::Glow;
    }
    {
        const int src = settings.value("led_color_source", (int)LedColorSource::Materials);
        led_color_source_ = (src >= (int)LedColorSource::Materials && src <= (int)LedColorSource::Effect)
                                ? (LedColorSource)src
                                : LedColorSource::Materials;
    }
    part_overrides_.clear();
    if(settings.contains("parts") && settings["parts"].is_array())
    {
        for(const auto& jp : settings["parts"])
        {
            PartOverride p;
            p.part_id = jp.value("id", std::string{});
            p.visible = jp.value("visible", true);
            p.light_leds = jp.value("light_leds", true);
            p.tint_mode = (PartTintMode)jp.value("tint_mode", (int)PartTintMode::Material);
            p.override_r = jp.value("override_r", 1.0f);
            p.override_g = jp.value("override_g", 1.0f);
            p.override_b = jp.value("override_b", 1.0f);
            p.emissive_scale = jp.value("emissive_scale", 1.0f);
            if(!p.part_id.empty())
            {
                part_overrides_.push_back(std::move(p));
            }
        }
    }

    samples_sequence_ = 0;
    mesh_samples_.clear();
    syncPartsFromScene();
    syncCustomSlidersFromModel();
    refreshPathLabel();
    refreshSummaryLabel();
    rebuildPartsUi();
}
