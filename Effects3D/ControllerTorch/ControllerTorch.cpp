// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerTorch.h"
#include "ControllerTiltManager.h"
#include "ControllerTiltTypes.h"
#include "EffectColorUtils.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(ControllerTorch);

namespace
{
float Dot3(const Vector3D& a, const Vector3D& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length3(const Vector3D& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vector3D Normalize3(Vector3D v)
{
    const float len = Length3(v);
    if(len < 1e-5f)
    {
        return {0.0f, 0.0f, 1.0f};
    }
    const float inv = 1.0f / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

float Smoothstep(float edge0, float edge1, float x)
{
    if(edge1 <= edge0)
    {
        return (x >= edge1) ? 1.0f : 0.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

ControllerTorch::ControllerTorch(QWidget* parent)
    : SpatialEffect3D(parent)
{
    // Orientation tracks MotionPlus ~95 Hz — keep the stack ticking fast enough.
    effect_fps = 90;
}

EffectInfo3D ControllerTorch::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Controller Torch";
    info.effect_description =
        "Wii Remote torch with full 360° MotionPlus orientation. "
        "Sensor-bar IR soft-locks yaw when visible to cut drift. "
        "Apex follows a User / \"Wii\" reference point.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_CONTROLLER_TORCH;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_frequency = false;
    info.default_speed_scale = 10.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_color_controls = false;
    return info;
}

void ControllerTorch::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    QLabel* hint = new QLabel(
        QStringLiteral(
            "Mayflash DolphinBar mode 4 (no HID-Wiimote gamepad driver). "
            "MotionPlus = full 360° aim. Sensor bar / IR locks yaw when visible (less drift). "
            "Add a User (or Custom \"Wii\") reference as the torch apex. "
            "Home = flat, buttons up, IR toward the front wall / bar. Recenter."),
        w);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    status_label_ = new QLabel(QStringLiteral("Status: …"), w);
    status_label_->setWordWrap(true);
    layout->addWidget(status_label_);
    status_timer_ = new QTimer(w);
    status_timer_->setInterval(500);
    QObject::connect(status_timer_, &QTimer::timeout, this, [this]() {
        RefreshStatusLabel();
    });
    status_timer_->start();
    RefreshStatusLabel();

    QPushButton* recenter_btn = new QPushButton(
        QStringLiteral("Recenter pose"), w);
    recenter_btn->setToolTip(
        QStringLiteral(
            "Lay flat and still (buttons up, IR toward front / sensor bar), hold ~1s, then click. "
            "Sets home, MotionPlus bias, and IR yaw-0. Status shows \"IR lock\" when the bar is seen."));
    QObject::connect(recenter_btn, &QPushButton::clicked, this, [this]() {
        if(ControllerTiltManager* tilt = ControllerTiltManager::instance())
        {
            tilt->RecenterHeading();
        }
        RefreshStatusLabel();
    });
    layout->addWidget(recenter_btn);

    QLabel* size_hdr = new QLabel(QStringLiteral("Beam size"), w);
    layout->addWidget(size_hdr);

    EffectSliderRow* beam_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Beam width (°):"), 10, 140,
        (int)std::lround(beam_degrees_),
        QStringLiteral("Full cone angle — larger = wider spotlight."));
    beam_slider_ = beam_row->slider();
    beam_row->bindValueChanged(
        this,
        [this](int v) { beam_degrees_ = (float)v; },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* soft_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Soft edge:"), 0, 100,
        (int)std::lround(soft_edge_ * 100.0f),
        QStringLiteral("Feather at the cone rim."));
    soft_slider_ = soft_row->slider();
    soft_row->bindValueChanged(
        this,
        [this](int v) { soft_edge_ = v / 100.0f; },
        [this](int) { return QString::number(soft_edge_, 'f', 2); },
        on_changed);

    EffectSliderRow* range_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Range (grid):"), 20, 400,
        (int)std::lround(range_grid_),
        QStringLiteral("How far the beam travels in grid units."));
    range_slider_ = range_row->slider();
    range_row->bindValueChanged(
        this,
        [this](int v) { range_grid_ = (float)v; },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* falloff_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Falloff start:"), 0, 95,
        (int)std::lround(falloff_start_ * 100.0f),
        QStringLiteral("Where distance fade begins (fraction of range)."));
    falloff_slider_ = falloff_row->slider();
    falloff_row->bindValueChanged(
        this,
        [this](int v) { falloff_start_ = v / 100.0f; },
        [this](int) { return QString::number(falloff_start_, 'f', 2); },
        on_changed);

    EffectSliderRow* core_size_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Hotspot size:"), 0, 90,
        (int)std::lround(core_size_ * 100.0f),
        QStringLiteral("Inner bright core as a fraction of beam width."));
    core_size_slider_ = core_size_row->slider();
    core_size_row->bindValueChanged(
        this,
        [this](int v) { core_size_ = v / 100.0f; },
        [this](int) { return QString::number(core_size_, 'f', 2); },
        on_changed);

    EffectSliderRow* core_boost_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Hotspot boost:"), 0, 150,
        (int)std::lround(core_boost_ * 100.0f),
        QStringLiteral("Extra brightness in the beam centre."));
    core_boost_slider_ = core_boost_row->slider();
    core_boost_row->bindValueChanged(
        this,
        [this](int v) { core_boost_ = v / 100.0f; },
        [this](int) { return QString::number(core_boost_, 'f', 2); },
        on_changed);

