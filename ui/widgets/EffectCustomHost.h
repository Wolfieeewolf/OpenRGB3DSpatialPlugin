// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCUSTOMHOST_H
#define EFFECTCUSTOMHOST_H

#include <QWidget>
#include <QVBoxLayout>

/** Empty QWidget host with a tight vertical layout for effect custom panels. */
class EffectCustomHost : public QWidget
{
public:
    explicit EffectCustomHost(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
    }
};

#endif
