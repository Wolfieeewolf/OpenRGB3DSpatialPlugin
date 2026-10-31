// SPDX-License-Identifier: GPL-2.0-only
#ifndef EFFECTSLIDERPROMOTE_H
#define EFFECTSLIDERPROMOTE_H

#include "EffectDoubleSliderRow.h"
#include "EffectSliderRow.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

/**
 * Plugin form layout norm (Qt QFormLayout / desktop settings panels):
 * - Captions: right-aligned in a shared-width column
 * - Controls: left edge lined up, expanding to the right
 * - Not centre-aligned; not ragged left captions of uneven width
 */
inline void ApplyRightAlignedCaption(QLabel* caption, int min_width = 0)
{
    if(!caption)
    {
        return;
    }
    caption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if(min_width > 0)
    {
        caption->setMinimumWidth(min_width);
    }
}

/** Equalize caption widths and right-align so control columns share one left edge. */
inline void AlignCaptionLabels(std::initializer_list<QLabel*> labels, int min_floor = 96)
{
    int width = min_floor;
    for(QLabel* label : labels)
    {
        if(label)
        {
            width = std::max(width, label->sizeHint().width());
        }
    }
    for(QLabel* label : labels)
    {
        ApplyRightAlignedCaption(label, width);
    }
}

/** Treat item 0 of each HBox as the caption; stretch item 1 as the control. */
inline void AlignHBoxRowCaptions(std::initializer_list<QHBoxLayout*> rows, int min_floor = 96)
{
    std::vector<QLabel*> labels;
    labels.reserve(rows.size());
    for(QHBoxLayout* row : rows)
    {
        if(!row || row->count() < 1 || !row->itemAt(0))
        {
            continue;
        }
        if(QLabel* label = qobject_cast<QLabel*>(row->itemAt(0)->widget()))
        {
            labels.push_back(label);
        }
        if(row->count() > 1)
        {
            row->setStretch(1, 1);
        }
    }
    int width = min_floor;
    for(QLabel* label : labels)
    {
        width = std::max(width, label->sizeHint().width());
    }
    for(QLabel* label : labels)
    {
        ApplyRightAlignedCaption(label, width);
    }
}

/** Right-align every QLabel in column 0 of a QGridLayout; stretch the next column. */
inline void AlignGridCaptionColumn(QGridLayout* grid, int caption_col = 0, int min_floor = 96)
{
    if(!grid)
    {
        return;
    }
    int width = min_floor;
    std::vector<QLabel*> labels;
    for(int i = 0; i < grid->count(); ++i)
    {
        int row = -1;
        int col = -1;
        int rs = 0;
        int cs = 0;
        grid->getItemPosition(i, &row, &col, &rs, &cs);
        if(col != caption_col || !grid->itemAt(i))
        {
            continue;
        }
        if(QLabel* label = qobject_cast<QLabel*>(grid->itemAt(i)->widget()))
        {
            labels.push_back(label);
            width = std::max(width, label->sizeHint().width());
        }
    }
    for(QLabel* label : labels)
    {
        ApplyRightAlignedCaption(label, width);
    }
    grid->setColumnStretch(caption_col, 0);
    if(grid->columnCount() > caption_col + 1)
    {
        grid->setColumnStretch(caption_col + 1, 1);
    }
}

inline void AlignFormCaptions(QFormLayout* form)
{
    if(!form)
    {
        return;
    }
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
}

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

/** Replace a QSpinBox inside a known QHBoxLayout with EffectSliderRow (spin = row->spinBox()). */
inline EffectSliderRow* ReplaceSpinInHBox(QHBoxLayout* hbox, QSpinBox* spin, const QString& suffix = QString())
{
    if(!hbox || !spin)
    {
        return nullptr;
    }
    const int idx = hbox->indexOf(spin);
    if(idx < 0)
    {
        return nullptr;
    }
    auto* row = new EffectSliderRow(hbox->parentWidget());
    row->configure(spin->minimum(), spin->maximum(), spin->value(), spin->toolTip());
    if(!suffix.isEmpty())
    {
        row->setSuffix(suffix);
    }
    else if(!spin->suffix().isEmpty())
    {
        row->setSuffix(spin->suffix());
    }
    hbox->removeWidget(spin);
    spin->hide();
    spin->deleteLater();
    hbox->insertWidget(idx, row, 1);
    return row;
}

