// SPDX-License-Identifier: GPL-2.0-only

#ifndef DNAHELIX_H
#define DNAHELIX_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class QComboBox;
class StratumBandPanel;
class StripKernelColormapPanel;

class DNAHelix : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DNAHelix(QWidget* parent = nullptr);
    ~DNAHelix();

    EFFECT_REGISTERER_3D("DNAHelix", "DNA Helix", "Spatial", [](){return new DNAHelix;});

    static std::string const ClassName() { return "DNAHelix"; }
    static std::string const UIName() { return "DNA Helix"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnDNAParameterChanged();
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    QSlider*        radius_slider;
    QLabel*         radius_label;
    QComboBox*      shape_combo = nullptr;
    QSlider*        vertical_drift_slider = nullptr;
    QLabel*         vertical_drift_label = nullptr;
    QSlider*        ring_pulse_slider = nullptr;
    QLabel*         ring_pulse_label = nullptr;
    QComboBox*      pulse_dir_combo = nullptr;
    QSlider*        plane_count_slider = nullptr;
    QLabel*         plane_count_label = nullptr;

    unsigned int    helix_radius = 180;
    float           progress = 0.0f;
    /** 0 circle, 1 square, 2 squircle, 3 flat disc, 4 stacked planes */
    int             helix_shape_mode = 0;
    int             vertical_drift_pct = 0;
    int             ring_pulse_pct = 0;
    /** 0 off, 1 outward from center, 2 inward toward center */
    int             ring_pulse_dir = 0;
    int             plane_layers = 8;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool dnahelix_strip_cmap_on = false;
    int dnahelix_strip_cmap_kernel = 0;
    float dnahelix_strip_cmap_rep = 4.0f;
    int dnahelix_strip_cmap_unfold = 0;
    float dnahelix_strip_cmap_dir = 0.0f;
    int dnahelix_strip_cmap_color_style = 0;
};

#endif
