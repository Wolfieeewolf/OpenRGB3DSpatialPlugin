// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialEffect3D.h"
#include "SpatialVolumeFieldAssist.h"
#include "viewport/MeshAnim.h"
#include "viewport/MeshImport.h"

#include <QImage>
#include <QString>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;
class EffectSliderRow;

/**
 * Stack effect: place an animated/static glTF/GLB via volume tools.
 * LEDs / room grid light from proximity to contributing parts.
 * Speed = clip rate; Frequency/Detail/Size drive spatial colour + falloff.
 * Parts keep material baseColor / textures with per-part overrides.
 */
class AnimatedMeshEffect : public SpatialEffect3D
{
    Q_OBJECT

public:
    enum class PartTintMode : int
    {
        Effect = 0,
        Material = 1,
        Override = 2
    };

    /** How the mesh drives LEDs / room grid (viewport mesh always draws). */
    enum class MeshLightMode : int
    {
        Off = 0,      /**< No LED / grid lighting from this effect. */
        Surface = 1,  /**< Mesh itself is the light — tight surface only (good for lasers). */
        Glow = 2      /**< Soft wash using Light radius. */
    };

    /** What colours LEDs + viewport mesh use (Rainbow only applies in Effect mode). */
    enum class LedColorSource : int
    {
        Materials = 0, /**< File materials / textures (original look). */
        Effect = 1     /**< Effect colour / Rainbow / strip colormap. */
    };

    struct PartOverride
    {
        std::string part_id;
        std::string display_name;
        bool visible = true;
        bool light_leds = true;
        PartTintMode tint_mode = PartTintMode::Material;
        float override_r = 1.0f;
        float override_g = 1.0f;
        float override_b = 1.0f;
        float emissive_scale = 1.0f;
        float base_r = 1.0f;
        float base_g = 1.0f;
        float base_b = 1.0f;
    };

    /** Viewport / LED resolved part colour (linear 0–1 RGB before brightness). */
    struct PartRenderTint
    {
        int part_index = 0;
        bool visible = true;
        bool light_leds = true;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float emis_scale = 1.0f;
    };

    explicit AnimatedMeshEffect(QWidget* parent = nullptr);

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    bool PrefersTrueGpuVolumeSamples() const override;
    void BakeTrueGpuVolumeSamples(const float* sample_xyz, int count, float time,
                                  const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return true; }
    void GetVolumeHalfExtents(const GridContext3D& grid, float& hx, float& hy, float& hz) const override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    const std::string& MeshAssetRelative() const { return mesh_asset_; }
    float PlaybackRate() const;
    float Emissive() const { return emissive_; }
    float LightRadius() const { return light_radius_; }
    MeshLightMode LightMode() const { return light_mode_; }
    LedColorSource ColorSource() const { return led_color_source_; }
    /** True when Rainbow / strip should recolour mesh + LEDs (Effect colour source). */
    bool UsesSpatialEffectColour() const;
    /** Half-extents the mesh is fitted into (Scale shrinks the object, not the LED volume). */
    void GetMeshFitHalfExtents(const GridContext3D& grid, float& hx, float& hy, float& hz) const;
    bool HasMesh() const { return !mesh_asset_.empty(); }
    const std::vector<PartOverride>& PartOverrides() const { return part_overrides_; }
    std::vector<PartRenderTint> BuildPartRenderTints(float time_sec) const;

