// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOCONTAINER3D_H
#define AUDIOCONTAINER3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class AudioContainer3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit AudioContainer3D(QWidget* parent = nullptr) : SpatialEffect3D(parent) {}

    EFFECT_REGISTERER_3D("AudioContainer3D", "Audio Effect", "Audio", [](){ return new AudioContainer3D; })

    EffectInfo3D GetEffectInfo() override
    {
        EffectInfo3D info{};
        info.info_version            = 2;
        info.effect_name             = "Audio Effect";
        info.effect_description      = "Activates the audio rendering pipeline for per-range effects";
        info.category                = "Audio";
        info.show_speed_control      = false;
        info.show_brightness_control = false;
        info.show_frequency_control  = false;
        info.show_size_control       = false;
        info.show_scale_control      = false;
        info.show_fps_control        = false;
        info.show_color_controls     = false;
        return info;
    }

    void SetupCustomUI(QWidget*) override {}
    void UpdateParams(SpatialEffectParams&) override {}

    RGBColor CalculateColor(float, float, float, float) override
    {
        return 0x00000000;
    }
};

#endif
