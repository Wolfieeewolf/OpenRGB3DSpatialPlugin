// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLUGIN_UI_UTILS_H
#define PLUGIN_UI_UTILS_H

#include "EffectCollapsibleSection.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include <algorithm>

inline QColor PluginUiPaletteColor(const QWidget* w, QPalette::ColorRole role)
{
    if(!w)
    {
        return QColor();
    }
    return w->palette().color(role);
}

inline QColor PluginUiGridEmptyCellColor(const QWidget* w)
{
    return PluginUiPaletteColor(w, QPalette::Base);
}

inline QColor PluginUiGridHoleCellColor(const QWidget* w)
{
    return PluginUiPaletteColor(w, QPalette::AlternateBase);
}

inline QColor PluginUiGridSelectionColor(const QWidget* w)
{
    return PluginUiPaletteColor(w, QPalette::Highlight);
}

inline QColor PluginUiBlendColors(const QColor& base, const QColor& overlay, float overlay_weight)
{
    const float keep = 1.0f - overlay_weight;
    return QColor(
        static_cast<int>(overlay.red() * overlay_weight + base.red() * keep),
        static_cast<int>(overlay.green() * overlay_weight + base.green() * keep),
        static_cast<int>(overlay.blue() * overlay_weight + base.blue() * keep));
}

inline QColor PluginUiReadableTextOn(const QColor& background, const QWidget* w)
{
    if(!w)
    {
        return QColor();
    }
    const int luminance = background.red() + background.green() + background.blue();
    return luminance > 382 ? w->palette().color(QPalette::WindowText)
                           : w->palette().color(QPalette::HighlightedText);
}

inline void PluginUiApplyMutedSecondaryLabel(QWidget* w)
{
    if(!w) return;
    const QPalette src  = w->palette();
    QColor fg           = src.color(QPalette::Disabled, QPalette::WindowText);
    if(!fg.isValid() || fg == src.color(QPalette::Active, QPalette::WindowText))
    {
        fg = src.color(QPalette::Active, QPalette::Mid);
    }
    QPalette pal = w->palette();
    pal.setColor(QPalette::Active, QPalette::WindowText, fg);
    pal.setColor(QPalette::Inactive, QPalette::WindowText, fg);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, fg);
    w->setPalette(pal);
}

inline void PluginUiApplyItalicSecondaryLabel(QWidget* w)
{
    if(!w) return;
    PluginUiApplyMutedSecondaryLabel(w);
    QFont f = w->font();
    f.setItalic(true);
    w->setFont(f);
}

inline void PluginUiApplyBoldLabel(QWidget* w)
{
    if(!w) return;
    QFont f = w->font();
    f.setBold(true);
    w->setFont(f);
}

inline void PluginUiApplyPrimaryButton(QPushButton* btn)
{
    if(!btn) return;
    btn->setDefault(true);
    btn->setAutoDefault(true);
}

inline void PluginUiSetRgbSwatchButton(QPushButton* btn, int r, int g, int b)
{
    if(!btn) return;
    int w = btn->maximumWidth() > 0 ? btn->maximumWidth() : 40;
    int h = btn->maximumHeight() > 0 ? btn->maximumHeight() : 30;
    w = std::max(4, w - 2);
    h = std::max(4, h - 2);
    QPixmap pm(w, h);
    pm.fill(QColor(r, g, b));
    btn->setIcon(QIcon(pm));
    btn->setIconSize(QSize(w, h));
    btn->setText(QString());
}

inline QFrame* PluginUiWrapInSettingsPanel(QWidget* inner, int inner_margin = 9)
{
    if(!inner)
    {
        return nullptr;
    }
    QFrame* panel = new QFrame();
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setFrameShadow(QFrame::Sunken);
    panel->setAutoFillBackground(true);
    QVBoxLayout* panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(inner_margin, inner_margin, inner_margin, inner_margin);
    panel_layout->setSpacing(0);
    panel_layout->addWidget(inner);
    return panel;
}

inline void PluginUiAddSectionBlock(QVBoxLayout* main_layout,
                                    const QString& title,
                                    const QString& tip,
                                    QWidget* body,
                                    QWidget** out_entire_section = nullptr,
                                    bool start_expanded = true)
{
    if(!main_layout || !body)
    {
        return;
    }

    auto* section = new EffectCollapsibleSection(title, tip);
    section->setExpanded(start_expanded);
    QFrame* framed = PluginUiWrapInSettingsPanel(body);
    section->bodyLayout()->addWidget(framed);
    main_layout->addWidget(section);
    if(out_entire_section)
    {
        *out_entire_section = section;
    }
}

inline void PluginUiAddEffectTransportButtons(QHBoxLayout* row,
                                              QPushButton** out_start = nullptr,
                                              QPushButton** out_stop = nullptr)
{
    if(!row)
    {
        return;
    }
    QPushButton* start = new QPushButton(QStringLiteral("Start Effect"));
    QPushButton* stop  = new QPushButton(QStringLiteral("Stop Effect"));
    stop->setEnabled(false);
    row->addWidget(start);
    row->addWidget(stop);
    row->addStretch();
    if(out_start)
    {
        *out_start = start;
    }
    if(out_stop)
    {
        *out_stop = stop;
    }
}

#endif
