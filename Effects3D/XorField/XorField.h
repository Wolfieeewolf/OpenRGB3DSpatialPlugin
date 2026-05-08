// SPDX-License-Identifier: GPL-2.0-only

#ifndef XORFIELD_H
#define XORFIELD_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class QLabel;
class QSlider;
class StripKernelColormapPanel;

class XorField : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit XorField(QWidget* parent = nullptr);
    ~XorField() override;

    EFFECT_REGISTERER_3D("XorField", "Xor Field", "Spatial", []() { return new XorField; });

    static std::string const ClassName() { return "XorField"; }
    static std::string const UIName() { return "Xor Field"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    /** 0 = all cells share the same time direction; 1 = odd XOR parity runs phases backward. */
    float direction_alternate = 1.0f;
    /** Quantization scale for XOR grid (roughly cells across the room). */
    float cell_scale = 5.0f;
    /** Scales the moving wave terms (higher = busier motion). */
    float wave_drive = 1.0f;

    QSlider* alt_slider = nullptr;
    QLabel* alt_label = nullptr;
    QSlider* cell_slider = nullptr;
    QLabel* cell_label = nullptr;
    QSlider* drive_slider = nullptr;
    QLabel* drive_label = nullptr;
    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool xorfield_strip_cmap_on = false;
    int xorfield_strip_cmap_kernel = 0;
    float xorfield_strip_cmap_rep = 4.0f;
    int xorfield_strip_cmap_unfold = 0;
    float xorfield_strip_cmap_dir = 0.0f;
    int xorfield_strip_cmap_color_style = 0;

private slots:
    void SyncStripColormapFromPanel();
};

#endif
