// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCOLORSWATCHROW_H
#define EFFECTCOLORSWATCHROW_H

#include "RGBController.h"

#include <QWidget>
#include <functional>

class QLabel;
class QObject;
class QPushButton;
class SpatialEffect3D;

namespace Ui {
class EffectColorSwatchRow;
}

class EffectColorSwatchRow : public QWidget
{
public:
    explicit EffectColorSwatchRow(QWidget* parent = nullptr);
    ~EffectColorSwatchRow() override;

    void setCaptionText(const QString& text);
    void setSwatchColor(RGBColor color);

    void bindColorIndex(SpatialEffect3D* effect,
                        int color_index,
                        const std::function<void()>& on_changed = nullptr);

private:
    Ui::EffectColorSwatchRow* ui = nullptr;
};

#endif
