// SPDX-License-Identifier: GPL-2.0-or-later

#include "PluginTheme.h"

#include <QAbstractButton>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QScreen>
#include <QToolButton>
#include <QWidget>

#include <cmath>

namespace PluginTheme
{

namespace
{
/* Icons are drawn on a 24-unit grid, then scaled: line icons with round caps and joins. */
void DrawIcon(QPainter& p, Icon icon, const QColor& color)
{
    QPen pen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto arrow_head = [&](QPointF tip, QPointF a, QPointF b) {
        QPainterPath path;
        path.moveTo(a);
        path.lineTo(tip);
        path.lineTo(b);
        p.drawPath(path);
    };

    switch(icon)
    {
        case Icon::Undo:
        {
            QPainterPath path;
            path.moveTo(5, 9);
            path.lineTo(14, 9);
            path.cubicTo(18, 9, 20, 12, 20, 14.5);
            path.cubicTo(20, 17, 18, 20, 14, 20);
            path.lineTo(9, 20);
            p.drawPath(path);
            arrow_head(QPointF(5, 9), QPointF(9, 5), QPointF(9, 13));
            break;
        }
        case Icon::Redo:
        {
            QPainterPath path;
            path.moveTo(19, 9);
            path.lineTo(10, 9);
            path.cubicTo(6, 9, 4, 12, 4, 14.5);
            path.cubicTo(4, 17, 6, 20, 10, 20);
            path.lineTo(15, 20);
            p.drawPath(path);
            arrow_head(QPointF(19, 9), QPointF(15, 5), QPointF(15, 13));
            break;
        }
        case Icon::Play:
        {
            QPainterPath path;
            path.moveTo(7, 4.5);
            path.lineTo(19, 12);
            path.lineTo(7, 19.5);
            path.closeSubpath();
            p.setBrush(color);
            p.drawPath(path);
            break;
        }
        case Icon::Pause:
            p.setBrush(color);
            p.drawRoundedRect(QRectF(6, 5, 4, 14), 1, 1);
            p.drawRoundedRect(QRectF(14, 5, 4, 14), 1, 1);
            break;
        case Icon::Stop:
            p.setBrush(color);
            p.drawRoundedRect(QRectF(6, 6, 12, 12), 2, 2);
            break;
        case Icon::ToStart:
        {
            p.drawLine(QPointF(6, 5), QPointF(6, 19));
            QPainterPath path;
            path.moveTo(18, 5.5);
            path.lineTo(8.5, 12);
            path.lineTo(18, 18.5);
            path.closeSubpath();
            p.setBrush(color);
            p.drawPath(path);
            break;
        }
        case Icon::Snap:
        {
            /* Magnet: a U with two pole caps. */
            QPainterPath path;
            path.moveTo(6, 4);
            path.lineTo(6, 12);
            path.cubicTo(6, 16, 8.5, 19, 12, 19);
            path.cubicTo(15.5, 19, 18, 16, 18, 12);
            path.lineTo(18, 4);
            p.drawPath(path);
            p.drawLine(QPointF(4, 8), QPointF(8, 8));
            p.drawLine(QPointF(16, 8), QPointF(20, 8));
            break;
        }
        case Icon::Follow:
            /* Playhead with a forward chevron. */
            p.drawLine(QPointF(8, 4), QPointF(8, 20));
            p.setBrush(color);
            p.drawPolygon(QPolygonF({QPointF(5, 4), QPointF(11, 4), QPointF(8, 7.5)}));
            p.setBrush(Qt::NoBrush);
            arrow_head(QPointF(19, 12), QPointF(15, 8), QPointF(15, 16));
            p.drawLine(QPointF(12, 12), QPointF(19, 12));
            break;
        case Icon::ZoomIn:
        case Icon::ZoomOut:
        case Icon::ZoomFit:
            p.drawEllipse(QPointF(10.5, 10.5), 6, 6);
            p.drawLine(QPointF(15, 15), QPointF(20, 20));
            if(icon == Icon::ZoomFit)
            {
                p.drawRect(QRectF(8, 8.5, 5, 4));
            }
            else
            {
                p.drawLine(QPointF(8, 10.5), QPointF(13, 10.5));
                if(icon == Icon::ZoomIn)
                {
                    p.drawLine(QPointF(10.5, 8), QPointF(10.5, 13));
                }
            }
            break;
        case Icon::Save:
        {
            QPainterPath path;
            path.moveTo(5, 4);
            path.lineTo(16, 4);
            path.lineTo(20, 8);
            path.lineTo(20, 20);
            path.lineTo(4, 20);
            path.lineTo(4, 5);
            path.closeSubpath();
            p.drawPath(path);
            p.drawRect(QRectF(8, 4, 7, 5));
            p.drawRect(QRectF(7.5, 13, 9, 7));
            break;
        }
        case Icon::Marker:
            p.drawLine(QPointF(6, 3.5), QPointF(6, 20.5));
            p.setBrush(color);
            p.drawPolygon(QPolygonF({QPointF(6, 4), QPointF(18, 7.5), QPointF(6, 11)}));
            break;
        case Icon::Tap:
            /* A finger tap: dot with two ripples. */
            p.setBrush(color);
            p.drawEllipse(QPointF(12, 12), 2.5, 2.5);
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(6.5, 6.5, 11, 11), 30 * 16, 120 * 16);
            p.drawArc(QRectF(3, 3, 18, 18), 30 * 16, 120 * 16);
            p.drawArc(QRectF(6.5, 6.5, 11, 11), 210 * 16, 120 * 16);
            p.drawArc(QRectF(3, 3, 18, 18), 210 * 16, 120 * 16);
            break;
        case Icon::Loop:
        {
            QPainterPath path;
            path.moveTo(17, 7);
            path.lineTo(8, 7);
            path.cubicTo(5, 7, 4, 9.5, 4, 12);
            p.drawPath(path);
            QPainterPath back;
            back.moveTo(7, 17);
            back.lineTo(16, 17);
            back.cubicTo(19, 17, 20, 14.5, 20, 12);
            p.drawPath(back);
            arrow_head(QPointF(17, 7), QPointF(14, 4), QPointF(14, 10));
            arrow_head(QPointF(7, 17), QPointF(10, 14), QPointF(10, 20));
            break;
        }
        case Icon::Add:
            p.drawLine(QPointF(12, 5), QPointF(12, 19));
            p.drawLine(QPointF(5, 12), QPointF(19, 12));
            break;
    }
}

QPixmap RenderIcon(Icon icon, const QColor& color, int size)
{
    const qreal dpr = qGuiApp && qGuiApp->primaryScreen() ? qGuiApp->primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / 24.0, size / 24.0);
    DrawIcon(p, icon, color);
    return pm;
}
} // namespace

