// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMEWRAPPERCAPTUREEFFECT3D_H
#define GAMEWRAPPERCAPTUREEFFECT3D_H

#include "GameWrapperCapture.h"
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Game/GameLightingFormat.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <cstdint>
#include <string>

class ChromaGridPreview;

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
    struct ChannelHud
    {
        QWidget* row = nullptr;
        QLabel* swatch = nullptr;
        QLabel* text = nullptr;
        ChromaGridPreview* matrix = nullptr;
        QString last_sig;
        QString last_line;
        QString last_swatch_css;
        bool anim_latch = false;
    };

    struct ChannelMapRow
    {
        QCheckBox* enable = nullptr;
        QToolButton* dest_btn = nullptr;
    };

    void RefreshStatusLabel();
    void EnsureChannelHud(QWidget* root, QVBoxLayout* body);
    void RebuildDestMenu(std::uint8_t channel);
    void RefreshDestButtonFor(std::uint8_t channel);
    void RefreshProfileGamesCombo();
    void RefreshMapRowWidgets();

    GameWrapperCapture::Settings capture_settings_;
    QLabel* status_label_ = nullptr;
    QLabel* active_game_label_ = nullptr;
    QComboBox* profile_games_combo_ = nullptr;
    QPlainTextEdit* activity_log_ = nullptr;
    QTimer* status_timer_ = nullptr;
    std::array<ChannelHud, GameLightingFormat::kDeviceCount> channel_hud_{};
    std::array<ChannelMapRow, GameLightingFormat::kDeviceCount> channel_map_{};
    std::string last_activity_game_;
};

#endif
