// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROTATINGCONESPOTLIGHTS3D_H
#define ROTATINGCONESPOTLIGHTS3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QSlider;
class QLabel;
class StripKernelColormapPanel;

/**
 * Double-cone spotlight field in a unit cube, with a rotation matrix whose axis and angle
 * vary over time. Intended for dense 3D grids / room volumes with normalized coordinates.
 */
class RotatingConeSpotlights3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit RotatingConeSpotlights3D(QWidget* parent = nullptr);
    ~RotatingConeSpotlights3D() override;

    EFFECT_REGISTERER_3D("RotatingConeSpotlights3D", "Rotating Cone Spotlights 3D", "Spatial", []() {
        return new RotatingConeSpotlights3D;
    });

    static std::string const ClassName() { return "RotatingConeSpotlights3D"; }
    static std::string const UIName() { return "Rotating Cone Spotlights 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    static void SetupRotationMatrix(float ux, float uy, float uz, float angle_rad, float R[3][3]);
    static void Rotate3D(float x, float y, float z, const float R[3][3], float* rx, float* ry, float* rz);
    static RGBColor Hsv01ToBgr(float h, float s, float v);

    /** Matches 1/(pi^2) in reference tunings; larger = wider cones (smaller sqrt term divisor). */
    float cone_scale = 0.10132118364233777f;
    /** Added to hue from room X (aligned to the centered-x hue convention). */
    float hue01 = 0.0f;
    /** Multiplier on time for axis wobble frequencies. */
    float motion_rate = 1.0f;

    QSlider* cone_slider = nullptr;
    QLabel* cone_label = nullptr;
    QSlider* hue_slider = nullptr;
    QLabel* hue_label = nullptr;
    QSlider* motion_slider = nullptr;
    QLabel* motion_label = nullptr;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool cones_spot_strip_cmap_on = false;
    int cones_spot_strip_cmap_kernel = 0;
    float cones_spot_strip_cmap_rep = 4.0f;
    int cones_spot_strip_cmap_unfold = 0;
    float cones_spot_strip_cmap_dir = 0.0f;
    int cones_spot_strip_cmap_color_style = 0;
};

#endif
