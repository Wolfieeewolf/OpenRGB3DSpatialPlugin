// SPDX-License-Identifier: GPL-2.0-only
/** Helpers for plugin UI: no QSS; no per-widget palette on panels/frames (inherit app/theme palette).
 *  OpenRGBEffectsPlugin leans on Qt Designer .ui files and named composite widgets; this
 *  plugin builds the same kinds of sections in code and centralizes repeated pieces here
 *  (muted labels, swatches, section blocks, transport buttons) for consistency. */

#ifndef PLUGIN_UI_UTILS_H
#define PLUGIN_UI_UTILS_H

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include <algorithm>

/** Helper / secondary text: use theme’s Disabled+WindowText (OpenRGB dark theme sets this to muted gray). */
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

/** Color preview on a push button without QSS (uses icon swatch). */
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

/** Inset panel: shape/shadow only; fill comes from the active Qt style (CONTRIBUTING — no custom chrome palette). */
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

/** Bold title + framed body + gap. out_entire_section = outer widget (title + frame) for show/hide (e.g. Minecraft). */
inline void PluginUiAddSectionBlock(QVBoxLayout* main_layout,
                                    const QString& title,
                                    const QString& tip,
                                    QWidget* body,
                                    QWidget** out_entire_section = nullptr)
{
    if(!main_layout || !body)
    {
        return;
    }
    QLabel* heading = new QLabel(title);
    QFont f = heading->font();
    f.setBold(true);
    heading->setFont(f);
    if(!tip.isEmpty())
    {
        heading->setToolTip(tip);
    }
    QFrame* framed = PluginUiWrapInSettingsPanel(body);
    if(out_entire_section)
    {
        QWidget* root = new QWidget();
        QVBoxLayout* root_layout = new QVBoxLayout(root);
        root_layout->setContentsMargins(0, 0, 0, 0);
        root_layout->setSpacing(0);
        root_layout->addWidget(heading);
        root_layout->addSpacing(2);
        root_layout->addWidget(framed);
        root_layout->addSpacing(10);
        main_layout->addWidget(root);
        *out_entire_section = root;
    }
    else
    {
        main_layout->addWidget(heading);
        main_layout->addSpacing(2);
        main_layout->addWidget(framed);
        main_layout->addSpacing(10);
    }
}

/** Start / Stop pair aligned with EffectLayerBanner (shared wording and initial enabled state). */
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
