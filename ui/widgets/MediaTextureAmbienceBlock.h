// SPDX-License-Identifier: GPL-2.0-only

#ifndef MEDIATEXTUREAMBIENCEBLOCK_H
#define MEDIATEXTUREAMBIENCEBLOCK_H

#include <QWidget>

class EffectSliderRow;
class EffectCheckRow;
class QCheckBox;

namespace Ui {
class MediaTextureAmbienceBlock;
}

class MediaTextureAmbienceBlock : public QWidget
{
public:
    explicit MediaTextureAmbienceBlock(QWidget* parent = nullptr);
    ~MediaTextureAmbienceBlock() override;

    EffectSliderRow* ambienceDistRow() const;
    EffectSliderRow* ambienceCurveRow() const;
    EffectSliderRow* ambienceEdgeRow() const;
    EffectSliderRow* ambiencePropRow() const;
    EffectSliderRow* motionScrollRow() const;
    EffectSliderRow* motionWarpRow() const;
    EffectSliderRow* motionPhaseRow() const;
    EffectCheckRow*  tileRepeatRow() const;
    QCheckBox*       tileRepeatCheck() const;
    EffectSliderRow* mediaResolutionRow() const;

private:
    Ui::MediaTextureAmbienceBlock* ui = nullptr;
};

#endif
