// SPDX-License-Identifier: GPL-2.0-only

#ifndef MATRIX_H
#define MATRIX_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Matrix : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Matrix(QWidget* parent = nullptr);
    ~Matrix() override = default;

    EFFECT_REGISTERER_3D("Matrix", "Matrix", "Spatial", [](){ return new Matrix; });

    static std::string const ClassName() { return "Matrix"; }
    static std::string const UIName() { return "Matrix"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
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
    QSlider*        head_brightness_slider;
    QLabel*         head_brightness_label;
    unsigned int    density;
    unsigned int    trail;
    unsigned int    char_height;
    unsigned int    char_gap;
    unsigned int    char_variation;
    unsigned int    char_spacing;
    unsigned int    head_brightness;  /* 0..100: how much the leading LED is whitish (0=no white, 100=full white tip) */

    /* Returns intensity 0..1; if out_head is non-null, sets *out_head to 0..1 for leading edge blend (only very tip is head). */
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
