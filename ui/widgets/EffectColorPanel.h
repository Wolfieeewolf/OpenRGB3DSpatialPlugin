// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCOLORPANEL_H
#define EFFECTCOLORPANEL_H

#include <QWidget>

class QCheckBox;
class QHBoxLayout;
class QPushButton;
class QVBoxLayout;

class EffectColorPanel : public QWidget
{
public:
    explicit EffectColorPanel(bool rainbow_mode, QWidget* parent = nullptr);

    QCheckBox* rainbowModeCheck() const { return rainbow_mode_check_; }
    QWidget* colorButtonsWidget() const { return color_buttons_widget_; }
    QHBoxLayout* colorButtonsLayout() const { return color_buttons_layout_; }
    QWidget* patternHostWidget() const { return pattern_host_widget_; }
    QVBoxLayout* patternHostLayout() const { return pattern_host_layout_; }
    QPushButton* addColorButton() const { return add_color_button_; }
    QPushButton* removeColorButton() const { return remove_color_button_; }

private:
    QCheckBox* rainbow_mode_check_ = nullptr;
    QWidget* color_buttons_widget_ = nullptr;
    QHBoxLayout* color_buttons_layout_ = nullptr;
    QWidget* pattern_host_widget_ = nullptr;
    QVBoxLayout* pattern_host_layout_ = nullptr;
    QPushButton* add_color_button_ = nullptr;
    QPushButton* remove_color_button_ = nullptr;
};

#endif
