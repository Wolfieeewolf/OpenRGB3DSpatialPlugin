// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "IldaReader.h"
#include "ShowFolderReader.h"
#include "SpatialEffect3D.h"
#include "SpatialVolumeFieldAssist.h"

#include <QVector3D>
#include <cstdint>
#include <unordered_map>
#include <vector>

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
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    bool PrefersTrueGpuVolumeSamples() const override { return true; }
    void BakeTrueGpuVolumeSamples(const float* sample_xyz,
                                  int count,
                                  float time,
                                  const GridContext3D& grid) override;
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

    static constexpr int kGpuMaxStrokes = 256;
    /** Spur/track origins packed into u_params[20..55] (ox,oy,oz × 12). */
    static constexpr int kMaxProjectorTracks = 12;
    static constexpr int kGpuParamCount = 20 + kMaxProjectorTracks * 3;

    void LoadLaserPath(const QString& path);
    void RebuildShowGeometry();
    void RebuildPlayIndices();
    void UpdateStatusLabel();
    void UpdatePathLabel();
    const LaserFrame* CurrentFrame(float time) const;

    /** Pack current frame strokes into u_media + push u_params; returns stroke count. */
    int UploadGpuStrokeMedia(const LaserFrame& frame);
    void FillGpuParams(float* params, int stroke_count) const;
    /** Active Spur/ILDA heads used for multi-origin layout (sorted unique). */
    std::vector<int> ActiveProjectorTracks() const;
    static std::uint32_t PackNorm01Key(float n1, float n2, float n3);
    QVector3D SampleGpuField01(float n1, float n2, float n3);

    QWidget* settings_panel_ = nullptr;

    LaserShow show_;
    ShowFolder show_folder_;
    bool use_show_folder_ = false;
    /** Scratch frame for ShowEditor timeline composite (CurrentFrame). */
    mutable LaserFrame show_frame_;
    std::vector<size_t> play_indices_;
    QString path_;
    float beam_thickness_ = 0.04f;
    int fps_ = 30;
    bool flatten_z_ = true;
    bool loop_ = true;
    /** ShowEditor Simulation "Beam" — projector→gallery wedges. */
    bool show_beams_ = true;
    /** ShowEditor Simulation "Graphic" — lit strokes on the far/gallery plane. */
    bool show_graphic_ = true;
    float projector_depth_ = 0.0f;
    /**
     * Multi-projector room look: each Spur/track (or ILDA scanner head) throws
     * from its own origin along the near face. Off = all wedges share center.
     */
    bool multi_projectors_ = true;
    /** 0 = stack all heads at centre; 1 = full left→right spread of active tracks. */
    float projector_spread_ = 0.85f;
    /** ShowEditor OpenGL transparency sliders (0–1). */
    float fog_ = 0.85f;
    float transparency_ = 0.90f;
    float beam_gain_ = 1.0f;
    float screen_ = 1.0f;
    int max_beams_ = 0;
    /** Off by default — matches ilda-viewer (blanked travel stays invisible). */
    bool join_lit_path_ = false;
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

    SpatialVolumeFieldAssist volume_assist_;
    std::unordered_map<std::uint32_t, QVector3D> true_sample_map_;
    int gpu_stroke_count_ = 0;

    QPushButton* browse_button_ = nullptr;
    QPushButton* clear_button_ = nullptr;
    QLabel* path_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QComboBox* scanner_combo_ = nullptr;
    QComboBox* machine_combo_ = nullptr;
    EffectDoubleSliderRow* thickness_row_ = nullptr;
    EffectSliderRow* fps_row_ = nullptr;
    EffectDoubleSliderRow* projector_depth_row_ = nullptr;
    EffectDoubleSliderRow* projector_spread_row_ = nullptr;
    EffectDoubleSliderRow* fog_row_ = nullptr;
    EffectDoubleSliderRow* transparency_row_ = nullptr;
    EffectDoubleSliderRow* beam_gain_row_ = nullptr;
    EffectDoubleSliderRow* screen_row_ = nullptr;
    EffectSliderRow* max_beams_row_ = nullptr;
    QCheckBox* flatten_check_ = nullptr;
    QCheckBox* loop_check_ = nullptr;
    QCheckBox* beam_check_ = nullptr;
    QCheckBox* graphic_check_ = nullptr;
    QCheckBox* multi_projectors_check_ = nullptr;
    QCheckBox* join_path_check_ = nullptr;
    QCheckBox* intensity_widen_check_ = nullptr;
    QCheckBox* dwell_widen_check_ = nullptr;
    QCheckBox* crisp_beams_check_ = nullptr;
    EffectDoubleSliderRow* width_dyn_row_ = nullptr;
};

void RegisterLaserVectorEngine();
