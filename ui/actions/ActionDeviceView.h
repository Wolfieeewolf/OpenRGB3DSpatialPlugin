// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"
#include <QPoint>
#include <QWidget>
#include <memory>
#include <vector>

class QLabel;
class QTimer;
class QToolButton;
class ZoneManager3D;
struct ControllerTransform;
struct LEDPosition3D;

/**
 * Live look at one timeline row's lights (a device, a hardware zone or a scene zone),
 * in a small window beside the editor. Flat shows the layout face-on (a keyboard reads
 * as its key matrix); 3D shows the lights where they sit in the room, drag to orbit.
 * Colours are the same preview colours the viewport draws, refreshed while it is open.
 */
class ActionDeviceViewCanvas : public QWidget
{
    Q_OBJECT

public:
    ActionDeviceViewCanvas(const Action::Target& target,
                           std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                           ZoneManager3D* zone_manager,
                           QWidget* parent = nullptr);

    void setThreeD(bool on);
    bool threeD() const { return three_d_; }
    /** Lights currently shown (0 when the device is not in the scene). */
    int lightCount() const { return last_count_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void collect(std::vector<const LEDPosition3D*>* out) const;

    Action::Target target_;
    std::vector<std::unique_ptr<ControllerTransform>>* transforms_ = nullptr;
    ZoneManager3D* zone_manager_ = nullptr;
    bool three_d_ = false;
    float yaw_deg_ = -30.0f;
    float pitch_deg_ = 20.0f;
    float zoom_ = 1.0f;
    QPoint drag_last_;
    mutable int last_count_ = 0;
    /* Neighbour spacing is costly to measure; layouts rarely change while the window is open. */
    mutable float cached_spacing_ = 1.0f;
    mutable int cached_count_ = -1;
    mutable bool cached_local_ = false;
};

class ActionDeviceViewWindow : public QWidget
{
    Q_OBJECT

public:
    ActionDeviceViewWindow(const Action::Target& target,
                           const QString& label,
                           std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                           ZoneManager3D* zone_manager,
                           QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    ActionDeviceViewCanvas* canvas_ = nullptr;
    QToolButton* flat_button_ = nullptr;
    QToolButton* three_d_button_ = nullptr;
    QLabel* info_ = nullptr;
    QTimer* timer_ = nullptr;
};
