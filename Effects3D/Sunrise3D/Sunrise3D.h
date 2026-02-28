// SPDX-License-Identifier: GPL-2.0-only

#ifndef SUNRISE3D_H
#define SUNRISE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Sunrise3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Sunrise3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Sunrise3D", "Realtime Environment", "3D Spatial", [](){ return new Sunrise3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

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
};

#endif
