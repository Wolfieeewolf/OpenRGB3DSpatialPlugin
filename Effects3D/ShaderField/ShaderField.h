// SPDX-License-Identifier: GPL-2.0-only

#ifndef SHADERFIELD_H
#define SHADERFIELD_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Shaders/SpatialShaderEngine.h"

#include <QImage>
#include <QMutex>
#include <memory>
#include <vector>

class QComboBox;
class QCheckBox;
class QLabel;

class ShaderField : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit ShaderField(QWidget* parent = nullptr);
    ~ShaderField() override;

    EFFECT_REGISTERER_3D("ShaderField", "Shader Field", "Spatial", []() { return new ShaderField; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
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
    void OnUseAudioChanged(int state);
    void OnOpenShadersFolder();

private:
    void EnsureShaderEngineRunning();
    void RebuildPresetList();
    void LoadPresetAtIndex(int index);
    void SyncUniforms(float time);
    void SampleUv(float x, float y, float z, const GridContext3D& grid, const Vector3D& origin, float& u, float& v) const;
    RGBColor SampleField(float u, float v) const;

    SpatialShaderEngine* shader_engine = nullptr;
    QComboBox* preset_combo = nullptr;
    QComboBox* projection_combo = nullptr;
    QCheckBox* use_audio_check = nullptr;
    QLabel* compile_log_label = nullptr;

    std::vector<QString> preset_paths;
    int projection_mode = 0;
    bool use_audio = true;
    float audio_uniform[128] = {};
    uint64_t last_uniform_sequence = 0;

    mutable QMutex display_mutex;
    std::shared_ptr<QImage> display_frame;
};

#endif
