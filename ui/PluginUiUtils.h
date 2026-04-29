// SPDX-License-Identifier: GPL-2.0-only
/** Helpers for plugin UI without hardcoded stylesheets (follow OpenRGB application style). */

#ifndef PLUGIN_UI_UTILS_H
#define PLUGIN_UI_UTILS_H

#include <QPushButton>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QWidget>
#include <algorithm>

/** Muted helper text using a dimmed WindowText color (PlaceholderText is for line-edit hints only). */
inline void PluginUiApplyMutedSecondaryLabel(QWidget* w)
{
    if(!w) return;
    QColor fg = QGuiApplication::palette().color(QPalette::Active, QPalette::WindowText);
    fg.setAlphaF(0.70f);
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

#endif
