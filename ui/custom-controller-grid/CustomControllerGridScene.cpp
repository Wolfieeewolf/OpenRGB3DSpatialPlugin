// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerGridScene.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <algorithm>

CustomControllerGridScene::CustomControllerGridScene(QObject* parent)
    : QGraphicsScene(parent)
{
}

void CustomControllerGridScene::SetSceneSizeMm(qreal width_mm, qreal height_mm)
{
    scene_width_mm_  = std::max(0.0, width_mm);
    scene_height_mm_ = std::max(0.0, height_mm);
    setSceneRect(0, 0, scene_width_mm_, scene_height_mm_);
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

    if(!show_grid_ || scene_width_mm_ <= 0.0 || scene_height_mm_ <= 0.0)
    {
        return;
    }

    const QRectF grid_bounds(0, 0, scene_width_mm_, scene_height_mm_);
    if(!rect.intersects(grid_bounds))
    {
        return;
    }

    painter->save();
    painter->setClipRect(grid_bounds);
    painter->fillRect(grid_bounds, palette().base());
    painter->restore();
}
