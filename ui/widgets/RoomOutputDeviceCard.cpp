// SPDX-License-Identifier: GPL-2.0-only

#include "RoomOutputDeviceCard.h"

#include "OpenRGBPluginsFont.h"
#include "PluginUiUtils.h"

#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QToolButton>

RoomOutputDeviceCard::RoomOutputDeviceCard(const QString& title, QWidget* parent)
    : QWidget(parent),
      title_text_(title)
{
    auto* outer = new QGridLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    card_frame_ = new QFrame(this);
    card_frame_->setFrameShape(QFrame::StyledPanel);
    card_frame_->setFrameShadow(QFrame::Sunken);
    card_frame_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    auto* inner = new QHBoxLayout(card_frame_);
    inner->setContentsMargins(6, 5, 6, 5);
    inner->setSpacing(4);

    name_label_ = new QLabel(title_text_, card_frame_);
    name_label_->setWordWrap(false);
    name_label_->setMinimumWidth(0);
    name_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    inner->addWidget(name_label_, 1);

    action_button_ = new QToolButton(card_frame_);
    action_button_->setFont(OpenRGBPluginsFont::GetFont());
    action_button_->setMinimumSize(26, 0);
    action_button_->setMaximumSize(26, 16777215);
    action_button_->setToolTip(tr("Add or remove from this role"));
    connect(action_button_, &QToolButton::clicked, this, [this]() {
        if(!action_button_->isEnabled())
        {
            return;
        }
        emit actionToggled(!added_);
    });
    inner->addWidget(action_button_, 0);

    outer->addWidget(card_frame_, 0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    updateActionIcon();
    updateNameLabelLayout();
}

void RoomOutputDeviceCard::setTitle(const QString& title)
{
    title_text_ = title;
    updateNameLabelLayout();
}

void RoomOutputDeviceCard::setAdded(bool added)
{
    added_ = added;
    updateActionIcon();
}

void RoomOutputDeviceCard::setInteractionEnabled(bool enabled)
{
    if(action_button_)
    {
        action_button_->setEnabled(enabled);
    }
}

void RoomOutputDeviceCard::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateNameLabelLayout();
}

void RoomOutputDeviceCard::updateActionIcon()
{
    if(!action_button_)
    {
        return;
    }
    action_button_->setText(added_ ? OpenRGBPluginsFont::icon(OpenRGBPluginsFont::math_minus)
                                   : OpenRGBPluginsFont::icon(OpenRGBPluginsFont::math_plus));
}

int RoomOutputDeviceCard::nameLabelContentWidth() const
{
    if(!card_frame_ || !action_button_)
    {
        return 0;
    }

    auto* inner = qobject_cast<QHBoxLayout*>(card_frame_->layout());
    if(!inner)
    {
        return 0;
    }

    const QMargins margins = inner->contentsMargins();
    int width                = card_frame_->width() - margins.left() - margins.right();
    width -= action_button_->sizeHint().width() + inner->spacing();
    return std::max(0, width);
}

void RoomOutputDeviceCard::updateNameLabelLayout()
{
    if(!name_label_ || title_text_.isEmpty())
    {
        return;
    }

    name_label_->setWordWrap(true);
    name_label_->setText(title_text_);
    name_label_->setToolTip(QString());

    const int wrap_width = std::max(name_label_->width(), nameLabelContentWidth());
    if(wrap_width > 0)
    {
        const int wrapped_height = name_label_->heightForWidth(wrap_width);
        if(wrapped_height > 0)
        {
            name_label_->setMinimumHeight(wrapped_height);
        }
    }

    updateGeometry();
}
