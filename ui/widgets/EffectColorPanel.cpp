// SPDX-License-Identifier: GPL-2.0-only

#include "EffectColorPanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

EffectColorPanel::EffectColorPanel(bool rainbow_mode, QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* color_layout = new QVBoxLayout(this);
    color_layout->setContentsMargins(0, 0, 0, 0);

    rainbow_mode_check_ = new QCheckBox(QStringLiteral("Rainbow"));
    rainbow_mode_check_->setChecked(rainbow_mode);
    rainbow_mode_check_->setToolTip(QStringLiteral("Use full-spectrum rainbow instead of manual color stops."));
    color_layout->addWidget(rainbow_mode_check_);

    color_buttons_widget_ = new QWidget();
    color_buttons_layout_ = new QHBoxLayout(color_buttons_widget_);
    color_buttons_layout_->setContentsMargins(0, 0, 0, 0);

    add_color_button_ = new QPushButton(QStringLiteral("+"));
    add_color_button_->setMaximumSize(30, 30);
    add_color_button_->setToolTip(QStringLiteral("Add a new color stop"));

    remove_color_button_ = new QPushButton(QStringLiteral("-"));
    remove_color_button_->setMaximumSize(30, 30);
    remove_color_button_->setToolTip(QStringLiteral("Remove the last color stop"));

    color_buttons_layout_->addWidget(add_color_button_);
    color_buttons_layout_->addWidget(remove_color_button_);
    color_buttons_layout_->addStretch();

    color_layout->addWidget(color_buttons_widget_);

    pattern_host_widget_ = new QWidget();
    pattern_host_layout_ = new QVBoxLayout(pattern_host_widget_);
    pattern_host_layout_->setContentsMargins(0, 0, 0, 0);
    pattern_host_layout_->setSpacing(6);
    color_layout->addWidget(pattern_host_widget_);
}
