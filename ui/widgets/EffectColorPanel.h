// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCOLORPANEL_H
#define EFFECTCOLORPANEL_H

#include <QWidget>

class QCheckBox;
class QHBoxLayout;
class QPushButton;

/** Rainbow mode toggle and color-stop strip (+ / − at end of row). */
class EffectColorPanel : public QWidget
{
public:
    explicit EffectColorPanel(bool rainbow_mode, QWidget* parent = nullptr);

    QCheckBox* rainbowModeCheck() const { return rainbow_mode_check_; }
    QWidget* colorButtonsWidget() const { return color_buttons_widget_; }
    QHBoxLayout* colorButtonsLayout() const { return color_buttons_layout_; }
    QPushButton* addColorButton() const { return add_color_button_; }
    QPushButton* removeColorButton() const { return remove_color_button_; }

private:
    QCheckBox* rainbow_mode_check_ = nullptr;
    QWidget* color_buttons_widget_ = nullptr;
    QHBoxLayout* color_buttons_layout_ = nullptr;
    QPushButton* add_color_button_ = nullptr;
    QPushButton* remove_color_button_ = nullptr;
};

#endif
