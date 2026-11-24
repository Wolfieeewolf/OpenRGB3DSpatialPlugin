// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SHADERFIELD_H
#define SHADERFIELD_H

#include "SpatialEffect3D.h"
#include "SpatialShaderEngine.h"
#include "SpatialShaderCatalog.h"

#include <QList>
#include <QString>
#include <QVector3D>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class QComboBox;
class QLabel;
class QSlider;

struct ShaderFieldSpec
{
    std::string class_name;
    std::string ui_name;
    std::string description;
    QString     path;
};

class ShaderField : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit ShaderField(ShaderFieldSpec spec, QWidget* parent = nullptr);
    ~ShaderField() override;

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    bool PrefersTrueGpuVolumeSamples() const override { return true; }
    void BakeTrueGpuVolumeSamples(const float* sample_xyz,
                                  int count,
                                  float time,
                                  const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    /** Overlay: atlas bake of the same spatialMain LEDs evaluate at exact UVs. */
    bool TryFillGpuRoomGridOverlayLayer(struct GpuRoomGridOverlayLayer& out,
                                        const GridContext3D& grid,
                                        float time_sec) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnCompileMessage(const QString& message);
    void OnProjectionModeChanged(int index);
    void OnOpenShadersFolder();

private:
    void EnsureShaderEngineRunning();
    void LoadShaderBody();
    void SyncUniforms(float time);
    void SampleUv(float nx, float ny, float nz, float& u, float& v) const;
    static std::uint32_t PackUvKey(float u, float v);
    QVector3D SampleField01(float u, float v) const;
    RGBColor SampleField(float u, float v) const;

    enum ProjectionMode : int
    {
        PROJ_FLOOR = 0,
        PROJ_FRONT = 1,
        PROJ_SIDE_LEFT = 2,
        PROJ_SPHERE = 3,
        PROJ_CEILING = 4,
        PROJ_BACK = 5,
        PROJ_SIDE_RIGHT = 6,
        PROJ_CYLINDER_Y = 7,
        PROJ_RADIAL_XZ = 8,
        PROJ_TRIPLANAR = 9,       /* UI: "Box unwrap" */
        PROJ_CUBE_FACE = 10,      /* Deprecated alias of PROJ_TRIPLANAR — coerce on load */
        PROJ_COUNT
    };

    void PopulateProjectionCombo();
    void SyncProjectionComboToMode();
    static int NormalizeProjectionMode(int mode);

    ShaderFieldSpec spec_{};
    SpatialShaderEngine* shader_engine = nullptr;
    QComboBox* projection_combo = nullptr;
    QSlider* contrast_slider = nullptr;
    QSlider* hue_slider = nullptr;
    QLabel* compile_log_label = nullptr;

    int projection_mode = 0;
    /** Header sliders for u_params[4..7] (SpatialShaderCatalog::LoadExtraParamSliders). */
    QList<SpatialShaderCatalog::ExtraParamSlider> extra_specs_;
    /** User values by param name (raw slider units); missing = the file's default. */
    std::map<std::string, int> extra_raw_;
    std::vector<QSlider*> extra_sliders_;
    int ExtraRaw(const SpatialShaderCatalog::ExtraParamSlider& s) const;
    float contrast = 1.0f;
    float hue_shift = 0.0f;
    uint64_t last_uniform_sequence = 0;
    float last_uniform_time = -1.0f;

    std::unordered_map<std::uint32_t, QVector3D> true_sample_map_;
    /** Set after BakeTrueGpuVolumeSamples; miss → black (no per-LED GL). */
    bool true_samples_ready_ = false;
};

#endif
