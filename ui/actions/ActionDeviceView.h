// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"
#include "LEDPosition3D.h"
#include <QOpenGLWidget>
#include <QPoint>
#include <QRectF>
#include <QWidget>
#include <memory>
#include <string>
#include <vector>

class LEDViewport3D;
class QLabel;
class QStackedWidget;
class QTimer;
class QToolButton;
class ZoneManager3D;

namespace Action
{
namespace device_view
{
/** The lights a timeline row drives, grouped by the transform they live in. */
struct RowLights
{
    struct Group
    {
        ControllerTransform* transform = nullptr;
        std::vector<const LEDPosition3D*> leds;
    };
    std::vector<Group> groups;
    size_t count = 0;
};
RowLights CollectRowLights(const Target& target,
                           std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                           ZoneManager3D* zone_manager);
/** Colour to show for an LED: the editor's preview colour, or the device's own when unset. */
RGBColor ShownColor(const LEDPosition3D& led);
} // namespace device_view
} // namespace Action

/**
 * Flat view: the row's lights as OpenRGB lays them out on its device page — matrix zones
 * as their key grid (with key names), strips and single LEDs as rows of cells. Drawn with
 * QPainter on an OpenGL surface, so it is GPU rasterised.
 */
class ActionDeviceMatrixView : public QOpenGLWidget
{
    Q_OBJECT

public:
    ActionDeviceMatrixView(const Action::Target& target,
                           std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                           ZoneManager3D* zone_manager,
                           QWidget* parent = nullptr);

    /** Re-read the row's lights (rebuilds the layout only when it changed) and repaint. */
    void refresh();
    int lightCount() const { return light_count_; }

protected:
    void paintGL() override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Cell
    {
        int col = 0;
        int row = 0;
        RGBControllerInterface* controller = nullptr;
        unsigned int zone_idx = 0;
        unsigned int led_idx = 0;
        QString label;
        const LEDPosition3D* led = nullptr; // refreshed every tick
    };
    struct Section
    {
        QString device_title; // set on the first section of each device when several are shown
        QString title;
        int cols = 1;
        int rows = 1;
        bool matrix = false;
        std::vector<Cell> cells;
    };

    void rebuildLayout(const Action::device_view::RowLights& lights);
    QRectF cellRect(const Section& s, const Cell& c, float top) const;

    Action::Target target_;
    std::vector<std::unique_ptr<ControllerTransform>>* transforms_ = nullptr;
    ZoneManager3D* zone_manager_ = nullptr;
    std::vector<Section> sections_;
    size_t signature_ = 0;
    int light_count_ = 0;
    /* Layout metrics from the last paint, for hover hit tests. */
    float cell_px_ = 20.0f;
    float origin_x_ = 0.0f;
    std::vector<float> section_tops_;
    int hover_section_ = -1;
    int hover_cell_ = -1;
};

/**
 * 3D view: the plugin's own GPU viewport, showing a private copy of just this row's
 * controller or zone on a small grid (like the Custom Controller preview).
 */
class ActionDevice3DView : public QWidget
{
    Q_OBJECT

public:
    ActionDevice3DView(const Action::Target& target,
                       std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                       ZoneManager3D* zone_manager,
                       QWidget* parent = nullptr);
    ~ActionDevice3DView() override;

    void refresh();
    void setActive(bool active);
    void frameCamera();
    int lightCount() const { return light_count_; }

private:
    void rebuild(const Action::device_view::RowLights& lights);

    Action::Target target_;
    std::vector<std::unique_ptr<ControllerTransform>>* source_ = nullptr;
    ZoneManager3D* zone_manager_ = nullptr;
    LEDViewport3D* viewport_ = nullptr;
    std::vector<std::unique_ptr<ControllerTransform>> copies_;
    size_t signature_ = 0;
    int light_count_ = 0;
    int grid_x_ = 6, grid_y_ = 6, grid_z_ = 6;
    bool framed_ = false;
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
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setThreeD(bool on);
    void tick();

    ActionDeviceMatrixView* flat_ = nullptr;
    ActionDevice3DView* three_d_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QToolButton* flat_button_ = nullptr;
    QToolButton* three_d_button_ = nullptr;
    QToolButton* frame_button_ = nullptr;
    QLabel* info_ = nullptr;
    QTimer* timer_ = nullptr;
};
