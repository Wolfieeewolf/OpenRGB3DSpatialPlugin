// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QWidget>

/**
 * Effect icons (Basic/Pixel/Volume) + color/gradient/curve swatches.
 */
class EffectPackToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit EffectPackToolBar(QWidget* parent = nullptr);

signals:
    /** Fallback click-add using current timeline selection (optional). */
    void effectClicked(int block_type);
    void colorClicked(unsigned int rgb);
    void gradientPresetClicked(const QString& preset_id);
    void curvePresetClicked(const QString& preset_id);

private:
    void buildUi();
};