bool IsDark(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

QColor WithAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

QColor Line(const QPalette& palette, int light_alpha, int dark_alpha)
{
    return WithAlpha(palette.color(QPalette::Text), IsDark(palette) ? dark_alpha : light_alpha);
}

QColor Accent(const QPalette& palette)
{
    return palette.color(QPalette::Highlight);
}

QIcon MakeIcon(Icon icon, const QPalette& palette, int size)
{
    QIcon out;
    const QColor normal = palette.color(QPalette::Active, QPalette::ButtonText);
    out.addPixmap(RenderIcon(icon, normal, size), QIcon::Normal, QIcon::Off);
    out.addPixmap(RenderIcon(icon, normal, size), QIcon::Normal, QIcon::On);
    out.addPixmap(RenderIcon(icon, WithAlpha(normal, 90), size), QIcon::Disabled);
    return out;
}

QToolButton* MakeToolButton(Icon icon, const QString& tooltip, QWidget* parent, bool checkable)
{
    auto* button = new QToolButton(parent);
    button->setIcon(MakeIcon(icon, parent ? parent->palette() : QPalette()));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip.section(QLatin1Char('('), 0, 0).trimmed());
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::TabFocus);
    button->setFixedSize(30, 28);
    return button;
}

void ApplyFlatToolBar(QWidget* bar)
{
    if(!bar)
    {
        return;
    }
    const QPalette pal = bar->palette();
    const QColor text = pal.color(QPalette::Text);
    const QColor accent = Accent(pal);
    auto rgba = [](const QColor& c, int a) {
        return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };
    if(bar->objectName().isEmpty())
    {
        bar->setObjectName(QStringLiteral("pluginFlatToolBar"));
    }
    const QString sel = QStringLiteral("QWidget#%1 QToolButton").arg(bar->objectName());
    bar->setStyleSheet(QStringLiteral(
        "%1 { background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 2px; }"
        "%1:hover { background: %2; }"
        "%1:pressed { background: %3; }"
        "%1:checked { background: %4; border-color: %5; }"
        "%1:checked:hover { background: %6; }"
        "%1:focus { border-color: %5; }")
        .arg(sel, rgba(text, 34), rgba(text, 64), rgba(accent, 70), rgba(accent, 170), rgba(accent, 95))
        /* Menu buttons: a soft outlined box, with the arrow in its own space on the right. */
        + QStringLiteral(
        "%1[pluginMenuButton=\"true\"] { background: %2; border: 1px solid %3; border-radius: 5px;"
        " padding: 2px 22px 2px 10px; }"
        "%1[pluginMenuButton=\"true\"]:hover { background: %4; border-color: %5; }"
        "%1[pluginMenuButton=\"true\"]:pressed, %1[pluginMenuButton=\"true\"]:open { background: %6; border-color: %5; }"
        "%1[pluginMenuButton=\"true\"]:disabled { background: transparent; border-color: %7; }"
        "%1[pluginMenuButton=\"true\"]::menu-indicator { subcontrol-origin: padding;"
        " subcontrol-position: center right; right: 7px; }")
        .arg(sel, rgba(text, 12), rgba(text, 55), rgba(text, 34), rgba(accent, 170), rgba(accent, 70), rgba(text, 25)));
}

void PaintCardFrame(QPainter& painter, const QRectF& rect, const QColor& base, bool selected, bool ghost,
                    const QPalette& palette, qreal radius)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    const qreal width = selected ? 2.0 : 1.0;
    const QRectF r = rect.adjusted(width / 2.0, width / 2.0, -width / 2.0, -width / 2.0);
    QColor edge = selected ? Accent(palette).lighter(125) : base.darker(160);
    if(!selected)
    {
        edge.setAlpha(ghost ? 140 : 230);
    }
    painter.setPen(QPen(edge, width, ghost ? Qt::DashLine : Qt::SolidLine));
    painter.drawRoundedRect(r, radius, radius);
    painter.restore();
}

void PaintGuideLine(QPainter& painter, qreal x, qreal top, qreal bottom, const QColor& color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(color, 1.0, Qt::DashLine);
    pen.setDashPattern({3.0, 3.0});
    painter.setPen(pen);
    painter.drawLine(QPointF(std::round(x) + 0.5, top), QPointF(std::round(x) + 0.5, bottom));
    painter.restore();
}

} // namespace PluginTheme
