// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include <QWidget>
#include <vector>

/** Editable intensity curve. */
class EffectPackCurveBar : public QWidget
{
    Q_OBJECT

public:
    explicit EffectPackCurveBar(QWidget* parent = nullptr);

    void setPoints(std::vector<EffectPack::CurvePoint> points);
    const std::vector<EffectPack::CurvePoint>& points() const { return points_; }
    bool isDragging() const { return drag_index_ >= 0; }

signals:
    void pointsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    int hitTestPoint(const QPoint& pos) const;
    float xToPos(int x) const;
    float yToValue(int y) const;
    int posToX(float pos) const;
    int valueToY(float value) const;
    QRect plotRect() const;
    void sortPoints();
    void ensureEndpoints();

    std::vector<EffectPack::CurvePoint> points_;
    int drag_index_ = -1;
};