    EffectSliderRow* boost_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Overall boost:"), 20, 250,
        (int)std::lround(boost_ * 100.0f),
        QStringLiteral("Extra intensity on top of global brightness."));
    boost_slider_ = boost_row->slider();
    boost_row->bindValueChanged(
        this,
        [this](int v) { boost_ = v / 100.0f; },
        [this](int) { return QString::number(boost_, 'f', 2); },
        on_changed);

    QLabel* color_hdr = new QLabel(QStringLiteral("Colour"), w);
    layout->addWidget(color_hdr);

    split_check_ = new QCheckBox(QStringLiteral("Split colours (roll visualization)"), w);
    split_check_->setChecked(split_colors_);
    split_check_->setToolTip(
        QStringLiteral("Top/bottom colours twist when you roll the Wii Remote."));
    QObject::connect(split_check_, &QCheckBox::toggled, this, [this, on_changed](bool on) {
        split_colors_ = on;
        on_changed();
    });
    layout->addWidget(split_check_);

    EffectSliderRow* hue_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Top hue:"), 0, 360,
        (int)std::lround(hue_),
        QStringLiteral("Hue for the 'up' half (or solid colour when split is off)."));
    hue_slider_ = hue_row->slider();
    hue_row->bindValueChanged(
        this,
        [this](int v) { hue_ = (float)v; },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* hue_bot_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Bottom hue:"), 0, 360,
        (int)std::lround(hue_bottom_),
        QStringLiteral("Hue for the 'down' half when split is on."));
    hue_bottom_slider_ = hue_bot_row->slider();
    hue_bot_row->bindValueChanged(
        this,
        [this](int v) { hue_bottom_ = (float)v; },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* sat_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Saturation:"), 0, 100,
        (int)std::lround(saturation_ * 100.0f),
        QStringLiteral("Colour saturation of the beam."));
    sat_slider_ = sat_row->slider();
    sat_row->bindValueChanged(
        this,
        [this](int v) { saturation_ = v / 100.0f; },
        [this](int) { return QString::number(saturation_, 'f', 2); },
        on_changed);

    EffectSliderRow* split_amt_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Split amount:"), 0, 100,
        (int)std::lround(split_amount_ * 100.0f),
        QStringLiteral("How strongly top/bottom hues separate (0 = blended)."));
    split_amount_slider_ = split_amt_row->slider();
    split_amt_row->bindValueChanged(
        this,
        [this](int v) { split_amount_ = v / 100.0f; },
        [this](int) { return QString::number(split_amount_, 'f', 2); },
        on_changed);

    EffectSliderRow* split_soft_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Split softness:"), 5, 80,
        (int)std::lround(split_soft_ * 100.0f),
        QStringLiteral("Softness of the seam between top and bottom halves."));
    split_soft_slider_ = split_soft_row->slider();
    split_soft_row->bindValueChanged(
        this,
        [this](int v) { split_soft_ = v / 100.0f; },
        [this](int) { return QString::number(split_soft_, 'f', 2); },
        on_changed);

    AddWidgetToParent(w, parent);
}

void ControllerTorch::RefreshStatusLabel()
{
    if(!status_label_)
    {
        return;
    }
    ControllerTiltManager* tilt = ControllerTiltManager::instance();
    if(!tilt)
    {
        status_label_->setText(QStringLiteral("Status: tilt manager unavailable"));
        return;
    }
    const ControllerTiltLinkStatus link = tilt->LinkStatus();
    const ControllerTiltSample sample = tilt->Latest();
    QString text = QStringLiteral("Status: %1")
                       .arg(QString::fromUtf8(ControllerTiltLinkStatusText(link)));
    if(link == ControllerTiltLinkStatus::Ok && sample.valid)
    {
        text += QStringLiteral("\n%1  %2  X:%3°  Y:%4°  Z:%5°  (0,0,0 = flat home)")
                    .arg(sample.mplus_live ? QStringLiteral("M+ ok")
                                           : QStringLiteral("M+ no"))
                    .arg(sample.ir_locked ? QStringLiteral("IR lock")
                                          : QStringLiteral("IR free"))
                    .arg(sample.euler_deg.x, 0, 'f', 1)
                    .arg(sample.euler_deg.y, 0, 'f', 1)
                    .arg(sample.euler_deg.z, 0, 'f', 1);
    }
    status_label_->setText(text);
}

