// SPDX-License-Identifier: GPL-2.0-only

#include "MediaTextureAmbienceBlock.h"
#include "EffectCheckRow.h"
#include "EffectSliderRow.h"
#include "ui_MediaTextureAmbienceBlock.h"

MediaTextureAmbienceBlock::MediaTextureAmbienceBlock(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MediaTextureAmbienceBlock)
{
    ui->setupUi(this);
    ui->tileRepeatRow->configure(QStringLiteral("Tile / repeat outside image bounds"), false);
}

MediaTextureAmbienceBlock::~MediaTextureAmbienceBlock()
{
    delete ui;
}

EffectSliderRow* MediaTextureAmbienceBlock::ambienceDistRow() const { return ui->ambienceDistRow; }
EffectSliderRow* MediaTextureAmbienceBlock::ambienceCurveRow() const { return ui->ambienceCurveRow; }
EffectSliderRow* MediaTextureAmbienceBlock::ambienceEdgeRow() const { return ui->ambienceEdgeRow; }
EffectSliderRow* MediaTextureAmbienceBlock::ambiencePropRow() const { return ui->ambiencePropRow; }
EffectSliderRow* MediaTextureAmbienceBlock::motionScrollRow() const { return ui->motionScrollRow; }
EffectSliderRow* MediaTextureAmbienceBlock::motionWarpRow() const { return ui->motionWarpRow; }
EffectSliderRow* MediaTextureAmbienceBlock::motionPhaseRow() const { return ui->motionPhaseRow; }
EffectCheckRow* MediaTextureAmbienceBlock::tileRepeatRow() const { return ui->tileRepeatRow; }
QCheckBox* MediaTextureAmbienceBlock::tileRepeatCheck() const { return ui->tileRepeatRow->checkBox(); }
EffectSliderRow* MediaTextureAmbienceBlock::mediaResolutionRow() const { return ui->mediaResolutionRow; }
