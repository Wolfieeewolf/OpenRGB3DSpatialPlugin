// SPDX-License-Identifier: GPL-2.0-only

#ifndef SHADERFIELD_H
#define SHADERFIELD_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Shaders/SpatialShaderEngine.h"

#include <QImage>
#include <QMutex>
#include <memory>
#include <vector>

class QComboBox;
class QLabel;
class QSlider;

class ShaderField : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit ShaderField(QWidget* parent = nullptr);
    ~ShaderField() override;

    EFFECT_REGISTERER_3D("ShaderField", "Shader Field", "Spatial", []() { return new ShaderField; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    void SetSpeed(unsigned int speed) override;

private slots:
    void OnFrameReady(const QImage& image);
    void OnCompileMessage(const QString& message);
    void OnPresetChanged(int index);
    void OnProjectionModeChanged(int index);
    void OnOpenShadersFolder();

private:
    void EnsureShaderEngineRunning();
    void RebuildPresetList();
    void LoadPresetAtIndex(int index);
    void SyncUniforms(float time);
    void SampleUv(float x, float y, float z, const GridContext3D& grid, const Vector3D& origin, float& u, float& v) const;
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
        PROJ_TRIPLANAR = 9,
        PROJ_CUBE_FACE = 10,
        PROJ_COUNT
    };

    SpatialShaderEngine* shader_engine = nullptr;
    QComboBox* preset_combo = nullptr;
    QComboBox* projection_combo = nullptr;
    QSlider* contrast_slider = nullptr;
    QSlider* hue_slider = nullptr;
    QLabel* compile_log_label = nullptr;

    std::vector<QString> preset_ids;
    int active_preset_index = 0;
    int projection_mode = 0;
    float contrast = 1.0f;
    float hue_shift = 0.0f;
    uint64_t last_uniform_sequence = 0;
    float last_uniform_time = -1.0f;

    mutable QMutex display_mutex;
    std::shared_ptr<QImage> display_frame;
};

#endif