void ControllerTorch::CapturePose(const GridContext3D& grid)
{
    if(grid.render_sequence != 0 && grid.render_sequence == last_prepare_seq_)
    {
        return;
    }
    last_prepare_seq_ = grid.render_sequence;

    ControllerTiltManager* tilt = ControllerTiltManager::instance();
    if(!tilt)
    {
        have_pose_ = false;
        return;
    }

    const ControllerTiltSample sample = tilt->Latest();
    Vector3D origin{};
    const bool have_origin = tilt->TorchOrigin(origin);
    if(!have_origin)
    {
        origin = GetEffectOriginGrid(grid);
    }

    cached_origin_ = origin;
    if(sample.valid)
    {
        cached_aim_ = sample.aim;
        cached_up_ = sample.up;
        have_pose_ = true;
    }
    else
    {
        cached_aim_ = {0.0f, 0.0f, -1.0f};
        cached_up_ = {0.0f, 1.0f, 0.0f};
        have_pose_ = have_origin;
    }
}

RGBColor ControllerTorch::CalculateColorGrid(float x, float y, float z, float /*time*/,
                                             const GridContext3D& grid)
{
    CapturePose(grid);

    if(!have_pose_)
    {
        return 0;
    }

    const Vector3D delta{x - cached_origin_.x, y - cached_origin_.y, z - cached_origin_.z};
    const float dist = Length3(delta);
    if(dist < 1e-4f || dist > range_grid_)
    {
        return 0;
    }

    const Vector3D to_led = Normalize3(delta);
    const float cos_a = Dot3(to_led, cached_aim_);
    const float half_rad = beam_degrees_ * 0.5f * (float)M_PI / 180.0f;
    const float cos_outer = std::cos(half_rad);
    const float soft = std::clamp(soft_edge_, 0.0f, 0.95f);
    const float inner_angle = half_rad * (1.0f - soft);
    const float cos_inner = std::cos(inner_angle);

    const float cone = Smoothstep(cos_outer, cos_inner, cos_a);
    if(cone <= 1e-4f)
    {
        return 0;
    }

    const float fade_start = std::clamp(falloff_start_, 0.0f, 0.95f) * range_grid_;
    const float dist_fade = 1.0f - Smoothstep(fade_start, range_grid_, dist);

    const float core_frac = std::clamp(core_size_, 0.0f, 0.9f);
    const float core_half = half_rad * core_frac;
    const float cos_core = std::cos(core_half);
    const float hotspot = Smoothstep(cos_outer, cos_core, cos_a) * std::clamp(core_boost_, 0.0f, 1.5f);

    float intensity = cone * dist_fade * boost_ * (1.0f + hotspot);
    intensity = std::clamp(intensity, 0.0f, 2.0f);

    float h01 = std::fmod(std::max(0.0f, hue_), 360.0f) / 360.0f;
    if(split_colors_ && split_amount_ > 1e-3f)
    {
        Vector3D radial{
            to_led.x - cached_aim_.x * cos_a,
            to_led.y - cached_aim_.y * cos_a,
            to_led.z - cached_aim_.z * cos_a
        };
        float along_up = 0.0f;
        if(Length3(radial) > 1e-5f)
        {
            radial = Normalize3(radial);
            along_up = Dot3(radial, cached_up_);
        }

        const float soft_w = std::clamp(split_soft_, 0.05f, 0.8f);
        const float mix = Smoothstep(-soft_w, soft_w, along_up);
        const float top_h = h01;
        const float bot_h = std::fmod(std::max(0.0f, hue_bottom_), 360.0f) / 360.0f;
        const float split_h = top_h * mix + bot_h * (1.0f - mix);
        const float amt = std::clamp(split_amount_, 0.0f, 1.0f);
        h01 = h01 * (1.0f - amt) + split_h * amt;
    }

    const float s = std::clamp(saturation_, 0.0f, 1.0f) * (0.75f + 0.25f * cone);
    const float v = std::clamp(intensity, 0.0f, 1.0f);
    return EffectHsv01ToBgr(h01, s, v);
}

