// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTUIROWS_H
#define EFFECTUIROWS_H

#include "EffectCheckRow.h"
#include "EffectInfoLabel.h"
#include "EffectLabeledComboRow.h"
#include "EffectLabeledSpinRow.h"
#include "EffectCollapsibleSection.h"
#include "EffectSectionHeading.h"
#include "EffectSliderRow.h"

#include <QVBoxLayout>
#include <functional>

namespace EffectUiRows
{

inline QWidget* NewEffectPanel(const char* panel_object_name = nullptr)
{
    auto* panel = new QWidget();
    if(panel_object_name && panel_object_name[0] != '\0')
    {
        panel->setObjectName(panel_object_name);
    }
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    return panel;
}

inline QVBoxLayout* PanelLayout(QWidget* panel)
{
    return panel ? qobject_cast<QVBoxLayout*>(panel->layout()) : nullptr;
}

inline EffectCollapsibleSection* AppendCollapsibleSection(QVBoxLayout* layout,
                                                          const QString& title,
                                                          const QString& tooltip = QString(),
                                                          bool start_expanded = true)
{
    if(!layout || title.isEmpty())
    {
        return nullptr;
    }
    auto* section = new EffectCollapsibleSection(title, tooltip);
    section->setExpanded(start_expanded);
    layout->addWidget(section);
    return section;
}

inline QVBoxLayout* AppendCollapsibleSectionBody(QVBoxLayout* layout,
                                                   const QString& title,
                                                   const QString& tooltip = QString(),
                                                   bool start_expanded = true)
{
    EffectCollapsibleSection* section = AppendCollapsibleSection(layout, title, tooltip, start_expanded);
    return section ? section->bodyLayout() : nullptr;
}

inline void AppendSectionHeading(QVBoxLayout* layout, const QString& title)
{
    if(!layout || title.isEmpty())
    {
        return;
    }
    auto* heading = new EffectSectionHeading();
    heading->setTitle(title);
    layout->addWidget(heading);
}

inline EffectCheckRow* AppendCheckRow(QVBoxLayout* layout,
                                       const QString& text,
                                       bool checked,
                                       const QString& tooltip = QString())
{
    if(!layout || text.isEmpty())
    {
        return nullptr;
    }
    auto* row = new EffectCheckRow();
    row->configure(text, checked, tooltip);
    layout->addWidget(row);
    return row;
}

inline EffectInfoLabel* AppendInfoLabel(QVBoxLayout* layout, const QString& text)
{
    if(!layout)
    {
        return nullptr;
    }
    auto* row = new EffectInfoLabel();
    row->setText(text);
    layout->addWidget(row);
    return row;
}

inline EffectLabeledComboRow* AppendComboRow(QVBoxLayout* layout, const QString& caption)
{
    if(!layout || caption.isEmpty())
    {
        return nullptr;
    }
    auto* row = new EffectLabeledComboRow();
    row->setCaptionText(caption);
    layout->addWidget(row);
    return row;
}

inline EffectSliderRow* AppendSliderRow(QVBoxLayout* layout,
                                        const QString& caption,
                                        int min,
                                        int max,
                                        int value,
                                        const QString& tooltip = QString())
{
    if(!layout || caption.isEmpty())
    {
        return nullptr;
    }
    auto* row = new EffectSliderRow();
    row->setCaptionText(caption);
    row->configure(min, max, value, tooltip);
    layout->addWidget(row);
    return row;
}

inline EffectLabeledSpinRow* AppendSpinRow(QVBoxLayout* layout,
                                          const QString& caption,
                                          int min,
                                          int max,
                                          int value,
                                          const QString& tooltip = QString())
{
    if(!layout || caption.isEmpty())
    {
        return nullptr;
    }
    auto* row = new EffectLabeledSpinRow();
    row->setCaptionText(caption);
    row->configure(min, max, value, tooltip);
    layout->addWidget(row);
    return row;
}

} // namespace EffectUiRows

#endif
