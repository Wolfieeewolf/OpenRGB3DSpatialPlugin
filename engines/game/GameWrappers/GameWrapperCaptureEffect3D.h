// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMEWRAPPERCAPTUREEFFECT3D_H
#define GAMEWRAPPERCAPTUREEFFECT3D_H

#include "GameWrapperCapture.h"
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

#include <QLabel>
#include <QTimer>

class GameWrapperCaptureEffect3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit GameWrapperCaptureEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D_GAME("GameWrapperCapture",
                              "Game wrapper capture",
                              "wrappers",
                              "Game Wrappers",
                              []() { return new GameWrapperCaptureEffect3D; })

    EffectInfo3D GetEffectInfo() const override;
    void ApplyControlVisibility() override;
    void SetupCustomUI(QWidget* parent) override;
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
    RGBColor CalculateColorGrid(float gx, float gy, float gz, float time, const GridContext3D& grid) override;
    bool IsPointOnActiveSurface(float x, float y, float z, const GridContext3D& grid) const override;

private:
    void RefreshStatusLabel();

    GameWrapperCapture::Settings capture_settings_;
    QLabel* status_label_ = nullptr;
    QTimer* status_timer_ = nullptr;
};

#endif