    std::string ResolvedMeshPath() const;

private slots:
    void onBrowseMesh();
    void onClearMesh();
    void onPartListSelectionChanged();
    void onPartVisibleToggled(bool on);
    void onPartLedsToggled(bool on);
    void onPartTintModeChanged(int index);
    void onPartPickColor();
    void onPartUseMaterialColor();
    void onPartResetDefaults();
    void onPartsShowAll();
    void onPartsHideAll();
    void onPartsLedsAllOn();
    void onPartsLedsAllOff();

private:
    /** Local-space light sample (built once per asset); transformed each frame. */
    struct LocalSample
    {
        float lx = 0.0f;
        float ly = 0.0f;
        float lz = 0.0f;
        float u = 0.5f;
        float v = 0.5f;
        int part_index = 0;
        int node_index = 0;
    };
    struct SamplePoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float u = 0.5f;
        float v = 0.5f;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float part_emis = 1.0f;
        int part_index = 0;
        bool baked_rgb = false; /* Material/Override colour precomputed */
    };

    void refreshPathLabel();
    void refreshSummaryLabel();
    void importMeshFile(const QString& source_path);
    void syncCustomSlidersFromModel();
    void syncPartsFromScene();
    void rebuildPartsUi();
    void syncPartEditorFromSelection();
    void refreshPartListItem(int index);
    void markPartsDirty();
    void syncLightModeUi();
    void syncLedColorSourceUi();
    float effectiveLightRadius(float smallest_half_extent) const;
    PartOverride* selectedPart();
    const PartOverride* selectedPart() const;
    void ensureLocalLightSamples(const MeshAnim::Scene& anim);
    void rebuildMeshSamples(float time_sec, const GridContext3D& grid);
    void rebuildSampleGrid(float search_radius);
    bool findNearestSample(float lx, float ly, float lz, float search_radius,
                           int* out_index, float* out_d2) const;
    RGBColor sampleEffectColor(float x, float y, float z, float time,
                               const GridContext3D& grid,
                               float gradient_pos01) const;
    RGBColor evalMeshLightAt(float x, float y, float z, float time, const GridContext3D& grid);
    static std::uint32_t PackNorm01Key(float n1, float n2, float n3);
    void ensureGlowVolumeBody();
    QImage packGlowSampleMedia(float hx, float hy, float hz, int* out_count) const;
    bool prepareGlowVolume(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid);
    bool bakeGlowVolumeGpu(const float* sample_xyz, int count, float time, const GridContext3D& grid);
    void resolvePartRgb(const PartOverride& part, float time_sec,
                        float* r, float* g, float* b) const;
    void setPartSwatchColor(QPushButton* button, float r, float g, float b);

    std::string mesh_asset_;
    float emissive_ = 2.0f;
    float light_radius_ = 0.35f;
    MeshLightMode light_mode_ = MeshLightMode::Glow;
    LedColorSource led_color_source_ = LedColorSource::Materials;
    std::vector<PartOverride> part_overrides_;
    int selected_part_index_ = -1;

    std::vector<LocalSample> local_light_samples_;
    std::string local_samples_path_;
    std::vector<SamplePoint> mesh_samples_;
    /** Uniform grid over mesh_samples_ for O(k) nearest queries. */
    std::vector<int> sample_grid_heads_;
    std::vector<int> sample_grid_next_;
    int sample_grid_nx_ = 0;
    int sample_grid_ny_ = 0;
    int sample_grid_nz_ = 0;
    float sample_grid_cell_ = 0.0f;
    float sample_grid_ox_ = 0.0f;
    float sample_grid_oy_ = 0.0f;
    float sample_grid_oz_ = 0.0f;
    float sample_grid_radius_ = 0.0f;
    bool samples_static_pose_ = false;
    std::uint64_t samples_sequence_ = 0;
    float samples_time_ = -1.0f;
    float samples_hx_ = 0.0f;
    float samples_hy_ = 0.0f;
    float samples_hz_ = 0.0f;
    float samples_fit_hx_ = 0.0f;
    float samples_fit_hy_ = 0.0f;
    float samples_fit_hz_ = 0.0f;
    float fit_cx_ = 0.0f;
    float fit_cy_ = 0.0f;
    float fit_cz_ = 0.0f;
    float fit_scale_x_ = 1.0f;
    float fit_scale_y_ = 1.0f;
    float fit_scale_z_ = 1.0f;

    /** Batched LED/overlay colours from BakeTrueGpuVolumeSamples (volume UV keys). */
    std::unordered_map<std::uint32_t, RGBColor> true_sample_map_;
    SpatialVolumeFieldAssist glow_volume_;
    bool glow_body_set_ = false;
    int glow_gpu_sample_count_ = 0;

    QLabel* path_label_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QPushButton* browse_button_ = nullptr;
    QPushButton* clear_button_ = nullptr;
    EffectSliderRow* emissive_row_ = nullptr;
    EffectSliderRow* light_radius_row_ = nullptr;
    QComboBox* light_mode_combo_ = nullptr;
    QComboBox* led_color_combo_ = nullptr;

    QListWidget* parts_list_ = nullptr;
    QWidget* part_editor_ = nullptr;
    QLabel* part_editor_title_ = nullptr;
    QLabel* part_id_label_ = nullptr;
    QCheckBox* part_visible_check_ = nullptr;
    QCheckBox* part_leds_check_ = nullptr;
    QComboBox* part_tint_combo_ = nullptr;
    QPushButton* part_color_button_ = nullptr;
    QPushButton* part_use_material_button_ = nullptr;
    QPushButton* part_reset_button_ = nullptr;
    EffectSliderRow* part_emissive_row_ = nullptr;
    QLabel* part_material_swatch_ = nullptr;
    QLabel* part_texture_note_ = nullptr;
};

void RegisterAnimatedMeshEngine();
