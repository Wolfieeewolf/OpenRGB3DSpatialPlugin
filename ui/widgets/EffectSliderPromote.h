// SPDX-License-Identifier: GPL-2.0-only
#ifndef EFFECTSLIDERPROMOTE_H
#define EFFECTSLIDERPROMOTE_H

#include "EffectSliderRow.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QString>

/** Replace a legacy caption-row QSlider + value QLabel with EffectSliderRow
    (spin over thin slider). Keeps the caption (and any trailing widgets). */
inline EffectSliderRow* PromoteSliderLabelToEffectRow(QHBoxLayout* hbox,
                                                     QSlider* slider,
                                                     QLabel* label,
                                                     const QString& suffix = QString())
{
    if(!hbox || !slider)
    {
        return nullptr;
    }
    auto* row = new EffectSliderRow(hbox->parentWidget());
    row->configure(slider->minimum(), slider->maximum(), slider->value(), slider->toolTip());
    if(!suffix.isEmpty())
    {
        row->setSuffix(suffix);
    }
    const int idx = hbox->indexOf(slider);
    hbox->removeWidget(slider);
    if(label)
    {
        hbox->removeWidget(label);
        label->hide();
        label->deleteLater();
    }
    slider->hide();
    slider->deleteLater();
    if(idx >= 0)
    {
        hbox->insertWidget(idx, row, /*stretch*/ 1);
    }
    else
    {
        hbox->addWidget(row, 1);
    }
    return row;
}

/** Same as PromoteSliderLabelToEffectRow for QGridLayout cells (slider + adjacent value label).
    Places EffectSliderRow at the slider's (row, col) spanning two columns. */
inline EffectSliderRow* PromoteSliderLabelInGrid(QGridLayout* grid,
                                                QSlider* slider,
                                                QLabel* label,
                                                const QString& suffix = QString())
{
    if(!grid || !slider)
    {
        return nullptr;
    }
    int row = -1;
    int col = -1;
    int row_span = 0;
    int col_span = 0;
    const int idx = grid->indexOf(slider);
    if(idx < 0)
    {
        return nullptr;
    }
    grid->getItemPosition(idx, &row, &col, &row_span, &col_span);

    auto* effect_row = new EffectSliderRow(grid->parentWidget());
    effect_row->configure(slider->minimum(), slider->maximum(), slider->value(), slider->toolTip());
    if(!suffix.isEmpty())
    {
        effect_row->setSuffix(suffix);
    }

    grid->removeWidget(slider);
    if(label)
    {
        grid->removeWidget(label);
        label->hide();
        label->deleteLater();
    }
    slider->hide();
    slider->deleteLater();

    grid->addWidget(effect_row, row, col, 1, 2);
    return effect_row;
}

/** Sync a slider that may live inside an EffectSliderRow (keeps spin in lockstep). */
inline void SyncEffectSliderValue(QSlider* slider, int value)
{
    if(!slider)
    {
        return;
    }
    if(EffectSliderRow* row = qobject_cast<EffectSliderRow*>(slider->parentWidget()))
    {
        row->syncSliderValue(value);
        return;
    }
    QSignalBlocker blocker(slider);
    slider->setValue(value);
}

#endif
