// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include <QWidget>
#include <vector>

/** Vixen-style gradient bar with draggable stop markers. */
class EffectPackGradientBar : public QWidget
{
    Q_OBJECT

public:
    explicit EffectPackGradientBar(QWidget* parent = nullptr);

    void setStops(std::vector<EffectPack::GradientStop> stops);
    const std::vector<EffectPack::GradientStop>& stops() const { return stops_; }
    bool isDragging() const { return drag_index_ >= 0; }

signals:
    void stopsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    int hitTestStop(const QPoint& pos) const;
    float xToPos(int x) const;
    int posToX(float pos) const;
    QRect barRect() const;
    void sortStops();
    void editStopColor(int index);

    std::vector<EffectPack::GradientStop> stops_;
    int drag_index_ = -1;
    int bar_h_ = 18;
    int marker_h_ = 12;
};
