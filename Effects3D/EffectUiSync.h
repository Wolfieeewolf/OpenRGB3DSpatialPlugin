// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTUISYNC_H
#define EFFECTUISYNC_H

#include "EffectLabeledComboRow.h"
#include "EffectSliderRow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QWidget>

#include <algorithm>
#include <functional>

namespace EffectUiSync
{

template<typename RowWidget>
inline RowWidget* findRowByName(QWidget* root, const char* object_name)
{
    if(!root)
    {
        return nullptr;
    }
    QWidget* child = root->findChild<QWidget*>(object_name);
    return child ? dynamic_cast<RowWidget*>(child) : nullptr;
}

inline EffectSliderRow* sliderRow(QWidget* root, const char* object_name)
{
    return findRowByName<EffectSliderRow>(root, object_name);
}

inline EffectLabeledComboRow* comboRow(QWidget* root, const char* object_name)
{
    return findRowByName<EffectLabeledComboRow>(root, object_name);
}

inline QCheckBox* checkBox(QWidget* root, const char* object_name)
{
    return root ? root->findChild<QCheckBox*>(object_name) : nullptr;
}

inline QWidget* effectPanel(QWidget* root, const char* panel_object_name)
{
    return root ? root->findChild<QWidget*>(panel_object_name) : nullptr;
}

inline void setComboIndex(QWidget* root, const char* row_name, int index)
{
    if(EffectLabeledComboRow* row = comboRow(root, row_name))
    {
        if(QComboBox* combo = row->combo())
        {
            const int clamped = std::clamp(index, 0, std::max(0, combo->count() - 1));
            QSignalBlocker blocker(combo);
            combo->setCurrentIndex(clamped);
        }
    }
}

inline void setComboData(QWidget* root, const char* row_name, int data_value)
{
    if(EffectLabeledComboRow* row = comboRow(root, row_name))
    {
        if(QComboBox* combo = row->combo())
        {
            int idx = combo->findData(data_value);
            if(idx < 0)
            {
                idx = 0;
            }
            QSignalBlocker blocker(combo);
            combo->setCurrentIndex(idx);
        }
    }
}

inline void setSliderValue(QWidget* root,
                          const char* row_name,
                          int value,
                          const std::function<QString(int)>& format_value = nullptr)
{
    if(EffectSliderRow* row = sliderRow(root, row_name))
    {
        row->syncSliderValue(value, format_value);
    }
}

inline void setCheckBox(QWidget* root, const char* name, bool checked)
{
    if(QCheckBox* box = checkBox(root, name))
    {
        QSignalBlocker blocker(box);
        box->setChecked(checked);
    }
}

inline void setSliderByCaption(QWidget* root,
                               const QString& caption,
                               int value,
                               const std::function<QString(int)>& format_value = nullptr)
{
    if(!root)
    {
        return;
    }
    for(QWidget* child : root->findChildren<QWidget*>())
    {
        EffectSliderRow* row = dynamic_cast<EffectSliderRow*>(child);
        if(!row || row->captionText() != caption)
        {
            continue;
        }
        row->syncSliderValue(value, format_value);
        return;
    }
}

inline void setComboDataByCaption(QWidget* root, const QString& caption, int data_value)
{
    if(!root)
    {
        return;
    }
    for(QWidget* child : root->findChildren<QWidget*>())
    {
        EffectLabeledComboRow* row = dynamic_cast<EffectLabeledComboRow*>(child);
        if(!row || row->captionText() != caption)
        {
            continue;
        }
        if(QComboBox* combo = row->combo())
        {
            int idx = combo->findData(data_value);
            if(idx < 0)
            {
                idx = 0;
            }
            QSignalBlocker blocker(combo);
            combo->setCurrentIndex(idx);
        }
        return;
    }
}

} // namespace EffectUiSync

#endif
