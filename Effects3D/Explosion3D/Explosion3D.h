// SPDX-License-Identifier: GPL-2.0-only

#ifndef Explosion3D_H
#define Explosion3D_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Explosion3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Explosion3D(QWidget* parent = nullptr);
    ~Explosion3D() override = default;

    EFFECT_REGISTERER_3D("Explosion3D", "3D Explosion", "3D Spatial", [](){ return new Explosion3D; });

    static std::string const ClassName() { return "Explosion3D"; }
    static std::string const UIName() { return "3D Explosion"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnExplosionParameterChanged();

private:
    QSlider*        intensity_slider;
    QLabel*         intensity_label;
    QComboBox*      type_combo;

    unsigned int    explosion_intensity;
    float           progress;
    int             explosion_type;
};

#endif
