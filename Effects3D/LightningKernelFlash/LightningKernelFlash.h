// SPDX-License-Identifier: GPL-2.0-only

#ifndef LIGHTNINGKERNELFLASH_H
#define LIGHTNINGKERNELFLASH_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class StripKernelColormapPanel;

/** Bright white sky flash with bolt color driven by strip kernel along the strike path. */
class LightningKernelFlash : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit LightningKernelFlash(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("LightningKernelFlash", "Lightning Kernel Flash", "Spatial", []() {
        return new LightningKernelFlash;
    });

    static std::string const ClassName() { return "LightningKernelFlash"; }
    static std::string const UIName() { return "Lightning Kernel Flash"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    static float hash11(float t);
    static float HashF(unsigned int seed);
    static float DistToSegment(float px, float py, float pz, float ax, float ay, float az, float bx, float by, float bz);

    float flash_rate = 0.15f;
    float flash_duration = 0.09f;
    int fork_branches = 2;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool lkflash_strip_cmap_on = true;
    int lkflash_strip_cmap_kernel = 0;
    float lkflash_strip_cmap_rep = 4.0f;
    int lkflash_strip_cmap_unfold = 0;
    float lkflash_strip_cmap_dir = 0.0f;
    int lkflash_strip_cmap_color_style = 0;
};

#endif
