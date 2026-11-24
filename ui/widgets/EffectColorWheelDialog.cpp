// SPDX-License-Identifier: GPL-2.0-or-later
/* Colour stop picker using OpenRGB's ColorWheel (Qt-Plus, via OpenRGB). */

#include "EffectColorWheelDialog.h"

#include "ColorWheel.h"
#include "PluginUiUtils.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

EffectColorWheelDialog::EffectColorWheelDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Pick colour"));
    setModal(true);
    setAccessibleName(tr("Colour wheel"));

    wheel_ = new ColorWheel(this);
    wheel_->setMinimumSize(180, 180);
    wheel_->setToolTip(tr(
        "OpenRGB colour wheel — drag the ring for hue, the square for saturation/value. "
        "Keyboard: Tab to OK/Cancel, Enter accepts, Escape cancels."));
    wheel_->setAccessibleName(tr("OpenRGB colour wheel"));

    swatch_ = new QPushButton(this);
    swatch_->setFixedSize(48, 32);
    swatch_->setFocusPolicy(Qt::NoFocus);
    swatch_->setFlat(true);
    swatch_->setToolTip(tr("Current colour"));
    swatch_->setAccessibleName(tr("Current colour swatch"));
    swatch_->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* credit = new QLabel(tr("Wheel from OpenRGB"), this);
    credit->setObjectName(QStringLiteral("mutedSecondaryLabel"));
    PluginUiApplyMutedSecondaryLabel(credit);

    auto* top = new QHBoxLayout;
    top->addWidget(wheel_, 1);
    auto* side = new QVBoxLayout;
    side->addWidget(swatch_, 0, Qt::AlignLeft);
    side->addWidget(credit, 0, Qt::AlignLeft);
    side->addStretch(1);
    top->addLayout(side);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    buttons->button(QDialogButtonBox::Ok)->setAccessibleName(tr("Accept colour"));
    buttons->button(QDialogButtonBox::Cancel)->setAccessibleName(tr("Cancel colour pick"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(top);
    root->addWidget(buttons);

    connect(wheel_, &ColorWheel::colorChanged, this, &EffectColorWheelDialog::onWheelColorChanged);
    refreshSwatch(wheel_->color());
}

void EffectColorWheelDialog::setColor(const QColor& color)
{
    if(!color.isValid())
    {
        return;
    }
    wheel_->setColor(color);
    refreshSwatch(wheel_->color());
}

QColor EffectColorWheelDialog::color() const
{
    return wheel_ ? wheel_->color() : QColor();
}

bool EffectColorWheelDialog::pickColor(QWidget* parent, QColor& color, const QString& title)
{
    EffectColorWheelDialog dialog(parent);
    if(!title.isEmpty())
    {
        dialog.setWindowTitle(title);
    }
    if(color.isValid())
    {
        dialog.setColor(color);
    }
    if(dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    color = dialog.color();
    return color.isValid();
}

void EffectColorWheelDialog::onWheelColorChanged(const QColor& color)
{
    refreshSwatch(color);
}

void EffectColorWheelDialog::refreshSwatch(const QColor& color)
{
    if(!swatch_ || !color.isValid())
    {
        return;
    }
    PluginUiSetRgbSwatchButton(swatch_, color.red(), color.green(), color.blue());
}
