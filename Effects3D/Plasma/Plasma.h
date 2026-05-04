// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLASMA_H
#define PLASMA_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class QComboBox;
class StratumBandPanel;
class StripKernelColormapPanel;

class Plasma : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Plasma(QWidget* parent = nullptr);
    ~Plasma();

    EFFECT_REGISTERER_3D("Plasma", "Plasma", "Spatial", [](){return new Plasma;});

    static std::string const ClassName() { return "Plasma"; }
    static std::string const UIName() { return "Plasma"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnPlasmaParameterChanged();
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    QComboBox*      pattern_combo;
    StratumBandPanel* stratum_panel = nullptr;

    int             pattern_type;
    float           progress;
    int             stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool plasma_strip_cmap_on = false;
    int plasma_strip_cmap_kernel = 0;
    float plasma_strip_cmap_rep = 4.0f;
    int plasma_strip_cmap_unfold = 0;
    float plasma_strip_cmap_dir = 0.0f;
    int plasma_strip_cmap_color_style = 0;
};

#endif
