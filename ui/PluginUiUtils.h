// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLUGIN_UI_UTILS_H
#define PLUGIN_UI_UTILS_H

#include "EffectCollapsibleSection.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
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

inline QString PluginUiColorCss(const QColor& c)
{
    return c.name(QColor::HexRgb);
}

/** Visual Map–like controller card: raised panel fill, subtle border, raised toolbuttons. */
inline void PluginUiApplyControllerCardChrome(QFrame* frame, bool selected = false)
{
    if(!frame)
    {
        return;
    }

    frame->setFrameShape(QFrame::NoFrame);
    frame->setFrameShadow(QFrame::Plain);
    frame->setAutoFillBackground(false);

    // Prefer the host window palette so cards match once parented into OpenRGB.
    const QWidget* palette_source = frame->window() ? frame->window() : frame;
    const QPalette pal            = palette_source->palette();
    const QColor window           = pal.color(QPalette::Window);
    const QColor base             = pal.color(QPalette::Base);
    QColor card_bg                = pal.color(QPalette::Button);

    // Some host themes make Button identical to Window/Base; lift AlternateBase instead.
    if(!card_bg.isValid() || card_bg == window || card_bg == base)
    {
        card_bg = PluginUiBlendColors(base, pal.color(QPalette::AlternateBase), 0.65f);
    }
    if(selected)
    {
        card_bg = PluginUiBlendColors(card_bg, pal.color(QPalette::Highlight), 0.35f);
    }

    QColor border = pal.color(QPalette::Dark);
    if(!border.isValid() || border == card_bg)
    {
        border = PluginUiBlendColors(card_bg, QColor(0, 0, 0), 0.35f);
    }

    QColor btn_bg = pal.color(QPalette::Light);
    if(!btn_bg.isValid() || btn_bg == card_bg || btn_bg == window)
    {
        btn_bg = PluginUiBlendColors(card_bg, QColor(255, 255, 255), 0.14f);
    }
    QColor btn_border = pal.color(QPalette::Mid);
    if(!btn_border.isValid() || btn_border == btn_bg)
    {
        btn_border = PluginUiBlendColors(btn_bg, QColor(0, 0, 0), 0.28f);
    }
    const QColor btn_hover   = PluginUiBlendColors(btn_bg, QColor(255, 255, 255), 0.10f);
    const QColor btn_pressed = PluginUiBlendColors(btn_bg, pal.color(QPalette::Highlight), 0.22f);

    // Stylesheet is set on the card frame itself (no shared objectName) so Available,
    // In-Scene, and room-output cards all get the same chrome reliably.
    frame->setStyleSheet(QStringLiteral(
        "QFrame {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 5px;"
        "}"
        "QToolButton {"
        "  background-color: %3;"
        "  border: 1px solid %4;"
        "  border-radius: 4px;"
        "  padding: 2px 4px;"
        "}"
        "QToolButton:hover {"
        "  background-color: %5;"
        "}"
        "QToolButton:pressed,"
        "QToolButton:checked {"
        "  background-color: %6;"
        "}"
        "QToolButton:disabled {"
        "  background-color: %1;"
        "  border-color: %2;"
        "}")
                             .arg(PluginUiColorCss(card_bg),
                                  PluginUiColorCss(border),
                                  PluginUiColorCss(btn_bg),
                                  PluginUiColorCss(btn_border),
                                  PluginUiColorCss(btn_hover),
                                  PluginUiColorCss(btn_pressed)));
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

/** Collapsible section header: Setup-tab charcoal (Button/Window), not Mid/light checked chrome. */
inline void PluginUiApplyCollapsibleSectionHeader(QToolButton* btn)
{
    if(!btn)
    {
        return;
    }

    btn->setAutoRaise(false);
    btn->setAutoFillBackground(false);
    btn->setCursor(Qt::PointingHandCursor);

    const QWidget* palette_source = btn->window() ? btn->window() : btn;
    const QPalette pal            = palette_source->palette();
    const QColor window           = pal.color(QPalette::Window);
    const QColor base             = pal.color(QPalette::Base);
    QColor bg                     = pal.color(QPalette::Button);

    // Prefer the same dark surface family as the selected Setup tab (Button / Window).
    if(!bg.isValid() || bg.lightness() > 140)
    {
        bg = window.isValid() ? window : base;
    }
    if(bg.lightness() > 140)
    {
        bg = PluginUiBlendColors(base, QColor(0, 0, 0), 0.45f);
    }

    QColor fg = pal.color(QPalette::ButtonText);
    if(!fg.isValid() || fg == bg)
    {
        fg = pal.color(QPalette::WindowText);
    }
    if(fg.lightness() < 80 && bg.lightness() < 120)
    {
        fg = PluginUiBlendColors(fg, QColor(255, 255, 255), 0.85f);
    }

    QColor border = pal.color(QPalette::Dark);
    if(!border.isValid() || border == bg)
    {
        border = PluginUiBlendColors(bg, QColor(255, 255, 255), 0.12f);
    }
    const QColor hover = PluginUiBlendColors(bg, QColor(255, 255, 255), 0.08f);

    btn->setStyleSheet(QStringLiteral(
                           "QToolButton {"
                           "  background-color: %1;"
                           "  color: %2;"
                           "  border: 1px solid %3;"
                           "  border-radius: 3px;"
                           "  padding: 5px 8px;"
                           "  text-align: left;"
                           "}"
                           "QToolButton:hover {"
                           "  background-color: %4;"
                           "  color: %2;"
                           "}"
                           "QToolButton:checked,"
                           "QToolButton:pressed {"
                           "  background-color: %1;"
                           "  color: %2;"
                           "  border: 1px solid %3;"
                           "}")
                           .arg(PluginUiColorCss(bg),
                                PluginUiColorCss(fg),
                                PluginUiColorCss(border),
                                PluginUiColorCss(hover)));
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