nlohmann::json ControllerTorch::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["beam_degrees"] = beam_degrees_;
    j["soft_edge"] = soft_edge_;
    j["range_grid"] = range_grid_;
    j["falloff_start"] = falloff_start_;
    j["core_size"] = core_size_;
    j["core_boost"] = core_boost_;
    j["hue"] = hue_;
    j["hue_bottom"] = hue_bottom_;
    j["saturation"] = saturation_;
    j["split_amount"] = split_amount_;
    j["split_soft"] = split_soft_;
    j["boost"] = boost_;
    j["split_colors"] = split_colors_;
    return j;
}

void ControllerTorch::SyncSlidersFromState()
{
    if(beam_slider_)
    {
        beam_slider_->setValue((int)std::lround(beam_degrees_));
    }
    if(soft_slider_)
    {
        soft_slider_->setValue((int)std::lround(soft_edge_ * 100.0f));
    }
    if(range_slider_)
    {
        range_slider_->setValue((int)std::lround(range_grid_));
    }
    if(falloff_slider_)
    {
        falloff_slider_->setValue((int)std::lround(falloff_start_ * 100.0f));
    }
    if(core_size_slider_)
    {
        core_size_slider_->setValue((int)std::lround(core_size_ * 100.0f));
    }
    if(core_boost_slider_)
    {
        core_boost_slider_->setValue((int)std::lround(core_boost_ * 100.0f));
    }
    if(hue_slider_)
    {
        hue_slider_->setValue((int)std::lround(hue_));
    }
    if(hue_bottom_slider_)
    {
        hue_bottom_slider_->setValue((int)std::lround(hue_bottom_));
    }
    if(sat_slider_)
    {
        sat_slider_->setValue((int)std::lround(saturation_ * 100.0f));
    }
    if(split_amount_slider_)
    {
        split_amount_slider_->setValue((int)std::lround(split_amount_ * 100.0f));
    }
    if(split_soft_slider_)
    {
        split_soft_slider_->setValue((int)std::lround(split_soft_ * 100.0f));
    }
    if(boost_slider_)
    {
        boost_slider_->setValue((int)std::lround(boost_ * 100.0f));
    }
    if(split_check_)
    {
        split_check_->setChecked(split_colors_);
    }
}

void ControllerTorch::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("beam_degrees") && settings["beam_degrees"].is_number())
    {
        beam_degrees_ = std::clamp(settings["beam_degrees"].get<float>(), 10.0f, 140.0f);
    }
    if(settings.contains("soft_edge") && settings["soft_edge"].is_number())
    {
        soft_edge_ = std::clamp(settings["soft_edge"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("range_grid") && settings["range_grid"].is_number())
    {
        range_grid_ = std::clamp(settings["range_grid"].get<float>(), 20.0f, 400.0f);
    }
    if(settings.contains("falloff_start") && settings["falloff_start"].is_number())
    {
        falloff_start_ = std::clamp(settings["falloff_start"].get<float>(), 0.0f, 0.95f);
    }
    if(settings.contains("core_size") && settings["core_size"].is_number())
    {
        core_size_ = std::clamp(settings["core_size"].get<float>(), 0.0f, 0.9f);
    }
    if(settings.contains("core_boost") && settings["core_boost"].is_number())
    {
        core_boost_ = std::clamp(settings["core_boost"].get<float>(), 0.0f, 1.5f);
    }
    if(settings.contains("hue") && settings["hue"].is_number())
    {
        hue_ = std::clamp(settings["hue"].get<float>(), 0.0f, 360.0f);
    }
    if(settings.contains("hue_bottom") && settings["hue_bottom"].is_number())
    {
        hue_bottom_ = std::clamp(settings["hue_bottom"].get<float>(), 0.0f, 360.0f);
    }
    if(settings.contains("saturation") && settings["saturation"].is_number())
    {
        saturation_ = std::clamp(settings["saturation"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("split_amount") && settings["split_amount"].is_number())
    {
        split_amount_ = std::clamp(settings["split_amount"].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("split_soft") && settings["split_soft"].is_number())
    {
        split_soft_ = std::clamp(settings["split_soft"].get<float>(), 0.05f, 0.8f);
    }
    if(settings.contains("boost") && settings["boost"].is_number())
    {
        boost_ = std::clamp(settings["boost"].get<float>(), 0.2f, 2.5f);
    }
    if(settings.contains("split_colors") && settings["split_colors"].is_boolean())
    {
        split_colors_ = settings["split_colors"].get<bool>();
    }

    SyncSlidersFromState();
}
