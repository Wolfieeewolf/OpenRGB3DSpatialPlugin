// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERGRIDSCENE_H
#define CUSTOMCONTROLLERGRIDSCENE_H

#include <QGraphicsScene>

class CustomControllerGridScene : public QGraphicsScene
{
public:
    explicit CustomControllerGridScene(QObject* parent = nullptr);

    void SetSceneSizeMm(qreal width_mm, qreal height_mm);
    void SetShowGrid(bool show);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    qreal scene_width_mm_  = 0.0;
    qreal scene_height_mm_ = 0.0;
    bool  show_grid_       = true;
};

#endif
