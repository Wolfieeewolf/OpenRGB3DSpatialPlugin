// SPDX-License-Identifier: GPL-2.0-only

#ifndef MATRIX3D_H
#define MATRIX3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Matrix3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Matrix3D(QWidget* parent = nullptr);
    ~Matrix3D() override = default;

    EFFECT_REGISTERER_3D("Matrix3D", "3D Matrix", "3D Spatial", [](){ return new Matrix3D; });

    static std::string const ClassName() { return "Matrix3D"; }
    static std::string const UIName() { return "3D Matrix"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnMatrixParameterChanged();

private:
    QSlider*        density_slider;
    QLabel*         density_label;
    QSlider*        trail_slider;
    QLabel*         trail_label;
    QSlider*        char_height_slider;
    QLabel*         char_height_label;
    QSlider*        char_gap_slider;
    QLabel*         char_gap_label;
    QSlider*        char_variation_slider;
    QLabel*         char_variation_label;
    QSlider*        char_spacing_slider;
    QLabel*         char_spacing_label;
    unsigned int    density;
    unsigned int    trail;
    unsigned int    char_height;
    unsigned int    char_gap;
    unsigned int    char_variation;
    unsigned int    char_spacing;

    float ComputeFaceIntensity(int face,
                               float x,
                               float y,
                               float z,
                               float time,
                               const GridContext3D& grid,
                               float column_spacing,
                               float size_normalized,
                               float speed_scale) const;
};

#endif
