// SPDX-License-Identifier: GPL-2.0-only

#ifndef MATRIX_H
#define MATRIX_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class Matrix : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Matrix(QWidget* parent = nullptr);
    ~Matrix() override = default;

    EFFECT_REGISTERER_3D("Matrix", "Matrix", "Spatial", [](){ return new Matrix; });

    static std::string const ClassName() { return "Matrix"; }
    static std::string const UIName() { return "Matrix"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnMatrixParameterChanged();
private:
    QSlider* density_slider         = nullptr;
    QSlider* trail_slider           = nullptr;
    QSlider* char_height_slider     = nullptr;
    QSlider* char_gap_slider        = nullptr;
    QSlider* char_variation_slider  = nullptr;
    QSlider* char_spacing_slider    = nullptr;
    QSlider* head_brightness_slider = nullptr;
    unsigned int    density;
    unsigned int    trail;
    unsigned int    char_height;
    unsigned int    char_gap;
    unsigned int    char_variation;
    unsigned int    char_spacing;
    unsigned int    head_brightness;
    float ComputeFaceIntensity(int face,
                               float x,
                               float y,
                               float z,
                               float time,
                               const GridContext3D& grid,
                               float column_spacing,
                               float size_normalized,
                               float speed_scale,
                               float* out_head = nullptr) const;
};

#endif
