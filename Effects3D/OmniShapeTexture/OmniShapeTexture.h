// SPDX-License-Identifier: GPL-2.0-only

#ifndef OMNISHAPETEXTURE_H
#define OMNISHAPETEXTURE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

#include <QImage>
#include <QMutex>
#include <QString>

#include <memory>

class QComboBox;
class QCheckBox;
class QLabel;
class QMovie;
class QPushButton;
class QSlider;
class QTimer;
class OmniShapeTexture : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit OmniShapeTexture(QWidget* parent = nullptr);
    ~OmniShapeTexture() override;

    EFFECT_REGISTERER_3D("OmniShapeTexture", "Omni shape texture", "Media", []() { return new OmniShapeTexture; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool UsesSpatialSamplingQuantization() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    void SetSpeed(unsigned int speed) override;

private slots:
    void OnBrowseMedia();
    void OnShapeChanged(int index);
    void OnMediaFrameChanged(int frameNumber);
    void OnGifFrameTimerTimeout();
private:
    void ClearMovie();
    void LoadMediaFile(const QString& path);
    void RefreshFrameFromMovie();
    void PublishDisplayFrame(const QImage& src);
    void ApplyGifPlaybackSpeed();

    QPushButton* browse_button;
    QLabel* path_label;
    QComboBox* shape_combo;
    QSlider* morph_slider = nullptr;
    QSlider* spin_slider = nullptr;

    QSlider* ambience_dist_slider = nullptr;
    QSlider* ambience_curve_slider = nullptr;
    QSlider* ambience_edge_slider = nullptr;
    QSlider* ambience_prop_slider = nullptr;
    QSlider* motion_scroll_slider = nullptr;
    QSlider* motion_warp_slider = nullptr;
    QSlider* motion_phase_slider = nullptr;
    QSlider* media_resolution_slider = nullptr;
    QCheckBox* tile_repeat_check = nullptr;

    unsigned int ambience_dist_falloff = 0;
    unsigned int ambience_falloff_curve = 0;
    unsigned int ambience_edge_soft = 0;
    unsigned int ambience_propagation = 0;
    unsigned int motion_scroll = 0;
    unsigned int motion_warp = 0;
    unsigned int motion_phase = 0;
    unsigned int media_resolution = 100;
    bool tile_repeat_enabled = false;

    QString media_path;
    QMovie* movie;
    QTimer* gif_frame_timer;
    bool media_is_gif;

    QMutex display_mutex;
    std::shared_ptr<QImage> display_frame;
    std::shared_ptr<QImage> previous_display_frame;
    qint64 last_gif_step_ms = 0;
    int gif_step_interval_ms = 0;

    int base_shape;
    unsigned int morph_percent;
    unsigned int spin_percent;
};

#endif
