// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* Shared look for plugin tool bars, timelines and cards. Every colour comes from the widget's
   palette, so it follows whatever theme OpenRGB uses (light or dark); the accent is the
   palette's Highlight. Icons are drawn in code and recoloured to match. */

#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QRectF>
#include <QString>

class QAbstractButton;
class QPainter;
class QToolButton;
class QWidget;

namespace PluginTheme
{

enum class Icon
{
    Undo,
    Redo,
    Play,
    Pause,
    Stop,
    ToStart,
    Snap,
    Follow,
    ZoomIn,
    ZoomOut,
    ZoomFit,
    Save,
    Marker,
    Tap,
    Loop,
    Add,
};

bool IsDark(const QPalette& palette);

/** Palette text colour at an alpha chosen for light or dark themes (lines, grids, hints). */
QColor Line(const QPalette& palette, int light_alpha, int dark_alpha);

QColor Accent(const QPalette& palette);

QColor WithAlpha(QColor color, int alpha);

/** Icon in the palette's text colour, with a dimmed disabled state. */
QIcon MakeIcon(Icon icon, const QPalette& palette, int size = 18);

/** Flat, rounded icon button: clear until hovered, accent tint when checked. */
QToolButton* MakeToolButton(Icon icon, const QString& tooltip, QWidget* parent, bool checkable = false);

/** Style every QToolButton inside a bar the same way (call once on the bar). */
void ApplyFlatToolBar(QWidget* bar);

/** Rounded card outline: dark edge normally, accent edge when selected, dashed for ghosts. */
void PaintCardFrame(QPainter& painter, const QRectF& rect, const QColor& base, bool selected,
                    bool ghost, const QPalette& palette, qreal radius = 5.0);

/** Thin vertical guide line, used for snap feedback and markers. */
void PaintGuideLine(QPainter& painter, qreal x, qreal top, qreal bottom, const QColor& color);

} // namespace PluginTheme
