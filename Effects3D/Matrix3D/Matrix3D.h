/*---------------------------------------------------------*\
| Matrix3D.h                                               |
|                                                          |
|   3D Matrix code rain effect                             |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

#ifndef MATRIX3D_H
#define MATRIX3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Matrix3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Matrix3D(QWidget* parent = nullptr);
    ~Matrix3D();

    EFFECT_REGISTERER_3D("Matrix3D", "3D Matrix", "3D Spatial", [](){return new Matrix3D;});

    static std::string const ClassName() { return "Matrix3D"; }
    static std::string const UIName() { return "3D Matrix"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private slots:
    void OnMatrixParameterChanged();

private:
    QSlider*        density_slider;
    QSlider*        trail_slider;
    unsigned int    density;      // columns density
    unsigned int    trail;        // trail length factor
};

#endif

