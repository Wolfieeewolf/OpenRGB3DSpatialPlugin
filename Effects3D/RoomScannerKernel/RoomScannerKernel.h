// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSCANNERKERNEL_H
#define ROOMSCANNERKERNEL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StripKernelColormapPanel;
class StratumBandPanel;

/** Moving plane slice: LEDs near the plane show the strip kernel; elsewhere dark. */
class RoomScannerKernel : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit RoomScannerKernel(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomScannerKernel", "Room Scanner Kernel", "Spatial", []() {
        return new RoomScannerKernel;
    });

    static std::string const ClassName() { return "RoomScannerKernel"; }
    static std::string const UIName() { return "Room Scanner Kernel"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    int plane_axis = 1; /* 0=X, 1=Y, 2=Z */
    float slice_thickness = 0.08f;
    float scan_phase_off = 0.0f;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool rscan_strip_cmap_on = true;
    int rscan_strip_cmap_kernel = 0;
    float rscan_strip_cmap_rep = 4.0f;
    int rscan_strip_cmap_unfold = 0;
    float rscan_strip_cmap_dir = 0.0f;
    int rscan_strip_cmap_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
