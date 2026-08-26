// SPDX-License-Identifier: GPL-2.0-only

#ifndef DEPTHTONE_H
#define DEPTHTONE_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QSlider;
class QComboBox;

class DepthTone : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DepthTone(QWidget* parent = nullptr);
    ~DepthTone() override;

    EFFECT_REGISTERER_3D("DepthTone", "Depth Tone", "Spatial", []() { return new DepthTone; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    enum Axis { AXIS_X = 0, AXIS_Y, AXIS_Z, AXIS_COUNT };
    enum Layout { LAYOUT_LINEAR = 0, LAYOUT_CENTER, LAYOUT_COUNT };

    static const char* AxisName(int a);
    static const char* LayoutName(int L);

    int depth_tone_count = 6;
    int depth_axis = AXIS_Z;
    int depth_layout = LAYOUT_LINEAR;
    float dim_amount = 0.72f;

    QSlider* depth_tones_slider = nullptr;
    QSlider* dim_slider = nullptr;
    QComboBox* axis_combo = nullptr;
    QComboBox* layout_combo = nullptr;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
