// SPDX-License-Identifier: GPL-2.0-only

#ifndef CONTROLLERTORCH_H
#define CONTROLLERTORCH_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "LEDPosition3D.h"

class QCheckBox;
class QSlider;

/** Showcase torch: DualSense IMU aim from the DualSense controller's layout position. */
class ControllerTorch : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit ControllerTorch(QWidget* parent = nullptr);
    ~ControllerTorch() override = default;

    EFFECT_REGISTERER_3D("ControllerTorch", "Controller Torch", "Spatial", []() {
        return new ControllerTorch;
    })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    /** Full cone angle in degrees (wider = bigger beam). */
    float beam_degrees_ = 55.0f;
    float soft_edge_ = 0.45f;
    float range_grid_ = 120.0f;
    /** Distance falloff start as fraction of range (0 = fade from apex, 1 = hard cutoff). */
    float falloff_start_ = 0.45f;
    float core_size_ = 0.35f;
    float core_boost_ = 0.55f;
    float hue_ = 0.0f;
    float hue_bottom_ = 180.0f;
    float saturation_ = 0.85f;
    float split_amount_ = 1.0f;
    float split_soft_ = 0.25f;
    float boost_ = 1.25f;
    bool split_colors_ = true;

    Vector3D cached_origin_{0.0f, 0.0f, 0.0f};
    Vector3D cached_aim_{0.0f, 0.0f, -1.0f};
    Vector3D cached_up_{0.0f, 1.0f, 0.0f};
    bool have_pose_ = false;
    std::uint64_t last_prepare_seq_ = UINT64_MAX;

    QSlider* beam_slider_ = nullptr;
    QSlider* soft_slider_ = nullptr;
    QSlider* range_slider_ = nullptr;
    QSlider* falloff_slider_ = nullptr;
    QSlider* core_size_slider_ = nullptr;
    QSlider* core_boost_slider_ = nullptr;
    QSlider* hue_slider_ = nullptr;
    QSlider* hue_bottom_slider_ = nullptr;
    QSlider* sat_slider_ = nullptr;
    QSlider* split_amount_slider_ = nullptr;
    QSlider* split_soft_slider_ = nullptr;
    QSlider* boost_slider_ = nullptr;
    QCheckBox* split_check_ = nullptr;

    void CapturePose(const GridContext3D& grid);
    void SyncSlidersFromState();
};

#endif
