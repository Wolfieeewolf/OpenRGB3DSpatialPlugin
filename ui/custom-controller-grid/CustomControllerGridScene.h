// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERGRIDSCENE_H
#define CUSTOMCONTROLLERGRIDSCENE_H

#include <QGraphicsScene>

class CustomControllerGridScene : public QGraphicsScene
{
public:
    explicit CustomControllerGridScene(QObject* parent = nullptr);

    void SetGridSize(int width, int height);
    void SetShowGrid(bool show);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    int  grid_width_  = 0;
    int  grid_height_ = 0;
    bool show_grid_   = true;
};

#endif
