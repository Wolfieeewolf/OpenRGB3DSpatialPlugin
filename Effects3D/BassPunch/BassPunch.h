// SPDX-License-Identifier: GPL-2.0-only

#ifndef BASSPUNCH_H
#define BASSPUNCH_H

#include "Effects3D/AudioPulse/AudioPulse.h"

/** Dedicated Low-register beat layer — kick / bass punch for stacking. */
class BassPunch : public AudioPulse
{
    Q_OBJECT

public:
    explicit BassPunch(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("BassPunch", "Bass Punch", "Audio", []() { return new BassPunch; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void LoadSettings(const nlohmann::json& settings) override;
};

#endif
