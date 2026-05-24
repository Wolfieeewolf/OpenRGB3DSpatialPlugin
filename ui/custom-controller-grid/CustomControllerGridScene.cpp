// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerGridScene.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <algorithm>

namespace
{
constexpr qreal kGridLineWidth = 0.1;
constexpr int   kGridLineAlpha   = 0xA0;
constexpr qreal kBoundsLineWidth = 0.16;

void PaintGridLines(QPainter* painter, int grid_width, int grid_height)
{
    if(grid_width <= 0 || grid_height <= 0)
    {
        return;
    }

    QColor line_color = QApplication::palette().color(QPalette::Mid);
    line_color.setAlpha(kGridLineAlpha);
    painter->setPen(QPen(line_color, kGridLineWidth));
    painter->setRenderHint(QPainter::Antialiasing, false);

    QVarLengthArray<QLineF, 64> lines;
    for(int x = 0; x <= grid_width; x++)
    {
        lines.append(QLineF(x, 0, x, grid_height));
    }
    for(int y = 0; y <= grid_height; y++)
    {
        lines.append(QLineF(0, y, grid_width, y));
    }
    painter->drawLines(lines.data(), lines.size());
}
} // namespace

CustomControllerGridScene::CustomControllerGridScene(QObject* parent)
    : QGraphicsScene(parent)
{
}

void CustomControllerGridScene::SetGridSize(int width, int height)
{
    grid_width_  = std::max(0, width);
    grid_height_ = std::max(0, height);
    setSceneRect(0, 0, grid_width_, grid_height_);
    invalidate(sceneRect());
}

void CustomControllerGridScene::SetShowGrid(bool show)
{
    show_grid_ = show;
    invalidate(sceneRect());
}

void CustomControllerGridScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, palette().window());

    if(grid_width_ <= 0 || grid_height_ <= 0)
    {
        return;
    }

    const QRectF grid_bounds(0, 0, grid_width_, grid_height_);
    if(!rect.intersects(grid_bounds))
    {
        return;
    }

    painter->save();
    painter->setClipRect(grid_bounds);
    painter->fillRect(grid_bounds, palette().base());

    painter->restore();
}

void CustomControllerGridScene::drawForeground(QPainter* painter, const QRectF& rect)
{
    if(!show_grid_ || grid_width_ <= 0 || grid_height_ <= 0)
    {
        return;
    }

    const QRectF grid_bounds(0, 0, grid_width_, grid_height_);
    if(!rect.intersects(grid_bounds))
    {
        return;
    }

    painter->save();
    painter->setClipRect(grid_bounds);

    PaintGridLines(painter, grid_width_, grid_height_);

    QColor bounds_color = QApplication::palette().color(QPalette::Highlight);
    painter->setPen(QPen(bounds_color, kBoundsLineWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(grid_bounds);

    painter->restore();
}
