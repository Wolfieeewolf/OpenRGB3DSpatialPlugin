// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "IldaReader.h"
#include "SpatialEffect3D.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class EffectSliderRow;
class EffectDoubleSliderRow;

class LaserVectorEffect : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit LaserVectorEffect(QWidget* parent = nullptr);
    ~LaserVectorEffect() override = default;

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool TryFillGpuRoomGridOverlayLayer(struct GpuRoomGridOverlayLayer& out,
                                        const GridContext3D& grid,
                                        float time_sec) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnBrowseIld();
    void OnClearIld();
    void OnPanelContextMenu(const QPoint& pos);

private:
    /** Playback colour / machine profile — not stored in .ild; mirrors Beyond/ShowEditor projector setup. */
    enum class MachineProfile : int
    {
        Rgb = 0,
        Rgy = 1,
        Rgby = 2,
        MonoGreen = 3,
        MonoWhite = 4,
        BeamPunch = 5,
    };

    void LoadIldPath(const QString& path);
    void RebuildShowGeometry();
    void RebuildPlayIndices();
    void UpdateStatusLabel();
    void UpdatePathLabel();
    const LaserFrame* CurrentFrame(float time) const;
    RGBColor SampleLaserAtLocal01(float u, float v, float w, const LaserFrame& frame) const;
    void MapMachineColour(float& r, float& g, float& b) const;
    /** 1 inside the shaft, 0 outside — steep when crisp_beams_ so edges stay saturated. */
    float BeamPresence(float dist, float thickness) const;
    float EffectiveThickness(float intensity01, float dwell) const;

    QWidget* settings_panel_ = nullptr;

    LaserShow show_;
    std::vector<size_t> play_indices_;
    QString path_;
    float beam_thickness_ = 0.04f;
    int fps_ = 30;
    bool flatten_z_ = true;
    bool loop_ = true;
    bool projector_beams_ = true;
    float projector_depth_ = 0.0f;
    int max_beams_ = 0;
    bool join_lit_path_ = true;
    int scanner_filter_ = -1;
    MachineProfile machine_profile_ = MachineProfile::Rgb;
    /** Brighter colour → fatter beam (haze/focus proxy; .ild has no width field). */
    bool intensity_widens_ = true;
    /** Longer dwell (repeated points) → fatter beam. */
    bool dwell_widens_ = true;
    /** Max thickness multiplier when intensity/dwell are high (1 = off, 4 = strong). */
    float width_dynamics_ = 2.5f;
    /**
     * Hard shaft, not soft paint: soft falloff greys colours and washes white.
     * Real scanners look like defined beams — on or off with a thin edge.
     */
    bool crisp_beams_ = true;
    QPushButton* browse_button_ = nullptr;
    QPushButton* clear_button_ = nullptr;
    QLabel* path_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QComboBox* scanner_combo_ = nullptr;
    QComboBox* machine_combo_ = nullptr;
    EffectDoubleSliderRow* thickness_row_ = nullptr;
    EffectSliderRow* fps_row_ = nullptr;
    EffectDoubleSliderRow* projector_depth_row_ = nullptr;
    EffectSliderRow* max_beams_row_ = nullptr;
    QCheckBox* flatten_check_ = nullptr;
    QCheckBox* loop_check_ = nullptr;
    QCheckBox* projector_check_ = nullptr;
    QCheckBox* join_path_check_ = nullptr;
    QCheckBox* intensity_widen_check_ = nullptr;
    QCheckBox* dwell_widen_check_ = nullptr;
    QCheckBox* crisp_beams_check_ = nullptr;
    EffectDoubleSliderRow* width_dyn_row_ = nullptr;
};

void RegisterLaserVectorEngine();