/** Wrap an existing QDoubleSpinBox in EffectDoubleSliderRow (does not insert into a layout). */
inline EffectDoubleSliderRow* MakeDoubleSpinOverThinSliderStack(QDoubleSpinBox* spin, QWidget* parent = nullptr)
{
    if(!spin)
    {
        return nullptr;
    }
    auto* row = new EffectDoubleSliderRow(parent);
    row->adoptExistingSpin(spin);
    return row;
}

/** Replace an in-layout QDoubleSpinBox with EffectDoubleSliderRow (grid/hbox only). */
inline EffectDoubleSliderRow* StackDoubleSpinOverThinSlider(QDoubleSpinBox* spin)
{
    if(!spin)
    {
        return nullptr;
    }

    QLayout* owning = nullptr;
    int owning_index = -1;
    for(QObject* walk = spin->parent(); walk; walk = walk->parent())
    {
        QWidget* w = qobject_cast<QWidget*>(walk);
        if(!w || !w->layout())
        {
            continue;
        }
        if(QHBoxLayout* hbox = qobject_cast<QHBoxLayout*>(w->layout()))
        {
            const int idx = hbox->indexOf(spin);
            if(idx >= 0)
            {
                owning = hbox;
                owning_index = idx;
                break;
            }
        }
        if(QGridLayout* grid = qobject_cast<QGridLayout*>(w->layout()))
        {
            const int idx = grid->indexOf(spin);
            if(idx >= 0)
            {
                owning = grid;
                owning_index = idx;
                break;
            }
        }
    }
    if(!owning)
    {
        return nullptr;
    }

    int grid_r = -1;
    int grid_c = -1;
    int grid_rs = 0;
    int grid_cs = 0;
    if(QGridLayout* grid = qobject_cast<QGridLayout*>(owning))
    {
        grid->getItemPosition(owning_index, &grid_r, &grid_c, &grid_rs, &grid_cs);
        grid->removeWidget(spin);
    }
    else if(QHBoxLayout* hbox = qobject_cast<QHBoxLayout*>(owning))
    {
        hbox->removeWidget(spin);
    }

    EffectDoubleSliderRow* row = MakeDoubleSpinOverThinSliderStack(spin, owning->parentWidget());
    if(!row)
    {
        return nullptr;
    }

    if(QGridLayout* grid = qobject_cast<QGridLayout*>(owning))
    {
        grid->addWidget(row, grid_r, grid_c, grid_rs, grid_cs > 0 ? grid_cs : 1);
    }
    else if(QHBoxLayout* hbox = qobject_cast<QHBoxLayout*>(owning))
    {
        hbox->insertWidget(owning_index, row, 1);
    }
    return row;
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

/** Enable/disable a thin-slider row (spin + slider), not only the QSlider child. */
inline void SetEffectSliderEnabled(QSlider* slider, bool enabled)
{
    if(!slider)
    {
        return;
    }
    if(EffectSliderRow* row = qobject_cast<EffectSliderRow*>(slider->parentWidget()))
    {
        row->setEnabled(enabled);
        return;
    }
    if(EffectDoubleSliderRow* row = qobject_cast<EffectDoubleSliderRow*>(slider->parentWidget()))
    {
        row->setEnabled(enabled);
        return;
    }
    slider->setEnabled(enabled);
}

/** Sync a double spin that may live inside an EffectDoubleSliderRow (keeps thin slider in lockstep). */
inline void SyncEffectDoubleSpinValue(QDoubleSpinBox* spin, double value)
{
    if(!spin)
    {
        return;
    }
    if(EffectDoubleSliderRow* row = qobject_cast<EffectDoubleSliderRow*>(spin->parentWidget()))
    {
        row->syncSpinValue(value);
        return;
    }
    QSignalBlocker blocker(spin);
    spin->setValue(value);
}

/** Enable/disable a double spin-over-slider row, not only the spin child. */
inline void SetEffectDoubleSpinEnabled(QDoubleSpinBox* spin, bool enabled)
{
    if(!spin)
    {
        return;
    }
    if(EffectDoubleSliderRow* row = qobject_cast<EffectDoubleSliderRow*>(spin->parentWidget()))
    {
        row->setEnabled(enabled);
        return;
    }
    spin->setEnabled(enabled);
}

#endif
