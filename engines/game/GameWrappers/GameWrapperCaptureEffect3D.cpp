// SPDX-License-Identifier: GPL-2.0-only

#include "GameWrapperCaptureEffect3D.h"
#include "Game/GameLightingBridge.h"
#include "EffectUiRows.h"
#include "EffectSliderRow.h"

#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(GameWrapperCaptureEffect3D);

GameWrapperCaptureEffect3D::GameWrapperCaptureEffect3D(QWidget* parent) : SpatialEffect3D(parent)
{
    effect_intensity = 100;
    effect_sharpness = 100;
}

EffectInfo3D GameWrapperCaptureEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Game wrapper capture";
    info.effect_description =
        "Full-grid colors from Chroma / iCUE / LightFX shims (UDP 127.0.0.1:9877). "
        "Install with tools/game_wrappers/install_chroma_shim.ps1.";
    info.category = "Game";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 8.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_axis_control = false;
    info.show_color_controls = false;
    info.show_surface_control = false;
    return info;
}

void GameWrapperCaptureEffect3D::ApplyControlVisibility()
{
    SpatialEffect3D::ApplyControlVisibility();
    if(speed_slider)
    {
        speed_slider->hide();
    }
    if(speed_label)
    {
        speed_label->hide();
    }
}

void GameWrapperCaptureEffect3D::SetupCustomUI(QWidget* parent)
{
    auto* root = new QWidget(parent);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    status_label_ = new QLabel(root);
    status_label_->setWordWrap(true);
    layout->addWidget(status_label_);

    auto* hint = new QLabel(
        QStringLiteral(
            "1) Build tools/game_wrappers/chroma_shim (Win32 or x64 to match the game).\n"
            "2) .\\tools\\game_wrappers\\install_chroma_shim.ps1 -GameExePath \"D:\\Games\\...\\game.exe\"\n"
            "3) Run this effect, then start the game. Aurora installs the Razer SDK globally instead; "
            "we use per-game DLL deploy like Artemis.Plugins.Wrappers."),
        root);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    if(QVBoxLayout* orient_body = EffectUiRows::AppendCollapsibleSectionBody(
           layout,
           QStringLiteral("Room orientation (future per-zone frames)"),
           QStringLiteral("Compass mapping reserved for zone frames; GW01 v0 is full-grid RGB."),
           true))
    {
        GameWrapperCapture::Settings& s = capture_settings_;

        EffectSliderRow* heading = EffectUiRows::AppendSliderRow(
            orient_body,
            QStringLiteral("Room heading offset"),
            -180,
            180,
            (int)std::lround(s.room_heading_offset_deg),
            QStringLiteral("Rotate room forward for compass mapping. Spin for fine, slider for coarse."));
        if(heading)
        {
            heading->setSuffix(QStringLiteral("\u00B0"));
            heading->bindValueChanged(root,
                                      [&s](int v) {
                                          s.room_heading_offset_deg = std::clamp((float)v, -180.0f, 180.0f);
                                      },
                                      nullptr);
        }

        EffectSliderRow* compass = EffectUiRows::AppendSliderRow(
            orient_body,
            QStringLiteral("Compass sector offset"),
            -180,
            180,
            (int)std::lround(s.compass_offset_deg),
            QStringLiteral("Rotate N/NE/E sector alignment. Spin for fine, slider for coarse."));
        if(compass)
        {
            compass->setSuffix(QStringLiteral("\u00B0"));
            compass->bindValueChanged(root,
                                      [&s](int v) {
                                          s.compass_offset_deg = std::clamp((float)v, -180.0f, 180.0f);
                                      },
                                      nullptr);
        }

        auto* discrete = new QCheckBox(QStringLiteral("Discrete compass zones"), root);
        discrete->setChecked(s.discrete_compass_zones);
        discrete->setToolTip(QStringLiteral("Toggle discrete compass sectors (mouse, keyboard Space/Enter, context menu on the control)."));
        orient_body->addWidget(discrete);
        QObject::connect(discrete, &QCheckBox::toggled, root, [&s](bool on) { s.discrete_compass_zones = on; });
    }

    status_timer_ = new QTimer(root);
    status_timer_->setInterval(500);
    connect(status_timer_, &QTimer::timeout, this, &GameWrapperCaptureEffect3D::RefreshStatusLabel);
    status_timer_->start();
    RefreshStatusLabel();

    AddWidgetToParent(root, parent);
}

void GameWrapperCaptureEffect3D::RefreshStatusLabel()
{
    if(!status_label_)
    {
        return;
    }

    unsigned int total = 0;
    unsigned int valid = 0;
    unsigned int err = 0;
    std::string source;
    GameLightingBridge::GetStats(total, valid, err, source);

    const bool connected = GameLightingBridge::IsConnected();
    const auto snap = GameLightingBridge::GetSnapshot();
    const QString mode =
        snap.uniform_rgb ? QStringLiteral("uniform RGB") : QStringLiteral("zones/per-zone");
    status_label_->setText(
        QStringLiteral("UDP :9877 | %1 | %2 | src %3 | seq %4 | pkts %5 ok")
            .arg(connected ? QStringLiteral("live") : QStringLiteral("waiting"))
            .arg(mode)
            .arg(QString::fromStdString(source.empty() ? "—" : source))
            .arg(snap.sequence)
            .arg(valid));
}

nlohmann::json GameWrapperCaptureEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["game_wrapper_capture"]["room_heading_offset_deg"] = capture_settings_.room_heading_offset_deg;
    j["game_wrapper_capture"]["compass_offset_deg"] = capture_settings_.compass_offset_deg;
    j["game_wrapper_capture"]["discrete_compass_zones"] = capture_settings_.discrete_compass_zones;
    return j;
}

void GameWrapperCaptureEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("game_wrapper_capture") && settings["game_wrapper_capture"].is_object())
    {
        const nlohmann::json& g = settings["game_wrapper_capture"];
        capture_settings_.room_heading_offset_deg =
            g.value("room_heading_offset_deg", capture_settings_.room_heading_offset_deg);
        capture_settings_.compass_offset_deg = g.value("compass_offset_deg", capture_settings_.compass_offset_deg);
        capture_settings_.discrete_compass_zones =
            g.value("discrete_compass_zones", capture_settings_.discrete_compass_zones);
    }
}

RGBColor GameWrapperCaptureEffect3D::CalculateColorGrid(float gx, float gy, float gz, float time, const GridContext3D& grid)
{
    (void)time;
    thread_local GameLightingBridge::LightingSnapshot tls_snap;
    const std::uint64_t rev = GameLightingBridge::DataRevision();
    static thread_local std::uint64_t cached_rev = 0;
    if(rev != cached_rev)
    {
        tls_snap = GameLightingBridge::GetSnapshot();
        cached_rev = rev;
    }

    const Vector3D effect_origin = GetEffectOriginGrid(grid);
    return GameWrapperCapture::RenderColor(tls_snap,
                                           gx,
                                           gy,
                                           gz,
                                           effect_origin.x,
                                           effect_origin.y,
                                           effect_origin.z,
                                           grid,
                                           capture_settings_);
}

bool GameWrapperCaptureEffect3D::IsPointOnActiveSurface(float, float, float, const GridContext3D&) const
{
    return true;
}
