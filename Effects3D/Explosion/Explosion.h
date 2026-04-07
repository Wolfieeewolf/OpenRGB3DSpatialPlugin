// SPDX-License-Identifier: GPL-2.0-only

#ifndef Explosion_H
#define Explosion_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QSpinBox;

class Explosion : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Explosion(QWidget* parent = nullptr);
    ~Explosion() override = default;

    EFFECT_REGISTERER_3D("Explosion", "Explosion", "Spatial", [](){ return new Explosion; });

    static std::string const ClassName() { return "Explosion"; }
    static std::string const UIName() { return "Explosion"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnExplosionParameterChanged();

private:
    static float explosionHash(unsigned int seed, unsigned int salt);
    float particleDebrisAt(float x, float y, float z, float burst_phase, float distance, float radius, int type_id) const;

    QSlider*        intensity_slider;
    QLabel*         intensity_label;
    QComboBox*      type_combo;
    QSpinBox*       burst_count_spin;
    QCheckBox*      loop_check;
    QSlider*        particle_slider;
    QLabel*         particle_label;

    unsigned int    explosion_intensity;
    float           progress;
    int             explosion_type;
    int             burst_count;      /* 0 = infinite, 1-10 = that many then stop or repeat */
    bool            loop;
    int             particle_amount;  /* 0-100, debris/spark strength for standard/bomb */
    static constexpr float CYCLE_DURATION = 2.5f;
};

#endif
