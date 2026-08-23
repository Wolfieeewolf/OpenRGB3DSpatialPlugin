// SPDX-License-Identifier: GPL-2.0-only

#ifndef CAPTUREZONESWIDGET_H
#define CAPTUREZONESWIDGET_H

#include <QWidget>
#include <vector>
#include <string>
#include <functional>

class DisplayPlane3D;
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
    bool Contains(float u, float v) const;
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
        float* black_bar_letterbox_percent = nullptr,
        float* black_bar_pillarbox_percent = nullptr,
        float propagation_speed = 0.0f,
        float wave_decay_ms = 0.0f,
        float wave_time_to_edge_sec = 0.0f,
        float front_back_balance = 0.0f,
        float left_right_balance = 0.0f,
        float top_bottom_balance = 0.0f,
        QWidget* parent = nullptr);
    ~CaptureZonesWidget() override;

    void SetDisplayPlane(DisplayPlane3D* plane);
    void SetValueChangedCallback(std::function<void()> callback);
    void SetCaptureZones(std::vector<CaptureZone>* zones);

    QPushButton* getAddZoneButton() const;
    QWidget* getPreviewWidget() const;

    QSlider* getPropagationSpeedSlider() const;
    QLabel* getPropagationSpeedLabel() const;
    QSlider* getWaveDecaySlider() const;
    QLabel* getWaveDecayLabel() const;
    QSlider* getWaveTimeToEdgeSlider() const;
    QLabel* getWaveTimeToEdgeLabel() const;
    QSlider* getFrontBackBalanceSlider() const;
    QLabel* getFrontBackBalanceLabel() const;
    QSlider* getLeftRightBalanceSlider() const;
    QLabel* getLeftRightBalanceLabel() const;
    QSlider* getTopBottomBalanceSlider() const;
    QLabel* getTopBottomBalanceLabel() const;

signals:
    void valueChanged();

private:
    void onInternalChange();
    void wireSliderConnections();

    Ui::CaptureZonesWidget* ui = nullptr;
    CaptureAreaPreviewWidget* preview_widget = nullptr;
    std::function<void()> value_changed_callback;
};

#endif
