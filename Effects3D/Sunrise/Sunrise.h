// SPDX-License-Identifier: GPL-2.0-only

#ifndef SUNRISE_H
#define SUNRISE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StratumBandPanel;
class StripKernelColormapPanel;

class Sunrise : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Sunrise(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Sunrise", "Sunrise", "Spatial", [](){ return new Sunrise; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();

private:
    enum Mode { MODE_MANUAL = 0, MODE_REALTIME, MODE_SIMULATED, MODE_COUNT };
    enum Preset { PRESET_REALISTIC_SUNRISE = 0, PRESET_REALISTIC_SUNSET, PRESET_DAYTIME, PRESET_NIGHT, PRESET_CUSTOM, PRESET_COUNT };
    static const char* ModeName(int m);
    static const char* PresetName(int p);
    void ApplyPreset(int preset);
    float GetTimeOfDayProgress(float time) const;

    int time_mode = MODE_REALTIME;
    int color_preset = PRESET_DAYTIME;
    float day_length_minutes = 10.0f;

    bool weather_rain = false;
    bool weather_fog = false;
    bool weather_cloudy = false;
    bool weather_lightning = false;

    StratumBandPanel* stratum_panel = nullptr;
    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
    bool sunrise_strip_cmap_on = false;
    int sunrise_strip_cmap_kernel = 0;
    float sunrise_strip_cmap_rep = 4.0f;
    int sunrise_strip_cmap_unfold = 0;
    float sunrise_strip_cmap_dir = 0.0f;
    int sunrise_strip_cmap_color_style = 1;
    void SyncStripColormapFromPanel();
};

#endif
