// SPDX-License-Identifier: GPL-2.0-only

#ifndef POSITIONAXISDRAGCONTROLLER_H
#define POSITIONAXISDRAGCONTROLLER_H

#include <QObject>

class QSlider;
class QDoubleSpinBox;
class QMouseEvent;
class QWheelEvent;
class OpenRGB3DSpatialTab;

class PositionAxisDragController : public QObject
{
    Q_OBJECT

public:
    PositionAxisDragController(OpenRGB3DSpatialTab* tab,
                               QSlider* slider,
                               QDoubleSpinBox* spin,
                               int axis,
                               bool clamp_non_negative);

    void setCurrentMm(double mm);
    double currentMm() const { return current_mm_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void fetchLimits(double& min_mm, double& max_mm) const;
    void applyMm(double mm);
    void syncSliderVisual();
    bool sliderHandleContains(const QPoint& pos) const;
    double mmPerPixel() const;
    void onDragDelta(int delta_px, Qt::KeyboardModifiers modifiers);
    void onWheel(QWheelEvent* event);

    OpenRGB3DSpatialTab* tab_;
    QSlider*               slider_;
    QDoubleSpinBox*        spin_;
    int                    axis_;
    bool                   clamp_non_negative_;
    bool                   dragging_      = false;
    int                    last_global_x_ = 0;
    double                 current_mm_    = 0.0;
};

#endif
