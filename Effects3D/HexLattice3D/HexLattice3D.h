// SPDX-License-Identifier: GPL-2.0-only

#ifndef HEXLATTICE3D_H
#define HEXLATTICE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class StripKernelColormapPanel;

class HexLattice3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit HexLattice3D(QWidget* parent = nullptr);
    ~HexLattice3D() override;

    EFFECT_REGISTERER_3D("HexLattice3D", "Hex Lattice 3D", "Spatial", []() { return new HexLattice3D; });

    static std::string const ClassName() { return "HexLattice3D"; }
    static std::string const UIName() { return "Hex Lattice 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    static RGBColor Hsv01ToBgr(float h, float s, float v);
    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool hexlattice_strip_cmap_on = false;
    int hexlattice_strip_cmap_kernel = 0;
    float hexlattice_strip_cmap_rep = 4.0f;
    int hexlattice_strip_cmap_unfold = 0;
    float hexlattice_strip_cmap_dir = 0.0f;
    int hexlattice_strip_cmap_color_style = 0;
};

#endif
