// SPDX-License-Identifier: GPL-2.0-only

#ifndef CAPTUREZONESWIDGET_H
#define CAPTUREZONESWIDGET_H

#include <QWidget>
#include <vector>
#include <string>

class DisplayPlane3D;
class EffectDoubleSliderRow;
class EffectSliderRow;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;

struct CaptureZone
{
    float u_min;
    float u_max;
    float v_min;
    float v_max;
    bool enabled;
    std::string name;

    CaptureZone();
    CaptureZone(float u0, float u1, float v0, float v1);
};

class CaptureAreaPreviewWidget;

namespace Ui {
class CaptureZonesWidget;
}

class CaptureZonesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CaptureZonesWidget(
        std::vector<CaptureZone>* zones,
        DisplayPlane3D* plane,
        bool* show_calibration_pattern = nullptr,
        bool* show_screen_preview = nullptr,
        float* black_bar_top_percent = nullptr,
        float* black_bar_bottom_percent = nullptr,
        float* black_bar_left_percent = nullptr,
        float* black_bar_right_percent = nullptr,
        float propagation_speed = 0.0f,
        float wave_decay_ms = 0.0f,
        float wave_time_to_edge_sec = 0.0f,
        QWidget* parent = nullptr);
    ~CaptureZonesWidget() override;

    void SetDisplayPlane(DisplayPlane3D* plane);
    void SetCaptureZones(std::vector<CaptureZone>* zones);

    QPushButton* getAddZoneButton() const;
    QPushButton* getRemoveZoneButton() const;
    QWidget* getPreviewWidget() const;
    void syncZoneButtonStates();

    QSlider* getPropagationSpeedSlider() const;
    QLabel* getPropagationSpeedLabel() const;
    QSlider* getWaveDecaySlider() const;
    QLabel* getWaveDecayLabel() const;
    QSlider* getWaveTimeToEdgeSlider() const;
    QDoubleSpinBox* getWaveTimeToEdgeSpin() const;

    QGroupBox* getWaveGroup() const;
    QGroupBox* getZonesGroup() const;
    void syncWaveIntensityVisibility();

signals:
    void valueChanged();

private:
    void onInternalChange();
    void wireSliderConnections();

    Ui::CaptureZonesWidget* ui = nullptr;
    CaptureAreaPreviewWidget* preview_widget = nullptr;
    EffectSliderRow* propagation_row_ = nullptr;
    EffectSliderRow* wave_decay_row_ = nullptr;
    EffectDoubleSliderRow* wave_time_row_ = nullptr;
};

#endif
