// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QWidget>

/**
 * Vixen-style dual strip: effect icons (Basic/Pixel) + color/gradient swatches.
 * Drag effects onto timeline rows; drag colors/gradients onto blocks.
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

private:
    void buildUi();
};
